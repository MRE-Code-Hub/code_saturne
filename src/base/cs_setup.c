/*============================================================================
 * Setup computation based on provided user data and functions.
 *============================================================================*/

/*
  This file is part of code_saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2024 EDF S.A.

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

#include "cs_defs.h"

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft_printf.h"
#include "cs_ale.h"
#include "cs_atmo.h"
#include "cs_at_data_assim.h"
#include "cs_cf_thermo.h"
#include "cs_coal_read_data.h"
#include "cs_ctwr.h"
#include "cs_domain_setup.h"
#include "cs_elec_model.h"
#include "cs_field.h"
#include "cs_field_default.h"
#include "cs_field_operator.h"
#include "cs_field_pointer.h"
#include "cs_function_default.h"
#include "cs_gui.h"
#include "cs_gui_boundary_conditions.h"
#include "cs_gui_mobile_mesh.h"
#include "cs_gui_output.h"
#include "cs_gui_radiative_transfer.h"
#include "cs_gui_specific_physics.h"
#include "cs_gwf.h"
#include "cs_ibm.h"
#include "cs_internal_coupling.h"
#include "cs_lagr.h"
#include "cs_lagr_options.h"
#include "cs_mobile_structures.h"
#include "cs_parameters.h"
#include "cs_parameters_check.h"
#include "cs_physical_properties.h"
#include "cs_porous_model.h"
#include "cs_porosity_from_scan.h"
#include "cs_post.h"
#include "cs_prototypes.h"
#include "cs_physical_constants.h"
#include "cs_physical_model.h"
#include "cs_pressure_correction.h"
#include "cs_rad_transfer.h"
#include "cs_thermal_model.h"
#include "cs_turbulence_model.h"
#include "cs_rad_transfer_options.h"
#include "cs_restart.h"
#include "cs_runaway_check.h"
#include "cs_turbomachinery.h"
#include "cs_velocity_pressure.h"
#include "cs_vof.h"
#include "cs_wall_condensation.h"
#include "cs_wall_functions.h"
#include "cs_wall_distance.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_setup.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*----------------------------------------------------------------------------
 * Local type definitions
 *----------------------------------------------------------------------------*/

/*============================================================================
 * External function prototypes
 *============================================================================*/

/* Bindings to Fortran routines */

void
cs_f_steady_laminar_flamelet_read_base(void);

void
cs_f_usipes(int *nmodpp);

void
cs_f_impini(void);

void
cs_f_indsui(void);

void
cs_f_colecd(void);

void
cs_f_fldini(void);

void
cs_f_usppmo(void);

void
cs_f_fldvar(int *nmodpp);

void
cs_f_atini1(void);

void
cs_f_solcat(int iappel);

void
cs_f_fldprp(void);

void
cs_f_usipsu(int *nmodpp);

void
cs_f_varpos(void);

void
cs_f_iniini(void);

void
cs_f_ppinii(void);

void
cs_f_ppini1(void);

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief First initialization stages
 */
/*----------------------------------------------------------------------------*/

