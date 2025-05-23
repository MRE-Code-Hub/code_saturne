/*============================================================================
 * Field utility functions.
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

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_error.h"
#include "bft/bft_printf.h"

#include "cdo/cs_equation_param.h"
#include "base/cs_log.h"
#include "base/cs_mem.h"
#include "base/cs_post.h"
#include "mesh/cs_mesh_location.h"
#include "pprt/cs_physical_model.h"
#include "turb/cs_turbulence_bc.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "base/cs_field.h"
#include "base/cs_field_default.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Additional doxygen documentation
 *============================================================================*/

/*!
  \file cs_field_default.cpp
        Field utility functions.
*/

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local macro definitions
 *============================================================================*/

/*============================================================================
 * Type definitions
 *============================================================================*/

/*============================================================================
 * Static global variables
 *============================================================================*/

/* Dimensions and pointers (mappable to legacy Fortran array,
   which is why this is not simply allocated locally for each field) */

static cs_lnum_t    _n_vars_bc = 0;
static cs_lnum_t    _n_b_faces = 0;
static int         *_icodcl = nullptr;
static cs_real_t   *_rcodcl = nullptr;

/*============================================================================
 * Global variables
 *============================================================================*/

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Initialize a variable field's associated \ref cs_equation_param_t
 *
 * This intermediate function is used to silence an error with
 * undefinedBehaviorSanitizer, which expects a function with a "void *"
 * argument.
 *
 * \param[in]  f_id  associated field id
 */
/*----------------------------------------------------------------------------*/

