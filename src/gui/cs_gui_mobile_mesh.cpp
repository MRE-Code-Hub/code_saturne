/*============================================================================
 * Management of the GUI parameters file: mobile mesh
 *============================================================================*/

/*
  This file is part of code_saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2025 EDF S.A.

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any later
  version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
  Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/*----------------------------------------------------------------------------*/

#include "base/cs_defs.h"

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_mem.h"
#include "bft/bft_error.h"
#include "bft/bft_printf.h"

#include "base/cs_ale.h"
#include "base/cs_ast_coupling.h"
#include "base/cs_base.h"
#include "base/cs_boundary.h"
#include "base/cs_boundary_zone.h"
#include "base/cs_field_default.h"
#include "base/cs_field_pointer.h"
#include "gui/cs_gui.h"
#include "gui/cs_gui_util.h"
#include "gui/cs_gui_boundary_conditions.h"
#include "meg/cs_meg_prototypes.h"
#include "mesh/cs_mesh.h"
#include "base/cs_mobile_structures.h"
#include "base/cs_parameters.h"
#include "base/cs_timer.h"
#include "base/cs_time_step.h"
#include "base/cs_volume_zone.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "gui/cs_gui_mobile_mesh.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

/* debugging switch */
#define _XML_DEBUG_ 0

/*============================================================================
 * Static variables
 *============================================================================*/

/*-----------------------------------------------------------------------------
 * Possible values for boundary nature
 *----------------------------------------------------------------------------*/

enum ale_boundary_nature
{
  ale_boundary_nature_none,
  ale_boundary_nature_fixed_wall,
  ale_boundary_nature_sliding_wall,
  ale_boundary_nature_internal_coupling,
  ale_boundary_nature_external_aster_coupling,
  ale_boundary_nature_external_user_coupling,
  ale_boundary_nature_fixed_velocity,
  ale_boundary_nature_fixed_displacement,
  ale_boundary_nature_free_surface
};

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*-----------------------------------------------------------------------------
 * Return value for ALE method
 *
 * parameters:
 *   tn_ale    <-- tree node associated with ALE
 *
 * return:
 *   0 for isotropic, 1 for orthotropic
 *----------------------------------------------------------------------------*/

static int
_ale_visc_type(cs_tree_node_t  *tn_ale)
{
  int mvisc_type = 0;

  cs_tree_node_t *tn_mv = cs_tree_get_node(tn_ale, "mesh_viscosity");

  const char *type = cs_tree_node_get_tag(tn_mv, "type");
  if (type != nullptr) {
    if (strcmp(type, "isotrop") != 0) {
      if (strcmp(type, "orthotrop") == 0)
        mvisc_type = 1;
      else
        bft_error(__FILE__, __LINE__, 0,
                  "invalid mesh viscosity type: %s", type);
    }
  }

  return mvisc_type;
}

/*-----------------------------------------------------------------------------
 * Return value for ALE solver
 *
 * parameters:
 *   tn_ale    <-- tree node associated with ALE
 *
 * return:
 *   CS_ALE_LEGACY for legacy, CS_ALE_CDO for cdo
 *----------------------------------------------------------------------------*/

static cs_ale_type_t
_ale_solver(cs_tree_node_t *tn_ale)
{
  int ale_status = 0;
  cs_gui_node_get_status_int(tn_ale, &ale_status);

  if (ale_status == 0) {
    return CS_ALE_NONE;
  }

  cs_ale_type_t msolver = CS_ALE_LEGACY;

  cs_tree_node_t *tn_mv = cs_tree_get_node(tn_ale, "ale_solver");

  const char *type = cs_tree_node_get_tag(tn_mv, "type");
  if (type != nullptr) {
    if (strcmp(type, "legacy") != 0) {
      if (strcmp(type, "cdo") == 0)
        msolver = CS_ALE_CDO;
      else
        bft_error(__FILE__, __LINE__, 0, "invalid ALE solver: %s", type);
    }
  }

  return msolver;
}

/*-----------------------------------------------------------------------------
 * Get the ale boundary formula
 *
 * parameters:
 *   tn_w    <-- pointer to tree node for a given BC
 *   choice  <-- nature: "fixed_velocity" or "fixed_displacement"
 *----------------------------------------------------------------------------*/

static const char*
_get_ale_boundary_formula(cs_tree_node_t  *tn_w,
                          const char      *choice)
{
  cs_tree_node_t *tn = cs_tree_get_node(tn_w, "ale");
  tn = cs_tree_node_get_sibling_with_tag(tn, "choice", choice);

  const char *formula = cs_tree_node_get_child_value_str(tn, "formula");

  return formula;
}

/*-----------------------------------------------------------------------------
 * Get uialcl data for fixed displacement
 *
 * parameters:
 *   tn_w           <-- pointer to tree node for a given mobile_boundary BC
 *   z              <-- selected boundary zone
 *   impale         --> impale
 *   disale         --> disale
 *----------------------------------------------------------------------------*/