static void
_init_setup(void)
{
  cs_log_printf
    (CS_LOG_DEFAULT,
     _("\n\n"
       "===============================================================\n\n\n"
       "                   CALCULATION PREPARATION\n"
       "                   =======================\n\n\n"
       "===============================================================\n\n\n"));

  /* File for some specific physical models */

  cs_atmo_set_meteo_file_name("meteo");

  /* Handle some reference and physical values */

  cs_fluid_properties_t *fp = cs_get_glob_fluid_properties();
  fp->pther = fp->p0;

  /* Other mappings */

  cs_f_iniini();
  cs_f_ppinii();
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add a property field.
 */
/*----------------------------------------------------------------------------*/

static int
_add_property_field
(
  const char *f_name,
  const char *f_label,
  int         dim,
  bool        has_previous
)
{
  const int keyvis = cs_field_key_id("post_vis");
  const int keylog = cs_field_key_id("log");
  const int keylbl = cs_field_key_id("label");

  cs_physical_property_define_from_field(f_name,
                                         CS_FIELD_INTENSIVE | CS_FIELD_PROPERTY,
                                         CS_MESH_LOCATION_CELLS,
                                         dim,
                                         has_previous);

  int f_id = cs_physical_property_field_id_by_name(f_name);

  cs_field_set_key_int(cs_field_by_id(f_id), keyvis, 0);
  cs_field_set_key_int(cs_field_by_id(f_id), keylog, 1);
  if (f_label != NULL)
    cs_field_set_key_str(cs_field_by_id(f_id), keylbl, f_label);

  return f_id;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add a simple variable field.
 */
/*----------------------------------------------------------------------------*/

static cs_field_t *
_add_variable_field
(
  const char *f_name,
  const char *f_label,
  int         dim
)
{
  cs_field_t *f
    = cs_field_by_id(cs_variable_field_create(f_name,
                                              f_label,
                                              CS_MESH_LOCATION_CELLS,
                                              dim));

  cs_add_variable_field_indexes(f->id);

  return f;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add a model scalar field.
 */
/*----------------------------------------------------------------------------*/

static cs_field_t *
_add_model_scalar_field
(
  const char *f_name,
  const char *f_label,
  int         dim
)
{
  cs_field_t *f
    = cs_field_by_id(cs_variable_field_create(f_name,
                                              f_label,
                                              CS_MESH_LOCATION_CELLS,
                                              dim));

  cs_add_model_field_indexes(f->id);

  return f;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Create additional fields based on user options.
 */
/*----------------------------------------------------------------------------*/

static void
_additional_fields(void)
{
  const int n_fields = cs_field_n_fields();
  cs_turb_les_model_t *turb_les_param = cs_get_glob_turb_les_model();

  int field_type = CS_FIELD_INTENSIVE | CS_FIELD_VARIABLE;

  /* Keys not stored globally */
  const int kturt = cs_field_key_id("turbulent_flux_model");
  const int kfturt = cs_field_key_id("turbulent_flux_id");
  const int kfturt_alpha = cs_field_key_id("alpha_turbulent_flux_id");
  const int keycpl = cs_field_key_id("coupled");
  const int kscavr = cs_field_key_id("first_moment_id");
  const int ks = cs_field_key_id_try("scalar_id");
  const int keyvis = cs_field_key_id("post_vis");
  const int kwgrec = cs_field_key_id("gradient_weighting_id");
  const int kivisl = cs_field_key_id("diffusivity_id");
  const int kscacp = cs_field_key_id("is_temperature");
  const int key_turb_diff = cs_field_key_id("turbulent_diffusivity_id");
  const int key_sgs_sca_coef = cs_field_key_id("sgs_scalar_flux_coef_id");
  const int key_clipping_id = cs_field_key_id("clipping_id");
  const int key_turb_schmidt = cs_field_key_id("turbulent_schmidt_id");
  const int kromsl = cs_field_key_id_try("density_id");

  /* Key id for drift scalar */
  const int keydri = cs_field_key_id("drift_scalar_model");

  /* Time interpolation ? */
  const int key_t_ext_id = cs_field_key_id("time_extrapolated");

  /* Restart file key */
  const int key_restart_id = cs_field_key_id("restart_file");

  /* Initialization */
  const int keylog = cs_field_key_id("log");
  const int keylbl = cs_field_key_id("label");

  /* Additional variable fields
     -------------------------- */

  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    int scalar_id = (ks > -1) ? cs_field_get_key_int(f, ks) : -1;
    if (scalar_id < 0)
      continue;

    cs_equation_param_t *eqp = cs_field_get_equation_param(f);
    if (eqp != NULL) {
      int post_flag = cs_field_get_key_int(f, keyvis);
      int log_flag = cs_field_get_key_int(f, keylog);
      int turb_flux_model = cs_field_get_key_int(f, kturt);
      int turb_flux_model_type = turb_flux_model / 10.;

      if (turb_flux_model_type > 0) {
        char f_tf_name[128];
        snprintf(f_tf_name, 127, "%s_turbulent_flux", f->name);
        f_tf_name[127] = '\0';
        cs_field_t *f_turb_flux = NULL;

        if (turb_flux_model_type == 3) {
          f_turb_flux = _add_variable_field(f_tf_name,
                                            f_tf_name,
                                            3);
          cs_field_set_key_int(f_turb_flux, keycpl, 1);
          cs_field_set_key_int(f_turb_flux, key_clipping_id, 1);

          /* Tensorial diffusivity */
          cs_equation_param_t *eqp_turb_flux
            = cs_field_get_equation_param(f_turb_flux);
          eqp_turb_flux->idften = CS_ANISOTROPIC_RIGHT_DIFFUSION;

          /* If variance is required */
          cs_real_t grav = cs_math_3_norm(cs_glob_physical_constants->gravity);
          if (   (   cs_glob_fluid_properties->irovar > 0
                  || cs_glob_velocity_pressure_model->idilat == 0)
              && grav > cs_math_epzero) {
            char f_var_name[128];
            snprintf(f_var_name, 127, "%s_variance", f->name);
            f_var_name[127] = '\0';

            cs_field_t *f_var = _add_model_scalar_field(f_var_name,
                                                        f_var_name,
                                                        1);
            cs_field_set_key_int(f_var, kscavr, f_id);
          }
        }
        else {
          f_turb_flux = cs_field_create(f_tf_name,
                                        CS_FIELD_INTENSIVE | CS_FIELD_PROPERTY,
                                        CS_MESH_LOCATION_CELLS,
                                        3,
                                        true);
          cs_field_set_key_int(f_turb_flux, keyvis, post_flag);
          cs_field_set_key_int(f_turb_flux, keylog, log_flag);
        }

        cs_field_set_key_int(f, kfturt, f_turb_flux->id);

        /* Elliptic Blending (AFM or DFM) */
        if (   turb_flux_model == 11
            || turb_flux_model == 21
            || turb_flux_model == 31) {
          char f_alp_name[128];
          snprintf(f_alp_name, 127, "%s_alpha", f->name);
          f_alp_name[127] = '\0';

          cs_field_t *f_alpha = _add_variable_field(f_alp_name,
                                                    f_alp_name,
                                                    1);
          cs_field_set_key_int(f_alpha, kfturt_alpha, f_turb_flux->id);

          cs_equation_param_t *eqp_alp = cs_field_get_equation_param(f_alpha);
          eqp_alp->iconv = 0;
          eqp_alp->istat = 0;
        }
      }
    }
  }

  /* Hydrostatic pressure used to update pressure BCs */
  if (cs_glob_velocity_pressure_param->icalhy == 1) {
    cs_field_t *f = _add_variable_field("hydrostatic_pressure",
                                        "Hydrostatic Pressure",
                                        1);
    cs_field_set_key_int(f, keyvis, 0);

    /* Elliptic equation (no convection, no time term) */
    cs_equation_param_t *eqp = cs_field_get_equation_param(f);
    eqp->iconv = 0;
    eqp->istat = 0;
    eqp->nswrsm = 2;
    eqp->idifft = 0;
    eqp->relaxv = 1.; /* No relaxation even for steady algorithm */
  }

  /* Head losses weighting field in case of Lagrangian deposition and
   * reentrainment model (general case in varpos, but the Lagrangian
   * options are not known yet at the call site, so we have a similar
   * code block here for this special case. */
  cs_field_t *f_dttens = cs_field_by_name_try("dttens");
  if (   cs_glob_lagr_reentrained_model->iflow > 0
      && f_dttens == NULL) {
    cs_field_t *f = cs_field_create("dttens",
                                    field_type,
                                    CS_MESH_LOCATION_CELLS,
                                    6,
                                    false);
    cs_field_set_key_int(f, keyvis, CS_POST_ON_LOCATION);
    cs_field_set_key_int(f, keylog, 1);
    cs_field_set_key_int(CS_F_(p), kwgrec, f->id);
  }

  /* Additional property fields
     --------------------------

     Add a scalar diffusivity when defined as variable.
     The kivisl key should be equal to -1 for constant diffusivity,
     and f_id for a variable diffusivity defined by field f_id
     Assuming the first field created is not a diffusivity property
     (we define variables first), f_id > 0, so we use 0 to indicate
     the diffusivity is variable but its field has not been created yet. */

  cs_field_t *thm_field = cs_thermal_model_field();

  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    int scalar_id = (ks > -1) ? cs_field_get_key_int(f, ks) : -1;
    if (scalar_id < 0)
      continue;
    int ifcvsl = cs_field_get_key_int(f, kivisl);
    int var_f_id = cs_field_get_key_int(f, kscavr);
    if (ifcvsl == 0 && var_f_id < 0) {
      /* Build name and label, using a general rule, with
       * a fixed name for temperature or enthalpy */
      const char th_name[] = "thermal";
      const char th_label[] = "Th";
      const char *f_name = f->name;
      const char *f_label = cs_field_get_label(f);
      if (f == thm_field) {
        f_name = th_name;
        f_label = th_label;
      }
      char s_name[128];
      char s_label[128];
      int iscacp = cs_field_get_key_int(f, kscacp);
      if (iscacp > 0) {
        snprintf(s_name, 127, "%s_conductivity", f_name);
        snprintf(s_label, 127, "%s Cond", f_label);
      }
      else {
        snprintf(s_name, 127, "%s_diffusivity", f_name);
        snprintf(s_label, 127, "%s Diff", f_label);
      }
      /* Special case for electric arcs: real and imaginary electric
       * conductivity is the same (and ipotr < ipoti) */
      if (   cs_glob_physical_model_flag[CS_JOULE_EFFECT] == 2
          || cs_glob_physical_model_flag[CS_JOULE_EFFECT] == 4) {
        cs_field_t *f_elec_port_r = cs_field_by_name_try("elec_port_r");
        cs_field_t *f_elec_port_i = cs_field_by_name_try("elec_port_i");
        if (f == f_elec_port_r) {
          snprintf(s_name, 127, "elec_sigma");
          snprintf(s_label, 127, "Sigma");
        }
        else if(f == f_elec_port_i) {
          int potr_ifcvsl = cs_field_get_key_int(f_elec_port_r, kivisl);
          cs_field_set_key_int(f, kivisl, potr_ifcvsl);
          continue; /* go to next scalar in loop, avoid creating a property */
        }
      }
      s_name[127] = '\0';
      s_label[127] = '\0';

      /* Now create matching property */
      int f_s_id = _add_property_field(s_name,
                                       s_label,
                                       1,
                                       false);
      cs_field_set_key_int(f, kivisl, f_s_id);
    }
  }

  /* For variances, the diffusivity is that of the associated scalar,
   * and must not be initialized first. */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    const int iscavr = cs_field_get_key_int(f, kscavr);
    if (iscavr < 0)
      continue;
    int ifcvsl = cs_field_get_key_int(cs_field_by_id(iscavr), kivisl);
    if (cs_field_is_key_set(f, kivisl) == true)
      cs_parameters_error
        (CS_ABORT_DELAYED,
         _("initial data verification"),
         _("The field %d, represents the variance\n"
           "of fluctuations of the field %d.\n"
           "The diffusivity must not be set, it will\n"
           "automatically set equal to that of the associated scalar.\n"),
         f_id, iscavr);
    else
      cs_field_set_key_int(f, kivisl, ifcvsl);
  }

  /* Add a scalar turbulent diffusivity field */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    int ifcvsl = cs_field_get_key_int(f, kivisl);
    int var_f_id = cs_field_get_key_int(f, kscavr);
    if (ifcvsl >= 0 && var_f_id < 0) {
      /* Build name and label, using a general rule, with
       * a fixed name for temperature or enthalpy */
      char s_name[128];
      char s_label[128];
      snprintf(s_name, 127, "%s_turb_diffusivity", f->name);
      snprintf(s_label, 127, "%s Turb Diff", cs_field_get_label(f));
      s_name[127] = '\0';
      s_label[127] = '\0';

      if (cs_glob_physical_model_flag[CS_COMBUSTION_SLFM] >= 0) {
        int ifm_ifcvsl = cs_field_get_key_int(CS_F_(fm), key_turb_diff);
        if (f != CS_F_(fm))
          cs_field_set_key_int(f, key_turb_diff, ifm_ifcvsl);
        continue;
      }

      /* Now create matching property */
      int f_s_id = _add_property_field(s_name,
                                       s_label,
                                       1,
                                       false);
      cs_field_set_key_int(f, key_turb_diff, f_s_id);
    }
  }

  /* For variances, the turbulent diffusivity is that of the associated scalar,
   * and must not be initialized first */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    const int iscavr = cs_field_get_key_int(f, kscavr);
    if (iscavr < 0)
      continue;
    int ifcvsl = cs_field_get_key_int(cs_field_by_id(iscavr), kivisl);
    if (cs_field_is_key_set(f, key_turb_diff) == true)
      cs_parameters_error
        (CS_ABORT_DELAYED,
         _("initial data verification"),
         _("The field %d, represents the variance\n"
           "of fluctuations of the field %d.\n"
           "The diffusivity must not be set, it will\n"
           "automatically set equal to that of the associated scalar.\n"),
         f_id, iscavr);
    else
      cs_field_set_key_int(f, key_turb_diff, ifcvsl);
  }

  if (cs_glob_turb_model->iturb == CS_TURB_LES_SMAGO_DYN) {
    /* Add a subgrid-scale scalar flux coefficient field */
    for (int f_id = 0; f_id < n_fields; f_id++) {
      cs_field_t *f = cs_field_by_id(f_id);
      if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
        continue;
      int ifcvsl = cs_field_get_key_int(f, key_sgs_sca_coef);
      int var_f_id = cs_field_get_key_int(f, kscavr);
      if (ifcvsl >= 0 && var_f_id < 0) {
        /* Build name and label using a general rule */
        char s_name[128];
        char s_label[128];
        snprintf(s_name, 127, "%s_sgs_flux_coef", f->name);
        snprintf(s_label, 127, "%s SGS Flux Coef", cs_field_get_label(f));
        s_name[127] = '\0';
        s_label[127] = '\0';

        if (cs_glob_physical_model_flag[CS_COMBUSTION_SLFM] >= 0) {
          int ifm_ifcvsl = cs_field_get_key_int(CS_F_(fm), key_sgs_sca_coef);
          if (f != CS_F_(fm))
            cs_field_set_key_int(f, key_sgs_sca_coef, ifm_ifcvsl);
          continue;
        }
        /* Now create matching property */
        int f_s_id = _add_property_field(s_name,
                                         s_label,
                                         1,
                                         false);
        cs_field_set_key_int(f, key_sgs_sca_coef, f_s_id);
      }
    }

    /* For variances, the subgrid scale flux is that of the associated scalar,
     * and must not be initialized first */
    for (int f_id = 0; f_id < n_fields; f_id++) {
      cs_field_t *f = cs_field_by_id(f_id);
      if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
        continue;
      const int iscavr = cs_field_get_key_int(f, kscavr);
      if (iscavr > -1) {
        int ifcvsl = cs_field_get_key_int(cs_field_by_id(iscavr), kivisl);
        if (cs_field_is_key_set(f, key_sgs_sca_coef) == true)
          cs_parameters_error
            (CS_ABORT_DELAYED,
             _("initial data verification"),
             _("The field %d, represents the variance\n"
               "of fluctuations of the field %d.\n"
               "The diffusivity must not be set, it will\n"
               "automatically set equal to that of the associated scalar.\n"),
             f_id, iscavr);
        else
          cs_field_set_key_int(f, key_sgs_sca_coef, ifcvsl);
      }
    }
  } /* End for CS_TURB_LES_SMAGO_DYN) */

  /* Add a scalar density when defined as variable and different from the bulk.
   * WARNING: it must be consitent with continuity equation, this is used
   * for fluid solid computation with passive scalars with different
   * density in the solid.
   * The kromsl key should be equal to -1 for constant density
   * and f_id for a variable density defined by field f_id
   * Assuming the first field created is not a density property
   * (we define variables first), f_id > 0, so we use 0 to indicate */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    int scalar_id = (ks > -1) ? cs_field_get_key_int(f, ks) : -1;
    if (scalar_id < 0)
      continue;
    int ifcvsl = cs_field_get_key_int(f, kromsl);
    int var_f_id = cs_field_get_key_int(f, kscavr);
    if (ifcvsl == 0 && var_f_id < 0) {
      char f_name[128];
      char f_label[128];
      snprintf(f_name, 127, "%s_density", f->name);
      snprintf(f_label, 127, "%s Rho", cs_field_get_label(f));
      f_name[127] = '\0';
      f_label[127] = '\0';

      /* Now create matching property */
      int f_s_id = _add_property_field(f_name,
                                       f_label,
                                       1,
                                       false);
      cs_field_set_key_int(f, kromsl, f_s_id);
    }
  }

  /* For variances, the density is that associated to the scalar,
   * and must not be initialized first */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    const int iscavr = cs_field_get_key_int(f, kscavr);
    if (iscavr < 0)
      continue;
    int ifcvsl = cs_field_get_key_int(cs_field_by_id(iscavr), kivisl);
    if (cs_field_is_key_set(f, kromsl) == true)
      cs_parameters_error
        (CS_ABORT_DELAYED,
         _("initial data verification"),
         _("The field %d, represents the variance\n"
           "of fluctuations of the field %d.\n"
           "The diffusivity must not be set, it will\n"
           "automatically set equal to that of the associated scalar.\n"),
         f_id, iscavr);
    else
      cs_field_set_key_int(f, kromsl, ifcvsl);
  }

  /* Add a scalar turbulent Schmidt field. */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    const int iscavr = cs_field_get_key_int(f, kscavr);
    if (iscavr < 0)
      continue;
    int ifcvsl = cs_field_get_key_int(f, key_turb_schmidt);
    int var_f_id = cs_field_get_key_int(f, kscavr);
    if (ifcvsl == 0 && var_f_id < 0) {
      char f_name[128];
      char f_label[128];
      snprintf(f_name, 127, "%s_turb_schmidt", f->name);
      snprintf(f_label, 127, "%s ScT", cs_field_get_label(f));
      f_name[127] = '\0';
      f_label[127] = '\0';

      /* Now create matching property */
      int f_s_id = _add_property_field(f_name,
                                       f_label,
                                       1,
                                       false);
      cs_field_set_key_int(f, key_turb_schmidt, f_s_id);
    }
  }

  /* For variances, the Schmidt is that associated to the scalar,
   * and must not be initialized first */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    const int iscavr = cs_field_get_key_int(f, kscavr);
    if (iscavr < 0)
      continue;
    int ifcvsl = cs_field_get_key_int(cs_field_by_id(iscavr), kivisl);
    if (cs_field_is_key_set(f, key_turb_schmidt) == true)
      cs_parameters_error
        (CS_ABORT_DELAYED,
         _("initial data verification"),
         _("The field %d, represents the variance\n"
           "of fluctuations of the field %d.\n"
           "The diffusivity must not be set, it will\n"
           "automatically set equal to that of the associated scalar.\n"),
         f_id, iscavr);
    else
      cs_field_set_key_int(f, key_turb_schmidt, ifcvsl);
  }

  /* Boundary roughness (may be already created by the atmospheric module) */
  if (   cs_glob_wall_functions->iwallf == 5
      || cs_glob_wall_functions->iwallf == 6) {
    cs_field_find_or_create("boundary_rougness",
                            CS_FIELD_INTENSIVE | CS_FIELD_PROPERTY,
                            CS_MESH_LOCATION_BOUNDARY_FACES,
                            1,
                            false);
  }

  /* Van Driest damping */
  if (cs_glob_turb_les_model->idries == -1) {
    if (cs_glob_turb_model->iturb == 40)
      turb_les_param->idries = 1;
    else if (   cs_glob_turb_model->iturb == CS_TURB_LES_SMAGO_DYN
             || cs_glob_turb_model->iturb == CS_TURB_LES_WALE)
      turb_les_param->idries = 0;
  }

  /* Wall distance for some turbulence models
   * and for Lagrangian multilayer deposition for DRSM models, needed for inlets */
  cs_wall_distance_options_t *wdo = cs_get_glob_wall_distance_options();
  if (   cs_glob_turb_model->iturb == CS_TURB_K_EPSILON_QUAD
      || cs_glob_turb_model->itytur == 3
      || (   cs_glob_turb_model->iturb == CS_TURB_RIJ_EPSILON_LRR
          && cs_glob_turb_rans_model->irijec == 1)
      || (cs_glob_turb_model->itytur == 4 && cs_glob_turb_les_model->idries == 1)
      || cs_glob_turb_model->iturb == CS_TURB_K_OMEGA
      || cs_glob_turb_model->iturb == CS_TURB_SPALART_ALLMARAS
      || cs_glob_lagr_reentrained_model->iflow == 1
      || cs_glob_turb_model->hybrid_turb == 4)
    wdo->need_compute = 1;

  if (wdo->need_compute == 1) {
    cs_field_t *f_wd = _add_variable_field("wall_distance",
                                           "Wall distance",
                                           1);
    cs_field_set_key_int(f_wd, key_restart_id, CS_RESTART_AUXILIARY);

    /* Elliptic equation (no convection, no time term) */
    cs_equation_param_t *eqp_wd = cs_field_get_equation_param(f_wd);
    eqp_wd->iconv = 0;
    eqp_wd->istat = 0;
    eqp_wd->nswrsm = 2;
    eqp_wd->idifft = 0;
    eqp_wd->relaxv = 1.; // No relaxation, even for steady algorithm.

    /* Working field to store value of the solved variable at the previous time step
     * if needed (ALE) */
    int model = cs_turbomachinery_get_model();
    if (cs_glob_ale != 0 || model > 0) {
      cs_field_t *f_wd_aux_pre
        = cs_field_by_id(_add_property_field("wall_distance_aux_pre",
                                             NULL,
                                             1,
                                             false));
      cs_field_set_key_int(f_wd_aux_pre, keyvis, 0);
      cs_field_set_key_int(f_wd_aux_pre, keylog, 0);
    }

    /* Dimensionless wall distance "y+"
     * non-dimensional distance \f$y^+\f$ between a given volume and the
     * closest wall, when it is necesary (LES with van Driest wall damping */
    if (   cs_glob_turb_model->itytur == 4
        && cs_glob_turb_les_model->idries == 1) {
      cs_field_t *f_yp = _add_variable_field("wall_yplus",
                                             "Wall Y+",
                                             1);
      cs_field_set_key_int(f_yp, keyvis, CS_POST_ON_LOCATION);
      cs_field_set_key_int(f_yp, keylog, 1);

      /* Pure convection (no time term) */
      cs_equation_param_t *eqp_yp = cs_field_get_equation_param(f_yp);
      eqp_yp->iconv = 1;
      eqp_yp->istat = 0;
      eqp_yp->idiff = 0;
      eqp_yp->idifft = 0;
      eqp_yp->relaxv = 1.; // No relaxation, even for steady algorithm.
      eqp_yp->blencv = 0.; // Pure upwind
      eqp_yp->epsilo = 1.e-5; // By default, not a high precision

      int drift = CS_DRIFT_SCALAR_ON + CS_DRIFT_SCALAR_ADD_DRIFT_FLUX
        + CS_DRIFT_SCALAR_IMPOSED_MASS_FLUX;
      cs_field_set_key_int(f_yp, keydri, drift);
    }
  }

  if (   cs_glob_physical_model_flag[CS_ATMOSPHERIC] >= 0
      && cs_glob_atmo_option->compute_z_ground > 0) {
    cs_field_t *f_ground = _add_variable_field("z_ground",
                                               "Z ground",
                                               1);
    cs_equation_param_t *eqp_ground = cs_field_get_equation_param(f_ground);
    eqp_ground->iconv = 1;
    eqp_ground->blencv = 0.; // Pure upwind
    eqp_ground->istat = 0;
    eqp_ground->nswrsm = 100;
    eqp_ground->epsrsm = 1.e-3;
    eqp_ground->idiff = 0;
    eqp_ground->idifft = 0;
    eqp_ground->relaxv = 1.; // No relaxation, even for steady algorithm.

    int drift = CS_DRIFT_SCALAR_ON + CS_DRIFT_SCALAR_ADD_DRIFT_FLUX
      + CS_DRIFT_SCALAR_IMPOSED_MASS_FLUX;
    cs_field_set_key_int(f_ground, keydri, drift);
  }

  if (cs_glob_atmo_option->meteo_profile >= 2) {
    cs_field_t *f = _add_variable_field("meteo_pressure",
                                        "Meteo pressure",
                                        1);

    cs_equation_param_t *eqp = cs_field_get_equation_param(f);
    eqp->iconv = 0;
    eqp->blencv = 0.; // Pure upwind
    eqp->istat = 0;
    eqp->idircl = 1;
    eqp->nswrsm = 100;
    eqp->nswrgr = 100;
    eqp->imrgra = 0;
    eqp->imligr = -1;
    eqp->epsilo = 0.000001;
    eqp->epsrsm = 1.e-3;
    eqp->epsrgr = 0.0001;
    eqp->climgr = 1.5;
    eqp->idiff = 1;
    eqp->idifft = 0;
    eqp->idften = 1;
    eqp->relaxv = 1.; // No relaxation, even for steady algorithm
    eqp->thetav = 1;

    _add_property_field("meteo_density",
                        "Meteo density",
                        1,
                        false);

    _add_property_field("meteo_temperature",
                        "Meteo temperature",
                        1,
                        false);

    _add_property_field("meteo_pot_temperature",
                        "Meteo pot temperature",
                        1,
                        false);

    if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == 2) {
      _add_property_field("meteo_humidity",
                          "Meteo humidity",
                          1,
                          false);

      _add_property_field("meteo_drop_nb",
                          "Meteo drop nb",
                          1,
                          false);
    }

    _add_property_field("meteo_velocity",
                        "Meteo velocity",
                        3,
                        false);

    _add_property_field("meteo_tke",
                        "Meteo TKE",
                        1,
                        false);

    _add_property_field("meteo_eps",
                        "Meteo epsilon",
                        1,
                        false);

    /* DRSM models, store Rxz/k */
    if (cs_glob_turb_model->itytur == 3) {
      _add_property_field("meteo_shear_anisotropy",
                          "Meteo shear anisotropy",
                          1,
                          false);
    }
  }

  if (cs_glob_porosity_from_scan_opt->compute_porosity_from_scan) {
    int f_id = _add_property_field("nb_scan_points",
                                   "nb scan points",
                                   1,
                                   false);
    cs_field_set_key_int(cs_field_by_id(f_id), keyvis, CS_POST_ON_LOCATION);

    f_id = _add_property_field("solid_roughness",
                               "solid roughness",
                               1,
                               false);
    cs_field_set_key_int(cs_field_by_id(f_id), keyvis, CS_POST_ON_LOCATION);

    f_id = _add_property_field("cell_scan_points_cog",
                               "Point centers",
                               3,
                               false);
    cs_field_set_key_int(cs_field_by_id(f_id), keyvis, CS_POST_ON_LOCATION);

    f_id = _add_property_field("cell_scan_points_color",
                               "Cell color",
                               3,
                               false);
    cs_field_set_key_int(cs_field_by_id(f_id), keyvis, CS_POST_ON_LOCATION);
  }

  /* Additional postprocessing fields
     -------------------------------- */

  /* Fields used to save postprocessing data */

  /* Boundary efforts postprocessing for immersed boundaries, create field */
  if (cs_glob_porous_model == 3) {
    cs_field_t *f = cs_field_create("immersed_pressure_force",
                                    CS_FIELD_EXTENSIVE | CS_FIELD_POSTPROCESS,
                                    CS_MESH_LOCATION_CELLS,
                                    3,
                                    false);

    if (cs_glob_turb_model->iturb != 0) {
      f = cs_field_create("immersed_boundary_uk",
                          CS_FIELD_EXTENSIVE | CS_FIELD_POSTPROCESS,
                          CS_MESH_LOCATION_CELLS,
                          1,
                          false);
      cs_field_set_key_int(f, keyvis, CS_POST_ON_LOCATION);
      f = cs_field_create("immersed_boundary_yplus",
                          CS_FIELD_EXTENSIVE | CS_FIELD_POSTPROCESS,
                          CS_MESH_LOCATION_CELLS,
                          1,
                          false);
      cs_field_set_key_int(f, keyvis, CS_POST_ON_LOCATION);
      cs_field_set_key_int(f, keylog, 1);
      f = cs_field_create("immersed_boundary_dplus",
                          CS_FIELD_EXTENSIVE | CS_FIELD_POSTPROCESS,
                          CS_MESH_LOCATION_CELLS,
                          1,
                          false);
      cs_field_set_key_int(f, keyvis, CS_POST_ON_LOCATION);
    }
  }

  /* In case of ALE or postprocessing, ensure boundary forces are tracked */
  if (cs_glob_ale >= 1) {
    cs_field_find_or_create("boundary_forces",
                            CS_FIELD_EXTENSIVE | CS_FIELD_POSTPROCESS,
                            CS_MESH_LOCATION_BOUNDARY_FACES,
                            3,
                            false);
  }

  if (   cs_glob_wall_condensation->icondb >= 0
      || cs_glob_wall_condensation->icondv >= 0) {
    int f_id = cs_field_by_name_try("yplus")->id;
    cs_field_t *f = cs_field_find_or_create("yplus",
                                            CS_FIELD_INTENSIVE | CS_FIELD_PROPERTY,
                                            CS_MESH_LOCATION_BOUNDARY_FACES,
                                            1,
                                            false);
    if (f_id < 0) { // Set some properties if the field is new
      cs_field_set_key_str(f, keylbl, "Yplus");
      cs_field_set_key_int(f, keylog, 1);
    }
  }

  /* Some mappings */
  cs_field_pointer_map_boundary();

  /* Cooling towers mapping */
  if (cs_glob_physical_model_flag[CS_COOLING_TOWERS] >= 0) {
    cs_ctwr_field_pointer_map();
  }

  cs_field_t *f_temp_b = cs_field_by_name_try("boundary_temperature");
  if (f_temp_b != NULL) {
    cs_field_t *f_temp = cs_field_by_name_try("temperature");
    if (f_temp != NULL)
      cs_field_set_key_str(f_temp_b, keylbl, cs_field_get_label(f_temp));
  }

  /* Set some field keys and number of previous values if needed
     ----------------------------------------------------------- */

  /* Density at the second previous time step for VOF algorithm
   * or dilatable algorithm */
  if (   cs_glob_vof_parameters->vof_model > 0
      || (   cs_glob_velocity_pressure_model->idilat > 1
          && cs_glob_velocity_pressure_param->ipredfl == 0)
      || cs_glob_fluid_properties->irovar == 1) {
    cs_field_set_n_time_vals(CS_F_(rho), 3);
    cs_field_set_n_time_vals(CS_F_(rho_b), 3);
  }
  else if (   cs_glob_velocity_pressure_param->icalhy > 0
           || cs_glob_fluid_properties->ipthrm == 1
           || cs_glob_physical_model_flag[CS_COMPRESSIBLE] >= 0
           || cs_glob_velocity_pressure_model->idilat > 1) {
    cs_field_set_n_time_vals(CS_F_(rho), 2);
    cs_field_set_n_time_vals(CS_F_(rho_b), 2);
  }

  /* Time extrapolation */

  /* Density */
  int t_ext = cs_field_get_key_int(CS_F_(rho), key_t_ext_id);
  if (t_ext == -1) {
    if (cs_glob_time_scheme->time_order == 1)
      t_ext = 0;
    else if (cs_glob_time_scheme->time_order == 2)
      t_ext = 0;
    cs_field_set_key_int(CS_F_(rho), key_t_ext_id, t_ext);
  }

  /* Molecular viscosity */
  t_ext = cs_field_get_key_int(CS_F_(mu), key_t_ext_id);
  if (t_ext == -1) {
    if (cs_glob_time_scheme->time_order == 1)
      t_ext = 0;
    else if (cs_glob_time_scheme->time_order == 2)
      t_ext = 0;
    cs_field_set_key_int(CS_F_(mu), key_t_ext_id, t_ext);
  }

  /* Turbulent viscosity */
  t_ext = cs_field_get_key_int(CS_F_(mu_t), key_t_ext_id);
  if (t_ext == -1) {
    if (cs_glob_time_scheme->time_order == 1)
      t_ext = 0;
    else if (cs_glob_time_scheme->time_order == 2)
      t_ext = 0;
    cs_field_set_key_int(CS_F_(mu_t), key_t_ext_id, t_ext);
  }

  /* Specific heat */
  if (CS_F_(cp) != NULL) {
    t_ext = cs_field_get_key_int(CS_F_(cp), key_t_ext_id);
    if (t_ext == -1) {
      if (cs_glob_time_scheme->time_order == 1)
        t_ext = 0;
      else if (cs_glob_time_scheme->time_order == 2)
        t_ext = 0;
    }
    cs_field_set_key_int(CS_F_(cp), key_t_ext_id, t_ext);
  }

  /* Scalar diffusivity time extrapolation */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    if (!(f->type & CS_FIELD_VARIABLE) || f->type & CS_FIELD_CDO)
      continue;
    int scalar_id = (ks > -1) ? cs_field_get_key_int(f, ks) : -1;
    if (scalar_id < 0)
      continue;
    int f_s_id = cs_field_get_key_int(f, kivisl);
    if (f_s_id >= 0) {
      t_ext = cs_field_get_key_int(cs_field_by_id(f_s_id), key_t_ext_id);
      if (t_ext == -1) {
        if (cs_glob_time_scheme->time_order == 1)
          t_ext = 0;
        else if (cs_glob_time_scheme->time_order == 2)
          t_ext = 0;
      }
      cs_field_set_key_int(CS_F_(cp), key_t_ext_id, t_ext);
    }
  }

  /* If time extrapolation, set previous values */
  for (int f_id = 0; f_id < n_fields; f_id++) {
    cs_field_t *f = cs_field_by_id(f_id);
    t_ext = cs_field_get_key_int(f, key_t_ext_id);
    if (t_ext > 0) {
      int nprev = f->n_time_vals - 1;
      if (nprev < 1)
        cs_field_set_n_time_vals(f, nprev + 1);
    }
  }

  /* Stop the calculation if needed once all checks have been done */
  cs_parameters_error_barrier();

  /* Map some field pointers (could be deone "on the fly") */
  cs_field_pointer_map_base();
  cs_field_pointer_map_boundary();
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Read specific physical model data file
 */