static void
_variable_equation_param_uninit(cs_field_t  *f)
{
  if (f->type & CS_FIELD_VARIABLE) {
    cs_equation_param_t *eqp = cs_field_get_equation_param(f);

    eqp->isstpc = -999;
    eqp->nswrsm = -1;
    eqp->theta = -1.;
    eqp->blencv = -1.;
    eqp->epsilo = -1.;
    eqp->epsrsm = -1.;
    eqp->relaxv = -1.;
  }
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*=============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Add a field shared between CDO and legacy schemes. This field is
 *         related to a general solved variable, with default options.
 *
 * \param[in]  name          field name
 * \param[in]  label         field default label, or empty
 * \param[in]  location_id   id of associated location
 * \param[in]  dim           field dimension
 * \param[in]  has_previous  no if lower than 1
 *
 * \return  newly defined field id
 */
/*----------------------------------------------------------------------------*/

int
cs_variable_cdo_field_create(const char  *name,
                             const char  *label,
                             int          location_id,
                             int          dim,
                             int          has_previous)
{
  int field_type = CS_FIELD_INTENSIVE | CS_FIELD_VARIABLE | CS_FIELD_CDO;

  /* If cmp_id > -1 then this is an existing field. This situation may happen
     with CDO field if a previous creation was made in the Fortran part */

  int cmp_id = cs_field_id_by_name(name);

  /* Conversion from int to bool (done in C to avoid spurious behavior with
     a boolean variable defined in the FORTRAN part */

  bool  previous = (has_previous < 1) ? false : true;
  cs_field_t *f = cs_field_find_or_create(name,
                                          field_type,
                                          location_id,
                                          dim,
                                          previous);  /* has_previous */

  if (cmp_id == -1) { /* Do not modify post_flag if the field was previously
                         created */

    const int post_flag = CS_POST_ON_LOCATION | CS_POST_MONITOR;
    cs_field_set_key_int(f, cs_field_key_id("log"), 1);
    cs_field_set_key_int(f, cs_field_key_id("post_vis"), post_flag);

    if (label != nullptr) {
      if (strlen(label) > 0)
        cs_field_set_key_str(f, cs_field_key_id("label"), label);
    }

  }

  return f->id;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add field defining a general solved variable, with default options.
 *
 * \param[in]  name         field name
 * \param[in]  label        field default label, or empty
 * \param[in]  location_id  id of associated location
 * \param[in]  dim          field dimension
 *
 * \return  newly defined field id
 */
/*----------------------------------------------------------------------------*/

int
cs_variable_field_create(const char  *name,
                         const char  *label,
                         int          location_id,
                         int          dim)
{
  int field_type = CS_FIELD_INTENSIVE | CS_FIELD_VARIABLE;

  int cmp_id = cs_field_id_by_name(name);

  if (cmp_id > -1)
    bft_error(__FILE__, __LINE__, 0,
              _("Error defining variable \"%s\";\n"
                "this name is already reserved for field with id %d."),
              name, cmp_id);

  cs_field_t *f = cs_field_create(name,
                                  field_type,
                                  location_id,
                                  dim,
                                  true);  /* has_previous */

  const int post_flag = CS_POST_ON_LOCATION | CS_POST_MONITOR;
  cs_field_set_key_int(f, cs_field_key_id("log"), 1);
  cs_field_set_key_int(f, cs_field_key_id("post_vis"), post_flag);

  if (label != nullptr) {
    if (strlen(label) > 0)
      cs_field_set_key_str(f, cs_field_key_id("label"), label);
  }

  if (dim > 1)
    cs_field_set_key_int(f, cs_field_key_id("coupled"), 1);

  if (location_id == CS_MESH_LOCATION_CELLS)
    _variable_equation_param_uninit(f);

  return f->id;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Access a field's equation parameters.
 *
 * If the equation parameters were never initialized, they will be initialized
 * based on the current defaults.
 *
 * If the field does not have associated equation paremeters (i.e. is not
 * a variable field or is a CDO field (which is referenced by but does not
 * directly reference equations), nullptr is returned.
 *
 * \param[in, out]  f  pointer to associated field
 *
 * \return  pointer to field's equation parameters, or nullptr
 */
/*----------------------------------------------------------------------------*/

cs_equation_param_t *
cs_field_get_equation_param(cs_field_t  *f)
{
  assert(f != nullptr);
  cs_equation_param_t *eqp = nullptr;

  static int k_id = -1;
  if (k_id < 0)
    k_id = cs_field_key_id_try("var_cal_opt");

  if (k_id >= 0 && (f->type & CS_FIELD_VARIABLE))
    eqp = reinterpret_cast<cs_equation_param_t *>
            (cs_field_get_key_struct_ptr(f, k_id));

  return eqp;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Access a field's equation parameters for read only.
 *
 * If the equation parameters were never initialized, the current default
 * parameters will be returned instead.
 *
 * If the field does not have associated equation parameters (i.e. is not
 * a variable field or is a CDO field (which is referenced by but does not
 * directly reference equations), nullptr is returned.
 *
 * \param[in]  f  pointer to associated field
 *
 * \return  const-qualified pointer to field's equation parameters, or nullptr
 */
/*----------------------------------------------------------------------------*/

const cs_equation_param_t *
cs_field_get_equation_param_const(const cs_field_t  *f)
{
  assert(f != nullptr);
  const cs_equation_param_t *eqp = nullptr;

  static int k_id = -1;
  if (k_id < 0)
    k_id = cs_field_key_id_try("var_cal_opt");

  if (k_id >= 0 && (f->type & CS_FIELD_VARIABLE))
    eqp = reinterpret_cast<const cs_equation_param_t *>
            (cs_field_get_key_struct_const_ptr(f, k_id));

  return eqp;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief For a given field, returns field defined as its variance, if present.
 *
 * \param[in]  f  field
 *
 * \return  pointer to matching variance (variable) field, or nullptr.
 */
/*----------------------------------------------------------------------------*/

cs_field_t *
cs_field_get_variance(const cs_field_t  *f)
{
  static int kscavr = -1;
  if (kscavr < 0)
    kscavr = cs_field_key_id("first_moment_id");

  if (kscavr >= 0) {
    const int n_fields = cs_field_n_fields();

    for (int i = 0; i < n_fields; i++) {
      cs_field_t *f_c = cs_field_by_id(i);

      if (f_c->type & CS_FIELD_VARIABLE) {
        int parent_id = cs_field_get_key_int(f_c, kscavr);
        if (parent_id == f->id)
          return f_c;
      }
    }
  }

  return nullptr;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Allocate and map boundary condition coefficients for all
 *        variable fields.
 */
/*----------------------------------------------------------------------------*/

void
cs_field_build_bc_codes_all(void)
{
  const cs_lnum_t n_b_faces
    = cs_mesh_location_get_n_elts(CS_MESH_LOCATION_BOUNDARY_FACES)[0];

  /* Determine number of solved variables */

  cs_lnum_t n_vars = 0;

  const int kv = cs_field_key_id("variable_id");
  const int n_fields = cs_field_n_fields();

  for (int f_id = 0; f_id < n_fields; f_id++) {

    const cs_field_t  *f = cs_field_by_id(f_id);

    int var_id_p1 = (f->type & CS_FIELD_VARIABLE) ?
      cs_field_get_key_int(f, kv) : 0;

    if (var_id_p1 > 0)
      var_id_p1 += f->dim - 1;

    if (var_id_p1 > n_vars)
      n_vars = var_id_p1;

  }

  /* Allocate or remap only if needed */

  if (n_vars == _n_vars_bc && n_b_faces == _n_b_faces) {
    if (   (_icodcl != nullptr && n_b_faces > 0)
        || (_icodcl == nullptr && n_b_faces == 0))
      return;
  }

  /* Update and map otherwise */

  _n_vars_bc = n_vars;
  _n_b_faces = n_b_faces;

  CS_REALLOC(_icodcl, n_vars*n_b_faces, int);
  CS_REALLOC(_rcodcl, 3*n_vars*n_b_faces, cs_real_t);

  for (int f_id = 0; f_id < n_fields; f_id++) {

    const cs_field_t  *f = cs_field_by_id(f_id);

    int var_id = (f->type & CS_FIELD_VARIABLE) ?
      cs_field_get_key_int(f, kv) -1 : -1;

    if (var_id > -1) {

      int * icodcl  = _icodcl + n_b_faces*var_id;
      cs_real_t *rcodcl1 = _rcodcl + n_b_faces*var_id;
      cs_real_t *rcodcl2 = _rcodcl + n_b_faces*(n_vars+var_id);
      cs_real_t *rcodcl3 = _rcodcl + n_b_faces*(2*n_vars+var_id);

      if (f->bc_coeffs != nullptr) {

        f->bc_coeffs->icodcl  = icodcl;
        f->bc_coeffs->rcodcl1 = rcodcl1;
        f->bc_coeffs->rcodcl2 = rcodcl2;
        f->bc_coeffs->rcodcl3 = rcodcl3;

        /* For multi-dimensional arrays, access using

           f->bcs->icodcl[coo_id*n_b_faces + face_id]

           and equivalent for icodcl, rcodc1, rcodcl2, rcodcl3. */

      }

      /* Initialize icodcl and rcodcl values to defaults
         (even if they are not mapped to C, to avoid
         issues in some Fortran initialization loops
         which do not do the appropriate checks for
         variable type. */

      for (cs_lnum_t coo_id = 0; coo_id < f->dim; coo_id++) {
        cs_lnum_t s_id = n_b_faces * coo_id;
        cs_lnum_t e_id = n_b_faces * (coo_id+1);

        for (cs_lnum_t i = s_id; i < e_id; i++) {
          icodcl[i] = 0;
          rcodcl1[i] = cs_math_infinite_r;
          rcodcl2[i] = cs_math_infinite_r;
          rcodcl3[i] = 0;
        }
      }

    }

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Deallocate and unmap boundary condition coefficients for all
 *        variable fields.
 */
/*----------------------------------------------------------------------------*/

void
cs_field_free_bc_codes_all(void)
{
  const int kv = cs_field_key_id("variable_id");
  const int n_fields = cs_field_n_fields();

  for (int f_id = 0; f_id < n_fields; f_id++) {

    const cs_field_t  *f = cs_field_by_id(f_id);

    int var_id = (f->type & CS_FIELD_VARIABLE) ?
      cs_field_get_key_int(f, kv) : -1;

    if (var_id > -1 && f->bc_coeffs != nullptr) {
      f->bc_coeffs->icodcl = nullptr;
      f->bc_coeffs->rcodcl1 = nullptr;
      if (f->bc_coeffs->_hext != nullptr) {
        const cs_real_t *rcodcl2 = f->bc_coeffs->rcodcl2;
        cs_real_t *_hext = f->bc_coeffs->_hext;
        for (cs_lnum_t i = 0; i < _n_b_faces; i++)
          _hext[i] = rcodcl2[i];
      }
      f->bc_coeffs->rcodcl2 = f->bc_coeffs->_hext;
      f->bc_coeffs->rcodcl3 = nullptr;
    }

  }

  CS_FREE(_icodcl);
  CS_FREE(_rcodcl);

  _n_vars_bc = 0;
  _n_b_faces = 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize all field BC coefficients.
 */
/*----------------------------------------------------------------------------*/

void
cs_field_map_and_init_bcs(void)
{
  const int k_turb_flux_model = cs_field_key_id("turbulent_flux_model");
  const int n_fields = cs_field_n_fields();

  /* Some fields may require BC coefficients; use 4 flags per field:
     - 0: have_flux_bc
     - 1: have_mom_bc
     - 2: have_conv_bc
     - 3: have_exch_bc */

  bool *bc_flags;
  CS_MALLOC(bc_flags, n_fields*4, bool);
  for (int i = 0; i < n_fields*4; i++)
    bc_flags[i] = false;

  if (cs_glob_physical_model_flag[CS_COMPRESSIBLE] >= 0) {
    bc_flags[cs_field_by_name("velocity")->id * 4 + 2] = true;
    bc_flags[cs_field_by_name("total_energy")->id * 4 + 2] = true;
  }

  /* BC coeffs also used for some fields which are not directly
     saved variables (to shave values between various computation
     stages). */

  const char *sp_names[] = {"pressure_increment", "lagr_time"};
  for (int i = 0; i < 2; i++) {
    const cs_field_t *f = cs_field_by_name_try(sp_names[i]);
    if (f != nullptr) {
      if (! (f->type & CS_FIELD_USER))
        bc_flags[f->id * 4] = true;
    }
  }

  /* Loop over variables
     ------------------- */

  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t  *f = cs_field_by_id(f_id);

    if (! (f->type & CS_FIELD_VARIABLE))
      continue;
    if (f->type & CS_FIELD_CDO) {
      bool continu = true;
      if (cs_glob_param_cdo_mode == CS_PARAM_CDO_MODE_NS_WITH_FV &&
          (f == cs_field_by_name("velocity") ||
           f == cs_field_by_name("pressure"))) {
        // Velocity and pressure solved with CDO
        // Turbulence solved with FV
        continu = false;
      }

      if (continu) {
        continue;
      }
    }

    bc_flags[f->id * 4] = true;

    cs_equation_param_t *eqp = cs_field_get_equation_param(f);

    if (eqp->icoupl > 0)
      bc_flags[f_id*4 + 3] = true;

    if (f->dim == 6) {
      if (strcmp(f->name, "rij") == 0)
        bc_flags[f_id*4 + 1] = true;
    }

    int turb_flux_model_type = cs_field_get_key_int(f, k_turb_flux_model) / 10;

    /* Boundary conditions of the turbulent fluxes T'u' or Y'u' */
    if (turb_flux_model_type == 3) {
      cs_field_t  *f_ut = cs_field_by_composite_name_try(f->name,
                                                         "turbulent_flux");
      bc_flags[f_ut->id*4 + 1] = true;
    }
  }

  /* Now allocate BC coefficients */

  for (int f_id = 0; f_id < n_fields; f_id++) {

    /* have flux_bc always true if other coefficients are used */

    if (bc_flags[f_id*4] == false)
      continue;

    cs_field_t  *f = cs_field_by_id(f_id);

    bool has_mom_bc  = bc_flags[f_id*4 + 1];
    bool has_conv_bc = bc_flags[f_id*4 + 2];
    bool has_exch_bc = bc_flags[f_id*4 + 3];

    cs_field_allocate_bc_coeffs(f, true, has_mom_bc, has_conv_bc, has_exch_bc);
    cs_field_init_bc_coeffs(f);

  }

  CS_FREE(bc_flags);

  /* Map pointers to BC's now that BC coefficient pointers are allocated */

  cs_turbulence_bc_init_pointers();
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return the number of defined scalar fields.
 *
 * \return number of legacy FV scalar variavble fields.
 */
/*----------------------------------------------------------------------------*/

int
cs_field_n_scalar_fields(void)
{
  int n_scal = 0;

  /* Count the number of scalars nscal */
  const int k_sca = cs_field_key_id("scalar_id");

  for (int f_id = 0; f_id < cs_field_n_fields(); f_id++) {
    cs_field_t *f     = cs_field_by_id(f_id);
    if (   f->type & CS_FIELD_VARIABLE
        && !(f->type & CS_FIELD_CDO)) {
      int i_sca = cs_field_get_key_int(f, k_sca);
      if (i_sca > 0)
        n_scal++;
    }
  }

  return n_scal;
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