static void
_uialcl_fixed_displacement(cs_tree_node_t   *tn_w,
                           const cs_zone_t  *z,
                           int              *impale,
                           cs_real_t         disale[][3])
{
  const cs_mesh_t *m = cs_glob_mesh;

  const cs_real_3_t *face_cen = cs_glob_mesh_quantities->b_face_cog;

  /* Get formula */
  const char *formula = _get_ale_boundary_formula(tn_w, "fixed_displacement");

  if (!formula)
    bft_error(__FILE__, __LINE__, 0,
              _("Boundary nature formula is null for %s."),
              cs_gui_node_get_tag(tn_w, "label"));

  /* Evaluate formula using meg */
  cs_real_t *bc_vals = nullptr;
  CS_MALLOC(bc_vals, 3 * z->n_elts, cs_real_t);
  cs_meg_boundary_function(z->name,
                           z->n_elts,
                           z->elt_ids,
                           face_cen,
                           "mesh_velocity",
                           "fixed_displacement",
                           bc_vals);

  /* Loop over boundary faces */
  for (cs_lnum_t elt_id = 0; elt_id < z->n_elts; elt_id++) {
    const cs_lnum_t face_id = z->elt_ids[elt_id];

    /* ALE BC on vertices */
    const cs_lnum_t s = m->b_face_vtx_idx[face_id];
    const cs_lnum_t e = m->b_face_vtx_idx[face_id+1];

    /* Compute the portion of surface associated to v_id_1 */
    for (cs_lnum_t k = s; k < e; k++) {

      const cs_lnum_t v_id = m->b_face_vtx_lst[k];
      cs_real_t *_val = disale[v_id];

      impale[v_id] = 1;
      //FIXME prorata
      for (int d = 0; d < 3; d++)
        _val[d] = bc_vals[elt_id + d * z->n_elts];

    }
  }

  CS_FREE(bc_vals);
}

/*-----------------------------------------------------------------------------
 * Get uialcl data for fixed velocity
 *
 * parameters:
 *   tn_w         <-- pointer to tree node for a given mobile_boundary BC
 *   nfabor       <-- Number of boundary faces
 *   z            <-- selected boundary zone
 *----------------------------------------------------------------------------*/

static void
_uialcl_fixed_velocity(cs_tree_node_t  *tn_w,
                       const cs_zone_t *z,
                       int              ialtyb[])
{
  /* Get formula */
  const char *formula = _get_ale_boundary_formula(tn_w, "fixed_velocity");

  const cs_real_3_t *face_cen = cs_glob_mesh_quantities->b_face_cog;

  if (!formula)
    bft_error(__FILE__, __LINE__, 0,
              _("Boundary nature formula is null for %s."),
              cs_gui_node_get_tag(tn_w, "label"));

  /* Evaluate formula using meg */
  cs_real_t *bc_vals = nullptr;
  CS_MALLOC(bc_vals, 3 * z->n_elts, cs_real_t);
  cs_meg_boundary_function(z->name,
                           z->n_elts,
                           z->elt_ids,
                           face_cen,
                           "mesh_velocity",
                           "fixed_velocity",
                           bc_vals);

  /* mesh_velocity rcodcl values handled through dof_function */

  /* Loop over boundary faces */
  for (cs_lnum_t elt_id = 0; elt_id < z->n_elts; elt_id++) {
    const cs_lnum_t face_id = z->elt_ids[elt_id];
    ialtyb[face_id] = CS_BOUNDARY_ALE_IMPOSED_VEL;
  }

  /* Free memory */
  CS_FREE(bc_vals);
}

/*-----------------------------------------------------------------------------
 * Return the ale boundary nature of a given boundary condition
 *
 * parameters:
 *   tn <-> pointer to tree node for a given mobile_boundary BC
 *
 * return:
 *   associated nature
 *----------------------------------------------------------------------------*/

static enum  ale_boundary_nature
_get_ale_boundary_nature(cs_tree_node_t  *tn)
{
  const char *nat_bndy = cs_tree_node_get_tag(tn, "nature");

  if (cs_gui_strcmp(nat_bndy, "free_surface"))
    return ale_boundary_nature_free_surface;

  else {

    const char *label = cs_tree_node_get_tag(tn, "label");

    /* get the matching BC node */
    tn = cs_tree_node_get_child(tn->parent, nat_bndy);

    /* Now search from siblings */
    tn = cs_tree_node_get_sibling_with_tag(tn, "label", label);

    /* Finaly get child node ALE */
    cs_tree_node_t *tn_ale = cs_tree_get_node(tn, "ale/choice");
    const char *nat_ale = cs_tree_node_get_value_str(tn_ale);

    cs_tree_node_t *tn_solver  = cs_tree_get_node(tn, "ale/solver");
    const char     *nat_solver = cs_tree_node_get_value_str(tn_solver);

    if (cs_gui_strcmp(nat_ale, "fixed_boundary"))
      return ale_boundary_nature_fixed_wall;
    else if (cs_gui_strcmp(nat_ale, "sliding_boundary"))
      return ale_boundary_nature_sliding_wall;
    else if (cs_gui_strcmp(nat_ale, "internal_coupling"))
      return ale_boundary_nature_internal_coupling;
    else if (cs_gui_strcmp(nat_ale, "external_coupling")) {
      if (cs_gui_strcmp(nat_solver, "user")) {
        return ale_boundary_nature_external_user_coupling;
      }
      return ale_boundary_nature_external_aster_coupling;
    }
    else if (cs_gui_strcmp(nat_ale, "fixed_velocity"))
      return ale_boundary_nature_fixed_velocity;
    else if (cs_gui_strcmp(nat_ale, "fixed_displacement"))
      return ale_boundary_nature_fixed_displacement;
    else
      return ale_boundary_nature_none;
  }
}