/*----------------------------------------------------------------------------*/

static void
_read_specific_physics_data(void)
{
  /* Diffusion flame - 3-point chemistry
   * premix flame    - EBU model
   * premix flame    - LWC model */
  if (   cs_glob_physical_model_flag[CS_COMBUSTION_3PT] != -1
      || cs_glob_physical_model_flag[CS_COMBUSTION_EBU] != -1
      || cs_glob_physical_model_flag[CS_COMBUSTION_LW] != -1)
    cs_f_colecd();

  /* Diffusion flame - steady laminar flamelet approach */
  if (cs_glob_physical_model_flag[CS_COMBUSTION_SLFM] != -1)
    cs_f_steady_laminar_flamelet_read_base();

  /* Pulverized coal combustion */
  if (cs_glob_physical_model_flag[CS_COMBUSTION_COAL] != -1) {
    cs_gui_coal_model();
    cs_coal_read_data();
  }

  /* Joule effect, electric arc or ionic conduction */
  if (   cs_glob_physical_model_flag[CS_JOULE_EFFECT] >= 1
      || cs_glob_physical_model_flag[CS_ELECTRIC_ARCS] >= 1)
    cs_electrical_model_param();
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Function calling the user-defined functions for the definition
 *        of computation parameters: we run through here in any case.
 */
/*----------------------------------------------------------------------------*/

static void
_init_user
(
  int   *nmodpp
)
{
  cs_fluid_properties_t *fluid_props = cs_get_glob_fluid_properties();

  int icondb = cs_glob_wall_condensation->icondb;
  int condv = cs_glob_wall_condensation->icondv;

  /* Check for restart and read matching time steps and notebook values */
  cs_parameters_read_restart_info();

  /* Flow model selection through GUI */
  cs_gui_physical_model_select();

  /* Flow model selection through user Fortran subroutine */
  /* Warning : This is a user function maybe no need to translate it...
   * It is only called in the case 69_PARISFOG */
  cs_f_usppmo();
  cs_wall_condensation_set_onoff_state(icondb, condv);

  /* Other model selection through GUI */
  /* ALE parameters */
  cs_gui_ale_params();

  /* Thermal model */
  cs_gui_thermal_model();

  /* Turbulence model */
  cs_gui_turb_model();

  cs_gui_cp_params();
  cs_gui_dt();
  cs_gui_hydrostatic_pressure();

  /* Gravity and Coriolis
   * Presence or not of gravity may be needed to determine whether some fields
   * are created, so this is called before cs_user_model (to provide a
   * user call site allowing to modify GUI-defined value programatically
   * before property fields are created). */
  cs_gui_physical_constants();

  /* Activate radiative transfer model */
  cs_gui_radiative_transfer_parameters();
  cs_user_radiative_transfer_parameters();

  /* Flow and other model selection through user C routines */
  cs_user_model();

  /* Set type and order of the turbulence model */
  cs_set_type_order_turbulence_model();

  /* If CDO is active, initialize the context structures for models which
   * have been activated */
  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_OFF) {
    /* Groundwater flow module */
    if (cs_gwf_is_activated())
      cs_gwf_init_model_context();
  }

  /* Activate CDO for ALE */
  if (cs_glob_ale == CS_ALE_CDO)
    cs_ale_activate();

  if (cs_glob_ale == CS_ALE_LEGACY)
    cs_gui_mobile_mesh_structures_add();

  /* Read thermomechanical data for specific physics */
  _read_specific_physics_data();

  /* Other model parameters, including user-defined scalars */
  cs_gui_user_variables();
  cs_gui_user_arrays();
  cs_gui_calculator_functions();

  /* Solid zones */
  cs_velocity_pressure_set_solid();

  /* Initialize parameters for specific physics */
  cs_rad_transfer_options();

  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_ONLY) {
    cs_f_fldvar(nmodpp);

    /* Activate pressure correction model if CDO mode is not stand-alone */
    cs_pressure_correction_model_activate();
  }

  if (cs_glob_ale != CS_ALE_NONE) {
    cs_gui_ale_diffusion_type();

    /* Add auxiliary property fields dedicated to the ALE modelling */
    cs_ale_add_property_fields();
  }

  cs_gui_laminar_viscosity();

  /* Specific physics modules
   * Note: part of what is inside ppini1 could be moved here
   * so that usipsu / cs_user_parameters can be used by the user
   * to modify default settings */

  /* Atmospheric flows */
  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] != -1)
    cs_f_atini1();

  /* Compressible flows */
  cs_gui_hydrostatic_equ_param();
  const cs_field_t *f_id = cs_field_by_name_try("velocity");
  if (f_id != NULL) {
    if (cs_glob_physical_model_flag[CS_COMPRESSIBLE] != -1)
      cs_runaway_check_define_field_max(f_id->id, 1.e5);
    else
      cs_runaway_check_define_field_max(f_id->id, 1.e4);
  }

  /* Atmospheric module */
  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] != -1) {
    /* Advanced init/allocation for the soil model */
    if (cs_glob_atmo_option->soil_cat >= 0)
      cs_f_solcat(1);
  }

  /* Initialization of global parameters */
  cs_gui_output_boundary();

  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_ONLY)
    cs_f_fldprp();

  /* Initialization of additional user parameters */
  cs_gui_checkpoint_parameters();

  cs_gui_dt_param();

  /* Local numerical options */
  cs_gui_equation_parameters();

  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_ONLY)
    cs_gui_numerical_options();

  /* Physical properties */
  cs_gui_physical_properties();

  /* Turbulence reference values (uref, almax) */
  cs_gui_turb_ref_values();

  /* Set turbulence constants according to model choices.
   * This can be overwritten by the user in cs_user_parameters() */
  cs_turb_compute_constants(-1);

  /* Scamin, scamax, turbulent flux model, diffusivities
   * (may change physical properties for some scalars, so called
   * after cs_gui_physical_properties). */
  cs_gui_scalar_model_settings();

  /* Porosity model */
  cs_gui_porous_model();

  /* Init fan */
  cs_gui_define_fans();

  /* Init error estimator */
  cs_gui_error_estimator();

  /* Initialize base evaluation functions */
  cs_function_default_define();

  /* User functions */
  cs_f_usipsu(nmodpp);
  cs_user_parameters(cs_glob_domain);

  /* If time step is local or variable, pass information to C layer, as it
   * may be needed for some field (or moment) definitions */
  if (cs_glob_time_step_options->idtvar != 0)
    cs_time_step_define_variable(1);

  if (cs_glob_time_step_options->idtvar == 2
    || cs_glob_time_step_options->idtvar == -1)
    cs_time_step_define_local(1);

  /* Initialize Fortran restarted computation flag (isuite) */
  cs_f_indsui();

  /* Default value of physical properties for the compressible model */
  if (cs_glob_physical_model_flag[CS_COMPRESSIBLE] != -1) {
    /* EOS has been set above with the GUI or in cs_user_model.
     * The variability of the thermal conductivity
     * (diffusivity_id for itempk) and the volume viscosity (iviscv) has
     * been set in fldprp.
     *
     * Compute cv0 according to chosen EOS */

    cs_real_t l_cp[1] = {fluid_props->cp0};
    cs_real_t l_xmasmr[1] = {fluid_props->xmasmr};
    cs_real_t l_cv[1] = {-1};
    cs_cf_thermo_cv(l_cp, l_xmasmr, l_cv, 1);
    fluid_props->cv0 = l_cv[0];
  }

  if (cs_glob_porosity_ibm_opt->porosity_mode > 0)
    cs_porous_model_set_model(3);

  /* Varpos
   * If CDO mode only, skip this stage */
  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_ONLY)
    cs_f_varpos();

  /* Internal coupling */
  cs_gui_internal_coupling();
  cs_user_internal_coupling();

  cs_internal_coupling_setup();

  /* Mobile structures
   * After call to cs_gui_mobile_mesh_structures_add possible
   * call by user to cs_mobile_structures_add_n_structures */
  if (cs_glob_ale != CS_ALE_NONE)
    cs_mobile_structures_setup();
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Setup computation based on provided user data and functions.
 */
/*----------------------------------------------------------------------------*/

void
cs_setup(void)
{
  /* Initialize modules before user has access */
  _init_setup();

  int nmodpp = 0;
  for (int i = 1; i < CS_N_PHYSICAL_MODEL_TYPES; i++) {
    if (cs_glob_physical_model_flag[i] > -1)
      nmodpp ++;
  }

  /* User input, variable definitions */
  _init_user(&nmodpp);

  cs_f_ppini1();

  /* Map Fortran pointers to C global data */
  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] != -1)
    cs_at_data_assim_initialize();

  /* Initialize lagr structures */
  cs_lagr_map_specific_physics();

  int have_thermal_model = 0;
  if (cs_thermal_model_field() != NULL)
    have_thermal_model = 1;

  int is_restart = cs_restart_present();
  cs_real_t dtref = cs_glob_time_step->dt_ref;
  cs_time_scheme_t *time_scheme = cs_get_glob_time_scheme();

  cs_lagr_options_definition(is_restart,
                             have_thermal_model,
                             dtref,
                             &time_scheme->iccvfg);
  cs_lagr_add_fields();

  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_ONLY) {
    /* Additional fields if not in CDO mode only */
    _additional_fields();

    /* Changes after user initialization and additional fields dependent on
     * main fields options. */
    cs_parameters_global_complete();

    cs_f_fldini();
  }

  cs_parameters_eqp_complete();

  /* Time moments called after additional creation */
  cs_gui_time_moments();
  cs_user_time_moments();

  /* GUI based boundary condition definitions */
  cs_gui_boundary_conditions_define(NULL);

  /* Some final settings */
  cs_gui_output();

  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_ONLY) {
    /* Warning: Used in 0 validation cases ? */
    cs_f_usipes(&nmodpp);

    /* Avoid a second spurious call to this function
     * called in the C part if CDO is activated */
    if (cs_glob_param_cdo_mode == CS_PARAM_CDO_MODE_OFF) {
      cs_user_boundary_conditions_setup(cs_glob_domain);
      cs_user_finalize_setup(cs_glob_domain);
    }
  }

  cs_parameters_output_complete();

  /* Coherency checks */
  if (cs_glob_param_cdo_mode != CS_PARAM_CDO_MODE_ONLY)
    cs_parameters_check();

  cs_log_printf(CS_LOG_DEFAULT,
                "\n"
                "No error detected during the data verification.\n");

  /* Print output */
  cs_f_impini();
}

/*-----------------------------------------------------------------------------*/

END_C_DECLS