/*-----------------------------------------------------------------------------
 * Return the ale boundary type of a given boundary condition
 *
 * parameters:
 *   tn_bndy <-- pointer to tree node for a given BC
 *
 * return:
 *   associated boundary type
 *----------------------------------------------------------------------------*/

static cs_boundary_type_t
_get_ale_boundary_type(cs_tree_node_t  *tn_bndy)
{
  const char *nat_bndy = cs_tree_node_get_tag(tn_bndy, "nature");

  if (cs_gui_strcmp(nat_bndy, "free_surface"))
    return CS_BOUNDARY_ALE_FREE_SURFACE;

  else {

    const char *label = cs_tree_node_get_tag(tn_bndy, "label");

    /* get the matching BC node */
    cs_tree_node_t *tn = cs_tree_node_get_child(tn_bndy->parent, nat_bndy);

    /* Now search from siblings */
    tn = cs_tree_node_get_sibling_with_tag(tn, "label", label);

    /* Finally get child node ALE */
    tn = cs_tree_get_node(tn, "ale/choice");
    const char *nat = cs_tree_node_get_value_str(tn);

    if (cs_gui_strcmp(nat, "fixed_boundary"))
      return CS_BOUNDARY_ALE_FIXED;
    else if (cs_gui_strcmp(nat, "sliding_boundary"))
      return CS_BOUNDARY_ALE_SLIDING;
    else if (cs_gui_strcmp(nat, "internal_coupling"))
      return CS_BOUNDARY_ALE_INTERNAL_COUPLING;
    else if (cs_gui_strcmp(nat, "external_coupling"))
      return CS_BOUNDARY_ALE_EXTERNAL_COUPLING;
    else if (cs_gui_strcmp(nat, "fixed_velocity"))
      return CS_BOUNDARY_ALE_IMPOSED_VEL;
    else if (cs_gui_strcmp(nat, "fixed_displacement"))
      return CS_BOUNDARY_ALE_IMPOSED_DISP;
    else if (cs_gui_strcmp(nat, "free_surface"))
      return CS_BOUNDARY_ALE_FREE_SURFACE;
    else
      return CS_BOUNDARY_UNDEFINED;
  }
}

/*-----------------------------------------------------------------------------
 * Retrieve internal coupling x, y and z values
 *
 * parameters:
 *   tn_ic <-- tree node for a given BC's internal coupling definitions
 *   name  <-- node name
 *   xyz   --> result matrix
 *----------------------------------------------------------------------------*/

static void
_get_internal_coupling_xyz_values(cs_tree_node_t  *tn_ic,
                                  const char      *name,
                                  double           xyz[3])
{
  cs_tree_node_t *tn = cs_tree_node_get_child(tn_ic, name);

  cs_gui_node_get_child_real(tn, "X", xyz);
  cs_gui_node_get_child_real(tn, "Y", xyz+1);
  cs_gui_node_get_child_real(tn, "Z", xyz+2);

#if _XML_DEBUG_
  bft_printf("==> %s\n", __func__);
  bft_printf("Values of %s: (X: %g, Y: %g, Z: %g)\n",
             name,
             xyz[0],
             xyz[1],
             xyz[2]);
#endif
}

/*-----------------------------------------------------------------------------
 * Retrieve data for internal coupling for a specific boundary
 *
 * parameters:
 *   label    <-- boundary label
 *   xmstru   --> Mass matrix
 *   xcstru   --> Damping matrix
 *   xkstru   --> Stiffness matrix
 *   forstr   --> Fluid force matrix
 *   istruc   <-- internal coupling boundary index
 *----------------------------------------------------------------------------*/

static void
_get_uistr2_data(const char   *label,
                 cs_real_t     xmstru[][3][3],
                 cs_real_t     xcstru[][3][3],
                 cs_real_t     xkstru[][3][3],
                 cs_real_t     forstr[][3],
                 int           istruc)
{
  /* Get mass matrix, damping matrix and stiffness matrix */

  cs_meg_fsi_struct("mass_matrix", label, nullptr,
                    (cs_real_t *)xmstru[istruc]);
  cs_meg_fsi_struct("damping_matrix", label, nullptr,
                    (cs_real_t *)xcstru[istruc]);
  cs_meg_fsi_struct("stiffness_matrix", label, nullptr,
                    (cs_real_t *)xkstru[istruc]);

  /* Set variable for fluid force vector */
  const cs_real_t fluid_f[3] = {forstr[istruc][0],
                                forstr[istruc][1],
                                forstr[istruc][2]};

  /* Get fluid force matrix */
  cs_meg_fsi_struct("fluid_force", label, fluid_f,
                    forstr[istruc]);

#if _XML_DEBUG_
  bft_printf("==> %s\n", __func__);
  bft_printf("--fluid_force = (%g, %g, %g)\n",
             forstr[istruc][0],
             forstr[istruc][1],
             forstr[istruc][2]);
  bft_printf("--mass_matrix = (%g, %g, %g)\n"
             "                (%g, %g, %g)\n"
             "                (%g, %g, %g)\n",
             xmstru[istruc][0][0], xmstru[istruc][0][1], xmstru[istruc][0][2],
             xmstru[istruc][1][0], xmstru[istruc][1][1], xmstru[istruc][1][2],
             xmstru[istruc][2][0], xmstru[istruc][2][1], xmstru[istruc][2][2]);

  bft_printf("--damping_matrix = (%g, %g, %g)\n"
             "                   (%g, %g, %g)\n"
             "                   (%g, %g, %g)\n",
             xcstru[istruc][0][0], xcstru[istruc][0][1], xcstru[istruc][0][2],
             xcstru[istruc][1][0], xcstru[istruc][1][1], xcstru[istruc][1][2],
             xcstru[istruc][2][0], xcstru[istruc][2][1], xcstru[istruc][2][2]);

  bft_printf("--stiffness_matrix = (%g, %g, %g)\n"
             "                     (%g, %g, %g)\n"
             "                     (%g, %g, %g)\n",
             xkstru[istruc][0][0], xkstru[istruc][0][1], xkstru[istruc][0][2],
             xkstru[istruc][1][0], xkstru[istruc][1][1], xkstru[istruc][1][2],
             xkstru[istruc][2][0], xkstru[istruc][2][1], xkstru[istruc][2][2]);
#endif
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * ALE related keywords
 *----------------------------------------------------------------------------*/

void
cs_gui_ale_params(void)
{
  cs_tree_node_t *tn
    = cs_tree_get_node(cs_glob_tree, "thermophysical_models/ale_method");

  cs_glob_ale = _ale_solver(tn);

  if (cs_glob_ale != CS_ALE_NONE) {
    cs_gui_node_get_child_int(tn, "fluid_initialization_sub_iterations",
                              &cs_glob_ale_n_ini_f);
    cs_gui_node_get_child_int(tn,
                              "max_iterations_implicitation",
                              &cs_glob_mobile_structures_n_iter_max);
    cs_gui_node_get_child_real(tn, "implicitation_precision",
                               &cs_glob_mobile_structures_i_eps);

    /* code_aster coupling */

    tn = cs_tree_get_node(tn, "code_aster_coupling");
    if (tn != nullptr) {
      int verbosity = cs_ast_coupling_get_verbosity();
      int visualization = cs_ast_coupling_get_visualization();
      cs_gui_node_get_child_int(tn, "verbosity", &verbosity);
      cs_gui_node_get_child_int(tn, "visualization", &visualization);
      cs_ast_coupling_set_verbosity(verbosity);
      cs_ast_coupling_set_visualization(visualization);
    }
  }

#if _XML_DEBUG_
  bft_printf("==> %s\n", __func__);
  bft_printf("--cs_glob_ale_info->type = %i\n", (int)cs_glob_ale);
  if (cs_glob_ale != CS_ALE_NONE) {
    bft_printf("--nalinf = %i\n", cs_glob_ale_n_ini_f);
    bft_printf("--nalimx = %i\n", cs_glob_mobile_structures_n_iter_max);
    bft_printf("--epalim = %g\n", cs_glob_mobile_structures_i_eps);
  }
#endif
}

/*-----------------------------------------------------------------------------
 * Return the viscosity's type of ALE method
 *
 * parameters:
 *   type --> type of viscosity's type
 *----------------------------------------------------------------------------*/

void
cs_gui_get_ale_viscosity_type(int  *type)
{
  cs_tree_node_t *tn
    = cs_tree_get_node(cs_glob_tree,
                       "thermophysical_models/ale_method");

  *type = _ale_visc_type(tn);
}

/*----------------------------------------------------------------------------
 * Set ALE diffusion type from GUI.
 *----------------------------------------------------------------------------*/

void
cs_gui_ale_diffusion_type(void)
{
  cs_tree_node_t *tn
    = cs_tree_get_node(cs_glob_tree, "thermophysical_models/ale_method");

  int iortvm = _ale_visc_type(tn);

  cs_field_t *f_mesh_u = cs_field_by_name("mesh_velocity");
  cs_equation_param_t *eqp = cs_field_get_equation_param(f_mesh_u);

  if (iortvm == 1) { /* orthotropic viscosity */
    eqp->idften = CS_ANISOTROPIC_LEFT_DIFFUSION;
  }
  else { /* isotropic viscosity */
    eqp->idften = CS_ISOTROPIC_DIFFUSION;
  }

#if _XML_DEBUG_
  bft_printf("==> %s\n", __func__);
  bft_printf("--iortvm = %i\n",  iortvm);
#endif
}

/*----------------------------------------------------------------------------
 * Mesh viscosity setting.
 *----------------------------------------------------------------------------*/

void
cs_gui_mesh_viscosity(void)
{
  cs_tree_node_t *tn0
    = cs_tree_get_node(cs_glob_tree, "thermophysical_models/ale_method");

  /* Get formula */
  const char *mvisc_expr = cs_tree_node_get_child_value_str(tn0, "formula");

  if (mvisc_expr == nullptr)
    return;

  /* Compute mesh viscosity using the MEG function.
   * The function accepts a field of arbitrary dimension, hence no need to
   * check here. The formula is generated by the code.
   */
  const cs_real_3_t *cell_cen = cs_glob_mesh_quantities->cell_cen;
  const cs_zone_t *z_all = cs_volume_zone_by_name("all_cells");
  cs_field_t *f = CS_F_(vism);
  cs_meg_volume_function(z_all->name,
                         z_all->n_elts,
                         z_all->elt_ids,
                         cell_cen,
                         f->name,
                         &(f->val));
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Translate the user settings for the domain boundaries into a
 *         structure storing the ALE boundaries (New mechanism used in CDO)
 *
 * \param[in, out]  domain   pointer to a \ref cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_gui_mobile_mesh_get_boundaries(cs_domain_t  *domain)
{
  assert(domain != nullptr);

  /* Only add xdef-based BC's for legacy fields for now, as CDO
     uses in intermediate mechanism so as to ensure face-based to
     vertex-based conditions.

     TODO: use a dedicated dof function evaluating the MEG expression
     and distributing values to vertices direclty, rather than using
     array-based DOF for this in cs_ale.c */

  cs_field_t *f_mesh_u = cs_field_by_name("mesh_velocity");
  cs_equation_param_t *eqp = cs_field_get_equation_param(f_mesh_u);

  cs_tree_node_t *tn_b0 = cs_tree_get_node(cs_glob_tree, "boundary_conditions");

  /* Loop on boundary zones */

  for (cs_tree_node_t *tn_bndy = cs_tree_node_get_child(tn_b0, "boundary");
       tn_bndy != nullptr;
       tn_bndy = cs_tree_node_get_next_of_name(tn_bndy)) {

    const char *label = cs_tree_node_get_tag(tn_bndy, "label");
    const cs_zone_t *z = cs_boundary_zone_by_name_try(label);
    if (z == nullptr)  /* possible in case of old XML file with "dead" nodes */
      continue;

    cs_boundary_type_t ale_bdy = _get_ale_boundary_type(tn_bndy);

    if (ale_bdy == CS_BOUNDARY_UNDEFINED)
      continue;

    cs_boundary_add(domain->ale_boundaries,
                    ale_bdy,
                    z->name);

    if (eqp == nullptr)
      continue;

    /* Ignore if already set (priority) */

    if (cs_equation_find_bc(eqp, z->name) != nullptr)
      continue;

    /* TODO */
    /* if (nature == ale_boundary_nature_fixed_displacement) { */
    /*   _uialcl_fixed_displacement(tn_bndy, z, */
    /*                              impale, disale); */
    /* } */

    if (ale_bdy == CS_BOUNDARY_ALE_IMPOSED_VEL) {

      cs_gui_boundary_meg_context_t *c
        = cs_gui_boundary_add_meg_context(z,
                                          f_mesh_u->name,
                                          "fixed_velocity",
                                          f_mesh_u->dim);

      cs_equation_add_bc_by_dof_func(eqp,
                                     CS_BC_DIRICHLET,
                                     z->name,
                                     cs_flag_boundary_face,
                                     cs_gui_boundary_conditions_dof_func_meg,
                                     c);

    }

  } /* Loop on mobile_boundary zones */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set mobile mesh boundary conditions based on setup.
 *
 * \param[in]    ialtyb  ALE BC type, per boundary face
 * \param[in]    impale  fixed displacement indicator
 * \param[out]   disale  fixed displacement, where indicated
 */
/*----------------------------------------------------------------------------*/

void
cs_gui_mobile_mesh_boundary_conditions(int          *const ialtyb,
                                       int          *const impale,
                                       cs_real_3_t        *disale)
{
  cs_tree_node_t *tn_b0 = cs_tree_get_node(cs_glob_tree, "boundary_conditions");

  /* Loop on boundary zones */

  for (cs_tree_node_t *tn_bndy = cs_tree_node_get_child(tn_b0, "boundary");
       tn_bndy != nullptr;
       tn_bndy = cs_tree_node_get_next_of_name(tn_bndy)) {

    const char *label = cs_tree_node_get_tag(tn_bndy, "label");

    const cs_zone_t *z = cs_boundary_zone_by_name_try(label);
    if (z == nullptr)  /* possible in case of old XML file with "dead" nodes */
      continue;

    cs_lnum_t n_faces = z->n_elts;
    const cs_lnum_t *faces_list = z->elt_ids;

    /* Get ALE nature and get sibling tn */
    enum ale_boundary_nature nature = _get_ale_boundary_nature(tn_bndy);

    if (nature == ale_boundary_nature_none)
      continue;

    /* get the matching BC node */
    const char *nat_bndy = cs_tree_node_get_tag(tn_bndy, "nature");
    cs_tree_node_t *tn_bc = cs_tree_node_get_child(tn_bndy->parent, nat_bndy);
    tn_bc = cs_tree_node_get_sibling_with_tag(tn_bc, "label", label);

    if (nature == ale_boundary_nature_fixed_wall) {
      for (cs_lnum_t ifac = 0; ifac < n_faces; ifac++) {
        cs_lnum_t ifbr = faces_list[ifac];
        ialtyb[ifbr] = CS_BOUNDARY_ALE_FIXED;
      }
    }
    else if (nature == ale_boundary_nature_sliding_wall) {
      for (cs_lnum_t ifac = 0; ifac < n_faces; ifac++) {
        cs_lnum_t ifbr = faces_list[ifac];
        ialtyb[ifbr] = CS_BOUNDARY_ALE_SLIDING;
      }
    }
    else if (nature == ale_boundary_nature_free_surface) {
      for (cs_lnum_t ifac = 0; ifac < n_faces; ifac++) {
        cs_lnum_t ifbr = faces_list[ifac];
        ialtyb[ifbr] = CS_BOUNDARY_ALE_FREE_SURFACE;
      }
    }
    else if (nature == ale_boundary_nature_fixed_displacement) {
      _uialcl_fixed_displacement(tn_bc,
                                 z,
                                 impale,
                                 disale);
    }
    else if (nature == ale_boundary_nature_fixed_velocity) {
      _uialcl_fixed_velocity(tn_bc, z, ialtyb);
    }
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return the fixed velocity for a boundary
 *
 * \param[in]  label boundary condition label
 *
 * \return a pointer to an array of cs_real_t values
 */
/*----------------------------------------------------------------------------*/

cs_real_t *
cs_gui_mobile_mesh_get_fixed_velocity(const char  *label)
{
  cs_tree_node_t *tn_b0 = cs_tree_get_node(cs_glob_tree, "boundary_conditions");

  const cs_real_3_t *face_cen = cs_glob_mesh_quantities->b_face_cog;

  /* Loop on boundary zones */

  for (cs_tree_node_t *tn_bndy = cs_tree_node_get_child(tn_b0, "boundary");
       tn_bndy != nullptr;
       tn_bndy = cs_tree_node_get_next_of_name(tn_bndy)) {

    const char *nat_bndy = cs_tree_node_get_tag(tn_bndy, "nature");
    const char *label_bndy = cs_tree_node_get_tag(tn_bndy, "label");

    /* get the matching BC node */
    cs_tree_node_t *tn = cs_tree_node_get_child(tn_bndy->parent, nat_bndy);

    /* Now search from siblings */
    tn = cs_tree_node_get_sibling_with_tag(tn, "label", label_bndy);

    if (strcmp(label_bndy, label) == 0) {

      /* Get formula */
      const char *formula = _get_ale_boundary_formula(tn, "fixed_velocity");

      if (!formula)
        bft_error(__FILE__, __LINE__, 0,
                  _("Boundary nature formula is null for %s."),
                  cs_gui_node_get_tag(tn, "label"));

      const cs_zone_t *bz = cs_boundary_zone_by_name(label);

      /* Evaluate formula using meg */
      cs_real_t *retvals = nullptr;
      CS_MALLOC(retvals, 3 * bz->n_elts, cs_real_t);
      cs_meg_boundary_function(bz->name,
                               bz->n_elts,
                               bz->elt_ids,
                               face_cen,
                               "mesh_velocity",
                               "fixed_velocity",
                               retvals);
      return retvals;

    }
  }

  return nullptr; /* avoid a compilation warning */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add mobile structures based on GUI BC definitions.
 */
/*----------------------------------------------------------------------------*/

void
cs_gui_mobile_mesh_structures_add(void)
{
  int n_i_struct = 0, n_e_struct = 0, n_a_struct = 0;

  cs_tree_node_t *tn_b0 = cs_tree_get_node(cs_glob_tree, "boundary_conditions");

  for (cs_tree_node_t *tn_bndy = cs_tree_node_get_child(tn_b0, "boundary");
       tn_bndy != nullptr;
       tn_bndy = cs_tree_node_get_next_of_name(tn_bndy)) {

    const char *label = cs_tree_node_get_tag(tn_bndy, "label");

    const cs_zone_t *z = cs_boundary_zone_by_name_try(label);
    if (z == nullptr)  /* possible in case of old XML file with "dead" nodes */
      continue;

    enum ale_boundary_nature nature = _get_ale_boundary_nature(tn_bndy);

    if (nature == ale_boundary_nature_internal_coupling) {
      n_i_struct++;
    }
    else if (nature == ale_boundary_nature_external_aster_coupling) {
      n_a_struct++;
    }
    else if (nature == ale_boundary_nature_external_user_coupling) {
      n_e_struct++;
    }
  }

  if (n_i_struct > 0)
    cs_mobile_structures_add_n_int_structures(n_i_struct);

  if (n_a_struct > 0) {
    cs_mobile_structures_add_n_ast_structures(n_a_struct);
  }

  if (n_e_struct > 0) {
    cs_mobile_structures_add_n_ext_structures(n_e_struct);
  }
}

/*-----------------------------------------------------------------------------
 * Retrieve data for internal coupling. Called once at initialization
 *
 * parameters:
 *   is_restart <-- restart or not ?
 *   aexxst   --> Displacement prediction alpha
 *   bexxst   --> Displacement prediction beta
 *   cfopre   --> Stress prediction alpha
 *   ihistr   --> Monitor point synchronisation
 *   xstr0    <-> Values of the initial displacement
 *   xstreq   <-> Values of the equilibrium displacement
 *   vstr0    <-> Values of the initial velocity
 *----------------------------------------------------------------------------*/

void
cs_gui_mobile_mesh_init_structures(bool    is_restart,
                                   double *aexxst,
                                   double *bexxst,
                                   double *cfopre,
                                   int    *ihistr,
                                   double *xstr0,
                                   double *xstreq,
                                   double *vstr0)
{
  int  istruct = 0;

  cs_tree_node_t *tn0
    = cs_tree_get_node(cs_glob_tree, "thermophysical_models/ale_method");

  /* Get advanced data */
  cs_gui_node_get_child_real(tn0, "displacement_prediction_alpha", aexxst);
  cs_gui_node_get_child_real(tn0, "displacement_prediction_beta", bexxst);
  cs_gui_node_get_child_real(tn0, "stress_prediction_alpha", cfopre);
  cs_gui_node_get_child_status_int(tn0, "monitor_point_synchronisation", ihistr);

#if _XML_DEBUG_
  bft_printf("==> %s\n", __func__);
  bft_printf("displacement_prediction_alpha: %f\n"
             "displacement_prediction_beta: %f\n"
             "stress_prediction_alpha: %f\n",
             aexxst[0],
             bexxst[0],
             cfopre[0]);
#endif

  /* Do not overwrite restart data */
  if (!is_restart) {
    cs_tree_node_t *tn = cs_tree_get_node(cs_glob_tree, "boundary_conditions");

    cs_tree_node_t *tn_b0 = cs_tree_node_get_child(tn, "boundary");

    /* At each time-step, loop on boundary faces */
    int izone = 0;

    for (tn = tn_b0; tn != nullptr;
         tn = cs_tree_node_get_next_of_name(tn), izone++) {
      const char *label = cs_tree_node_get_tag(tn, "label");

      ale_boundary_nature nature = _get_ale_boundary_nature(tn);

      /* Keep only internal coupling */
      if (nature == ale_boundary_nature_internal_coupling) {
        /* get the matching BC node */
        const char     *nat_bndy = cs_tree_node_get_tag(tn, "nature");
        cs_tree_node_t *tn_bc    = cs_tree_node_get_child(tn->parent, nat_bndy);
        tn_bc = cs_tree_node_get_sibling_with_tag(tn_bc, "label", label);

        const cs_zone_t *z = cs_boundary_zone_by_name_try(label);
        /* possible in case of old XML file with "dead" nodes */
        if (z == nullptr)
          continue;

        /* Read initial_displacement, equilibrium_displacement
           and initial_velocity */
        cs_tree_node_t *tn_ic = cs_tree_get_node(tn_bc, "ale");
        tn_ic                 = cs_tree_node_get_sibling_with_tag(tn_ic,
                                                  "choice",
                                                  "internal_coupling");
        _get_internal_coupling_xyz_values(tn_ic,
                                          "initial_displacement",
                                          &xstr0[3 * istruct]);
        _get_internal_coupling_xyz_values(tn_ic,
                                          "equilibrium_displacement",
                                          &xstreq[3 * istruct]);
        _get_internal_coupling_xyz_values(tn_ic,
                                          "initial_velocity",
                                          &vstr0[3 * istruct]);

        istruct++;
      }
    }
  }
}

/*-----------------------------------------------------------------------------
 * Retrieve data for internal coupling. Called at each step
 *
 * parameters:
 *   xmstru       --> Mass matrix
 *   xcstr        --> Damping matrix
 *   xkstru       --> Stiffness matrix
 *   forstr       --> Fluid force matrix
 *----------------------------------------------------------------------------*/

void
cs_gui_mobile_mesh_internal_structures(cs_real_t  xmstru[][3][3],
                                       cs_real_t  xcstru[][3][3],
                                       cs_real_t  xkstru[][3][3],
                                       cs_real_t  forstr[][3])
{
  int istru = 0;

  cs_tree_node_t *tn_b0 = cs_tree_get_node(cs_glob_tree, "boundary_conditions");

  /* At each time-step, loop on boundary faces */

  for (cs_tree_node_t *tn_bndy = cs_tree_node_get_child(tn_b0, "boundary");
       tn_bndy != nullptr;
       tn_bndy = cs_tree_node_get_next_of_name(tn_bndy)) {

    const char *label = cs_tree_node_get_tag(tn_bndy, "label");

    enum ale_boundary_nature nature = _get_ale_boundary_nature(tn_bndy);

    /* Keep only internal coupling */
    if (nature == ale_boundary_nature_internal_coupling) {

      /* get the matching BC node */
      const char *nat_bndy = cs_tree_node_get_tag(tn_bndy, "nature");
      cs_tree_node_t *tn_bc = cs_tree_node_get_child(tn_bndy->parent, nat_bndy);
      tn_bc = cs_tree_node_get_sibling_with_tag(tn_bc, "label", label);

      cs_tree_node_t *tn_ic = cs_tree_get_node(tn_bc, "ale");
      tn_ic = cs_tree_node_get_sibling_with_tag(tn_ic,
                                                "choice",
                                                "internal_coupling");

      _get_uistr2_data(label,
                       xmstru,
                       xcstru,
                       xkstru,
                       forstr,
                       istru);
      istru++;
    }
  }
}

/*-----------------------------------------------------------------------------
 * Retrieve structure id associated to faces for structure coupling
 *
 * parameters:
 *   idfstr    <-- structure number associated to each boundary face.
 *   idftyp    <-- structure type associated to each boundary face.
 *----------------------------------------------------------------------------*/

void
cs_gui_mobile_mesh_bc_structures(int                        *idfstr,
                                 cs_mobile_structure_type_t *idftyp)
{
  int i_struct = 0, e_struct = 0, a_struct = 0;

  cs_tree_node_t *tn = cs_tree_get_node(cs_glob_tree, "boundary_conditions");

  cs_tree_node_t *tn_b0 = cs_tree_node_get_child(tn, "boundary");
  cs_tree_node_t *tn_w0 = cs_tree_node_get_child(tn, "boundary");//FIXME

  /* Loop on boundary faces */

  for (tn = tn_b0; tn != nullptr; tn = cs_tree_node_get_next_of_name(tn)) {

    const char *label = cs_tree_node_get_tag(tn, "label");

    cs_tree_node_t *tn_w
      = cs_tree_node_get_sibling_with_tag(tn_w0, "label", label);

    enum ale_boundary_nature nature =_get_ale_boundary_nature(tn_w);

    if (nature == ale_boundary_nature_internal_coupling) {
      const cs_zone_t *z = cs_boundary_zone_by_name_try(label);
      if (z == nullptr)  /* possible in case of old XML file with "dead" nodes */
        continue;

      cs_lnum_t n_faces = z->n_elts;
      const cs_lnum_t *faces_list = z->elt_ids;

      /* Set idfstr to positive index starting at 1 */
      for (cs_lnum_t ifac = 0; ifac < n_faces; ifac++) {
        cs_lnum_t ifbr = faces_list[ifac];
        idfstr[ifbr]   = i_struct;
        idftyp[ifbr]   = CS_STRUCTURE_INTERNAL_0D;
      }
      i_struct++;
    }
    else if (nature == ale_boundary_nature_external_aster_coupling) {
      const cs_zone_t *z = cs_boundary_zone_by_name_try(label);
      if (z == nullptr) /* possible in case of old XML file with "dead" nodes */
        continue;

      cs_lnum_t        n_faces    = z->n_elts;
      const cs_lnum_t *faces_list = z->elt_ids;

      /* Set idfstr with negative value starting from -1 */
      for (cs_lnum_t ifac = 0; ifac < n_faces; ifac++) {
        cs_lnum_t ifbr = faces_list[ifac];
        idfstr[ifbr]   = a_struct;
        idftyp[ifbr]   = CS_STRUCTURE_EXTERNAL_CODE_ASTER;
      }
      a_struct++;
    }

    else if (nature == ale_boundary_nature_external_user_coupling) {
      const cs_zone_t *z = cs_boundary_zone_by_name_try(label);
      if (z == nullptr)  /* possible in case of old XML file with "dead" nodes */
        continue;

      cs_lnum_t n_faces = z->n_elts;
      const cs_lnum_t *faces_list = z->elt_ids;

      /* Set idfstr with negative value starting from -1 */
      for (cs_lnum_t ifac = 0; ifac < n_faces; ifac++) {
        cs_lnum_t ifbr = faces_list[ifac];
        idfstr[ifbr]   = e_struct;
        idftyp[ifbr]   = CS_STRUCTURE_EXTERNAL_USER;
      }
      e_struct++;
    }
  }
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
