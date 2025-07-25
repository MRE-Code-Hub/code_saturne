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

#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#if defined(HAVE_MPI)
#include <mpi.h>
#endif

#if defined(HAVE_DLOPEN)
#include <dlfcn.h>
#endif

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_mem.h"
#include "bft/bft_error.h"
#include "bft/bft_printf.h"

#include "atmo/cs_air_props.h"
#include "base/cs_array.h"
#include "atmo/cs_atmo_profile_std.h"
#include "base/cs_base.h"
#include "base/cs_boundary_conditions.h"
#include "base/cs_boundary_conditions_set_coeffs.h"
#include "base/cs_boundary_zone.h"
#include "cdo/cs_domain.h"
#include "base/cs_field.h"
#include "base/cs_field_default.h"
#include "base/cs_field_pointer.h"
#include "base/cs_halo.h"
#include "base/cs_log.h"
#include "base/cs_math.h"
#include "mesh/cs_mesh.h"
#include "mesh/cs_mesh_location.h"
#include "mesh/cs_mesh_quantities.h"
#include "base/cs_measures_util.h"
#include "base/cs_parall.h"
#include "base/cs_equation_iterative_solve.h"
#include "base/cs_parameters_check.h"
#include "base/cs_physical_constants.h"
#include "pprt/cs_physical_model.h"
#include "base/cs_post.h"
#include "base/cs_prototypes.h"
#include "rayt/cs_rad_transfer.h"
#include "base/cs_thermal_model.h"
#include "turb/cs_turbulence_bc.h"
#include "turb/cs_turbulence_model.h"
#include "base/cs_volume_zone.h"
#include "alge/cs_balance.h"
#include "alge/cs_blas.h"
#include "alge/cs_convection_diffusion.h"
#include "base/cs_parameters.h"
#include "base/cs_porous_model.h"
#include "base/cs_timer.h"
#include "alge/cs_matrix_building.h"
#include "alge/cs_sles.h"
#include "alge/cs_sles_default.h"
#include "alge/cs_face_viscosity.h"
#include "alge/cs_divergence.h"
#include "base/cs_restart.h"
#include "base/cs_restart_default.h"
#include "base/cs_velocity_pressure.h"
#include "atmo/cs_intprf.h"

#include "base/cs_porosity_from_scan.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "atmo/cs_atmo.h"
#include "atmo/cs_atmo_chemistry.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

/*=============================================================================
 * Local Type Definitions
 *============================================================================*/

/* global atmo options structure */
static cs_atmo_option_t  _atmo_option = {
  .syear = -1,
  .squant = -1,
  .shour = -1,
  .smin = -1,
  .ssec = -1.,
  .longitude = 1e12, // TODO use cs_math_big_r
  .latitude = 1e12,
  .altitude = 0.,
  .x_l93= 1e12,
  .y_l93 = 1e12,
  {.met_1d_nlevels_d = 0},
  {.met_1d_nlevels_t = 0},
  {.met_1d_ntimes = 0},
  {.met_1d_nlevels_max_t = 0},
  .radiative_model_1d = 0,
  .rad_1d_nvert = 1,
  .rad_1d_nlevels = 20,
  .rad_1d_nlevels_max = 0,
  .rad_1d_frequency = 1,
  .rad_1d_xy = nullptr,
  .rad_1d_z = nullptr,
  .rad_1d_acinfe = nullptr,
  .rad_1d_dacinfe = nullptr,
  .rad_1d_aco2 = nullptr,
  .rad_1d_aco2s = nullptr,
  .rad_1d_daco2 = nullptr,
  .rad_1d_daco2s = nullptr,
  .rad_1d_acsup = nullptr,
  .rad_1d_acsups = nullptr,
  .rad_1d_dacsup = nullptr,
  .rad_1d_dacsups = nullptr,
  .rad_1d_tauzq = nullptr,
  .rad_1d_tauz = nullptr,
  .rad_1d_zq = nullptr,
  .rad_1d_zray = nullptr,
  .rad_1d_ir_div = nullptr,
  .rad_1d_sol_div = nullptr,
  .rad_1d_iru = nullptr,
  .rad_1d_ird = nullptr,
  .rad_1d_solu = nullptr,
  .rad_1d_sold = nullptr,
  .rad_1d_qw = nullptr,
  .rad_1d_ql = nullptr,
  .rad_1d_qv = nullptr,
  .rad_1d_nc = nullptr,
  .rad_1d_fn = nullptr,
  .rad_1d_aerosols = nullptr,
  .rad_1d_albedo0 = nullptr,
  .rad_1d_emissi0 = nullptr,
  .rad_1d_temp0   = nullptr,
  .rad_1d_theta0  = nullptr,
  .rad_1d_qw0     = nullptr,
  .rad_1d_p0      = nullptr,
  .rad_1d_rho0    = nullptr,
  .domain_orientation = 0.,
  .compute_z_ground = false,
  .open_bcs_treatment = 0,
  .theo_interp = 0,
  .sedimentation_model = 0,
  .deposition_model = 0,
  .nucleation_model = 0,
  .subgrid_model = 0,
  .distribution_model = 1, /* all or nothing */
  .meteo_profile = 0, /* no meteo profile */
  .meteo_file_name = nullptr,
  .meteo_dlmo = 0.,
  .meteo_z0 = -1.,
  .meteo_zref = -1.,
  .meteo_zi = -1.,
  .meteo_zu1 = -1.,
  .meteo_zu2 = -1.,
  .meteo_zt1 = -1.,
  .meteo_zt2 = -1.,
  .meteo_uref = -1.,
  .meteo_u1 = -1.,
  .meteo_u2 = -1.,
  .meteo_ustar0 = -1.,
  .meteo_wstar0 = -1.,
  .meteo_angle = -1.,
  .meteo_t0 = 284.15, /* 11 deg C */
  .meteo_t1 = 0.,
  .meteo_t2 = 0.,
  .meteo_tstar = 0.,
  .meteo_psea = 101325.,
  .meteo_qw0 = 0.,
  .meteo_qwstar = DBL_MAX,
  .meteo_qw1 = DBL_MAX,
  .meteo_qw2 = DBL_MAX,
  .meteo_ql0 = 0.,
  .meteo_evapor = DBL_MAX,
  .meteo_sensi = DBL_MAX,
  .meteo_phim_s = 0, /* Cheng 2005 by default */
  .meteo_phih_s = 0, /* Cheng 2005 by default */
  .meteo_phim_u = 1, /* Hogstrom 1988 by default */
  .meteo_phih_u = 1, /* Hogstrom 1988 by default */
  .xyp_met    = nullptr,
  .u_met      = nullptr,
  .v_met      = nullptr,
  .w_met      = nullptr,
  .ek_met     = nullptr,
  .ep_met     = nullptr,
  .temp_met   = nullptr,
  .rho_met    = nullptr,
  .qw_met     = nullptr,
  .ndrop_met  = nullptr,
  .z_dyn_met  = nullptr,
  .z_temp_met = nullptr,
  .time_met   = nullptr,
  .hyd_p_met  = nullptr,
  .pot_t_met  = nullptr,
  .dpdt_met   = nullptr,
  .mom_met    = nullptr,
  .mom_cs     = nullptr,
  .hydrostatic_pressure_model = 0,
  .qv_profile = 0,
  .soil_model = 0, /* off or user defined */
  .soil_cat = (cs_atmo_soil_cat_t) 0, /* CS_ATMO_SOIL_5_CAT */
  .soil_zone_id = -1,
  .soil_meb_model = (cs_atmo_soil_meb_model_t) 0,
  .rain = false,
  .cloud_type = 0, /* 0 Continental, 1 Maritime */
  .accretion = false,
  .autoconversion = false,
  .autocollection_cloud = false,
  .autocollection_rain = false,
  .precipitation = false,
  .evaporation = false,
  .rupture = false,
  .soil_surf_temp = 20.0,
  .soil_temperature = 20.0,
  .soil_humidity = 0.0,
  .soil_w1_ini = 0.0,
  .soil_w2_ini = 0.0,
  .soil_cat_thermal_inertia = nullptr,
  .soil_cat_roughness = nullptr,
  .soil_cat_thermal_roughness = nullptr,
  .soil_cat_albedo = nullptr,
  .soil_cat_emissi = nullptr,
  .soil_cat_vegeta = nullptr,
  .soil_cat_w1 = nullptr,
  .soil_cat_w2 = nullptr,
  .soil_cat_r1 = nullptr,
  .soil_cat_r2 = nullptr,
  .sigc = 0.53,
  .infrared_1D_profile = -1,
  .solar_1D_profile    = -1,
  .aod_o3_tot = 0.2,
  .aod_h2o_tot = 0.1
};

static const char *_univ_fn_name[] = {N_("Cheng 2005"),
                                      N_("Hogstrom 1988"),
                                      N_("Businger 1971"),
                                      N_("Hartogensis 2007"),
                                      N_("Carl 1973")};

/* global atmo constants structure */
static cs_atmo_constants_t _atmo_constants = {
  .ps = 1.e5
};

/* atmo imbrication options structure */
static cs_atmo_imbrication_t _atmo_imbrication = {
  .imbrication_flag = false,
  .imbrication_verbose = false,
  .cressman_u = false,
  .cressman_v = false,
  .cressman_qw = false,
  .cressman_nc = false,
  .cressman_tke = false,
  .cressman_eps = false,
  .cressman_theta = false,
  .vertical_influence_radius = 100.0,
  .horizontal_influence_radius = 8500.0,
  .id_u     = -1,
  .id_v     = -1,
  .id_qw    = -1,
  .id_nc    = -1,
  .id_tke   = -1,
  .id_eps   = -1,
  .id_theta = -1
};

/*============================================================================
 * Static global variables
 *============================================================================*/

cs_atmo_option_t *cs_glob_atmo_option = &_atmo_option;

cs_atmo_constants_t *cs_glob_atmo_constants = &_atmo_constants;

cs_atmo_imbrication_t *cs_glob_atmo_imbrication = &_atmo_imbrication;

/*============================================================================
 * Prototypes for functions intended for use only by Fortran wrappers.
 * (descriptions follow, with function bodies).
 *============================================================================*/

void
cs_f_atmo_get_pointers(cs_real_t              **ps,
                       int                    **syear,
                       int                    **squant,
                       int                    **shour,
                       int                    **smin,
                       cs_real_t              **ssec,
                       cs_real_t              **longitude,
                       cs_real_t              **latitude,
                       cs_real_t              **x_l93,
                       cs_real_t              **y_l93,
                       bool                   **compute_z_ground,
                       int                    **open_bcs_treatment,
                       int                    **theo_interp,
                       int                    **sedimentation_model,
                       int                    **deposition_model,
                       int                    **nucleation_model,
                       int                    **subgrid_model,
                       int                    **distribution_model,
                       int                    **meteo_profile,
                       int                    **nbmetd,
                       int                    **nbmett,
                       int                    **nbmetm,
                       int                    **radiative_model_1d,
                       int                    **nbmaxt,
                       cs_real_t              **meteo_zi,
                       int                    **soil_model,
                       int                    **nvert,
                       int                    **kvert,
                       int                    **kmx,
                       int                    **ihpm,
                       int                    **iqv0,
                       int                    **nfatr1,
                       cs_real_t              **sigc,
                       int                    **idrayi,
                       int                    **idrayst,
                       cs_real_t              **aod_o3_tot,
                       cs_real_t              **aod_h2o_tot);

void
cs_f_atmo_arrays_get_pointers(cs_real_t **z_dyn_met,
                              cs_real_t **z_temp_met,
                              cs_real_t **xyp_met,
                              cs_real_t **u_met,
                              cs_real_t **v_met,
                              cs_real_t **w_met,
                              cs_real_t **time_met,
                              cs_real_t **hyd_p_met,
                              cs_real_t **pot_t_met,
                              cs_real_t **ek_met,
                              cs_real_t **ep_met,
                              cs_real_t **ttmet,
                              cs_real_t **rmet,
                              cs_real_t **qvmet,
                              cs_real_t **ncmet,
                              cs_real_t **dpdt_met,
                              cs_real_t **mom_met,
                              cs_real_t **mom,
                              cs_real_t **xyvert,
                              cs_real_t **zvert,
                              cs_real_t **acinfe,
                              cs_real_t **dacinfe,
                              cs_real_t **aco2,
                              cs_real_t **aco2s,
                              cs_real_t **daco2,
                              cs_real_t **daco2s,
                              cs_real_t **acsup,
                              cs_real_t **acsups,
                              cs_real_t **dacsup,
                              cs_real_t **dacsups,
                              cs_real_t **tauzq,
                              cs_real_t **tauz,
                              cs_real_t **zq,
                              cs_real_t **zray,
                              cs_real_t **rayi,
                              cs_real_t **rayst,
                              cs_real_t **iru,
                              cs_real_t **ird,
                              cs_real_t **solu,
                              cs_real_t **sold,
                              cs_real_t **soil_albedo,
                              cs_real_t **soil_emissi,
                              cs_real_t **soil_ttsoil,
                              cs_real_t **soil_tpsoil,
                              cs_real_t **soil_totwat,
                              cs_real_t **soil_pressure,
                              cs_real_t **soil_density,
                              int         dim_nd_nt[2],
                              int         dim_ntx_nt[2],
                              int         dim_nd_3[2],
                              int         dim_nt_3[2],
                              int         dim_xyvert[2],
                              int         dim_kmx2[2],
                              int         dim_kmx_nvert[2]);

void
cs_f_atmo_rad_1d_arrays_get_pointers(cs_real_t **qwvert,
                                     cs_real_t **qlvert,
                                     cs_real_t **qvvert,
                                     cs_real_t **ncvert,
                                     cs_real_t **fnvert,
                                     cs_real_t **aevert);

void
cs_f_atmo_get_soil_zone(cs_lnum_t         *n_elts,
                        int               *n_soil_cat,
                        const cs_lnum_t  **elt_ids);

void
cs_f_atmo_get_pointers_imbrication(bool      **imbrication_flag,
                                   bool      **imbrication_verbose,
                                   bool      **cressman_u,
                                   bool      **cressman_v,
                                   bool      **cressman_qw,
                                   bool      **cressman_nc,
                                   bool      **cressman_tke,
                                   bool      **cressman_eps,
                                   bool      **cressman_theta,
                                   cs_real_t **vertical_influence_radius,
                                   cs_real_t **horizontal_influence_radius,
                                   int       **id_u,
                                   int       **id_v,
                                   int       **id_qw,
                                   int       **id_nc,
                                   int       **id_tke,
                                   int       **id_eps,
                                   int       **id_theta);

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief This auxiliary function solve a Poisson equation for hydrostatic
 *  pressure :
 *  \f$ \divs ( \grad P ) = \divs ( f ) \f$
 * \param[in]      f_ext       external forcing
 * \param[in]      dfext       external forcing increment
 * \param[in]      name        field name
 * \param[in]      min_z       Minimum altitude of the domain
 * \param[in]      p_ground    Pressure at the minimum altitude
 * \param[in,out]  i_massflux  Internal mass flux
 * \param[in,out]  b_massflux  Boundary mass flux
 * \param[in,out]  i_viscm     Internal face viscosity
 * \param[in,out]  i_viscm     Boundary face viscosity
 * \param[in,out]  dam         Working array
 * \param[in,out]  xam         Working array
 * \param[in,out]  dpvar       Pressure increment
 * \param[in,out]  rhs         Working array
 */
/*----------------------------------------------------------------------------*/

static void
_hydrostatic_pressure_compute(cs_real_3_t  f_ext[],
                              cs_real_3_t  dfext[],
                              cs_real_t    pvar[],
                              const char  *name,
                              cs_real_t    min_z,
                              cs_real_t    p_ground,
                              cs_real_t    i_massflux[],
                              cs_real_t    b_massflux[],
                              cs_real_t    i_viscm[],
                              cs_real_t    b_viscm[],
                              cs_real_t    dam[],
                              cs_real_t    xam[],
                              cs_real_t    dpvar[],
                              cs_real_t    rhs[])

{
  /* Local variables */
  cs_domain_t *domain = cs_glob_domain;
  cs_mesh_t *m = domain->mesh;
  cs_mesh_quantities_t *mq = cs_glob_mesh_quantities;
  const cs_real_3_t *restrict b_face_cog = mq->b_face_cog;
  cs_field_t *f = cs_field_by_name(name);
  cs_equation_param_t *eqp_p = cs_field_get_equation_param(f);
  int f_id = f->id;
  int niterf;

  cs_real_t *coefa = f->bc_coeffs->a;
  cs_real_t *coefb = f->bc_coeffs->b;
  cs_real_t *cofaf = f->bc_coeffs->af;
  cs_real_t *cofbf = f->bc_coeffs->bf;

  /*==========================================================================
   * 0.  Initialization
   *==========================================================================*/

  /* solving info */
  cs_solving_info_t *sinfo = nullptr;
  int key_sinfo_id = cs_field_key_id("solving_info");
  if (f_id > -1) {
    f = cs_field_by_id(f_id);
    sinfo = (cs_solving_info_t *)cs_field_get_key_struct_ptr(f, key_sinfo_id);
    sinfo->n_it = 0;
  }

  /* Symmetric matrix, except if advection */
  int isym = 1;
  bool symmetric = true;

  cs_real_3_t *next_fext;
  CS_MALLOC_HD(next_fext, m->n_cells_with_ghosts, cs_real_3_t, cs_alloc_mode);
  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {
    next_fext[cell_id][0] = f_ext[cell_id][0] + dfext[cell_id][0];
    next_fext[cell_id][1] = f_ext[cell_id][1] + dfext[cell_id][1];
    next_fext[cell_id][2] = f_ext[cell_id][2] + dfext[cell_id][2];
  }

  /* Handle parallelism and periodicity */
  if (m->halo != nullptr)
    cs_halo_sync(m->halo, false, next_fext);

  /* Boundary conditions
     =================== */

  eqp_p->ndircl = 0;

  /* To solve hydrostatic pressure:
   * p_ground on the lowest face, homogeneous Neumann everywhere else */
  for (cs_lnum_t face_id = 0; face_id < m->n_b_faces; face_id++) {
    cs_real_t hint = 1. / mq->b_dist[face_id];
    cs_real_t qimp = 0.;
    if ((b_face_cog[face_id][2] - min_z) < 0.005) {//FIXME dimensionless value
      cs_real_t pimp = p_ground;
      cs_boundary_conditions_set_dirichlet_scalar(coefa[face_id],
                                                  cofaf[face_id],
                                                  coefb[face_id],
                                                  cofbf[face_id],
                                                  pimp,
                                                  hint,
                                                  cs_math_big_r);
      eqp_p->ndircl = 1;
    }
    else {
      cs_boundary_conditions_set_neumann_scalar(coefa[face_id],
                                                cofaf[face_id],
                                                coefb[face_id],
                                                cofbf[face_id],
                                                qimp,
                                                hint);
    }
  }

  cs_parall_max(1, CS_INT_TYPE, &(eqp_p->ndircl));
  cs_real_t *rovsdt;
  CS_MALLOC_HD(rovsdt, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells_with_ghosts; cell_id++)
    rovsdt[cell_id] = 0.;

  /* Faces viscosity */
  cs_real_t *c_visc;
  CS_MALLOC_HD(c_visc, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells_with_ghosts; cell_id++)
    c_visc[cell_id] = 1.;

  cs_face_viscosity(m, mq, eqp_p->imvisf, c_visc, i_viscm, b_viscm);

  cs_matrix_wrapper(eqp_p->iconv,
                    eqp_p->idiff,
                    eqp_p->ndircl,
                    isym,
                    eqp_p->theta,
                    0,
                    f->bc_coeffs,
                    rovsdt,
                    i_massflux,
                    b_massflux,
                    i_viscm,
                    b_viscm,
                    nullptr,
                    dam,
                    xam);

  /* Right hand side
     =============== */

  cs_ext_force_flux(m,
                    mq,
                    1, /* init */
                    eqp_p->nswrgr,
                    next_fext,
                    f->bc_coeffs->bf,
                    i_massflux,
                    b_massflux,
                    i_viscm,
                    b_viscm,
                    c_visc,
                    c_visc,
                    c_visc);

  cs_real_t *divergfext;
  CS_MALLOC_HD(divergfext, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  cs_divergence(m,
                1, /* init */
                i_massflux,
                b_massflux,
                divergfext);

  /* --- Right hand side residual */
  cs_real_t rnorm = sqrt(cs_gdot(m->n_cells, divergfext, divergfext));
  cs_real_t residu = rnorm;

  /* Log */
  if (sinfo != nullptr)
    sinfo->rhs_norm = residu;

  /* Initial Right-Hand-Side */
  cs_diffusion_potential(f_id,
                         m,
                         mq,
                         1, /* init */
                         1, /* inc */
                         eqp_p->imrgra,
                         eqp_p->nswrgr,
                         eqp_p->imligr,
                         1, /* iphydp */
                         eqp_p->iwgrec,
                         eqp_p->verbosity,
                         eqp_p->epsrgr,
                         eqp_p->climgr,
                         next_fext,
                         pvar,
                         f->bc_coeffs,
                         i_viscm,
                         b_viscm,
                         c_visc,
                         rhs);

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++)
    rhs[cell_id] = - divergfext[cell_id] - rhs[cell_id];

  cs_real_t ressol = 0;
  for (int sweep = 0;
       sweep < eqp_p->nswrsm && residu > (rnorm * eqp_p->epsrsm);
       sweep++) {

    for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++)
      dpvar[cell_id] = 0.;

    ressol = residu;

    cs_sles_solve_native(f_id,
                         nullptr,
                         symmetric,
                         1, /* db_size */
                         1, /* eb_size */
                         dam,
                         xam,
                         eqp_p->epsilo,
                         rnorm,
                         &niterf,
                         &ressol,
                         rhs,
                         dpvar);

    /* Log */
    if (sinfo != nullptr)
      sinfo->n_it += niterf;

    /* Update variable and right-hand-side */
    for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++)
      pvar[cell_id] += dpvar[cell_id];

    cs_diffusion_potential(f_id,
                           m,
                           mq,
                           1, /* init */
                           1, /* inc */
                           eqp_p->imrgra,
                           eqp_p->nswrgr,
                           eqp_p->imligr,
                           1, /* iphydp */
                           eqp_p->iwgrec,
                           eqp_p->verbosity,
                           eqp_p->epsrgr,
                           eqp_p->climgr,
                           next_fext,
                           pvar,
                           f->bc_coeffs,
                           i_viscm,
                           b_viscm,
                           c_visc,
                           rhs);

    for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++)
      rhs[cell_id] = - divergfext[cell_id] - rhs[cell_id];

    /* --- Convergence test */
    residu = sqrt(cs_gdot(m->n_cells, rhs, rhs));

    /* Writing */
    if (eqp_p->verbosity >= 2) {
      bft_printf("%s: CV_DIF_TS, IT: %d, Res: %12.5e, Norm: %12.5e\n",
                 name, sweep, residu, rnorm);
      bft_printf("%s: Current reconstruction sweep: %d, "
                 "Iterations for solver: %d\n", name, sweep, niterf);
    }

  }

  /* For log */
  if (sinfo != nullptr) {
    if (rnorm > 0.)
      sinfo->res_norm = residu/rnorm;
    else
      sinfo->res_norm = 0.;
  }

  cs_sles_free_native(f_id, "");

  CS_FREE_HD(divergfext);
  CS_FREE_HD(next_fext);
  CS_FREE_HD(rovsdt);
  CS_FREE_HD(c_visc);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Convert coordinates from Lambert-93 to WGS84 inside atmo options
 */
/*----------------------------------------------------------------------------*/

static void
_convert_from_l93_to_wgs84(void)
{
  //computation from https://georezo.net/forum/viewtopic.php?id=94465
  cs_real_t c = 11754255.426096; // projection constant
  cs_real_t e = 0.0818191910428158; // ellipsoid excentricity
  cs_real_t n = 0.725607765053267; // projection exponent
  cs_real_t xs = 700000; // projection's pole x-coordinate
  cs_real_t ys = 12655612.049876;  // projection's pole y-coordinate
  cs_real_t a = (log(c/(sqrt(  cs_math_pow2(cs_glob_atmo_option->x_l93-xs)
                             + cs_math_pow2(cs_glob_atmo_option->y_l93-ys))))/n);

  double t1 = a + e*atanh(e*(tanh(a+e*atanh(e*sin(1)))));
  double t2 = e*tanh(a+e*atanh(e*(tanh(t1))));
  double t3 = a+e*atanh(e*tanh(a+e*atanh(e*tanh(a+e*atanh(t2)))));

  cs_glob_atmo_option->longitude = ((atan(-(cs_glob_atmo_option->x_l93-xs)
                                          /(cs_glob_atmo_option->y_l93-ys)))/n
                                    + 3.*cs_math_pi/180.)/cs_math_pi*180.;
  cs_glob_atmo_option->latitude
    = asin(tanh((log(c/sqrt(  cs_math_pow2(cs_glob_atmo_option->x_l93-xs)
                            + cs_math_pow2(cs_glob_atmo_option->y_l93-ys)))/n)
                +e*atanh(e*tanh(t3))))/cs_math_pi*180.;

  cs_real_t lambda_0 = 3.; // longitude of the reference meridian in degrees

  // domain_orientation is the angle between the geographic north and the positive y-axis
  // in the direct (right-handed) frame of code_saturne.
  cs_glob_atmo_option->domain_orientation = n * (cs_glob_atmo_option->longitude - lambda_0);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Convert coordinates from WGS84 to Lambert-93 inside atmo options
 */
/*----------------------------------------------------------------------------*/

static void
_convert_from_wgs84_to_l93(void)
{
  //computation from https://georezo.net/forum/viewtopic.php?id=94465
  cs_real_t  c = 11754255.426096; // projection constant
  cs_real_t  e = 0.0818191910428158; // ellipsoid excentricity
  cs_real_t  n = 0.725607765053267; // projection exponent
  cs_real_t  xs = 700000; // projection's pole x-coordinate
  cs_real_t  ys = 12655612.049876;  // projection's pole y-coordinate

  //latitude in rad
  cs_real_t lat_rad = cs_glob_atmo_option->latitude*cs_math_pi / 180;
  // isometric latitude
  cs_real_t lat_iso = atanh(sin(lat_rad)) - e*atanh(e*sin(lat_rad));

  cs_glob_atmo_option->x_l93= ((c*exp(-n*(lat_iso)))
                               *sin(n*(cs_glob_atmo_option->longitude-3)
                                    *cs_math_pi/180)+xs);
  cs_glob_atmo_option->y_l93= (ys-(c*exp(-n*(lat_iso)))
                               *cos(n*(cs_glob_atmo_option->longitude-3)
                                    *cs_math_pi/180));

  cs_real_t lambda_0 = 3.; // longitude of the reference meridian in degrees

  // domain_orientation is the angle between the geographic north and the positive y-axis
  // in the direct (right-handed) frame of code_saturne.
  cs_glob_atmo_option->domain_orientation = n* (cs_glob_atmo_option->longitude - lambda_0);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal functions, for neutral
 *        (derivative function Phi_m and Phi_h)
 *
 * \param[in]  z             altitude
 * \param[in]  dlmo          Inverse Monin Obukhov length
 *
 * \return coef
 */
/*----------------------------------------------------------------------------*/

static cs_real_t
_mo_phim_n([[maybe_unused]] cs_real_t  z,
           [[maybe_unused]] cs_real_t  dlmo)
{
  return 1.0;
}

static cs_real_t
_mo_phih_n([[maybe_unused]] cs_real_t  z,
           [[maybe_unused]] cs_real_t  dlmo,
           cs_real_t                   prt)
{
  return prt;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal functions, for neutral
 *        (Integrated version from z0 to z)
 *
 * \param[in]  z             altitude
 * \param[in]  z0            altitude of the starting point integration
 * \param[in]  dlmo          Inverse Monin Obukhov length
 *
 * \return coef
 */
/*----------------------------------------------------------------------------*/

static cs_real_t
_mo_psim_n(cs_real_t                   z,
           cs_real_t                   z0,
           [[maybe_unused]] cs_real_t  dlmo)
{
  return log(z/z0);
}

static cs_real_t
_mo_psih_n(cs_real_t                   z,
           cs_real_t                   z0,
           [[maybe_unused]] cs_real_t  dlmo,
           cs_real_t                   prt)
{
  return prt*log(z/z0);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal functions for stable
 *        (derivative function Phi_m and Phi_h)
 *
 * \param[in]  z     altitude
 * \param[in]  dlmo  inverse Monin Obukhov length
 *
 * \return coef
 */
/*----------------------------------------------------------------------------*/

static cs_real_t
_mo_phim_s(cs_real_t  z,
           cs_real_t  dlmo)
{
  cs_real_t x = z * dlmo;

  switch(cs_glob_atmo_option->meteo_phim_s) {

  case CS_ATMO_UNIV_FN_CHENG:
    {
      cs_real_t a = 6.1;
      cs_real_t b = 2.5;

      return 1. + a*(x+(pow(x,b))*( pow(1.+pow(x,b),(1.-b)/b) ))
             / (x+ pow(1.+ pow(x,b),1./b));
    }

  case CS_ATMO_UNIV_FN_HOGSTROM:
    {
      if (x < 0.5) {
        cs_real_t b = 4.8;

        return 1. + b*x;
      }
      else if (x < 10.) {
        cs_real_t a = 7.9;

        return a - 4.25/x + 1./pow(x,2.);
      }
      else {
        cs_real_t a = 0.7485;

        return a*x;
      }
    }

  case CS_ATMO_UNIV_FN_BUSINGER:
    {
      if (x < 0.5) {
        cs_real_t b = 4.7;

        return 1. + b*x;
      }
      else if (x < 10.) {
        cs_real_t a = 7.85;

        return a - 4.25/x + 1./pow(x,2.);
      }
      else {
        cs_real_t a = 0.7435;

        return a*x;
      }
    }

  case CS_ATMO_UNIV_FN_HARTOGENSIS:
    {
      cs_real_t a = 1.;
      cs_real_t b = 2./3.;
      cs_real_t c = 5.;
      cs_real_t d = 0.35;

      return 1. + x*(a + b*exp(-d*x) - b*d*(x - c/d)*exp(-d*x));
    }

  default:
    assert(0);
    return -1;
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal functions for unstable
 *        (derivative function)
 *
 * \param[in]  z             altitude
 * \param[in]  dlmo          inverse Monin Obukhov length
 *
 * \return coef
 */
/*----------------------------------------------------------------------------*/

static cs_real_t
_mo_phih_s(cs_real_t              z,
           cs_real_t              dlmo,
           cs_real_t              prt)
{
  cs_real_t x = z * dlmo;

  switch(cs_glob_atmo_option->meteo_phih_s) {

    case CS_ATMO_UNIV_FN_CHENG:
      {
        cs_real_t a = 5.3;
        cs_real_t b = 1.1;

        return prt+a*(x+(pow(x,b))*(pow((1.+pow(x,b)), ((1.-b)/b))))
          / (x + pow((1.+pow(x,b)),1./b));
      }

  case CS_ATMO_UNIV_FN_HOGSTROM:
    {
      cs_real_t a = prt; /* Prt should be 0.95 to be coherent with Hogstrom */
      cs_real_t b = 7.8;

      return a + b*x;
    }

  case CS_ATMO_UNIV_FN_BUSINGER:
    {
      cs_real_t a = prt; /* Prt should be 0.74 to be coherent with Businger */
      cs_real_t b = 4.7;

      return a + b*x;
    }

  case CS_ATMO_UNIV_FN_HARTOGENSIS:
    {
      cs_real_t a = 1.;
      cs_real_t b = 2./3.;
      cs_real_t c = 5.;
      cs_real_t d = 0.35;

      /* Note: prt should be 1. to be coherent with Hartogensis */
      return prt + x*(a*sqrt((1. + 2./3. * a * x))
             + b*exp(-d*x) - b*d*(x - c/d)*exp(-d*x));
    }

  default:
    assert(0);
    return -1;
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal functions for unstable
 *        (derivative function)
 *
 * \param[in]  z             altitude
 * \param[in]  dlmo          inverse Monin Obukhov length
 *
 * \return coef
 */
/*----------------------------------------------------------------------------*/

static cs_real_t
_mo_phim_u(cs_real_t              z,
           cs_real_t              dlmo)
{
  cs_real_t x = z * dlmo;

  switch (cs_glob_atmo_option->meteo_phim_u) {

  case  CS_ATMO_UNIV_FN_HOGSTROM:
    {
      cs_real_t a = 1.;
      cs_real_t b = 19.3;
      cs_real_t e = -0.25;

      return a*pow((1.-b*x),e);
    }

  case CS_ATMO_UNIV_FN_BUSINGER:
    {
      cs_real_t a = 1.;
      cs_real_t b = 15.;
      cs_real_t e = -0.25;

      return a*pow((1.-b*x),e);
    }

  case CS_ATMO_UNIV_FN_CARL:
    {
      cs_real_t a = 1.;
      cs_real_t b = 16.;
      cs_real_t e = -1./3.;

      return a*pow((1.-b*x),e);
    }

  default:
    assert(0);
    return -1;
  }
}

static cs_real_t
_mo_phih_u(cs_real_t              z,
           cs_real_t              dlmo,
           cs_real_t              prt)
{
  cs_real_t x = z * dlmo;

  switch(cs_glob_atmo_option->meteo_phih_u) {

  case CS_ATMO_UNIV_FN_HOGSTROM:
    {
      /* Note Prt should be 0.95 to be coherent with Hogstrom */
      cs_real_t a = prt;
      cs_real_t b = 11.6;
      cs_real_t e = -0.5;

      return a*pow(1.-b*x, e);
    }

  case CS_ATMO_UNIV_FN_BUSINGER:
    {
      /* Note: Prt should be 0.74 to be coherent with Businger */
      cs_real_t a = prt;
      cs_real_t b = 9.;
      cs_real_t e = -0.5;

      return a*pow(1.-b*x, e);
    }

  case CS_ATMO_UNIV_FN_CARL:
    {
      /* Note: Prt should be 0.74 to be coherent with Carl */
      cs_real_t a = prt;
      cs_real_t b = 16.;
      cs_real_t e = -0.5;

      return a*pow(1.-b*x, e);
    }

  default:
    assert(0);
    return -1;
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal functions for stable
 *        (integral functions Psi_m and Psi_h, integrated version from z0 to z)
 *
 * \param[in]  z             altitude
 * \param[in]  z0            altitude of the starting point integration
 * \param[in]  dlmo          inverse Monin Obukhov length
 *
 * \return coef
 */
/*----------------------------------------------------------------------------*/

static cs_real_t
_mo_psim_s(cs_real_t              z,
           cs_real_t              z0,
           cs_real_t              dlmo)
{
  cs_real_t x = z * dlmo;
  cs_real_t x0 = z0 * dlmo;

  switch(cs_glob_atmo_option->meteo_phim_s) {

  case CS_ATMO_UNIV_FN_CHENG:
    {
      cs_real_t a = 6.1;
      cs_real_t b = 2.5;

      return log(z/z0) + a*log(x + pow((1. + pow(x,b)),1./b))
         - a*log(x0 + pow((1. + pow(x0,b)),1./b));
    }

    case CS_ATMO_UNIV_FN_HOGSTROM:
      {
        if (x < 0.5) {
          cs_real_t b = 4.8;

          return log(z/z0) + b*(x - x0);
        }
        else if (x < 10.) {
          cs_real_t a = 7.9;
          cs_real_t b = 4.8;
          cs_real_t c = 4.1;

          return a*log(2.*x) + 4.25/x - 0.5/pow(x,2.) - log(2.*x0) - b*x0 - c;
        }
        else {
          cs_real_t a = 0.7485;
          cs_real_t b = 7.9;
          cs_real_t c = 4.8;

          return a*x + b*log(20.) - 11.165 - log(2.*x0) - c*x0;
        }
      }

  case CS_ATMO_UNIV_FN_BUSINGER:
    {
      if (x < 0.5) {
        cs_real_t b = 4.7;

        return log(z/z0) + b*(x - x0);
      }
      else if (x < 10.) {
        cs_real_t a = 7.85;
        cs_real_t b = 4.7;
        cs_real_t c = 4.15;

        return a*log(2.*x) + 4.25/x - 0.5/pow(x,2.) - log(2.*x0) - b*x0 - c;
      }
      else {
        cs_real_t a = 0.7435;
        cs_real_t b = 7.85;
        cs_real_t c = 4.7;

        return a*x + b*log(20.) - 11.165 - log(2.*x0) - c*x0;
      }
    }

  case CS_ATMO_UNIV_FN_HARTOGENSIS:
    {
      cs_real_t a = 1.;
      cs_real_t b = 2./3.;
      cs_real_t c = 5.;
      cs_real_t d = 0.35;

      return log(z/z0) + a*(x - x0)
             + b*(x - c/d)*exp(-d*x) - b*(x0 - c/d)*exp(-d*x0);
    }

  default:
    assert(0);
    return -1;
  }
}

static cs_real_t
_mo_psih_s(cs_real_t              z,
           cs_real_t              z0,
           cs_real_t              dlmo,
           cs_real_t              prt)
{
  cs_real_t x = z * dlmo;
  cs_real_t x0 = z0 * dlmo;

  switch(cs_glob_atmo_option->meteo_phih_s) {

  case CS_ATMO_UNIV_FN_CHENG:
    {
      cs_real_t a = 5.3;
      cs_real_t b = 1.1;

      return prt*log(z/z0) + a*log(x + pow((1. + pow(x,b)),1./b))
        - a*log(x0 + pow((1. + pow(x0,b)),1./b));
    }

  case CS_ATMO_UNIV_FN_HOGSTROM:
    {
      cs_real_t a = prt; /* Prt should be 0.95 to be coherent with Hogstrom */
      cs_real_t b = 7.8;

      return a*log(z/z0) + b*(x - x0);
    }

  case CS_ATMO_UNIV_FN_BUSINGER:
    {
      cs_real_t a = prt; /* Prt should be 0.74 to be coherent with Businger */
      cs_real_t b = 4.7;

      return a*log(z/z0) + b*(x - x0);
    }

  case CS_ATMO_UNIV_FN_HARTOGENSIS:
    {
      cs_real_t a = 1.;
      cs_real_t b = 2./3.;
      cs_real_t c = 5.;
      cs_real_t d = 0.35;

      /* Note: prt should be 1. to be coherent with Hartogensis */
      return prt*log(z/z0) + pow((1. + 2./3. * a * x),3./2.)
        + b*(x - c/d)*exp(-d*x) - pow((1. + 2./3. * a * x0),3./2.)
        - b*(x0 - c/d)*exp(-d*x0);
    }

  default:
    assert(0);
    return -1;
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal functions for unstable
 *        (integral functions Psi_m and Psi_h, integrated version from z0 to z)
 *
 * \param[in]  z             altitude
 * \param[in]  z0            altitude of the starting point integration
 * \param[in]  dlmo          inverse Monin Obukhov length
 *
 * \return coef
 */
/*----------------------------------------------------------------------------*/

static cs_real_t
_mo_psim_u(cs_real_t              z,
           cs_real_t              z0,
           cs_real_t              dlmo)
{
  switch (cs_glob_atmo_option->meteo_phim_u) {

    case CS_ATMO_UNIV_FN_HOGSTROM:
      {
        cs_real_t b = 19.3;
        cs_real_t e = 0.25;
        cs_real_t x = pow((1. - b*z*dlmo), e);
        cs_real_t x0 = pow((1. - b*z0*dlmo), e);

        return log(z/z0) - 2.*log((1. + x)/(1. + x0))
          - log((1. + pow(x,2.))/(1. + pow(x0,2.))) + 2.*atan(x) - 2.*atan(x0);
      }

  case CS_ATMO_UNIV_FN_BUSINGER:
    {
      cs_real_t b = 15.;
      cs_real_t e = 0.25;
      cs_real_t x = pow((1. - b*z*dlmo), e);
      cs_real_t x0 = pow((1. - b*z0*dlmo), e);

      return log(z/z0) - 2.*log((1. + x)/(1. + x0))
        - log((1. + pow(x,2.))/(1. + pow(x0,2.))) + 2.*atan(x) - 2.*atan(x0);
    }

  case CS_ATMO_UNIV_FN_CARL:
    {
      cs_real_t b = 16.;
      cs_real_t e = 1./3.;
      cs_real_t x = pow((1. - b*z*dlmo), e);
      cs_real_t x0 = pow((1. - b*z0*dlmo), e);

      return log(z/z0) - 1.5*log((1. + x + pow(x,2.))/3.)
        + pow(3., 0.5)*atan((1. + 2.*x)/pow(3., 0.5))
        + 1.5*log((1. + x0 + pow(x0,2.))/3.)
        - pow(3., 0.5)*atan((1. + 2.*x0)/pow(3., 0.5));
    }

  default:
    assert(0);
    return -1;
  }
}

static cs_real_t
_mo_psih_u(cs_real_t              z,
           cs_real_t              z0,
           cs_real_t              dlmo,
           cs_real_t              prt)
{
   switch (cs_glob_atmo_option->meteo_phih_u) {

     case CS_ATMO_UNIV_FN_HOGSTROM:
       {
         /* Prt should be 0.95 to be coherent with Hogstrom */
         cs_real_t a = prt;
         cs_real_t b = 11.6;
         cs_real_t e = 0.5;
         cs_real_t x = pow((1. - b*z*dlmo), e);
         cs_real_t x0 = pow((1. - b*z0*dlmo), e);

         return a*(log(z/z0) - 2.*log((1. + x)/(1. + x0)));
       }

   case  CS_ATMO_UNIV_FN_BUSINGER:
     {
       /* Prt should be 0.74 to be coherent with Businger */
       cs_real_t a = prt;
       cs_real_t b = 9.;
       cs_real_t e = 0.5;
       cs_real_t x = pow((1. - b*z*dlmo), e);
       cs_real_t x0 = pow((1. - b*z0*dlmo), e);

       return a*(log(z/z0) - 2.*log((1. + x)/(1. + x0)));
     }

   case  CS_ATMO_UNIV_FN_CARL:
     {
       /* Prt should be 0.74 to be coherent with Carl */
       cs_real_t a = prt;
       cs_real_t b = 16.;
       cs_real_t e = 0.5;
       cs_real_t x = pow((1. - b*z*dlmo), e);
       cs_real_t x0 = pow((1. - b*z0*dlmo), e);

       return a*(log(z/z0) - 2.*log((1. + x)/(1. + x0)));
     }

   default:
     assert(0);
     return -1;
   }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute hydro pressure profile  (Laplace integration)
 *
 *        If hydrostatic_pressure_model = 0 (default) : bottom to top Laplace
 *        integration based on pressure atsea level
 *        If hydrostatic_pressure_model = 1 top to bottom Laplace integration
 *        based on pressure at the top of the domain (z_temp_met[nbmaxt])
 *        for the standard atmosphere
 *
 * \param[in]  itp            index on numbers of time steps of meteo profil
 * \param[in]  ih2o           flag to take into account the humidity
 * \param[in]  fp             pointer sturture to fluid properties
 * \param[in]  at_opt         pointer sturture to atmopheric option
 *
 */
/*----------------------------------------------------------------------------*/

static void
 _compute_hydro_pressure_profile(int                    itp,
                                 int                    ih2o,
                                 cs_fluid_properties_t  *fp,
                                 cs_atmo_option_t       *at_opt)
{
  const int nbmaxt = at_opt->met_1d_nlevels_max_t;

  const cs_real_t t_ref = 288.150;
  const cs_real_t p_ref = 101325.0;
  const cs_real_t gz = cs_glob_physical_constants->gravity[2];
  const cs_real_t tkelvi = cs_physical_constants_celsius_to_kelvin;

  at_opt->hyd_p_met[itp*nbmaxt] = at_opt->xyp_met[2 + itp*3];

  cs_real_2_t q_01;
  if (at_opt->hydrostatic_pressure_model == 0) {
    for (int kk = 1; kk < nbmaxt; kk++) {
      const cs_real_t tmoy = 0.5*(  at_opt->temp_met[kk-1+itp*nbmaxt]
                                  + at_opt->temp_met[kk+itp*nbmaxt]) + tkelvi;
      if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
        // take liquid water into account
        cs_real_t l_w0 = cs_air_yw_sat(at_opt->temp_met [kk-1 + itp*nbmaxt],
                                       at_opt->hyd_p_met[kk-1 + itp*nbmaxt]);
        cs_real_t l_w1 = cs_air_yw_sat(at_opt->temp_met [kk + itp*nbmaxt],
                                       at_opt->hyd_p_met[kk-1 + itp*nbmaxt]);
        /* !in  l_w1 =  ...hyd_p_met[kk-1 + itp*nbmaxt] is not a mistake:
           we can not use hyd_p_met[kk + itp*nbmaxt] since this is what we
           want to estimate */
        q_01[0] = fmin(at_opt->qw_met[kk-1 + itp*nbmaxt], l_w0);
        q_01[1] = fmin(at_opt->qw_met[kk + itp*nbmaxt], l_w1);
      }
      else {
        q_01[0] = at_opt->qw_met[kk-1 + itp*nbmaxt];
        q_01[1] = at_opt->qw_met[kk + itp*nbmaxt];
      }

      const cs_real_t rhmoy
        = fp->r_pg_cnst*(1.0 + (fp->rvsra-1.0)*(q_01[0] + q_01[1])/2.0*ih2o);
      const cs_real_t rap
        = -fabs(gz)*(at_opt->z_temp_met[kk]-at_opt->z_temp_met[kk-1])/rhmoy/tmoy;
      at_opt->hyd_p_met[kk + itp*nbmaxt]
        = at_opt->hyd_p_met[kk-1 + itp*nbmaxt]*exp(rap);
    }
  }
  else {
    // Standard pressure profile above the domain
    cs_real_t dum1, dum2, pptop;
    cs_atmo_profile_std(0., /* z_ref */
                        p_ref,
                        t_ref,
                        at_opt->z_temp_met[nbmaxt-1],
                        &pptop, &dum1, &dum2);
    at_opt->hyd_p_met[(itp+1)*nbmaxt - 1] = pptop;

    for (int kk = nbmaxt - 2; kk > -1; kk--) {
      const cs_real_t tmoy = 0.5*(  at_opt->temp_met[kk+1+itp*nbmaxt]
                                  + at_opt->temp_met[kk+itp*nbmaxt]) + tkelvi;
      if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
        // take liquid water into account
        cs_real_t l_w0 = cs_air_yw_sat(at_opt->temp_met [kk + itp*nbmaxt],
                                       at_opt->hyd_p_met[kk+1 + itp*nbmaxt]);
        cs_real_t l_w1 = cs_air_yw_sat(at_opt->temp_met [kk+1 + itp*nbmaxt],
                                       at_opt->hyd_p_met[kk+1 + itp*nbmaxt]);
        /* !in  l_w0 =  ... hyd_p_met[kk+1 + itp*nbmaxt] is not a mistake:
           we can not use hyd_p_met[kk+1 + itp*nbmaxt] since this is what we
           want to estimate */
        q_01[0] = fmin(at_opt->qw_met[kk + itp*nbmaxt], l_w0);
        q_01[1] = fmin(at_opt->qw_met[kk+1 + itp*nbmaxt], l_w1);
      }
      else {
        q_01[0] = at_opt->qw_met[kk + itp*nbmaxt];
        q_01[1] = at_opt->qw_met[kk+1 + itp*nbmaxt];
      }

      const cs_real_t rhmoy
        = fp->r_pg_cnst*(1.0 + (fp->rvsra-1.0)*(q_01[0] + q_01[1])/2.0*ih2o);
      const cs_real_t rap
        = fabs(gz)*(at_opt->z_temp_met[kk+1]-at_opt->z_temp_met[kk])/rhmoy/tmoy;
      at_opt->hyd_p_met[kk + itp*nbmaxt]
        = at_opt->hyd_p_met[kk+1 + itp*nbmaxt]*exp(rap);
    }
  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the pot. temperature profile and the density profile
 *
 * \param[in]  itp            index on numbers of time steps of meteo profil
 * \param[in]  ih2o           flag to take into account the humidity
 * \param[in]  fp             pointer sturture to fluid properties
 * \param[in]  at_opt         pointer sturture to atmopheric option
 *
 */
/*----------------------------------------------------------------------------*/

static void
_compute_pot_temperature_density_profile(int                    itp,
                                         int                    ih2o,
                                         cs_fluid_properties_t  *fp,
                                         cs_atmo_option_t       *at_opt)
{
  const int nbmaxt = at_opt->met_1d_nlevels_max_t;
  cs_air_fluid_props_t *ct_prop = cs_glob_air_props;

  const cs_real_t cpvcpa = ct_prop->cp_v / ct_prop->cp_a;

  const cs_real_t cst = fp->rvsra - 1.0;
  const cs_real_t rscp = fp->r_pg_cnst/fp->cp0;
  const cs_real_t cst1 = fp->rvsra - cpvcpa;
  const cs_real_t ps = cs_glob_atmo_constants->ps;
  const cs_real_t tkelvi = cs_physical_constants_celsius_to_kelvin;

  for (int kk = 0; kk < nbmaxt; kk++) {

    const cs_real_t rhum
      = fp->r_pg_cnst*(1.0+(cst)*at_opt->qw_met[kk + itp*nbmaxt]*ih2o);

    if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_CONSTANT_DENSITY)
      at_opt->rho_met[kk + itp*nbmaxt]
        = at_opt->hyd_p_met[itp*nbmaxt]
                  /(at_opt->temp_met[kk + itp*nbmaxt] + tkelvi)/rhum;
    else
      at_opt->rho_met[kk + itp*nbmaxt]
        = at_opt->hyd_p_met[kk+itp*nbmaxt]
                  /(at_opt->temp_met[kk + itp*nbmaxt] + tkelvi)/rhum;
    const cs_real_t _rscp
      = rscp*(1.0 + cst1*at_opt->qw_met[kk + itp*nbmaxt]*ih2o);
    at_opt->pot_t_met[kk + itp*nbmaxt] = (at_opt->temp_met[kk + itp*nbmaxt]+tkelvi)
                                         *pow((ps/at_opt->hyd_p_met[kk + itp*nbmaxt]), _rscp);

    printf(" comp pot temp_dens    %10.12lf        %10.17lf\n",
           at_opt->pot_t_met[kk + itp*nbmaxt],
           at_opt->rho_met[kk + itp*nbmaxt]);
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Print informations read in meteo profil file
 *
 * \param[in]  itp            index on numbers of time steps of meteo profil
 * \param[in]  date           array contains date (year hour ...)
 * \param[in]  at_opt         pointer sturture to atmopheric option
 *
 */
/*----------------------------------------------------------------------------*/

static void
_log_meteo_profile(int                    itp,
                   const int              date[],
                   cs_atmo_option_t       *at_opt)
{
  if (at_opt->meteo_profile != 1)
    return;

  const int nbmetd = at_opt->met_1d_nlevels_d;
  const int nbmaxt = at_opt->met_1d_nlevels_max_t;

  if (itp == 0)
    bft_printf("===================================================\n"
               "printing meteo profiles\n");
  bft_printf("year, quant-day, hour, minute, second\n"
             "%d  %d  %d %d  %10.2lf\n", date[0], date[1],
                                         date[2], date[3], (cs_real_t)date[4]);
  bft_printf("time_met[%d]\n%lf\n",itp, at_opt->time_met[itp]);
  bft_printf("z_dyn_met  u_met  v_met  ek_met ep_met\n");
  for (int ii = 0; ii < nbmetd; ii++)
    bft_printf("%10.5lf  %10.5lf  %10.5lf  %10.5lf %10.5lf\n",
               at_opt->z_dyn_met[ii],
               at_opt->u_met [ii + itp*nbmetd],
               at_opt->v_met [ii + itp*nbmetd],
               at_opt->ek_met[ii + itp*nbmetd],
               at_opt->ep_met[ii + itp*nbmetd]);

  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_CONSTANT_DENSITY)
    bft_printf("===================================================\n"
               "Warning: constant density option\n"
               "Warning: thermal profile will be ignored\n"
               "===================================================\n");

  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
    bft_printf("z_temp_met  temp_met  pot_t_met rho_met hyd_p_met"
               "  qw_met  qsat  ndrop_met\n");
    for (int ii = 0; ii < nbmaxt; ii++) {
      const cs_real_t qsat = cs_air_yw_sat(at_opt->temp_met [ii + itp*nbmaxt],
                                           at_opt->hyd_p_met[ii + itp*nbmaxt]);
      bft_printf("%10.5lf  %10.5lf  %10.5lf  %10.5lf %10.5lf %10.5lf"
                 "  %10.5lf %10.5lf\n",
                 at_opt->z_temp_met[ii],
                 at_opt->temp_met[ii + itp*nbmaxt],
                 at_opt->pot_t_met[ii + itp*nbmaxt],
                 at_opt->rho_met[ii + itp*nbmaxt],
                 at_opt->hyd_p_met[ii + itp*nbmaxt],
                 at_opt->qw_met[ii + itp*nbmaxt],
                 qsat,
                 at_opt->ndrop_met[ii + itp*nbmaxt]);
    }
  }
  else {
    bft_printf("z_temp_met  temp_met  pot_t_met rho_met hyd_p_met"
               "  qw_met  qsat  ndrop_met\n");
    for (int ii = 0; ii < nbmaxt; ii++) {
      bft_printf("%10.5lf  %10.5lf  %10.5lf  %10.5lf %10.5lf %10.5lf\n",
                 at_opt->z_temp_met[ii],
                 at_opt->temp_met[ii + itp*nbmaxt],
                 at_opt->pot_t_met[ii + itp*nbmaxt],
                 at_opt->rho_met[ii + itp*nbmaxt],
                 at_opt->hyd_p_met[ii + itp*nbmaxt],
                 at_opt->qw_met[ii + itp*nbmaxt]);
    }
  }

}

/*============================================================================
 * Fortran wrapper function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal function phim for neutral, stable and unstable
 *
 * \param[in]  z             altitude
 * \param[in]  dlmo          Inverse Monin Obukhov length
 *
 * \return                   factor
 */
/*----------------------------------------------------------------------------*/

cs_real_t
cs_mo_phim(cs_real_t              z,
           cs_real_t              dlmo)
{
  cs_real_t dlmoneutral = 1.e-12;
  cs_real_t coef;

  if (cs::abs(dlmo) < dlmoneutral)
    coef = _mo_phim_n(z,dlmo);
  else if (dlmo >= 0.)
    coef = _mo_phim_s(z,dlmo);
  else
    coef = _mo_phim_u(z,dlmo);

  return coef;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal function phih for neutral, stable and unstable
 *
 * \param[in]  z             altitude
 * \param[in]  dlmo          Inverse Monin Obukhov length
 * \param[in]  prt           Turbulent Prandtl number
 *
 * \return                   factor
 */
/*----------------------------------------------------------------------------*/

cs_real_t
cs_mo_phih(cs_real_t              z,
           cs_real_t              dlmo,
           cs_real_t              prt)
{
  cs_real_t dlmoneutral = 1.e-12;
  cs_real_t coef;

  if (cs::abs(dlmo) < dlmoneutral)
    coef = _mo_phih_n(z,dlmo,prt);
  else if (dlmo >= 0.)
    coef = _mo_phih_s(z,dlmo,prt);
  else
    coef = _mo_phih_u(z,dlmo,prt);

  return coef;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal function psim for neutral, stable and unstable
 *
 * \param[in]  z             altitude
 * \param[in]  z0            altitude of the starting point integration
 * \param[in]  dlmo          Inverse Monin Obukhov length
 *
 * \return                   factor
 */
/*----------------------------------------------------------------------------*/

cs_real_t
cs_mo_psim(cs_real_t              z,
           cs_real_t              z0,
           cs_real_t              dlmo)
{
  cs_real_t dlmoneutral = 1.e-12;
  cs_real_t coef;

  if (cs::abs(dlmo) < dlmoneutral)
    coef = _mo_psim_n(z, z0, dlmo);
  else if (dlmo >= 0.)
    coef = _mo_psim_s(z, z0, dlmo);
  else
    coef = _mo_psim_u(z, z0, dlmo);

  return coef;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Universal function psih for neutral, stable and unstable
 *
 * \param[in]  z             altitude
 * \param[in]  z0            altitude of the starting point integration
 * \param[in]  dlmo          Inverse Monin Obukhov length
 * \param[in]  prt           Turbulent Prandtl number
 *
 * \return                   factor
 */
/*----------------------------------------------------------------------------*/

cs_real_t
cs_mo_psih(cs_real_t              z,
           cs_real_t              z0,
           cs_real_t              dlmo,
           cs_real_t              prt)
{
  cs_real_t dlmoneutral = 1.e-12;
  cs_real_t coef;

  if (cs::abs(dlmo) < dlmoneutral)
    coef = _mo_psih_n(z, z0, dlmo, prt);
  else if (dlmo >= 0.)
    coef = _mo_psih_s(z, z0, dlmo, prt);
  else
    coef = _mo_psih_u(z, z0, dlmo, prt);

  return coef;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute LMO, friction velocity ustar
 *        from a thermal difference using Monin Obukhov
 *
 * \param[in]  z             altitude
 * \param[in]  z0
 * \param[in]  du            velocity difference
 * \param[in]  dt            thermal difference
 * \param[in]  beta          thermal expansion
 * \param[in]  gredu
 * \param[out] dlmo          Inverse Monin Obukhov length
 * \param[out] ustar         friction velocity
 */
/*----------------------------------------------------------------------------*/

void
cs_mo_compute_from_thermal_diff(cs_real_t   z,
                                cs_real_t   z0,
                                cs_real_t   du,
                                cs_real_t   dt,
                                cs_real_t   beta,
                                cs_real_t   gredu,
                                cs_real_t   *dlmo,
                                cs_real_t   *ustar)
{
  /* Local variables */
  cs_real_t kappa = cs_turb_xkappa;
  cs_real_t coef_mom_old = 0.;
  cs_real_t coef_moh_old = 0.;
  cs_real_t dlmoclip = 1.0;

  /* Precision initialisation */
  cs_real_t prec_ustar = 1.e-2;
  cs_real_t prec_tstar = 1.e-2;

  /* Initial LMO */
  *dlmo = 0.;

  cs_real_t prt = 1.;
  if (CS_F_(t) != nullptr)
    prt = cs_field_get_key_double(CS_F_(t),
                                  cs_field_key_id("turbulent_schmidt"));

  /* Call universal functions */
  cs_real_t zref = z+z0;
  cs_real_t coef_mom = cs_mo_psim(zref,z0, *dlmo);
  cs_real_t coef_moh = cs_mo_psih(zref,z0, *dlmo, prt);

  /* Initial ustar and tstar */
  *ustar = kappa * du / coef_mom;

  for (int icompt = 0;
      icompt < 1000 &&
      /* Convergence test */
      (   fabs(coef_mom-coef_mom_old) >= prec_ustar
       || fabs(coef_moh-coef_moh_old) >= prec_tstar
       || icompt == 0);
      icompt++) {

    /* Storage previous values */
    coef_mom_old = coef_mom;
    coef_moh_old = coef_moh;

    /* Update LMO */
    cs_real_t num = beta * cs_math_pow2(coef_mom) * gredu * dt;
    cs_real_t denom = cs_math_pow2(du) * coef_moh;
    if (fabs(denom) > (cs_math_epzero * fabs(num)))
      *dlmo = num / denom;
    else
      *dlmo = 0.; //FIXME

    /* Clipping dlmo (we want |LMO| > 1 m  ie 1/|LMO| < 1 m^-1) */
    if (fabs(*dlmo) >= dlmoclip) {
      if (*dlmo >= 0.)
        *dlmo =   dlmoclip;
      if (*dlmo <= 0.)
        *dlmo = - dlmoclip;
    }

    /* Evaluate universal functions */
    coef_mom = cs_mo_psim((z+z0),z0, *dlmo);
    coef_moh = cs_mo_psih(z+z0,z0, *dlmo, prt);

    /* Update ustarr */
    *ustar = kappa*du/coef_mom;
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute LMO, friction velocity ustar
 *        from a thermal flux using Monin Obukhov
 *
 * \param[in]  z             altitude
 * \param[in]  z0
 * \param[in]  du            velocity difference
 * \param[in]  flux          thermal flux
 * \param[in]  beta          thermal expansion
 * \param[in]  gredu
 * \param[out] dlmo          Inverse Monin Obukhov length
 * \param[out] ustar         friction velocity
 */
/*----------------------------------------------------------------------------*/

void
cs_mo_compute_from_thermal_flux(cs_real_t   z,
                                cs_real_t   z0,
                                cs_real_t   du,
                                cs_real_t   flux,
                                cs_real_t   beta,
                                cs_real_t   gredu,
                                cs_real_t   *dlmo,
                                cs_real_t   *ustar)
{
  /* Local variables */
  cs_real_t kappa = cs_turb_xkappa;
  cs_real_t coef_mom_old = 0.;
  cs_real_t dlmoclip = 1.0;

  /* Precision initialisation */
  cs_real_t prec_ustar = 1.e-2;

  /* Initial LMO */
  *dlmo = 0.;

  /* Call universal functions */
  cs_real_t zref = z+z0;
  cs_real_t coef_mom = cs_mo_psim(zref,z0, *dlmo);

  /* Initial ustar and tstar */
  *ustar = kappa * du / coef_mom;

  for (int icompt = 0;
      icompt < 1000 &&
      /* Convergence test */
      (   fabs(coef_mom-coef_mom_old) >= prec_ustar
       || icompt == 0);
      icompt++) {

    /* Storage previous values */
    coef_mom_old = coef_mom;

    /* Update LMO */
    cs_real_t num = beta * cs_math_pow3(coef_mom) * gredu * flux;
    cs_real_t denom = cs_math_pow3(du) * cs_math_pow2(kappa);

    if (fabs(denom) > (cs_math_epzero * fabs(num)))
      *dlmo = num / denom;
    else
      *dlmo = 0.; //FIXME other clipping ?

    /* Clipping dlmo (we want |LMO| > 1 m  ie 1/|LMO| < 1 m^-1) */
    if (fabs(*dlmo) >= dlmoclip) {
      if (*dlmo >= 0.)
        *dlmo =   dlmoclip;
      if (*dlmo <= 0.)
        *dlmo = - dlmoclip;
    }

    /* Evaluate universal functions */
    coef_mom = cs_mo_psim((z+z0),z0, *dlmo);

    /* Update ustar */
    *ustar = kappa*du/coef_mom;
  }
}

/*----------------------------------------------------------------------------
 * Access pointers for Fortran mapping.
 *----------------------------------------------------------------------------*/

void
cs_f_atmo_get_pointers(cs_real_t              **ps,
                       int                    **syear,
                       int                    **squant,
                       int                    **shour,
                       int                    **smin,
                       cs_real_t              **ssec,
                       cs_real_t              **longitude,
                       cs_real_t              **latitude,
                       cs_real_t              **x_l93,
                       cs_real_t              **y_l93,
                       bool                   **compute_z_ground,
                       int                    **open_bcs_treatment,
                       int                    **theo_interp,
                       int                    **sedimentation_model,
                       int                    **deposition_model,
                       int                    **nucleation_model,
                       int                    **subgrid_model,
                       int                    **distribution_model,
                       int                    **meteo_profile,
                       int                    **nbmetd,
                       int                    **nbmett,
                       int                    **nbmetm,
                       int                    **radiative_model_1d,
                       int                    **nbmaxt,
                       cs_real_t              **meteo_zi,
                       int                    **soil_model,
                       int                    **nvert,
                       int                    **kvert,
                       int                    **kmx,
                       int                    **ihpm,
                       int                    **iqv0,
                       int                    **nfatr1,
                       cs_real_t              **sigc,
                       int                    **idrayi,
                       int                    **idrayst,
                       cs_real_t              **aod_o3_tot,
                       cs_real_t              **aod_h2o_tot)
{
  *ps        = &(_atmo_constants.ps);
  *syear     = &(_atmo_option.syear);
  *squant    = &(_atmo_option.squant);
  *shour     = &(_atmo_option.shour);
  *smin      = &(_atmo_option.smin);
  *ssec      = &(_atmo_option.ssec);
  *longitude = &(_atmo_option.longitude);
  *latitude  = &(_atmo_option.latitude);
  *x_l93 = &(_atmo_option.x_l93);
  *y_l93 = &(_atmo_option.y_l93);
  *compute_z_ground = &(_atmo_option.compute_z_ground);
  *open_bcs_treatment = &(_atmo_option.open_bcs_treatment);
  *theo_interp = &(_atmo_option.theo_interp);
  *sedimentation_model = &(_atmo_option.sedimentation_model);
  *deposition_model = &(_atmo_option.deposition_model);
  *nucleation_model = &(_atmo_option.nucleation_model);
  *subgrid_model = &(_atmo_option.subgrid_model);
  *distribution_model = &(_atmo_option.distribution_model);
  *meteo_profile = &(_atmo_option.meteo_profile);
  *nbmetd     = &(_atmo_option.met_1d_nlevels_d);
  *nbmett     = &(_atmo_option.met_1d_nlevels_t);
  *nbmetm     = &(_atmo_option.met_1d_ntimes);
  *nbmaxt     = &(_atmo_option.met_1d_nlevels_max_t);
  *meteo_zi   = &(_atmo_option.meteo_zi);
  *soil_model = &(_atmo_option.soil_model);
  *radiative_model_1d = &(_atmo_option.radiative_model_1d);
  *nvert = &(_atmo_option.rad_1d_nvert);
  *kvert = &(_atmo_option.rad_1d_nlevels);
  *kmx = &(_atmo_option.rad_1d_nlevels_max);
  *ihpm = &(_atmo_option.hydrostatic_pressure_model);
  *iqv0 = &(_atmo_option.qv_profile);
  *nfatr1 = &(_atmo_option.rad_1d_frequency);
  *sigc  = &(_atmo_option.sigc);
  *idrayi = &(_atmo_option.infrared_1D_profile);
  *idrayst = &(_atmo_option.solar_1D_profile);
  *aod_o3_tot  = &(_atmo_option.aod_o3_tot);
  *aod_h2o_tot  = &(_atmo_option.aod_h2o_tot);
}

void
cs_f_atmo_get_soil_zone(cs_lnum_t         *n_elts,
                        int               *n_soil_cat,
                        const cs_lnum_t  **elt_ids)
{
  *n_elts = 0;
  *elt_ids = nullptr;

  /* Not defined */
  if (cs_glob_atmo_option->soil_zone_id < 0) {
    *n_soil_cat = 0;
    return;
  }

  const cs_zone_t *z = cs_boundary_zone_by_id(cs_glob_atmo_option->soil_zone_id);
  *elt_ids = z->elt_ids;
  *n_elts = z->n_elts;
  switch (cs_glob_atmo_option->soil_cat) {
    case CS_ATMO_SOIL_5_CAT:
      *n_soil_cat = 5;
      break;
    case CS_ATMO_SOIL_7_CAT:
      *n_soil_cat = 7;
      break;
    case CS_ATMO_SOIL_23_CAT:
      *n_soil_cat = 23;
      break;
    default:
      *n_soil_cat = 0;
      break;
  }

}

void
cs_f_atmo_arrays_get_pointers(cs_real_t **z_dyn_met,
                              cs_real_t **z_temp_met,
                              cs_real_t **xyp_met,
                              cs_real_t **u_met,
                              cs_real_t **v_met,
                              cs_real_t **w_met,
                              cs_real_t **time_met,
                              cs_real_t **hyd_p_met,
                              cs_real_t **pot_t_met,
                              cs_real_t **ek_met,
                              cs_real_t **ep_met,
                              cs_real_t **ttmet,
                              cs_real_t **rmet,
                              cs_real_t **qvmet,
                              cs_real_t **ncmet,
                              cs_real_t **dpdt_met,
                              cs_real_t **mom_met,
                              cs_real_t **mom,
                              cs_real_t **xyvert,
                              cs_real_t **zvert,
                              cs_real_t **acinfe,
                              cs_real_t **dacinfe,
                              cs_real_t **aco2,
                              cs_real_t **aco2s,
                              cs_real_t **daco2,
                              cs_real_t **daco2s,
                              cs_real_t **acsup,
                              cs_real_t **acsups,
                              cs_real_t **dacsup,
                              cs_real_t **dacsups,
                              cs_real_t **tauzq,
                              cs_real_t **tauz,
                              cs_real_t **zq,
                              cs_real_t **zray,
                              cs_real_t **rayi,
                              cs_real_t **rayst,
                              cs_real_t **iru,
                              cs_real_t **ird,
                              cs_real_t **solu,
                              cs_real_t **sold,
                              cs_real_t **soil_albedo,
                              cs_real_t **soil_emissi,
                              cs_real_t **soil_ttsoil,
                              cs_real_t **soil_tpsoil,
                              cs_real_t **soil_totwat,
                              cs_real_t **soil_pressure,
                              cs_real_t **soil_density,
                              int         dim_nd_nt[2],
                              int         dim_ntx_nt[2],
                              int         dim_nd_3[2],
                              int         dim_nt_3[2],
                              int         dim_xyvert[2],
                              int         dim_kmx2[2],
                              int         dim_kmx_nvert[2])
{
  int n_level = 0, n_level_t = 0;
  int n_times = 0;
  if (_atmo_option.meteo_profile) {
    n_level = cs::max(1, _atmo_option.met_1d_nlevels_d);
    n_level_t = cs::max(1, _atmo_option.met_1d_nlevels_max_t);
    n_times = cs::max(1, _atmo_option.met_1d_ntimes);
  }

  CS_REALLOC(_atmo_option.z_dyn_met, n_level, cs_real_t);
  CS_REALLOC(_atmo_option.z_temp_met, n_level_t, cs_real_t);
  CS_REALLOC(_atmo_option.xyp_met, n_times*3, cs_real_t);
  CS_REALLOC(_atmo_option.u_met, n_level*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.v_met, n_level*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.w_met, n_level*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.time_met, n_times, cs_real_t);
  CS_REALLOC(_atmo_option.hyd_p_met, n_times * n_level_t, cs_real_t);
  CS_REALLOC(_atmo_option.pot_t_met, n_level_t*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.ek_met, n_level*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.ep_met, n_level*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.temp_met, n_level_t*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.rho_met, n_level_t*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.qw_met, n_level_t*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.ndrop_met, n_level_t*n_times, cs_real_t);
  CS_REALLOC(_atmo_option.dpdt_met, n_level, cs_real_t);
  CS_REALLOC(_atmo_option.mom_met, n_level, cs_real_3_t);
  CS_REALLOC(_atmo_option.mom_cs, n_level, cs_real_3_t);

  *xyp_met   = _atmo_option.xyp_met;
  *u_met     = _atmo_option.u_met;
  *v_met     = _atmo_option.v_met;
  *w_met     = _atmo_option.w_met;
  *hyd_p_met = _atmo_option.hyd_p_met;
  *pot_t_met = _atmo_option.pot_t_met;
  *ek_met    = _atmo_option.ek_met;
  *ep_met    = _atmo_option.ep_met;
  *ttmet     = _atmo_option.temp_met;
  *rmet      = _atmo_option.rho_met;
  *qvmet     = _atmo_option.qw_met;
  *ncmet     = _atmo_option.ndrop_met;
  *dpdt_met  = _atmo_option.dpdt_met;
  *mom_met   = (cs_real_t *)_atmo_option.mom_met;
  *mom       = (cs_real_t *)_atmo_option.mom_cs;

  *z_dyn_met  = _atmo_option.z_dyn_met;
  *z_temp_met = _atmo_option.z_temp_met;
  *time_met   = _atmo_option.time_met;

  n_level = 0;
  int n_vert = 0;
  if (_atmo_option.radiative_model_1d != 0) {
    n_level = cs::max(1, _atmo_option.rad_1d_nlevels_max);
    n_vert = cs::max(1, _atmo_option.rad_1d_nvert);
  }

  if (         _atmo_option.rad_1d_xy == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_xy , 3*n_vert, cs_real_t);
  if (         _atmo_option.rad_1d_z == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_z , n_level, cs_real_t);
  if (         _atmo_option.rad_1d_acinfe == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_acinfe , n_level, cs_real_t);
  if (         _atmo_option.rad_1d_dacinfe == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_dacinfe , n_level, cs_real_t);
  if (         _atmo_option.rad_1d_aco2 == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_aco2, n_level*n_level, cs_real_t);
  if (         _atmo_option.rad_1d_aco2s == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_aco2s, n_level*n_level, cs_real_t);
  if (         _atmo_option.rad_1d_daco2 == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_daco2, n_level*n_level, cs_real_t);
  if (         _atmo_option.rad_1d_daco2s == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_daco2s, n_level*n_level, cs_real_t);
  if (         _atmo_option.rad_1d_acsup == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_acsup, n_level, cs_real_t);
  if (         _atmo_option.rad_1d_acsups == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_acsups, n_level, cs_real_t);
  if (         _atmo_option.rad_1d_dacsup == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_dacsup, n_level, cs_real_t);
  if (         _atmo_option.rad_1d_dacsups == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_dacsups, n_level, cs_real_t);
  if (         _atmo_option.rad_1d_tauzq == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_tauzq, n_level+1, cs_real_t);
  if (         _atmo_option.rad_1d_tauz == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_tauz, n_level+1, cs_real_t);
  if (         _atmo_option.rad_1d_zq == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_zq, n_level+1, cs_real_t);
  if (         _atmo_option.rad_1d_zray == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_zray, n_level, cs_real_t);
  if (         _atmo_option.rad_1d_ir_div == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_ir_div, n_level * n_vert, cs_real_t);
  if (         _atmo_option.rad_1d_sol_div == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_sol_div, n_level * n_vert, cs_real_t);
  if (         _atmo_option.rad_1d_iru == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_iru, n_level * n_vert, cs_real_t);
  if (         _atmo_option.rad_1d_ird == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_ird, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_ird);
  }
  if (         _atmo_option.rad_1d_solu == nullptr)
    CS_MALLOC(_atmo_option.rad_1d_solu, n_level * n_vert, cs_real_t);
  if (         _atmo_option.rad_1d_sold == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_sold, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_sold);
  }
  if (         _atmo_option.rad_1d_qw == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_qw, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_qw);
  }
  if (         _atmo_option.rad_1d_ql == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_ql, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_ql);
  }
  if (         _atmo_option.rad_1d_qv == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_qv, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_qv);
  }
  if (         _atmo_option.rad_1d_nc == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_nc, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_nc);
  }
  if (         _atmo_option.rad_1d_fn == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_fn, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_fn);
  }
  if (         _atmo_option.rad_1d_aerosols == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_aerosols, n_level * n_vert, cs_real_t);
    cs_array_real_fill_zero(n_level * n_vert, _atmo_option.rad_1d_aerosols);
  }
  if (         _atmo_option.rad_1d_albedo0 == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_albedo0, n_vert, cs_real_t);
    cs_array_real_fill_zero(n_vert, _atmo_option.rad_1d_albedo0);
  }
  if (         _atmo_option.rad_1d_emissi0== nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_emissi0, n_vert, cs_real_t);
    cs_array_real_fill_zero(n_vert, _atmo_option.rad_1d_emissi0);
  }
  if (         _atmo_option.rad_1d_temp0 == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_temp0, n_vert, cs_real_t);
    cs_array_real_fill_zero(n_vert, _atmo_option.rad_1d_temp0);
  }
  if (         _atmo_option.rad_1d_theta0 == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_theta0, n_vert, cs_real_t);
    cs_array_real_fill_zero(n_vert, _atmo_option.rad_1d_theta0);
  }
  if (         _atmo_option.rad_1d_qw0 == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_qw0, n_vert, cs_real_t);
    cs_array_real_fill_zero(n_vert, _atmo_option.rad_1d_qw0);
  }
  if (         _atmo_option.rad_1d_p0  == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_p0, n_vert, cs_real_t);
    cs_array_real_fill_zero(n_vert, _atmo_option.rad_1d_p0);
  }
  if (         _atmo_option.rad_1d_rho0 == nullptr) {
    CS_MALLOC(_atmo_option.rad_1d_rho0, n_vert, cs_real_t);
    cs_array_real_fill_zero(n_vert, _atmo_option.rad_1d_rho0);
  }

  *xyvert = _atmo_option.rad_1d_xy;
  *zvert  = _atmo_option.rad_1d_z;
  *acinfe = _atmo_option.rad_1d_acinfe;
  *dacinfe = _atmo_option.rad_1d_dacinfe;
  *aco2   = _atmo_option.rad_1d_aco2  ;
  *aco2s  = _atmo_option.rad_1d_aco2s ;
  *daco2  = _atmo_option.rad_1d_daco2 ;
  *daco2s = _atmo_option.rad_1d_daco2s;
  *acsup  = _atmo_option.rad_1d_acsup ;
  *acsups = _atmo_option.rad_1d_acsups;
  *dacsup = _atmo_option.rad_1d_dacsup;
  *dacsups = _atmo_option.rad_1d_dacsups;
  *tauzq  = _atmo_option.rad_1d_tauzq ;
  *tauz   = _atmo_option.rad_1d_tauz  ;
  *zq     = _atmo_option.rad_1d_zq    ;
  *zray   = _atmo_option.rad_1d_zray  ;
  *rayi   = _atmo_option.rad_1d_ir_div  ;
  *rayst  = _atmo_option.rad_1d_sol_div ;
  *iru    = _atmo_option.rad_1d_iru   ;
  *ird    = _atmo_option.rad_1d_ird   ;
  *solu   = _atmo_option.rad_1d_solu  ;
  *sold   = _atmo_option.rad_1d_sold  ;

  /* ground level arrays, of size n_vert */
  *soil_albedo   = _atmo_option.rad_1d_albedo0;
  *soil_emissi   = _atmo_option.rad_1d_emissi0;
  *soil_ttsoil   = _atmo_option.rad_1d_temp0;
  *soil_tpsoil   = _atmo_option.rad_1d_theta0;
  *soil_totwat   = _atmo_option.rad_1d_qw0;
  *soil_pressure = _atmo_option.rad_1d_p0;
  *soil_density  = _atmo_option.rad_1d_rho0;

  /* number of layers for dynamics times number of time steps */
  dim_nd_nt[0]     = _atmo_option.met_1d_nlevels_d;
  dim_nd_nt[1]     = _atmo_option.met_1d_ntimes;
  /* number of layers for temperature (max value) times number of time steps */
  dim_ntx_nt[0] = _atmo_option.met_1d_nlevels_max_t;
  dim_ntx_nt[1] = _atmo_option.met_1d_ntimes;
  dim_nd_3[0]    = _atmo_option.met_1d_nlevels_d;
  dim_nd_3[1]    = 3;
  dim_nt_3[0]    = 3;
  dim_nt_3[1]    = _atmo_option.met_1d_ntimes;
  dim_xyvert[0]    = _atmo_option.rad_1d_nvert;
  dim_xyvert[1]    = 3;
  dim_kmx2[0]    = _atmo_option.rad_1d_nlevels_max;
  dim_kmx2[1]    = _atmo_option.rad_1d_nlevels_max;
  dim_kmx_nvert[0] = _atmo_option.rad_1d_nlevels_max;
  dim_kmx_nvert[1] = _atmo_option.rad_1d_nvert;

}

void
cs_f_atmo_rad_1d_arrays_get_pointers(cs_real_t **qwvert,
                                     cs_real_t **qlvert,
                                     cs_real_t **qvvert,
                                     cs_real_t **ncvert,
                                     cs_real_t **fnvert,
                                     cs_real_t **aevert)
{
  *qwvert = _atmo_option.rad_1d_qw;
  *qlvert = _atmo_option.rad_1d_ql;
  *qvvert = _atmo_option.rad_1d_qv;
  *ncvert = _atmo_option.rad_1d_nc;
  *fnvert = _atmo_option.rad_1d_fn;
  *aevert = _atmo_option.rad_1d_aerosols;
}

void
cs_f_atmo_get_pointers_imbrication(bool      **imbrication_flag,
                                   bool      **imbrication_verbose,
                                   bool      **cressman_u,
                                   bool      **cressman_v,
                                   bool      **cressman_qw,
                                   bool      **cressman_nc,
                                   bool      **cressman_tke,
                                   bool      **cressman_eps,
                                   bool      **cressman_theta,
                                   cs_real_t **vertical_influence_radius,
                                   cs_real_t **horizontal_influence_radius,
                                   int       **id_u,
                                   int       **id_v,
                                   int       **id_qw,
                                   int       **id_nc,
                                   int       **id_tke,
                                   int       **id_eps,
                                   int       **id_theta)
{
  *imbrication_flag = &(_atmo_imbrication.imbrication_flag);
  *imbrication_verbose = &(_atmo_imbrication.imbrication_verbose);

  *cressman_u = &(_atmo_imbrication.cressman_u);
  *cressman_v = &(_atmo_imbrication.cressman_v);
  *cressman_qw = &(_atmo_imbrication.cressman_qw);
  *cressman_nc = &(_atmo_imbrication.cressman_nc);
  *cressman_tke = &(_atmo_imbrication.cressman_tke);
  *cressman_eps = &(_atmo_imbrication.cressman_eps);
  *cressman_theta = &(_atmo_imbrication.cressman_theta);

  *vertical_influence_radius = &(_atmo_imbrication.vertical_influence_radius);
  *horizontal_influence_radius
    = &(_atmo_imbrication.horizontal_influence_radius);

  *id_u     = &(_atmo_imbrication.id_u);
  *id_v     = &(_atmo_imbrication.id_v);
  *id_qw    = &(_atmo_imbrication.id_qw);
  *id_nc    = &(_atmo_imbrication.id_nc);
  *id_tke   = &(_atmo_imbrication.id_tke);
  *id_eps   = &(_atmo_imbrication.id_eps);
  *id_theta = &(_atmo_imbrication.id_theta);
}

void
cs_atmo_soil_init_arrays(int        *n_soil_cat,
                         cs_real_t  **csol,
                         cs_real_t  **rugdyn,
                         cs_real_t  **rugthe,
                         cs_real_t  **albedo,
                         cs_real_t  **emissi,
                         cs_real_t  **vegeta,
                         cs_real_t  **c1w,
                         cs_real_t  **c2w,
                         cs_real_t  **r1,
                         cs_real_t  **r2)
 /* The dimension is n_soil_cat + 1 because the element at index 0
    is kept unused. */
{
  CS_REALLOC(_atmo_option.soil_cat_roughness, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_thermal_inertia, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_thermal_roughness, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_albedo, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_emissi, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_vegeta, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_w1, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_w2, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_r1, *n_soil_cat+1, cs_real_t);
  CS_REALLOC(_atmo_option.soil_cat_r2, *n_soil_cat+1, cs_real_t);

  *rugdyn = _atmo_option.soil_cat_roughness;
  *csol   = _atmo_option.soil_cat_thermal_inertia;
  *rugthe = _atmo_option.soil_cat_thermal_roughness;

  *r1     = _atmo_option.soil_cat_r1;
  *r2     = _atmo_option.soil_cat_r2 ;
  *vegeta = _atmo_option.soil_cat_vegeta;
  *albedo = _atmo_option.soil_cat_albedo;
  *emissi = _atmo_option.soil_cat_emissi;
  *c1w    = _atmo_option.soil_cat_w1;
  *c2w    = _atmo_option.soil_cat_w2;

}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * initialize fields, stage 0
 *----------------------------------------------------------------------------*/

void
cs_atmo_fields_init0(void)
{
  cs_mesh_t *m = cs_glob_domain->mesh;
  cs_mesh_quantities_t *mq = cs_glob_domain->mesh_quantities;

  const cs_lnum_t n_cells = m->n_cells;
  const cs_real_3_t *cell_cen = mq->cell_cen;

  int has_restart = cs_restart_present();
  cs_atmo_option_t *at_opt = cs_glob_atmo_option;
  cs_atmo_chemistry_t *at_chem = cs_glob_atmo_chemistry;

  const char input_param_desc[] = N_("Atmospheric module input data");

  /* Reading the meteo profile file (if meteo_profile==1) */

  if (at_opt->meteo_profile == 1) {
    int imode = 1;
    cs_atmo_read_meteo_profile(imode);

    /* Check latitude / longitude from meteo file */
    int n_times = cs::max(1, at_opt->met_1d_ntimes);
    cs_real_t xyp_met_max = at_opt->xyp_met[0];
    for (int i = 0; i < n_times; i++) {
      if (at_opt->xyp_met[3 * i] > xyp_met_max)
        xyp_met_max = at_opt->xyp_met[3 * i];
      if (at_opt->xyp_met[3 * i + 1] > xyp_met_max)
        xyp_met_max = at_opt->xyp_met[3 * i + 1];
    }
    if (xyp_met_max >= cs_math_infinite_r*0.5)
      cs_parameters_error(CS_ABORT_DELAYED,
                          _(input_param_desc),
                          _("Wrong coordinates for the meteo profile.\n"
                            "Check your data and parameters."));

  }
  else if (at_opt->meteo_profile == 2) {
    cs_atmo_compute_meteo_profiles();
  }

  /* Atmospheric gaseous chemistry */
  if (at_chem->model > 0) {

    // Second reading of chemical profiles file
    int imode = 1;
    cs_atmo_read_chemistry_profile(imode);

    /* Check latitude / longitude from chemistry file */
    cs_real_t xy_chem[2] = {at_chem->x_conc_profiles[0],
                            at_chem->y_conc_profiles[0]};

    for (int ii = 1; ii < at_chem->nt_step_profiles; ii++) {
      if (xy_chem[0] <= at_chem->x_conc_profiles[ii])
        xy_chem[0] = at_chem->x_conc_profiles[ii];
      if (xy_chem[1] <= at_chem->y_conc_profiles[ii])
        xy_chem[1] = at_chem->y_conc_profiles[ii];
    }

    if (   xy_chem[0] >= cs_math_infinite_r*0.5
        || xy_chem[0] >= cs_math_infinite_r*0.5)
      cs_parameters_error
        (CS_ABORT_DELAYED,
         _(input_param_desc),
         _("Wrong coordinates for the concentration profile.\n"
           "Check your data and parameters."));

    /* Volume initialization with profiles
       for species present in the chemical profiles file */
    for (int kk = 0; kk < at_chem->n_species_profiles; kk++) {

      const int f_id = at_chem->species_profiles_to_field_id[kk];
      cs_real_t *cvar_despgi = cs_field_by_id(f_id)->val;
      for (cs_lnum_t c_id = 0; c_id < n_cells; c_id++)
        cvar_despgi[c_id]= cs_intprf(at_chem->n_z_profiles,
                                     at_chem->nt_step_profiles,
                                     at_chem->z_conc_profiles,
                                     at_chem->t_conc_profiles,
                                     at_chem->conc_profiles,
                                     cell_cen[c_id][2],
                                     cs_glob_time_step->t_cur);

    }

    /* Computation of the conversion factor
     *  matrix used for the reaction rates Jacobian matrix */
    for (int ii = 0; ii < at_chem->n_species; ii++)
      for (int kk = 0; kk < at_chem->n_species; kk++) {
        const int sp_id = (at_chem->chempoint[kk]-1)*at_chem->n_species
                        +  at_chem->chempoint[ii] - 1;
        at_chem->conv_factor_jac[sp_id]
          = at_chem->molar_mass[ii]/at_chem->molar_mass[kk];
      }
  }

  /* Atmospheric aerosol chemistry */
  if (at_chem->aerosol_model != CS_ATMO_AEROSOL_OFF) {

    /* Reading initial concentrations
       and numbers from file or from the aerosol library */
    cs_atmo_read_aerosol();

    if (has_restart == 0) {

      const int n_aer = at_chem->n_size;
      const int nlayer_aer = at_chem->n_layer;
      const int size = n_aer*(1+nlayer_aer);

      const cs_equation_param_t *eqp_p
        = cs_field_get_equation_param_const(CS_F_(p));
      const cs_equation_param_t *eqp_vel
        = cs_field_get_equation_param_const(CS_F_(vel));

      if (eqp_vel->verbosity > 0 || eqp_p->verbosity > 0)
        bft_printf("   ** INIT ATMO CHEMISTRY VARIABLE FROM FILE\n"
                   "      --------------------------------------\n");

      for (int ii = 0; ii < size; ii++) {
        int f_id = at_chem->species_to_field_id[at_chem->n_species + ii];
        cs_real_t *cvar_sc = cs_field_by_id(f_id)->val;
        for (cs_lnum_t c_id = 0; c_id < n_cells; c_id++)
          cvar_sc[c_id] = at_chem->dlconc0[ii];
      }
    }
  }

  /*-------------------------------------------------------------------------
   * Check simulation times used by atmo
   * radiative transfer or chemistry models
   *-------------------------------------------------------------------------*/

  if (   (at_opt->radiative_model_1d == 1 || at_chem->model > 0)
      && (   at_opt->syear == -1 || at_opt->squant == -1
          || at_opt->shour == -1 || at_opt->smin  == -1 || at_opt->ssec <= -1.0)  )
    cs_parameters_error
      (CS_ABORT_IMMEDIATE,
       _(input_param_desc),
       _("The simulation time is wrong\n"
         "Check variables syear, squant, shour, smin, ssec\n"
         "By decreasing priority, these variables can be defined\n"
         "in cs_user_parameters or the meteo file or the chemistry file."));

  /* Only if the simulation is not a restart from another one */
  if (has_restart != 0)
    return;

  /* Meteo large scale fields */
  cs_real_3_t *cpro_met_vel = nullptr;
  cs_field_t *f_met_vel = cs_field_by_name_try("meteo_velocity");
  if (f_met_vel != nullptr)
    cpro_met_vel = (cs_real_3_t *) (f_met_vel->val);

  cs_real_t *cpro_met_potemp = nullptr;
  cs_field_t *f_met_potemp = cs_field_by_name_try("meteo_pot_temperature");
  if (f_met_potemp != nullptr)
    cpro_met_potemp = f_met_potemp->val;

  cs_real_t *cpro_met_k = nullptr;
  cs_field_t *f_met_k = cs_field_by_name_try("meteo_tke");
  if (f_met_k != nullptr)
    cpro_met_k = f_met_k->val;

  cs_real_t *cpro_met_eps = nullptr;
  cs_field_t *f_met_eps = cs_field_by_name_try("meteo_eps");
  if (f_met_eps != nullptr)
    cpro_met_eps = f_met_eps->val;

  cs_real_6_t *cpro_met_rij = nullptr;
  cs_field_t *f_met_rij = cs_field_by_name_try("meteo_rij");
  if (f_met_rij != nullptr)
    cpro_met_rij = (cs_real_6_t *) (f_met_rij->val);

  cs_real_t *cpro_met_qv = nullptr;
  cs_field_t *f_met_qv = cs_field_by_name_try("meteo_humidity");
  if (f_met_qv != nullptr)
    cpro_met_qv = f_met_qv->val;

  cs_real_t *cpro_met_nc = nullptr;
  cs_field_t *f_met_nc = cs_field_by_name_try("meteo_drop_nb");
  if (f_met_nc != nullptr)
    cpro_met_nc = f_met_nc->val;

  cs_fluid_properties_t *phys_pro = cs_get_glob_fluid_properties();

  /* Atmospheric fields, velocity and turbulence  */

  cs_real_t *cvar_k = nullptr;
  if (CS_F_(k) != nullptr)
    cvar_k = CS_F_(k)->val;
  cs_real_t *cvar_eps = nullptr;
  if (CS_F_(eps) != nullptr)
    cvar_eps = CS_F_(eps)->val;

  cs_real_t *cvar_phi = nullptr;
  if (CS_F_(phi) != nullptr)
    cvar_phi = CS_F_(phi)->val;

  cs_real_t *cvar_omg = nullptr;
  if (CS_F_(omg) != nullptr)
    cvar_omg = CS_F_(omg)->val;

  cs_real_t *cvar_nusa = nullptr;
  if (CS_F_(nusa) != nullptr)
    cvar_nusa = CS_F_(nusa)->val;

  cs_real_6_t *cvar_rij = nullptr;
  if (CS_F_(rij) != nullptr)
    cvar_rij = (cs_real_6_t *)(CS_F_(rij)->val);

  cs_real_3_t *cvar_vel = (cs_real_3_t *)(CS_F_(vel)->val);
  cs_field_t *f_th = cs_thermal_model_field();

  cs_real_t *cvar_totwt = nullptr;
  cs_real_t *cvar_ntdrp = nullptr;
  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
    cvar_totwt = cs_field_by_name("ym_water")->val;
    cvar_ntdrp = cs_field_by_name("number_of_droplets")->val;
  }

  if (at_opt->meteo_profile == 0) {

    cs_atmo_option_t *aopt = &_atmo_option;
    if (f_th != nullptr) {
      /* Reference fluid properties set from meteo values */
      cs_real_t ps = cs_glob_atmo_constants->ps;
      cs_real_t rair = phys_pro->r_pg_cnst;
      cs_real_t cp0 = phys_pro->cp0;
      cs_real_t rscp = rair/cp0;
      cs_real_t clatev = phys_pro->clatev;
      cs_real_t theta0 = (phys_pro->t0 - clatev/cp0 * aopt->meteo_ql0)
                       * pow(ps/ phys_pro->p0, rscp);

      cs_array_real_set_value(m->n_cells_with_ghosts, 1, &theta0, f_th->val);
    }

    /* Note: cvar_totwt and cvar_ntdrp are already initialized by 0 */
  }
  /* When using meteo data */
  else {
    cs_equation_param_t *eqp_vel = cs_field_get_equation_param(CS_F_(vel));

    if (eqp_vel->verbosity > 0)
      bft_printf("   ** INIT DYNAMIC VARIABLES FROM METEO FILE\n"
                 "      --------------------------------------\n");

    /* Meteo profile or meteo data */
    for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {

      cs_real_t k_in, eps_in;
      cs_real_6_t rij_loc;
      if (cs_glob_atmo_option->meteo_profile == 1) {
        cs_real_t z_in = cell_cen[cell_id][2];

        /* Velocity */
        for (cs_lnum_t i = 0; i < 2; i++) {
          cs_real_t *vel_met;
          if (i == 0)
            vel_met = cs_glob_atmo_option->u_met;
          if (i == 1)
            vel_met = cs_glob_atmo_option->v_met;
          cvar_vel[cell_id][i]
            = cs_intprf(cs_glob_atmo_option->met_1d_nlevels_d,
                        cs_glob_atmo_option->met_1d_ntimes,
                        cs_glob_atmo_option->z_dyn_met,
                        cs_glob_atmo_option->time_met,
                        vel_met,
                        z_in,
                        cs_glob_time_step->t_cur);
        }

        /* Turbulence TKE and dissipation */
        k_in = cs_intprf(
              cs_glob_atmo_option->met_1d_nlevels_d,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_dyn_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->ek_met,
              z_in,
              cs_glob_time_step->t_cur);

        eps_in = cs_intprf(
            cs_glob_atmo_option->met_1d_nlevels_d,
            cs_glob_atmo_option->met_1d_ntimes,
            cs_glob_atmo_option->z_dyn_met,
            cs_glob_atmo_option->time_met,
            cs_glob_atmo_option->ep_met,
            z_in,
            cs_glob_time_step->t_cur);

        /* Theta */
        if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] >= 1) {
          f_th->val[cell_id] = cs_intprf(
              cs_glob_atmo_option->met_1d_nlevels_t,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_temp_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->pot_t_met,
              z_in,
              cs_glob_time_step->t_cur);

        }

        /*  Humid Atmosphere */
        if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
          cvar_totwt[cell_id] = cs_intprf(
              cs_glob_atmo_option->met_1d_nlevels_t,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_temp_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->qw_met,
              z_in,
              cs_glob_time_step->t_cur);

          cvar_ntdrp[cell_id] = cs_intprf(cs_glob_atmo_option->met_1d_nlevels_t,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_temp_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->ndrop_met,
              z_in,
              cs_glob_time_step->t_cur);

        }

      }
      else {
        for (cs_lnum_t i = 0; i < 3; i++)
          cvar_vel[cell_id][i] =cpro_met_vel[cell_id][i];

        /* Turbulence TKE and dissipation */
        k_in = cpro_met_k[cell_id];
        eps_in = cpro_met_eps[cell_id];
        if (f_th != nullptr)
          f_th->val[cell_id] = cpro_met_potemp[cell_id];

        if (cvar_totwt != nullptr)
          cvar_totwt[cell_id]= cpro_met_qv[cell_id];

        if (cvar_ntdrp != nullptr)
          cvar_ntdrp[cell_id] = cpro_met_nc[cell_id];

      }

      cs_real_t vel_dir[3];
      cs_math_3_normalize(cvar_vel[cell_id], vel_dir);

      if (f_met_rij == nullptr) {
        rij_loc[0] = 2. / 3. * k_in;
        rij_loc[1] = 2. / 3. * k_in;
        rij_loc[2] = 2. / 3. * k_in;
        rij_loc[3] = 0.; // Rxy
        rij_loc[4] = -sqrt(cs_turb_cmu) * k_in * vel_dir[1]; // Ryz
        rij_loc[5] = -sqrt(cs_turb_cmu) * k_in * vel_dir[0]; // Rxz

      }
      else
        for (cs_lnum_t i = 0; i < 6; i++)
          rij_loc[i] = cpro_met_rij[cell_id][i]; //TODO give a value


      /* Initialize turbulence from TKE, dissipation and anisotropy if needed */
      if (cvar_k != nullptr)
        cvar_k[cell_id]= k_in;

      if (cvar_eps != nullptr)
        cvar_eps[cell_id] = eps_in;

      if (cvar_rij != nullptr) {
        for (cs_lnum_t i = 0; i < 6; i++)
          cvar_rij[cell_id][i] = rij_loc[i];
      }

      /* Note cvar_fb is already 0. */
      if (cvar_phi != nullptr)
        cvar_phi[cell_id] = 2./3.;

      if (cvar_omg != nullptr)
        cvar_omg[cell_id] = eps_in / k_in / cs_turb_cmu;

      if (cvar_nusa != nullptr)
        cvar_nusa[cell_id] = cs_turb_cmu * k_in * k_in / eps_in ;
    }

  }
}

/*----------------------------------------------------------------------------
 * Automatic boundary condition for atmospheric flows
 *----------------------------------------------------------------------------*/

void
cs_atmo_bcond(void)
{
  cs_atmo_option_t *at_opt = cs_glob_atmo_option;
  cs_atmo_chemistry_t *at_chem = cs_glob_atmo_chemistry;

  bool rain = at_opt->rain;

  /* Mesh-related data */
  cs_mesh_quantities_t *mq = cs_glob_domain->mesh_quantities;
  const cs_lnum_t n_b_faces = cs_glob_mesh->n_b_faces;
  int *bc_type = cs_boundary_conditions_get_bc_type();
  const cs_lnum_t *b_face_cells = cs_glob_mesh->b_face_cells;
  const cs_real_3_t *restrict b_face_cog = mq->b_face_cog;
  const cs_real_3_t *cell_cen = mq->cell_cen;
  const cs_nreal_3_t *restrict b_face_u_normal = mq->b_face_u_normal;

  cs_physical_constants_t *phys_cst = cs_get_glob_physical_constants();
  const cs_fluid_properties_t *phys_pro = cs_get_glob_fluid_properties();
  cs_real_t *grav = phys_cst->gravity;
  cs_real_t ps = cs_glob_atmo_constants->ps;
  cs_real_t rair = phys_pro->r_pg_cnst;
  cs_real_t cp0 = phys_pro->cp0;
  cs_real_t rscp = rair/cp0;

  cs_field_t *th_f = cs_thermal_model_field();
  cs_field_t *ym_w = cs_field_by_name_try("ym_water");

  /* Soil atmosphere boundary conditions
   *------------------------------------ */

  if (at_opt->soil_model > 0) {
    const cs_zone_t *z = cs_boundary_zone_by_id(at_opt->soil_zone_id);
    const cs_real_t *bvar_tempp = cs_field_by_name("soil_pot_temperature")->val;
    const cs_real_t *bvar_total_water = cs_field_by_name("soil_total_water")->val;

    for (cs_lnum_t ii = 0; ii < z->n_elts; ii++) {

      const cs_lnum_t face_id = z->elt_ids[ii];

      // Rough wall if no specified
      // Note: roughness and thermal roughness are computed in solmoy
      if (bc_type[face_id] == 0)
        bc_type[face_id] = CS_ROUGHWALL;

      if (th_f != nullptr) {
        if (th_f->bc_coeffs->rcodcl1[face_id] > cs_math_infinite_r*0.5) {
          // Dirichlet with wall function Expressed directly in term of
          // potential temperature
          th_f->bc_coeffs->icodcl[face_id]  = -6;
          th_f->bc_coeffs->rcodcl1[face_id] = bvar_tempp[ii];
        }
      }
      if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
        // If not yet specified
        if (ym_w->bc_coeffs->rcodcl1[face_id] > cs_math_infinite_r*0.5) {
          ym_w->bc_coeffs->icodcl[face_id]  = 6;
          ym_w->bc_coeffs->rcodcl1[face_id] = bvar_total_water[ii];
        }
      }

    }
  }

  /* Imbrication
     ----------- */

  if (_atmo_imbrication.imbrication_flag) {

    const int id_type = 2; // interpolation on boundary faces
    const cs_lnum_t n_b_faces_max
      = (cs_lnum_t)cs::max(100.0, (cs_real_t)n_b_faces);

    cs_summon_cressman(cs_glob_time_step->t_cur);

    if (_atmo_imbrication.cressman_u) {
      cs_real_t *rcodcl1 = CS_F_(vel)->bc_coeffs->rcodcl1;
      cs_measures_set_t *ms = cs_measures_set_by_id(_atmo_imbrication.id_u);
      cs_cressman_interpol(ms, rcodcl1, id_type);

      if (_atmo_imbrication.imbrication_verbose)
        for (cs_lnum_t face_id = 0; face_id < n_b_faces_max; face_id++)
          bft_printf("cs_atmo_bcond:: xbord, ybord, zbord, ubord = %10.14e %10.14e"
                     "%10.14e %10.14e\n",
                     b_face_cog[face_id][0], b_face_cog[face_id][1],
                     b_face_cog[face_id][2], rcodcl1[face_id]);
    }

    if (_atmo_imbrication.cressman_v) {
      cs_real_t *rcodcl1 = CS_F_(vel)->bc_coeffs->rcodcl1 + n_b_faces;
      cs_measures_set_t *ms = cs_measures_set_by_id(_atmo_imbrication.id_v);
      cs_cressman_interpol(ms, rcodcl1, id_type);

      if (_atmo_imbrication.imbrication_verbose)
        for (cs_lnum_t face_id = 0; face_id < n_b_faces_max; face_id++)
          bft_printf("cs_atmo_bcond:: xbord, ybord, zbord, vbord = %10.14e %10.14e"
                     "%10.14e %10.14e\n",
                     b_face_cog[face_id][0], b_face_cog[face_id][1],
                     b_face_cog[face_id][2], rcodcl1[face_id]);
    }

    if (_atmo_imbrication.cressman_tke) {
      cs_real_t *rcodcl1 = CS_F_(k)->bc_coeffs->rcodcl1;
      cs_measures_set_t *ms = cs_measures_set_by_id(_atmo_imbrication.id_tke);
      cs_cressman_interpol(ms, rcodcl1, id_type);

      if (_atmo_imbrication.imbrication_verbose)
        for (cs_lnum_t face_id = 0; face_id < n_b_faces_max; face_id++)
          bft_printf("cs_atmo_bcond:: xbord, ybord, zbord, tkebord = %10.14e %10.14e"
                     "%10.14e %10.14e\n",
                     b_face_cog[face_id][0], b_face_cog[face_id][1],
                     b_face_cog[face_id][2], rcodcl1[face_id]);

    }

    if (_atmo_imbrication.cressman_eps) {
      cs_real_t *rcodcl1 = CS_F_(eps)->bc_coeffs->rcodcl1;
      cs_measures_set_t *ms = cs_measures_set_by_id(_atmo_imbrication.id_eps);
      cs_cressman_interpol(ms, rcodcl1, id_type);

      if (_atmo_imbrication.imbrication_verbose)
        for (cs_lnum_t face_id = 0; face_id < n_b_faces_max; face_id++)
          bft_printf("cs_atmo_bcond:: xbord, ybord, zbord, epsbord = %10.14e %10.14e"
                     "%10.14e %10.14e\n",
                     b_face_cog[face_id][0], b_face_cog[face_id][1],
                     b_face_cog[face_id][2], rcodcl1[face_id]);
    }

    if (   _atmo_imbrication.cressman_theta
        && cs_glob_physical_model_flag[CS_ATMOSPHERIC] > CS_ATMO_CONSTANT_DENSITY) {
      cs_real_t *rcodcl1 = th_f->bc_coeffs->rcodcl1;
      cs_measures_set_t *ms = cs_measures_set_by_id(_atmo_imbrication.id_theta);
      cs_cressman_interpol(ms, rcodcl1, id_type);

      if (_atmo_imbrication.imbrication_verbose)
        for (cs_lnum_t face_id = 0; face_id < n_b_faces_max; face_id++)
          bft_printf("cs_atmo_bcond:: xbord, ybord, zbord, thetabord = %10.14e %10.14e"
                     "%10.14e %10.14e\n",
                     b_face_cog[face_id][0], b_face_cog[face_id][1],
                     b_face_cog[face_id][2], rcodcl1[face_id]);
    }

    if (   _atmo_imbrication.cressman_qw
        && cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
      cs_real_t *rcodcl1 = ym_w->bc_coeffs->rcodcl1;
      cs_measures_set_t *ms = cs_measures_set_by_id(_atmo_imbrication.id_qw);
      cs_cressman_interpol(ms, rcodcl1, id_type);

      if (_atmo_imbrication.imbrication_verbose)
        for (cs_lnum_t face_id = 0; face_id < n_b_faces_max; face_id++)
          bft_printf("cs_atmo_bcond:: xbord, ybord, zbord, thetabord = %10.14e %10.14e"
                     "%10.14e %10.14e\n",
                     b_face_cog[face_id][0], b_face_cog[face_id][1],
                     b_face_cog[face_id][2], rcodcl1[face_id]);
    }

    if (   _atmo_imbrication.cressman_nc
        && cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
      cs_field_t *f = cs_field_by_name("number_of_droplets");
      cs_real_t *rcodcl1 = f->bc_coeffs->rcodcl1;
      cs_measures_set_t *ms = cs_measures_set_by_id(_atmo_imbrication.id_nc);
      cs_cressman_interpol(ms, rcodcl1, id_type);

      if (_atmo_imbrication.imbrication_verbose)
        for (cs_lnum_t face_id = 0; face_id < n_b_faces_max; face_id++)
          bft_printf("cs_atmo_bcond:: xbord, ybord, zbord, thetabord = %10.14e %10.14e"
                     "%10.14e %10.14e\n",
                     b_face_cog[face_id][0], b_face_cog[face_id][1],
                     b_face_cog[face_id][2], rcodcl1[face_id]);
    }

  } //  imbrication_flag

  /*  Atmospheric gaseous chemistry
      ----------------------------- */
  if (at_chem->model > 0) {

    for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {

      if (bc_type[face_id] != CS_INLET)
        continue;

      /* For species present in the concentration profiles chemistry file,
         profiles are used here as boundary conditions if boundary
         conditions have not been treated earlier (eg, in cs_user_boundary_conditions) */
      for (int ii = 0; ii < at_chem->n_species_profiles; ii++) {
        const int f_id = at_chem->species_to_scalar_id[ii];
        cs_field_t *f = cs_field_by_id(f_id);
        if (f->bc_coeffs->rcodcl1[face_id] <= cs_math_infinite_r*0.5)
          continue;
        const cs_real_t xcent = cs_intprf(at_chem->n_z_profiles,
                                          at_chem->nt_step_profiles,
                                          at_chem->z_conc_profiles,
                                          at_chem->t_conc_profiles,
                                          at_chem->conc_profiles,
                                          b_face_cog[face_id][2],
                                          cs_glob_time_step->t_cur);
        f->bc_coeffs->rcodcl1[face_id] = xcent;
      }

      /* For other species zero Dirichlet conditions are imposed,
       * unless they have already been treated earlier
       (eg, in cs_user_boundary_conditions) */
      for (int ii = 0; ii < at_chem->n_species; ii++) {
        const int f_id = at_chem->species_to_scalar_id[ii];
        cs_field_t *f = cs_field_by_id(f_id);
        if (f->bc_coeffs->rcodcl1[face_id] > cs_math_infinite_r*0.5)
          f->bc_coeffs->rcodcl1[face_id] = 0.0;
      }

    }

  }

  // Atmospheric aerosol chemistry
  if (at_chem->aerosol_model != CS_ATMO_AEROSOL_OFF) {

    const int n_aer = at_chem->n_size;
    const int nespg = at_chem->n_species;
    const int nlayer_aer = at_chem->n_layer;

    for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {

      if (bc_type[face_id] != CS_INLET)
        continue;

      for (int ii = 0; ii < nlayer_aer*n_aer+n_aer; ii++) {
        const int f_id = at_chem->species_to_scalar_id[ii];
        cs_field_t *f = cs_field_by_id(f_id);
        if (f->bc_coeffs->rcodcl1[face_id] > cs_math_infinite_r*0.5)
          f->bc_coeffs->rcodcl1[face_id] = at_chem->dlconc0[ii];
      }

      /* For other species zero dirichlet conditions are imposed,
         unless they have already been treated earlier */
      for (int ii = 0; ii < nlayer_aer*n_aer+n_aer; ii++) {
        const int f_id = at_chem->species_to_scalar_id[ii];
        cs_field_t *f = cs_field_by_id(f_id);
        if (f->bc_coeffs->rcodcl1[face_id] > cs_math_infinite_r*0.5)
          f->bc_coeffs->rcodcl1[face_id] = 0.0;
      }

      /* For gaseous species which have not been treated earlier
         (for example species not present in the third gaseous scheme,
         which can be treated in usatcl of with the file chemistry)
         zero Dirichlet conditions are imposed */
      for (int ii = 0; ii < nespg; ii++) {
        const int f_id = at_chem->species_to_scalar_id[ii];
        cs_field_t *f = cs_field_by_id(f_id);
        if (f->bc_coeffs->rcodcl1[face_id] > cs_math_infinite_r*0.5)
          f->bc_coeffs->rcodcl1[face_id] = 0.0;
      }
    }

  }

  /* Boundary condition for rain phase */
  if (rain == true) {
    cs_field_t *yr= cs_field_by_name("ym_l_r");

    for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {

      if (   bc_type[face_id] == CS_SMOOTHWALL
          || bc_type[face_id] == CS_ROUGHWALL) {

        yr->bc_coeffs->icodcl[face_id] = 1;
        yr->bc_coeffs->rcodcl1[face_id] = 0.;
      }
    }
  }

  /* Inlet BCs from meteo profiles
   * ----------------------------- */

  int *iautom = cs_glob_bc_pm_info->iautom;
  int *icodcl_vel = CS_F_(vel)->bc_coeffs->icodcl;
  int *icodcl_p = CS_F_(p)->bc_coeffs->icodcl;
  cs_real_t *rcodcl1_vel = CS_F_(vel)->bc_coeffs->rcodcl1;
  cs_real_t *rcodcl1_p = CS_F_(p)->bc_coeffs->rcodcl1;

  cs_real_t *rcodcl1_k = nullptr;
  if (CS_F_(k) != nullptr)
    rcodcl1_k = CS_F_(k)->bc_coeffs->rcodcl1;
  cs_real_t *rcodcl1_eps = nullptr;
  if (CS_F_(eps) != nullptr)
    rcodcl1_eps = CS_F_(eps)->bc_coeffs->rcodcl1;

  cs_field_t *f_th = cs_thermal_model_field();
  int *icodcl_theta = nullptr;
  cs_real_t *rcodcl1_theta = nullptr;
  if (f_th  != nullptr) {
    icodcl_theta = f_th->bc_coeffs->icodcl;
    rcodcl1_theta = f_th->bc_coeffs->rcodcl1;
  }

  cs_real_t *rcodcl1_qw = nullptr;
  cs_real_t *rcodcl1_nc = nullptr;
  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
    rcodcl1_qw = cs_field_by_name("ym_water")->bc_coeffs->rcodcl1;
    rcodcl1_nc = cs_field_by_name("number_of_droplets")->bc_coeffs->rcodcl1;
  }

  /* Meteo large scale fields */
  cs_real_3_t *cpro_met_vel = nullptr;
  cs_field_t *f_met_vel = cs_field_by_name_try("meteo_velocity");
  if (f_met_vel != nullptr)
    cpro_met_vel = (cs_real_3_t *) (f_met_vel->val);

  cs_real_t *cpro_met_potemp = nullptr;
  cs_field_t *f_met_potemp = cs_field_by_name_try("meteo_pot_temperature");
  if (f_met_potemp != nullptr)
    cpro_met_potemp = f_met_potemp->val;

  cs_real_t *cpro_met_p = nullptr;
  cs_field_t *f_met_p = cs_field_by_name_try("meteo_pressure");
  if (f_met_p != nullptr)
    cpro_met_p = f_met_p->val;

  cs_real_t *cpro_met_rho = nullptr;
  cs_field_t *f_met_rho = cs_field_by_name_try("meteo_density");
  if (f_met_rho != nullptr)
    cpro_met_rho = f_met_rho->val;

  cs_real_t *cpro_met_k = nullptr;
  cs_field_t *f_met_k = cs_field_by_name_try("meteo_tke");
  if (f_met_k != nullptr)
    cpro_met_k = f_met_k->val;

  cs_real_t *cpro_met_eps = nullptr;
  cs_field_t *f_met_eps = cs_field_by_name_try("meteo_eps");
  if (f_met_eps != nullptr)
    cpro_met_eps = f_met_eps->val;

  cs_real_6_t *cpro_met_rij = nullptr;
  cs_field_t *f_met_rij = cs_field_by_name_try("meteo_rij");
  if (f_met_rij != nullptr)
    cpro_met_rij = (cs_real_6_t *) (f_met_rij->val);

  cs_real_t *cpro_met_qv = nullptr;
  cs_field_t *f_met_qv = cs_field_by_name_try("meteo_humidity");
  if (f_met_qv != nullptr)
    cpro_met_qv = f_met_qv->val;

  cs_real_t *cpro_met_nc = nullptr;
  cs_field_t *f_met_nc = cs_field_by_name_try("meteo_drop_nb");
  if (f_met_nc != nullptr)
    cpro_met_nc = f_met_nc->val;

  for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {

    cs_lnum_t cell_id = b_face_cells[face_id];

    /* If meteo profile is on, we take the value and store it in rcolcl if not
     * already modified
     * It will be used for inlet or backflows */

    if (cs_glob_atmo_option->meteo_profile >= 1) {
      cs_real_t z_in = b_face_cog[face_id][2];

      cs_real_t vel_in[3] = {0., 0., 0.};
      /* If specified by the user or by code-code coupling */
      for (cs_lnum_t i = 0; i < 3; i++) {
        if (rcodcl1_vel[i*n_b_faces + face_id] < 0.5 * cs_math_infinite_r)
          vel_in[i] = rcodcl1_vel[i*n_b_faces + face_id];
        else if (cs_glob_atmo_option->meteo_profile == 1) {
          cs_real_t *vel_met;
          if (i == 0)
            vel_met = cs_glob_atmo_option->u_met;
          if (i == 1)
            vel_met = cs_glob_atmo_option->v_met;

          if (i != 2)
            vel_in[i] = cs_intprf(
                cs_glob_atmo_option->met_1d_nlevels_d,
                cs_glob_atmo_option->met_1d_ntimes,
                cs_glob_atmo_option->z_dyn_met,
                cs_glob_atmo_option->time_met,
                vel_met,
                z_in,
                cs_glob_time_step->t_cur);
        }
        else
          vel_in[i] = cpro_met_vel[cell_id][i];
      }

      cs_real_t k_in = cs_math_infinite_r;
      if (CS_F_(k) != nullptr) {
        if (rcodcl1_k[face_id] < 0.5 * cs_math_infinite_r)
          k_in = rcodcl1_k[face_id];
      }
      if (k_in > 0.5 * cs_math_infinite_r) {
        if (cs_glob_atmo_option->meteo_profile == 1)
          k_in = cs_intprf(
              cs_glob_atmo_option->met_1d_nlevels_d,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_dyn_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->ek_met,
              z_in,
              cs_glob_time_step->t_cur);

        else
          k_in = cpro_met_k[cell_id];
      }

      cs_real_t eps_in = cs_math_infinite_r;
      if (CS_F_(eps) != nullptr) {
        if (rcodcl1_eps[face_id] < 0.5 * cs_math_infinite_r)
          eps_in = rcodcl1_eps[face_id];
      }
      if (eps_in > 0.5 * cs_math_infinite_r) {
        if (cs_glob_atmo_option->meteo_profile == 1)
          eps_in = cs_intprf(
              cs_glob_atmo_option->met_1d_nlevels_d,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_dyn_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->ep_met,
              z_in,
              cs_glob_time_step->t_cur);

        else
          eps_in = cpro_met_eps[cell_id];
      }

      cs_real_t theta_in;
      if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] >= 1) {
        if (rcodcl1_theta[face_id] < 0.5 * cs_math_infinite_r)
          theta_in = rcodcl1_theta[face_id];
        else if (cs_glob_atmo_option->meteo_profile == 1)
          theta_in = cs_intprf(
              cs_glob_atmo_option->met_1d_nlevels_t,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_temp_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->pot_t_met,
              z_in,
              cs_glob_time_step->t_cur);


        else
          theta_in = cpro_met_potemp[cell_id];
      }

      cs_real_t vel_dot_n =
        cs_math_3_dot_product(vel_in, b_face_u_normal[face_id]);

      /* We update the boundary type if automatic setting is on
       * We update the Dirichlet value if not already specified by the user
       * */
      if (iautom[face_id] >= 1) {
        if (vel_dot_n > cs_math_epzero)
          bc_type[face_id] = CS_OUTLET;
        else
          if (bc_type[face_id] == 0)
            bc_type[face_id] = CS_INLET;
      }

      cs_real_t vel_dir[3];

      /* Normalized velocity direction */
      cs_math_3_normalize(vel_in, vel_dir);

      cs_real_t rij_loc[6];
      if (f_met_rij == nullptr) {
        rij_loc[0] = 2. / 3. * k_in;
        rij_loc[1] = 2. / 3. * k_in;
        rij_loc[2] = 2. / 3. * k_in;
        rij_loc[3] = 0.; // Rxy
        rij_loc[4] = -sqrt(cs_turb_cmu) * k_in * vel_dir[1]; // Ryz
        rij_loc[5] = -sqrt(cs_turb_cmu) * k_in * vel_dir[0]; // Rxz

      }
      else
        for (cs_lnum_t i = 0; i < 6; i++)
          rij_loc[i] = cpro_met_rij[cell_id][i];

      if (   bc_type[face_id] == CS_INLET
          || bc_type[face_id] == CS_CONVECTIVE_INLET) {

        /* If not already specified */
        for (cs_lnum_t i = 0; i < 3; i++) {
          if (rcodcl1_vel[i*n_b_faces + face_id] > cs_math_infinite_r * 0.5)
            rcodcl1_vel[i*n_b_faces + face_id] = vel_in[i];
        }

        /* Turbulence inlet */
        cs_turbulence_bc_set_uninit_inlet(face_id, k_in, rij_loc, eps_in);

        /* Thermal scalar and humid atmosphere variables */
        if (f_th != nullptr) {

          if (rcodcl1_theta[face_id] > 0.5 * cs_math_infinite_r)
            rcodcl1_theta[face_id] = theta_in;

          /*  Humid Atmosphere */
          if (rcodcl1_qw != nullptr) {
            if (rcodcl1_qw[face_id] > 0.5 * cs_math_infinite_r) {
              cs_real_t qw_in;
              if (cs_glob_atmo_option->meteo_profile == 1)
                qw_in = cs_intprf(
                    cs_glob_atmo_option->met_1d_nlevels_t,
                    cs_glob_atmo_option->met_1d_ntimes,
                    cs_glob_atmo_option->z_temp_met,
                    cs_glob_atmo_option->time_met,
                    cs_glob_atmo_option->qw_met,
                    z_in,
                    cs_glob_time_step->t_cur);

              else
                qw_in = cpro_met_qv[cell_id];
              rcodcl1_qw[face_id] = qw_in;

            }
          }
          if (rcodcl1_nc != nullptr) {
            if (rcodcl1_nc[face_id] > 0.5 * cs_math_infinite_r) {
              cs_real_t nc_in;
              if (cs_glob_atmo_option->meteo_profile == 1)
                nc_in = cs_intprf(cs_glob_atmo_option->met_1d_nlevels_t,
                    cs_glob_atmo_option->met_1d_ntimes,
                    cs_glob_atmo_option->z_temp_met,
                    cs_glob_atmo_option->time_met,
                    cs_glob_atmo_option->ndrop_met,
                    z_in,
                    cs_glob_time_step->t_cur);


              else
                nc_in = cpro_met_nc[cell_id];
              rcodcl1_nc[face_id] = nc_in;
            }
          }

        }

      } /* End of inlets */

      /* Large scale forcing with momentum source terms
       * ---------------------------------------------- */
      if (cs_glob_atmo_option->open_bcs_treatment > 0) {

        if (iautom[face_id] >= 1) {

          /* Dirichlet on the pressure: expressed in solved pressure directly
           * (not in total pressure), that is why -1 is used
           * (transformed as 1 in cs_boundary_conditions_type). */
          icodcl_p[face_id] = -1;
          rcodcl1_p[face_id] = CS_F_(p)->bc_coeffs->a[face_id];

          /* Dirichlet on turbulent variables */
          cs_turbulence_bc_set_uninit_inlet(face_id, k_in, rij_loc, eps_in);

          if (iautom[face_id] == 1) {

            /* Homogeneous Neumann on the velocity */
            icodcl_vel[face_id] = 3;
            for (cs_lnum_t i = 0; i < 3; i++)
              rcodcl1_vel[i*n_b_faces + face_id] = 0.;
          }
          else if (iautom[face_id] == 2) {

            /* Dirichlet on the velocity */
            icodcl_vel[face_id] = 1;
            for (cs_lnum_t i = 0; i < 3; i++)
              rcodcl1_vel[i*n_b_faces + face_id] = vel_in[i];
          }
        }
      } /* End of open BCs */
    }

    /* Conversion Temperature to potential temperature for Dirichlet and
     * wall boundary conditions
     *
     * if icodcl < 0 it is directly expressed in term of potential temperature
     * so no need of conversion. */
    if (icodcl_theta != nullptr) {

      if (icodcl_theta[face_id] < 0)
        icodcl_theta[face_id] = cs::abs(icodcl_theta[face_id]);
      else if ((icodcl_theta[face_id] == 1
            || icodcl_theta[face_id] == 5 || icodcl_theta[face_id] == 6)
         && rcodcl1_theta[face_id] < 0.5 * cs_math_infinite_r) {

        cs_real_t z_in = b_face_cog[face_id][2];
        cs_real_t pp;
        cs_real_t dum1;
        cs_real_t dum2;
        if (cs_glob_atmo_option->meteo_profile == 0)
          cs_atmo_profile_std(0., /* z_ref */
                              phys_pro->p0,
                              phys_pro->t0,
                              z_in,
                              &pp, &dum1, &dum2);

        /* Pressure profile from meteo file: */
        else if (cs_glob_atmo_option->meteo_profile == 1)
          pp = cs_intprf(
              cs_glob_atmo_option->met_1d_nlevels_t,
              cs_glob_atmo_option->met_1d_ntimes,
              cs_glob_atmo_option->z_temp_met,
              cs_glob_atmo_option->time_met,
              cs_glob_atmo_option->hyd_p_met,
              z_in,
              cs_glob_time_step->t_cur);

        else
          pp = cpro_met_p[cell_id]
            - cpro_met_rho[cell_id] * grav[2] * (cell_cen[cell_id][2] - z_in);

        /* Convert from temperature in Kelvin to potential temperature */
        rcodcl1_theta[face_id] *= pow(ps/pp, rscp);
      }
    }
  } /* End loop of boundary faces */

  /* Inlet BCs for thermal turbulent fluxes
   * -------------------------------------- */
  if (cs_glob_atmo_option->meteo_profile == 2) {

    cs_field_t *f_tf
      = cs_field_by_composite_name_try("temperature", "turbulent_flux");

    if (f_tf != nullptr) {
      int *icodcl_tf = f_tf->bc_coeffs->icodcl;
      cs_real_t *rcodcl1_tf = f_tf->bc_coeffs->rcodcl1;

      cs_real_3_t *muptp =
        (cs_real_3_t *)(cs_field_by_name("meteo_temperature_turbulent_flux")->val);
      for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {

        if (   bc_type[face_id] == CS_INLET
            || bc_type[face_id] == CS_CONVECTIVE_INLET) {

          cs_lnum_t cell_id = b_face_cells[face_id];
          icodcl_tf[face_id] = 1;
          for (cs_lnum_t k = 0; k < f_tf->dim; k++)
            rcodcl1_tf[k*n_b_faces+face_id] = muptp[cell_id][k];
        }
      }
    }

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Deardorff force restore model
 */
/*----------------------------------------------------------------------------*/

void
cs_soil_model(void)
{
  int z_id = cs_glob_atmo_option->soil_zone_id;
  if (z_id > -1) {
    int micro_scale_option = cs_glob_atmo_option->soil_meb_model;

    cs_rad_transfer_params_t *rt_params = cs_glob_rad_transfer_params;

    const cs_real_t stephn = cs_physical_constants_stephan;
    const cs_zone_t *z = cs_boundary_zone_by_id(z_id);
    const cs_lnum_t *elt_ids = z->elt_ids;
    const cs_lnum_t *b_face_cells = cs_glob_mesh->b_face_cells;
    cs_mesh_quantities_t *fvq   = cs_glob_mesh_quantities;
    const cs_real_3_t *cell_cen = (const cs_real_3_t *)fvq->cell_cen;
    cs_real_t *dt = cs_field_by_name("dt")->val;
    /* Post treatment fields */
    cs_field_t *soil_sensible_heat
      = cs_field_by_name_try("soil_sensible_heat");
    cs_field_t *soil_latent_heat
      = cs_field_by_name_try("soil_latent_heat");
    cs_field_t *soil_thermal_rad_upward
      = cs_field_by_name_try("soil_thermal_rad_upward");
    cs_field_t *soil_thermal_rad_downward
      = cs_field_by_name_try("soil_thermal_rad_downward");
    cs_field_t *soil_visible_rad_absorbed
      = cs_field_by_name_try("soil_visible_rad_absorbed");

    /* Soil related fields */
    cs_field_t *soil_temperature = cs_field_by_name("soil_temperature");
    cs_field_t *soil_pot_temperature = cs_field_by_name("soil_pot_temperature");
    cs_field_t *soil_total_water = cs_field_by_name("soil_total_water");
    cs_field_t *soil_w1 = cs_field_by_name("soil_w1");
    cs_field_t *soil_w2 = cs_field_by_name("soil_w2");
    cs_real_t *f_fos = cs_field_by_name("soil_solar_incident_flux")->val;
    cs_real_t *f_foir = cs_field_by_name("soil_infrared_incident_flux")->val;
    cs_field_t *soil_temperature_deep = cs_field_by_name("soil_temperature_deep");
    cs_field_t *soil_r1 = cs_field_by_name("soil_r1");
    cs_field_t *soil_r2 = cs_field_by_name("soil_r2");
    cs_field_t *soil_water_capacity = cs_field_by_name("soil_water_capacity");
    cs_field_t *soil_water_ratio = cs_field_by_name("soil_water_ratio");
    cs_field_t *soil_thermal_capacity = cs_field_by_name("soil_thermal_capacity");
    cs_field_t *soil_percentages = cs_field_by_name("atmo_soil_percentages");
    cs_field_t *boundary_vegetation = cs_field_by_name("boundary_vegetation");
    /* Fields related to all faces */
    cs_field_t *boundary_albedo = cs_field_by_name_try("boundary_albedo");
    cs_field_t *emissivity = cs_field_by_name_try("emissivity");
    /* Cell fields  */
    cs_field_t *atm_total_water = cs_field_by_name_try("ym_water");
    cs_field_t *atm_temp = CS_F_(t);
    cs_field_t *meteo_pressure = cs_field_by_name_try("meteo_pressure");
    /* Radiative tables */
    cs_real_t *sold = (cs_real_t *)  cs_glob_atmo_option->rad_1d_sold;
    cs_real_t *ird = (cs_real_t *) cs_glob_atmo_option->rad_1d_ird;
    /* Pointer to the spectral flux density field */
    cs_field_t *f_qinspe = nullptr;
    if (rt_params->atmo_model != CS_RAD_ATMO_3D_NONE)
      f_qinspe = cs_field_by_name_try("spectral_rad_incident_flux");

    /* Exchange coefficients*/
    cs_real_t *h_t = CS_F_(t)->bc_coeffs->bf;
    cs_real_t *h_q = atm_total_water->bc_coeffs->bf;

    const cs_fluid_properties_t *phys_pro = cs_get_glob_fluid_properties();
    /* Deardorff parameterisation */
    const cs_real_t tau_1 = 86400.;

    cs_air_fluid_props_t *ct_prop = cs_glob_air_props;

    /* Update previous values */
    cs_field_current_to_previous(soil_pot_temperature);
    cs_field_current_to_previous(soil_total_water);

    /* In case of multi energy balance (MEB) models including PV */
    cs_field_t *cover_geometry_ratio
      = cs_field_by_name_try("cover_geometry_ratio");
    cs_field_t *cover_reflectivity
      = cs_field_by_name_try("cover_reflectivity");
    cs_field_t *cover_temperature_radiative
      = cs_field_by_name_try("cover_temperature_radiative");

    for (cs_lnum_t soil_id = 0; soil_id < z->n_elts; soil_id++) {
      cs_real_t ray2 = 0;
      cs_real_t chas2 = 0;
      cs_real_t chal2 = 0;
      cs_real_t rapp2 = 0;
      cs_real_t secmem = 0.;
      cs_real_t premem = 0.;
      cs_real_t pphy = 0.;
      cs_real_t dum = 0.;
      cs_real_t qvs_new = 0.;
      cs_real_t w1_min = 0.;
      cs_real_t w1_max = 1.;
      cs_real_t w2_min = 0.;
      cs_real_t w2_max = 1.;
      cs_real_t precip = 0.;
      cs_real_t tseuil = 16. + cs_physical_constants_celsius_to_kelvin;
      cs_real_t ts_new = 0.;
      cs_real_t ts_c_new = 0.; /* in Celsius */
      cs_real_t cpvcpa = ct_prop->cp_v / ct_prop->cp_a;
      cs_real_t rvsra = phys_pro->rvsra;
      cs_real_t clatev = phys_pro->clatev;
      cs_real_t cp0 = phys_pro->cp0;
      cs_real_t rair = phys_pro->r_pg_cnst;
      cs_real_t ps = cs_glob_atmo_constants->ps;
      cs_real_t esat = 0.;

      cs_lnum_t face_id = elt_ids[soil_id];
      cs_lnum_t cell_id = b_face_cells[face_id];
      cs_real_t foir = 0.;
      cs_real_t fos = 0.;

      cs_real_t gcr = 0.;
      cs_real_t refl = 0.;
      if (cover_geometry_ratio != nullptr)
        gcr = cover_geometry_ratio->val[soil_id];
      if (cover_reflectivity != nullptr)
        refl = cover_reflectivity->val[soil_id];

      /* Infrared and Solar radiative fluxes
       * Warning: should be adapted for many verticales */
      if (cs_glob_atmo_option->radiative_model_1d == 1
         && rt_params->atmo_model == CS_RAD_ATMO_3D_NONE) {
        foir = ird[0];
        fos = sold[0];
      }
      /* In case of 3D, take the incident flux */
      else if (rt_params->atmo_model != CS_RAD_ATMO_3D_NONE) {

        /* Number of bands (stride) */
        cs_lnum_t stride = rt_params->nwsgg;

        /* Compute fos */
        int gg_id = rt_params->atmo_dr_id;
        if (gg_id > -1)
          fos += f_qinspe->val[gg_id + face_id * stride];

        gg_id = rt_params->atmo_dr_o3_id;
        if (gg_id > -1)
          fos += f_qinspe->val[gg_id + face_id * stride];

        gg_id = rt_params->atmo_df_id;
        if (gg_id > -1)
          fos += f_qinspe->val[gg_id + face_id * stride];

        gg_id = rt_params->atmo_df_o3_id;
        if (gg_id > -1)
          fos += f_qinspe->val[gg_id + face_id * stride];

        /* Compute foir */
        gg_id = rt_params->atmo_ir_id;
        if (gg_id > -1)
          foir += f_qinspe->val[gg_id + face_id * stride];
      }

      /* f_fos and f_foir store the radiations coming ontop of
       * the boundary cell */
      f_fos[soil_id] = fos;
      f_foir[soil_id] = foir;

      if (cs_glob_atmo_option->meteo_profile == 0) {
        cs_atmo_profile_std(0., /* z_ref */
                            phys_pro->p0,
                            phys_pro->t0,
                            cell_cen[cell_id][2], &pphy, &dum, &dum);
      }
      else if (cs_glob_atmo_option->meteo_profile == 1) {
        int nbmett = cs_glob_atmo_option->met_1d_nlevels_t;
        int nbmetm = cs_glob_atmo_option->met_1d_ntimes;
        pphy = cs_intprf(nbmett,
            nbmetm,
            cs_glob_atmo_option->z_temp_met,
            cs_glob_atmo_option->time_met,
            cs_glob_atmo_option->hyd_p_met,
            cell_cen[cell_id][2],
            cs_glob_time_step->t_cur);
      }
      else {
        pphy = meteo_pressure->val[cell_id];
      }

      /* ====================================
       * Specific case for water faces (sea/lake)
       * ==================================== */

      /* Water is second component of soil_percentages */
      cs_lnum_t cat_id = 1 + soil_percentages->dim * soil_id;
      if (soil_percentages->val[cat_id] > 50.) {
        /* NB: soil_temperature in °C */
        esat = cs_air_pwv_sat(soil_temperature->val[soil_id]);
        qvs_new = esat / ( rvsra * pphy
          + esat * (1.0 - rvsra));
        /* Constant temperature at the moment
         * TODO integrate the lake model GLM to match with LNHE models */
        ts_c_new = soil_temperature->val[soil_id];
        ts_new = ts_c_new + cs_physical_constants_celsius_to_kelvin;
      }
      else {

      cs_real_t rscp1 = (rair / cp0) * (1. + (rvsra - cpvcpa)
          * soil_total_water->val[soil_id] );

      /* ===============================
       * Compute coefficients for heat and latent heat fluxes
       * =============================== */

      /* ratio specific heat of humide air/ specidif heat of dry air
       * Cph/Cpd */
      cs_real_t cph_dcpd = (1. + (cpvcpa - 1.)
        * soil_total_water->val[soil_id] );
      /* Conversion theta -> T */
      cs_real_t cht = h_t[face_id] * pow(ps / pphy, rscp1) * cph_dcpd;

      cs_real_t chq = h_q[face_id]
        * (clatev - 2370.* (soil_temperature->val[soil_id]) );

      /* ===============================
       * Compute reservoirs water (shallow and deep)
       * =============================== */

      cs_real_t evapor = - h_q[face_id] * (atm_total_water->val[cell_id]
        - soil_total_water->val[soil_id]);
      cs_real_t dtref  = dt[cell_id];

      cs_real_t w1_num = soil_w1->val[soil_id]
        + dtref * (precip - evapor) / soil_water_capacity->val[soil_id]
        + soil_w2->val[soil_id] * dtref
          / (tau_1 + soil_water_ratio->val[soil_id] * dtref);
      cs_real_t w1_den = 1.
        + 1. / (tau_1/dtref + soil_water_ratio->val[soil_id]);
      cs_real_t w1_new = w1_num / w1_den;
      w1_new = cs::max(w1_new, w1_min);
      w1_new = cs::min(w1_new, w1_max);
      cs_real_t w2_num = soil_w2->val[soil_id] * tau_1
        + w1_new * dtref * soil_water_ratio->val[soil_id];
      cs_real_t w2_den = tau_1 + dtref * soil_water_ratio->val[soil_id];
      cs_real_t w2_new = w2_num / w2_den;
      w2_new = cs::max(w2_new, w2_min);
      w2_new = cs::min(w2_new, w2_max);

      soil_w1->val[soil_id] = w1_new;
      soil_w2->val[soil_id] = w2_new;

      cs_real_t hu = 0.5 * (1. - cos(cs_math_pi * w1_new));

      /* ============================
       * Compute saturated pressure and DL1
       * ============================ */

      esat = cs_air_pwv_sat(soil_temperature->val[soil_id]);
      cs_real_t rapsat = rvsra * pphy + esat * (1. - rvsra);
      cs_real_t qsat = esat / rapsat;
      cs_real_t cstder = 17.438 * 239.78; //#TODO transfer in cs_air_props.c
      cs_real_t dqsat = pphy * rvsra * cstder * esat
        / cs_math_pow2(rapsat * (soil_temperature->val[soil_id] + 239.78));

      /* Compute equivalent emissivity and reflexivity du to pannels/plants */
      cs_real_t c_refl = 1. - refl;
      cs_real_t emi = emissivity->val[face_id];
      cs_real_t emi_eq = gcr * emi / (emi * refl + c_refl);
      cs_real_t c_gcr = 1. - gcr;
      /* ============================
       * Compute the first member of soil_temperature equation
       * ============================ */

      cs_lnum_t iseuil = 0;
      if (soil_temperature->val[soil_id] < tseuil)
        iseuil = 1;

      cs_lnum_t ichal = 1;

      cs_real_t ray1 = 4. * c_gcr * emissivity->val[face_id] * stephn
        * cs_math_pow3(soil_temperature->val[soil_id]
        + cs_physical_constants_celsius_to_kelvin);
      cs_real_t chas1 = cht;
      cs_real_t chal1 = chq * hu * dqsat;
      cs_real_t rapp1 = 2.* cs_math_pi / tau_1;

      premem = soil_thermal_capacity->val[soil_id] * ( ray1
        + chas1 + chal1 * ichal + soil_r2->val[soil_id] * iseuil ) + rapp1;

      /* ============================
       * Compute the second member of soil_temperature equation
       * ============================ */

      ray2 = c_gcr * fos*(1. - boundary_albedo->val[face_id])
        + emi * c_gcr * foir
        + 3. * (emi_eq * c_refl  + c_gcr) * emi * stephn
        * cs_math_pow4(soil_temperature->val[soil_id]
        + cs_physical_constants_celsius_to_kelvin);

      if ((micro_scale_option == CS_ATMO_SOIL_PHOTOVOLTAICS) ||
          (micro_scale_option == CS_ATMO_SOIL_VEGETATION)) {
        ray2 += cs_math_pow4(cover_temperature_radiative->val[soil_id]
            + cs_physical_constants_celsius_to_kelvin) * stephn
          * emi_eq * c_refl * refl;
      }
      chas2 = cht * atm_temp->val[cell_id]
        * pow(pphy / ps , rscp1);
      chal2 = chq * (atm_total_water->val[cell_id]
        * (1. - boundary_vegetation->val[soil_id] * ( 1. - hu))
        - hu * (qsat - (soil_temperature->val[soil_id]
              + cs_physical_constants_celsius_to_kelvin)
              * dqsat));
      rapp2 = 2.*cs_math_pi * (soil_temperature_deep->val[soil_id]
        + cs_physical_constants_celsius_to_kelvin) / tau_1;

      secmem = soil_thermal_capacity->val[soil_id] * ( ray2
        + chas2 + chal2 * ichal + soil_r1->val[soil_id]
        + tseuil * soil_r2->val[soil_id] * iseuil ) + rapp2;

      /* ============================
       * Compute new soil variables
       * ============================ */

      ts_new = (soil_temperature->val[soil_id]
        + cs_physical_constants_celsius_to_kelvin + dtref * secmem )
        / ( 1. + dtref * premem );
      ts_c_new = (soil_temperature->val[soil_id]
        + dtref * (secmem - cs_physical_constants_celsius_to_kelvin * premem))
        / ( 1. + dtref * premem );

      qvs_new = hu * ( qsat + dqsat * ( ts_c_new
        - soil_temperature->val[soil_id] ))
        + boundary_vegetation->val[soil_id] * atm_total_water->val[cell_id]
        * ( 1. - hu );

      /* TODO filling soil fields should be done one for loop below
       * by computing cht and chq for the ""lake model""
       * At the moment the allocation is performed ONLY for ""soil model""  */

      if (soil_latent_heat != nullptr)
        soil_latent_heat->val[soil_id] = chq * (qvs_new
            - atm_total_water->val[cell_id]);
      if (soil_sensible_heat != nullptr)
        soil_sensible_heat->val[soil_id] = cht * ((ts_new
              * (pow(ps/pphy,(rair/cp0)
                  * (1. + (rvsra - cpvcpa) * qvs_new ))))
            - atm_temp->val[cell_id]);
      if (soil_thermal_rad_upward != nullptr)
        soil_thermal_rad_upward->val[soil_id] = stephn * emi
          * cs_math_pow4(ts_new);
      if (soil_thermal_rad_downward != nullptr)
        soil_thermal_rad_downward->val[soil_id] = emi * foir;
      if (soil_visible_rad_absorbed != nullptr)
        soil_visible_rad_absorbed->val[soil_id] =
          (1. - boundary_albedo->val[face_id]) * fos;

      }

      /* ============================
       * Update new soil variables
       * ============================ */

      soil_temperature->val[soil_id] = ts_c_new;

      soil_pot_temperature->val[soil_id] = ts_new
        * (pow(ps/pphy,(rair/cp0)
        * (1. + (rvsra - cpvcpa) * qvs_new )));
      soil_total_water->val[soil_id] = qvs_new;
    }
  }
  else {
    bft_error(__FILE__, __LINE__, 0,
              _("cs_glob_atmo_option->soil_zone_id is missing."));
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize meteo profiles if no meteo file is given.
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_init_meteo_profiles(void)
{
  /* Some turbulence constants */
  cs_real_t kappa = cs_turb_xkappa;

  cs_atmo_option_t *aopt = &_atmo_option;
  cs_fluid_properties_t *phys_pro = cs_get_glob_fluid_properties();

  /* potential temp at ref */
  cs_real_t ps = cs_glob_atmo_constants->ps;
  cs_real_t rair = phys_pro->r_pg_cnst;
  cs_real_t cp0 = phys_pro->cp0;
  cs_real_t rscp = rair/cp0;
  cs_real_t g = cs_math_3_norm(cs_glob_physical_constants->gravity);
  if (g <= 0.)
    bft_error(__FILE__, __LINE__, 0,
              _("Atmo meteo profiles: gravity must not be 0."));

  /* Reference fluid properties set from meteo values */
  phys_pro->p0 = aopt->meteo_psea;
  phys_pro->t0 = aopt->meteo_t0; /* ref temp T0 */

  /* Compute reference q_l, theta_liq and rho */
  cs_real_t t_c = aopt->meteo_t0 - cs_physical_constants_celsius_to_kelvin;
  cs_real_t q_sat = cs_air_yw_sat(t_c, aopt->meteo_psea);
  aopt->meteo_ql0 = cs::max(aopt->meteo_qw0 - q_sat, 0.);
  cs_real_t rvsra = phys_pro->rvsra;
  cs_real_t rhum = rair*(1. + (rvsra - 1.)*(aopt->meteo_qw0 - aopt->meteo_ql0)
                        - aopt->meteo_ql0);
  phys_pro->ro0 = phys_pro->p0/(rhum * aopt->meteo_t0); /* ref density T0 */
  cs_real_t clatev = phys_pro->clatev;
  cs_real_t theta0 = (aopt->meteo_t0 - clatev/cp0 * aopt->meteo_ql0)
                   * pow(ps/ aopt->meteo_psea, rscp);

  cs_real_t z0 = aopt->meteo_z0;
  cs_real_t zref = aopt->meteo_zref;
  bool is_humid = (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == 2);

  cs_real_t prt = 1.;
  if (CS_F_(t) != nullptr)
    prt = cs_field_get_key_double(CS_F_(t),
                                  cs_field_key_id("turbulent_schmidt"));

  cs_real_t scht = 1.;
  cs_field_t *f_qw = cs_field_by_name_try("ym_water");
  if (f_qw != nullptr)
    scht = cs_field_get_key_double(f_qw,
                                   cs_field_key_id("turbulent_schmidt"));

  if (!is_humid)
    aopt->meteo_qwstar = 0.;

  if (aopt->meteo_ustar0 <= 0. && aopt->meteo_uref <= 0. && aopt->meteo_u1 <= 0.
      && aopt->meteo_u2 <= 0.)
    bft_error(__FILE__,
              __LINE__,
              0,
              _("Meteo preprocessor: meteo_ustar0 or meteo_uref"
                " or velocity measurements."));

  if (aopt->meteo_ustar0 > 0. && aopt->meteo_uref > 0.) {
  /* Recompute LMO inverse */

    /* U+ */
    cs_real_t up = aopt->meteo_uref / aopt->meteo_ustar0;

    /* U+ if neutral, dlmo = 0 */
    cs_real_t up_l = cs_mo_psim(zref + z0,
                                z0,
                                0.) / kappa;

    cs_real_t dlmo = 0.;
    cs_real_t error = up_l - up;

    /* Dichotomy */
    cs_real_t dl_min = -1.e6;
    cs_real_t dl_max =  1.e6;
    cs_real_t tol = 1e-6;
    int it;
    int it_max = 1000;
    for (it = 0;
         it < it_max && cs::abs(error) > tol && 0.5*(dl_max - dl_min) > tol;
         it++) {
      cs_real_t dl_mid = 0.5 * (dl_min + dl_max);

      cs_real_t error_min = cs_mo_psim(zref + z0,
                                       z0,
                                       dl_min) / kappa - up;
      cs_real_t error_mid = cs_mo_psim(zref + z0,
                                       z0,
                                       dl_mid) / kappa - up;

      /* The solution is between min and mid */
      if (error_min * error_mid < 0) {
        dl_max = dl_mid;
        if (cs::abs(error_min) < cs::abs(error_mid)) {
          dlmo = dl_min;
          error = error_min;
        }
        else {
          dlmo = dl_mid;
          error = error_mid;
        }
      }
      /* The solution is between mid and max */
      else {
        cs_real_t error_max = cs_mo_psim(zref + z0,
                                         z0,
                                         dl_max) / kappa - up;
        dl_min = dl_mid;
        if (cs::abs(error_mid) < cs::abs(error_max)) {
          dlmo = dl_mid;
          error = error_mid;
        }
        else {
          dlmo = dl_max;
          error = error_max;
        }
      }
#if 0
      bft_printf("IT %d: dlmo = %f, error = %f\n", it, dlmo, error);
#endif
    }

    if (it == it_max) {
      cs_base_warn(__FILE__, __LINE__);
      bft_printf
        (_("Meteo preprocessor did not converge to find inverse\n"
           "of LMO length, current value is %f.\n"
           "Please, check reference velocity, reference altitude and ustar\n"),
         dlmo);
    }

    aopt->meteo_dlmo = dlmo;
  }

  /* Compute ground friction velocity from dlmo and uref */
  if (aopt->meteo_ustar0 < 0.)
    aopt->meteo_ustar0 =
      aopt->meteo_uref * kappa
      / cs_mo_psim(zref + z0,
                   z0,
                   aopt->meteo_dlmo);

  /* Compute uref from ground friction velocity and dlmo */
  if (aopt->meteo_uref < 0. && zref > 0.)
    aopt->meteo_uref =   aopt->meteo_ustar0 / kappa
                       * cs_mo_psim(zref + z0, z0, aopt->meteo_dlmo);

  /* LMO inverse, ustar at ground */
  cs_real_t dlmo = aopt->meteo_dlmo;
  cs_real_t ustar0 = aopt->meteo_ustar0;

  /* Compute qwstar from evaporation */
  if ((aopt->meteo_qwstar > 0.5*DBL_MAX) && (aopt->meteo_evapor < 0.5*DBL_MAX))
    aopt->meteo_qwstar = aopt->meteo_evapor / (ustar0 * phys_pro->ro0);

  /* Deduce evaporation */
  if (aopt->meteo_evapor > 0.5*DBL_MAX)
    aopt->meteo_evapor = aopt->meteo_qwstar * (ustar0 * phys_pro->ro0);

  /* Note: rvsra - 1 = 0.61 */
  aopt->meteo_tstar = cs_math_pow2(ustar0) * theta0 * dlmo / (kappa * g)
                      - (phys_pro->rvsra - 1.) * theta0 * aopt->meteo_qwstar;
  if ((aopt->meteo_zu1 < 0. && aopt->meteo_u1 > 0.)
      || (aopt->meteo_zu2 < 0. && aopt->meteo_u2 > 0.))
    bft_error(__FILE__,
              __LINE__,
              0,
              _("When computing idealised atmospheric profiles,\n"
                "No heights are provided for velocities u1 and u2."));
  if ((aopt->meteo_zu1 > 0. && aopt->meteo_t1 < 0.)
      || (aopt->meteo_zu2 > 0. && aopt->meteo_t2 < 0.))
    bft_error(__FILE__,
              __LINE__,
              0,
              _("When computing idealised atmospheric profiles,\n"
                "No temperature measurements are provided."));
  if ((aopt->meteo_zu1 > 0. && aopt->meteo_qw1 > 0.5*DBL_MAX && is_humid)
      || (aopt->meteo_zu2 > 0. && aopt->meteo_qw2 > 0.5*DBL_MAX && is_humid))
    bft_error(__FILE__,
              __LINE__,
              0,
              _("When computing idealised atmospheric profiles,\n"
                "No humidity measurements are provided, though humid "
                "atmosphere is selected."));
  cs_real_t u1  = aopt->meteo_u1;
  cs_real_t u2  = aopt->meteo_u2;
  cs_real_t z1  = aopt->meteo_zu1;
  cs_real_t z2  = aopt->meteo_zu2;
  cs_real_t t1  = aopt->meteo_t1;
  cs_real_t t2  = aopt->meteo_t2;
  cs_real_t qw1 = aopt->meteo_qw1;
  cs_real_t qw2 = aopt->meteo_qw2;
  if ((u1 > 0.) || (u2 > 0.)) {
    cs_base_warn(__FILE__, __LINE__);
    bft_printf(_("Meteo preprocessor uses u1=%17.9e, u2=%17.9e\n"
                 "collected at z1=%17.9e [m], z2=%17.9e [m]\n"),
               u1,
               u2,
               z1,
               z2);

    cs_real_t du1u2 = u2 - u1;
    cs_real_t dt1t2 = t2 - t1; /* Should be negative for unstable case */
    cs_real_t tmoy  = (t1 + t2) * 0.5; /* Proxy for Lmo */

    cs_real_t dqw1qw2 = qw2 - qw1; /* is zero by default */

    cs_real_t tol = 1e-12;
    int it;
    int it_max = 1000;

    /* Neutral Assumption */
    cs_real_t dlmos = 0.; /* dlmos = dlmo starts */
    cs_real_t dlmou = 0.; /* dlmou = dlmo updated */
    cs_real_t err   = 1.; /* in order to enter the loop */
    for (it = 0; it < it_max && cs::abs(err) > tol; it++) {
      if (it != 0) {
        dlmos = dlmou;
      }
      cs_real_t ustaru  = kappa * du1u2 / (cs_mo_psim(z2, z1, dlmos));
      cs_real_t tstaru  = kappa * dt1t2 / (cs_mo_psih(z2, z1, dlmos, prt));
      cs_real_t qwstaru = kappa * dqw1qw2 / (cs_mo_psih(z2, z1, dlmos, scht));
      /* Note: rvsra - 1 = 0.61 */
      dlmou = kappa * (g / tmoy) * (tstaru
                                   + (phys_pro->rvsra - 1.) * tmoy * qwstaru)
              / (ustaru * ustaru);
      err = dlmou - dlmos;
    }
    if (it == it_max) {
      cs_base_warn(__FILE__, __LINE__);
      bft_printf(_("Meteo preprocessor did not converge to find inverse\n"
                   "of LMO length, current value is %f.\n"),
                 dlmou);
    }

    /* Update ustar and tstar */
    aopt->meteo_dlmo   = dlmou;
    aopt->meteo_ustar0 = kappa * du1u2 / (cs_mo_psim(z2, z1, dlmou));
    aopt->meteo_tstar  = kappa * dt1t2 / (cs_mo_psih(z2, z1, dlmou, prt));
    aopt->meteo_qwstar = kappa * dqw1qw2 / (cs_mo_psih(z2, z1, dlmou, scht));
    aopt->meteo_evapor = aopt->meteo_qwstar * (ustar0 * phys_pro->ro0);
    //FIXME conversion theta->T
    aopt->meteo_t0     = t1
      - aopt->meteo_tstar * cs_mo_psih(z1 + z0, z0, dlmo, prt) / kappa;
    aopt->meteo_qw0    = qw1
      - aopt->meteo_qwstar * cs_mo_psih(z1 + z0, z0, dlmo, scht) / kappa;
    aopt->meteo_z0     = z1 * exp(-cs_turb_xkappa * u1
        / aopt->meteo_ustar0);
  }

  /* Center of the domain */
  /* if neither latitude/longitude nor lambert coordinates are given */
  if ((  (   aopt->latitude > 0.5 * cs_math_big_r
          || aopt->longitude > 0.5 * cs_math_big_r)
      && (   aopt->x_l93 > 0.5 * cs_math_big_r
          || aopt->y_l93 > 0.5 * cs_math_big_r))) {
    bft_printf("Neither latitude nor center in Lambert-93 was given \n");
    bft_printf("It is set by default to 45°N 3°E\n");
    aopt->latitude = 45.;
    aopt->longitude = 3.;
    _convert_from_wgs84_to_l93();
  }
  /* else if latitude/longitude are given */
  else if (   aopt->x_l93 > 0.5 * cs_math_big_r
           || aopt->y_l93 > 0.5 * cs_math_big_r) {
    bft_printf("Latitude and longitude were given, Lambert center's coordinates"
               " are automatically computed\n");
    _convert_from_wgs84_to_l93();
  }
  /* All other cases */
  else{
    bft_printf("Lambert coordinates were given, latitude"
               " and longitude are automatically computed\n");
    _convert_from_l93_to_wgs84();
  }

  /* BL height according to Marht 1982 formula */
  /* value of C=0.2, 0.185, 0.06, 0.14, 0.07, 0.04 */
  cs_real_t zi_coef = 0.2;
  cs_real_t corio_f = 4. * cs_math_pi / 86164.
                         * sin(aopt->latitude * cs_math_pi / 180.);
  aopt->meteo_zi = zi_coef * ustar0 / fabs(corio_f);

  /* Force the computation of z_ground */
  aopt->compute_z_ground = true;

  if (is_humid && aopt->meteo_qw0 < 0. && aopt->meteo_qw1 < 0
      && aopt->meteo_qw2 < 0)
    bft_error(__FILE__,
              __LINE__,
              0,
              _("Meteo preprocessor: at least one information is required\n"
                "for humidity field preprocessing (qw0 || qw1 || qw2)."));

  bft_printf("\n Meteo preprocessing values for computation:\n"
             " dlmo=%17.9e\n z0=%17.9e\n ustar=%17.9e\n tstar=%17.9e\n"
             " qwstar=%17.9e\n t0=%17.9e\n qw0=%17.9e\n ql0=%17.9e\n"
             " zi=%17.9e\n"
             " longitude=%17.9e\n latitude=%17.9e\n"
             " x_l93=%17.9e\n y_l93=%17.9e\n"
             " domain_orientation=%17.9e\n",
             aopt->meteo_dlmo,
             aopt->meteo_z0,
             aopt->meteo_ustar0,
             aopt->meteo_tstar,
             aopt->meteo_qwstar,
             aopt->meteo_t0,
             aopt->meteo_qw0,
             aopt->meteo_ql0,
             aopt->meteo_zi,
             aopt->longitude,
             aopt->latitude,
             aopt->x_l93,
             aopt->y_l93,
             aopt->domain_orientation);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute meteo profiles if no meteo file is given.
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_compute_meteo_profiles(void)
{
  cs_domain_t *domain = cs_glob_domain;
  cs_mesh_t *m = domain->mesh;
  cs_mesh_quantities_t *mq = domain->mesh_quantities;

  const int *restrict c_disable_flag = (mq->has_disable_flag) ?
    mq->c_disable_flag : nullptr;

  const cs_real_3_t *restrict cell_cen = mq->cell_cen;

  /* In the log */
  bft_printf(" Computing meteo profiles from large scale meteo data\n\n");

  /* Get fields */
  cs_real_t *cpro_met_potemp = cs_field_by_name("meteo_pot_temperature")->val;
  cs_real_3_t *cpro_met_vel
    = (cs_real_3_t *) (cs_field_by_name("meteo_velocity")->val);
  cs_real_t *cpro_met_k = cs_field_by_name("meteo_tke")->val;
  cs_real_t *cpro_met_eps = cs_field_by_name("meteo_eps")->val;
  cs_field_t *f_met_qw = cs_field_by_name_try("meteo_humidity");

  /* Some turbulence constants */
  cs_real_t kappa = cs_turb_xkappa;
  cs_real_t cmu = cs_turb_cmu;

  cs_atmo_option_t *aopt = &_atmo_option;
  cs_real_t z0 = aopt->meteo_z0;

  const cs_fluid_properties_t *phys_pro = cs_get_glob_fluid_properties();
  /* potential temp at ref */
  cs_real_t ps = cs_glob_atmo_constants->ps;
  cs_real_t rair = phys_pro->r_pg_cnst;
  cs_real_t cp0 = phys_pro->cp0;
  cs_real_t rscp = rair/cp0;
  cs_real_t clatev = phys_pro->clatev;
  cs_real_t theta0 = (aopt->meteo_t0 - clatev/cp0 * aopt->meteo_ql0)
                   * pow(ps/ aopt->meteo_psea, rscp);

  /* LMO inverse, ustar at ground */
  cs_real_t dlmo = aopt->meteo_dlmo;
  cs_real_t ustar0 = aopt->meteo_ustar0;
  cs_real_t angle = (aopt->meteo_angle - aopt->domain_orientation);

  /* Friction temperature */
  cs_real_t tstar = aopt->meteo_tstar;
  cs_real_t prt = 1.;
  if (CS_F_(t) != nullptr)
    prt = cs_field_get_key_double(CS_F_(t),
                                  cs_field_key_id("turbulent_schmidt"));

  /* Humidity field */

  cs_real_t qw0    = aopt->meteo_qw0;
  cs_real_t qwstar = aopt->meteo_qwstar;

  /* Variables used for clipping */
  cs_real_t ri_max = cs_math_big_r;
  cs_real_t z_lim = cs_math_big_r;
  cs_real_t u_met_min= cs_math_big_r;
  cs_real_t theta_met_min= cs_math_big_r;

  cs_real_t *z_ground = nullptr;
  if (aopt->compute_z_ground == true) {

    cs_field_t *f_z_ground = cs_field_by_name("z_ground");

    /* Do not recompute in case of restart */
    int has_restart = cs_restart_present();
    if (has_restart == 1) {
      cs_restart_t *rp = cs_restart_create("main.csc",
                                           nullptr,
                                           CS_RESTART_MODE_READ);

      int retval = cs_restart_read_field_vals(rp,
                                              f_z_ground->id,
                                              0);    /* current value */
      if (retval != CS_RESTART_SUCCESS)
        has_restart = 0;
    }

    /* z_ground needs to be computed? */
    if (has_restart == 0)
      cs_atmo_z_ground_compute();

    z_ground = f_z_ground->val;
  }

  cs_real_t *dlmo_var = nullptr;
  CS_MALLOC(dlmo_var, m->n_cells_with_ghosts, cs_real_t);

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells_with_ghosts; cell_id++) {
    dlmo_var[cell_id] = 0.0;
  }

  /* For DRSM models store Rxz/k */
  cs_real_6_t *cpro_met_rij = nullptr;
  cs_field_t *f_met_rij = cs_field_by_name_try("meteo_rij");
  if (f_met_rij != nullptr)
    cpro_met_rij = (cs_real_6_t *) (f_met_rij->val);

  if (dlmo > 0)
    ri_max = 0.75; // Value chosen to limit buoyancy vs shear production

  /* Profiles */
  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {

    if (c_disable_flag != nullptr)
      if (c_disable_flag[cell_id] == 1)
        continue;

    cs_real_t z_grd = 0.;
    if (z_ground != nullptr)
      z_grd = z_ground[cell_id];

    /* Local elevation */
    cs_real_t z = cs::max(cell_cen[cell_id][2] - z_grd, 0.);

    /* Velocity profile */
    cs_real_t u_norm = ustar0 / kappa * cs_mo_psim(z+z0, z0, dlmo);

    cpro_met_vel[cell_id][0] = - sin(angle * cs_math_pi/180.) * u_norm;
    cpro_met_vel[cell_id][1] = - cos(angle * cs_math_pi/180.) * u_norm;

    /* Potential temperature profile
     * Note: same roughness as dynamics */
    cpro_met_potemp[cell_id] = theta0
                             + tstar / kappa * cs_mo_psih(z+z0, z0, dlmo,prt);

    /* Richardson flux number profile */
    // Note : ri_f = z/(Pr_t L) * phih/phim^2 = z/Lmo / phim
    cs_real_t ri_f = (z+z0) * dlmo / cs_mo_phim(z+z0, dlmo);

    /* TKE profile */
    cpro_met_k[cell_id] = cs_math_pow2(ustar0) / sqrt(cmu)
      * sqrt(1. - cs::min(ri_f, 1.));

    if (f_met_rij != nullptr) {
      cs_real_t vel_dir[3];
      cs_math_3_normalize(cpro_met_vel[cell_id], vel_dir);

      cs_real_t axz = -sqrt(cmu / (1. - cs::min(ri_f, ri_max)));
      cs_real_t k_in = cpro_met_k[cell_id];
      cpro_met_rij[cell_id][0] = 2. / 3. * k_in;
      cpro_met_rij[cell_id][1] = 2. / 3. * k_in;
      cpro_met_rij[cell_id][2] = 2. / 3. * k_in;
      cpro_met_rij[cell_id][3] = 0.; // Rxy
      cpro_met_rij[cell_id][4] = axz * k_in * vel_dir[1]; // Ryz
      cpro_met_rij[cell_id][5] = axz * k_in * vel_dir[0]; // Rxz
    }

    /* epsilon profile */
    cpro_met_eps[cell_id] = cs_math_pow3(ustar0) / (kappa * (z+z0))
       * cs_mo_phim(z+z0, dlmo)*(1.-cs::min(ri_f, 1.));

    /* Very stable cases */
    if (ri_f > ri_max) {
      if (z < z_lim) {
        //Ri_f is an increasing monotonic function, so the lowest value of
        //z for which Ri_f>Ri_max is needed
        z_lim = z;
        u_met_min=u_norm;
        theta_met_min=cpro_met_potemp[cell_id];
      }
    }
  }

  cs_parall_min(1, CS_REAL_TYPE, &z_lim);
  cs_parall_min(1, CS_REAL_TYPE, &u_met_min);
  cs_parall_min(1, CS_REAL_TYPE, &theta_met_min);
  if (f_met_qw != nullptr) {
    cs_real_t scht = 1.;
    cs_field_t *f_qw = cs_field_by_name_try("ym_water");
    if (f_qw != nullptr)
      scht = cs_field_get_key_double(f_qw,
                                     cs_field_key_id("turbulent_schmidt"));


    for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {
      cs_real_t z_grd = 0.;
      if (z_ground != nullptr)
        z_grd = z_ground[cell_id];
      cs_real_t z = cell_cen[cell_id][2] - z_grd;
      f_met_qw->val[cell_id]
        = qw0 + qwstar / kappa * cs_mo_psih(z + z0, z0, dlmo, scht);
    }
  }

  /* Very stable cases, corresponding to mode 0 in the Python prepro */
  if (z_lim < 0.5*cs_math_big_r) {
    // Clipping only if there are cells to be clipped
    bft_printf("Switching to very stable clipping for meteo profile.\n");
    bft_printf("All altitudes above %f have been modified by clipping.\n",
               z_lim);
    for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {

      cs_real_t z_grd = 0.;
      if (z_ground != nullptr)
        z_grd = z_ground[cell_id];

      cs_real_t z = cell_cen[cell_id][2] - z_grd;
      if (z >= z_lim) {
         /* mode = 0 is ustar=cst */
        dlmo_var[cell_id] = dlmo * (z_lim + z0) / (z + z0);

        /* Velocity profile */
        cs_real_t u_norm =   u_met_min + ustar0 / kappa
                           * cs_mo_phim(z_lim + z0, dlmo)
                           * log((z+z0) / (z_lim+z0));

        cpro_met_vel[cell_id][0] = - sin(angle * cs_math_pi/180.) * u_norm;
        cpro_met_vel[cell_id][1] = - cos(angle * cs_math_pi/180.) * u_norm;

       /* Potential temperature profile
        * Note: same roughness as dynamics */
        cpro_met_potemp[cell_id] =   theta_met_min
                                   + tstar * (z_lim+z0) / kappa
                                     * cs_mo_phih(z_lim+z0, dlmo, prt)
                                     * (-1./(z+z0) + 1./(z_lim+z0));
       /* TKE profile
          ri_max is necessarily lower than 1, but cs::min might be useful if
          that changes in the future */
        cpro_met_k[cell_id] = cs_math_pow2(ustar0) / sqrt(cmu)
          * sqrt(1. - cs::min(ri_max, 1.));

        if (f_met_rij != nullptr) {
          cs_real_t vel_dir[3];
          cs_math_3_normalize(cpro_met_vel[cell_id], vel_dir);

          cs_real_t axz = -sqrt(cmu / (1. - cs::min(ri_max, 1.)));
          cs_real_t k_in = cpro_met_k[cell_id];
          cpro_met_rij[cell_id][0] = 2. / 3. * k_in;
          cpro_met_rij[cell_id][1] = 2. / 3. * k_in;
          cpro_met_rij[cell_id][2] = 2. / 3. * k_in;
          cpro_met_rij[cell_id][3] = 0.; // Rxy
          cpro_met_rij[cell_id][4] = axz * k_in * vel_dir[1]; // Ryz
          cpro_met_rij[cell_id][5] = axz * k_in * vel_dir[0]; // Rxz
        }

        /* epsilon profile */
        cpro_met_eps[cell_id]
          =   cs_math_pow3(ustar0) / kappa  * dlmo_var[cell_id]
            * (1- cs::min(ri_max, 1.)) / cs::min(ri_max, 1.);
      }
    }
  }

  cs_atmo_hydrostatic_profiles_compute();
  CS_FREE(dlmo_var);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the ground elevation.
 *
 *  This function solves the following transport equation on \f$ \varia \f$:
 *  \f[
 *  \dfrac{\partial \varia}{\partial t} + \divs \left( \varia \vect{g} \right)
 *      - \divs \left( \vect{V} \right) \varia = 0
 *  \f]
 *  where \f$ \vect{g} \f$ is the gravity field
 *
 *  The boundary conditions on \f$ \varia \f$ read:
 *  \f[
 *   \varia = z \textrm{ on walls}
 *  \f]
 *  \f[
 *   \dfrac{\partial \varia}{\partial n} = 0 \textrm{ elsewhere}
 *  \f]
 *
 *  Remarks:
 *  - a steady state is looked for.
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_z_ground_compute(void)
{
  /* Initialization
   * ============== */

  if (!_atmo_option.compute_z_ground)
    return;

  const cs_domain_t *domain = cs_glob_domain;
  const cs_mesh_t *m = domain->mesh;
  const cs_mesh_quantities_t *mq = domain->mesh_quantities;
  const cs_mesh_quantities_t *mq_g = cs_glob_mesh_quantities_g;
  const cs_lnum_t n_cells = m->n_cells;

  const cs_nreal_3_t *restrict i_face_u_normal = mq->i_face_u_normal;
  const cs_nreal_3_t *restrict b_face_u_normal = mq->b_face_u_normal;
  const cs_real_3_t *restrict b_face_cog = mq->b_face_cog;

  const int *bc_type = cs_glob_bc_type;

  const cs_real_t *restrict i_face_surf = mq->i_face_surf;
  const cs_real_3_t *restrict i_face_cog = mq_g->i_face_cog;
  const cs_real_t *restrict i_face_surf_g = mq_g->i_face_surf;
  const cs_real_t *restrict b_face_surf = mq->b_face_surf;
  const cs_mesh_adjacencies_t *ma = cs_glob_mesh_adjacencies;
  const cs_lnum_t *c2c = ma->cell_cells;
  const cs_lnum_t *c2c_idx = ma->cell_cells_idx;
  const cs_lnum_t *cell_i_faces = ma->cell_i_faces;
  const short int *cell_i_faces_sgn = ma->cell_i_faces_sgn;

  /* Pointer to z_ground field */
  cs_field_t *f = cs_field_by_name_try("z_ground");

  cs_real_t *restrict i_massflux
    = cs_field_by_id
        (cs_field_get_key_int(f, cs_field_key_id("inner_mass_flux_id")))->val;
  cs_real_t *restrict b_massflux
    = cs_field_by_id
        (cs_field_get_key_int(f, cs_field_key_id("boundary_mass_flux_id")))->val;

  cs_equation_param_t *eqp_p = cs_field_get_equation_param(f);

  cs_real_t normal[3];
  /* Normal direction is given by the gravity */
  cs_math_3_normalize((const cs_real_t *)(cs_glob_physical_constants->gravity),
                      normal);

  for (int i = 0; i < 3; i++)
    normal[i] *= -1.;

  /* Compute the mass flux due to V = - g / ||g||
   * ============================================ */

  for (cs_lnum_t face_id = 0; face_id < m->n_i_faces; face_id++) {
    i_massflux[face_id] =   cs_math_3_dot_product(normal,
                                                  i_face_u_normal[face_id])
                          * i_face_surf[face_id];
  }

  for (cs_lnum_t face_id = 0; face_id < m->n_b_faces; face_id++) {
    b_massflux[face_id] =   cs_math_3_dot_product(normal,
                                                  b_face_u_normal[face_id])
                          * b_face_surf[face_id];
  }

  /* Boundary conditions
   * =================== */

  cs_real_t *coefa = f->bc_coeffs->a;
  cs_real_t *coefb = f->bc_coeffs->b;
  cs_real_t *cofaf = f->bc_coeffs->af;
  cs_real_t *cofbf = f->bc_coeffs->bf;

  cs_real_t norm = 0.;
  cs_real_t ground_surf = 0.;

  /* Dirichlet at walls, homogeneous Neumann elsewhere */

  for (cs_lnum_t face_id = 0; face_id < m->n_b_faces; face_id++) {
    /* Dirichlet BCs */
    if ((bc_type[face_id] == CS_SMOOTHWALL || bc_type[face_id] == CS_ROUGHWALL)
        && b_massflux[face_id] <= 0.) {

      eqp_p->ndircl = 1;
      cs_real_t hint = 1. / mq->b_dist[face_id];
      cs_real_t pimp = cs_math_3_dot_product(b_face_cog[face_id], normal);

      cs_boundary_conditions_set_dirichlet_scalar(coefa[face_id],
                                                  cofaf[face_id],
                                                  coefb[face_id],
                                                  cofbf[face_id],
                                                  pimp,
                                                  hint,
                                                  cs_math_infinite_r);
      norm += cs_math_pow2(f->bc_coeffs->a[face_id]) * mq->b_face_surf[face_id];
      ground_surf += mq->b_face_surf[face_id];
    }
    /* Neumann Boundary Conditions */
    else {

      cs_real_t hint = 1. / mq->b_dist[face_id];
      cs_real_t qimp = 0.;

      cs_boundary_conditions_set_neumann_scalar(coefa[face_id],
                                                cofaf[face_id],
                                                coefb[face_id],
                                                cofbf[face_id],
                                                qimp,
                                                hint);

    }
  }

  /* Matrix
   * ====== */

  cs_real_t *rovsdt = nullptr, *dpvar = nullptr;
  CS_MALLOC_HD(rovsdt, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);
  CS_MALLOC_HD(dpvar, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells_with_ghosts; cell_id++) {
    rovsdt[cell_id] = 0.;
    dpvar[cell_id] = 0.;
  }

  /* Right hand side
   * =============== */

  cs_real_t *rhs = nullptr;
  CS_MALLOC_HD(rhs, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells_with_ghosts; cell_id++)
    rhs[cell_id] = 0.;

  if (cs_glob_porosity_from_scan_opt->use_staircase) {
    for (cs_lnum_t c_id = 0; c_id < n_cells; c_id++) {
      if (mq->c_disable_flag[c_id] != 0)
        continue;

      /* Interior faces */
      const cs_lnum_t s_id_i = c2c_idx[c_id];
      const cs_lnum_t e_id_i = c2c_idx[c_id+1];

      /* Loop on interior faces of cell c_id */
      for (cs_lnum_t cidx = s_id_i; cidx < e_id_i; cidx++) {
        const cs_lnum_t face_id = cell_i_faces[cidx];
        const short int sign = cell_i_faces_sgn[cidx];

        const cs_lnum_t c_id_adj = c2c[cidx]; /*adjacent cell address*/

        if (mq->c_disable_flag[c_id_adj] != 1)
          continue;

        /*cs_real_t i_wall_dist
          = - sign * cs_math_3_distance_dot_product(cell_cen[c_id_adj],
                                                    i_face_cog[face_id],
                                                    i_face_u_normal[face_id]);*/

        eqp_p->ndircl = 1;
        cs_real_t pimp = cs_math_3_dot_product(i_face_cog[face_id], normal);

        cs_real_t dot_ng
          = cs_math_3_dot_product(i_face_u_normal[face_id], normal);
        cs_real_t tsimp =   cs::max(- sign * dot_ng, 0.)
                          * i_face_surf_g[face_id];

        rhs[c_id] += tsimp * (pimp - f->val[c_id]); //explicit term
        rovsdt[c_id] += cs::max(tsimp, 0.); //implicit term

        norm += cs_math_pow2(pimp) * i_face_surf[face_id];
        ground_surf += i_face_surf[face_id];
      } /* loop on interior faces */
    } /* loop on cells */
  }

  cs_parall_max(1, CS_INT_TYPE, &(eqp_p->ndircl));

  /* Norm
   * ==== */

  cs_parall_sum(1, CS_REAL_TYPE, &norm);
  cs_parall_sum(1, CS_REAL_TYPE, &ground_surf);

  if (ground_surf > 0.)
    norm = sqrt(norm / ground_surf) * mq->tot_vol;
  else {
    bft_printf("No ground BC or no gravity:"
               " no computation of ground elevation.\n");
    return;
  }

  /* Solving
   * ======= */

  /* In case of a theta-scheme, set theta = 1;
     no relaxation in steady case either */
  cs_real_t inf_norm = 1.;

  /* Overall loop in order to ensure convergence */
  for (int sweep = 0; sweep < eqp_p->nswrsm && inf_norm > eqp_p->epsrsm;
       sweep++) {

    cs_equation_iterative_solve_scalar(0,   /* idtvar: no steady state algo */
                                       -1,  /* no over loops */
                                       f->id,
                                       nullptr,
                                       0,   /* iescap */
                                       0,   /* imucpp */
                                       norm,
                                       eqp_p,
                                       f->val_pre,
                                       f->val,
                                       f->bc_coeffs,
                                       i_massflux,
                                       b_massflux,
                                       i_massflux, /* viscosity, not used */
                                       b_massflux, /* viscosity, not used */
                                       i_massflux, /* viscosity, not used */
                                       b_massflux, /* viscosity, not used */
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       0, /* icvflb (upwind) */
                                       nullptr,
                                       rovsdt,
                                       rhs,
                                       f->val,
                                       dpvar,
                                       nullptr,
                                       nullptr);

    /* Compute the L_infinity norm */
    inf_norm = 0.;
    for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++) {
      //FIXME make this dimensionless
      inf_norm = fmax(inf_norm, fabs(f->val[cell_id]-f->val_pre[cell_id]));

      /* Current to previous */
      f->val_pre[cell_id] = f->val[cell_id];
    }

    cs_parall_max(1, CS_REAL_TYPE, &inf_norm);
  }

  /* Free memory */
  CS_FREE_HD(rovsdt);

  CS_FREE_HD(rhs);
  CS_FREE_HD(dpvar);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute hydrostatic profiles of density and pressure.
 *
 *  This function solves the following transport equation on \f$ \varia \f$:
 *  \f[
 *  \divs \left( \grad \varia \right)
 *      = \divs \left( \dfrac{\vect{g}}{c_p \theta} \right)
 *  \f]
 *  where \f$ \vect{g} \f$ is the gravity field and \f$ \theta \f$
 *  is the potential temperature.
 *
 *  The boundary conditions on \f$ \varia \f$ read:
 *  \f[
 *   \varia = \left(\dfrac{P_{sea}}{p_s}\right)^{R/C_p} \textrm{on the ground}
 *  \f]
 *  and Neumann elsewhere.
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_hydrostatic_profiles_compute(void)
{
  /* Initialization
   * ============== */

  cs_mesh_t *m = cs_glob_mesh;
  cs_mesh_quantities_t *mq = cs_glob_mesh_quantities;

  const cs_real_3_t *restrict b_face_cog = mq->b_face_cog;
  const cs_real_3_t *restrict cell_cen = mq->cell_cen;

  cs_physical_constants_t *phys_cst = cs_get_glob_physical_constants();
  cs_field_t *f = cs_field_by_name("meteo_pressure");
  cs_field_t *potemp = cs_field_by_name("meteo_pot_temperature");
  cs_field_t *density = cs_field_by_name("meteo_density");
  cs_field_t *temp = cs_field_by_name("meteo_temperature");
  cs_equation_param_t *eqp_p = cs_field_get_equation_param(f);
  cs_atmo_option_t *aopt = &_atmo_option;
  cs_real_t g = cs_math_3_norm(phys_cst->gravity);

  const cs_fluid_properties_t *phys_pro = cs_get_glob_fluid_properties();
  /* potential temp at ref */
  cs_real_t ps = cs_glob_atmo_constants->ps;
  cs_real_t rair = phys_pro->r_pg_cnst;
  cs_real_t cp0 = phys_pro->cp0;
  cs_real_t rscp = rair/cp0; /* Around 2/7 */

  const cs_velocity_pressure_model_t  *vp_model
    = cs_glob_velocity_pressure_model;
  const int idilat = vp_model->idilat;

  /* Get the lowest altitude (should also be minimum of z_ground)
   * ============================================================ */

  cs_real_t z_min = cs_math_big_r;

  for (cs_lnum_t face_id = 0; face_id < m->n_b_faces; face_id ++) {
    if (b_face_cog[face_id][2] < z_min)
      z_min = b_face_cog[face_id][2];
  }

  cs_parall_min(1, CS_REAL_TYPE, &z_min);

  /* p_ground is pressure at the lowest level */

  cs_real_t p_ground = aopt->meteo_psea;

  /* Check if restart is read and if meteo_pressure is present */
  int has_restart = cs_restart_present();
  if (has_restart == 1) {
    cs_restart_t *rp = cs_restart_create("main.csc",
                                         nullptr,
                                         CS_RESTART_MODE_READ);

    int retval = cs_restart_read_field_vals(rp,
                                            f->id, /* meteo_pressure */
                                            0);    /* current value */
    if (retval != CS_RESTART_SUCCESS)
      has_restart = 0;

  }

  /* Initialize temperature, pressure and density from neutral conditions
   *
   * p(z) = p0 (1 - g z / (Cp T0))^(Cp/R)
   * Note: Cp/R = gamma/(gamma-1)
   *=====================================================================*/

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {
    cs_real_t z  = cell_cen[cell_id][2] - z_min;
    cs_real_t zt = fmin(z, 11000.);
    cs_real_t factor = fmax(1. - g * zt / (cp0 * aopt->meteo_t0), 0.);
    temp->val[cell_id] = aopt->meteo_t0 * factor;

    /* Do not overwrite pressure in case of restart */
    if (has_restart == 0)
      f->val[cell_id] =   p_ground * pow(factor, 1./rscp)
                          /* correction factor for z > 11000m */
                        * exp(- g/(rair*temp->val[cell_id]) * (z - zt));

    if (idilat > 0)
      temp->val[cell_id] =   potemp->val[cell_id]
                           * pow((f->val[cell_id]/ps), rscp);
    if (cs_glob_physical_model_flag[CS_ATMOSPHERIC]
        != CS_ATMO_CONSTANT_DENSITY)
      density->val[cell_id] = f->val[cell_id] / (rair * temp->val[cell_id]);
    else
      density->val[cell_id] = phys_pro->ro0;
  }

  if (has_restart == 1 || cs_glob_physical_model_flag[CS_ATMOSPHERIC]
        == CS_ATMO_CONSTANT_DENSITY)
    return;

  /* Boussinesq hypothesis */
  if (idilat == 0) {
    bft_printf
      ("Meteo profiles are computed according to Boussinesq approximation.\n"
       "Using adiabatic profiles for temperature and pressure."
       "Density is computed accordingly.\n");
  }

  cs_real_t *i_massflux = nullptr;
  cs_real_t *b_massflux = nullptr;
  CS_MALLOC_HD(i_massflux, m->n_i_faces, cs_real_t, cs_alloc_mode);
  CS_MALLOC_HD(b_massflux, m->n_b_faces, cs_real_t, cs_alloc_mode);

  for (cs_lnum_t face_id = 0; face_id < m->n_i_faces; face_id++)
    i_massflux[face_id] = 0.;

  for (cs_lnum_t face_id = 0; face_id < m->n_b_faces; face_id++)
    b_massflux[face_id] = 0.;

  cs_real_t *i_viscm = nullptr;
  CS_MALLOC_HD(i_viscm, m->n_i_faces, cs_real_t, cs_alloc_mode);

  cs_real_t *b_viscm = nullptr;
  CS_MALLOC_HD(b_viscm, m->n_b_faces, cs_real_t, cs_alloc_mode);

  for (cs_lnum_t face_id = 0; face_id < m->n_i_faces; face_id++)
    i_viscm[face_id] = 0.;

  for (cs_lnum_t face_id = 0; face_id < m->n_b_faces; face_id++)
    b_viscm[face_id] = 0.;

  cs_real_3_t *f_ext, *dfext;
  CS_MALLOC_HD(f_ext, m->n_cells_with_ghosts, cs_real_3_t, cs_alloc_mode);
  CS_MALLOC_HD(dfext, m->n_cells_with_ghosts, cs_real_3_t, cs_alloc_mode);

  /* dfext is actually a dummy used to copy _hydrostatic_pressure_compute */
  /* f_ext is initialized with an initial density */

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {
    f_ext[cell_id][0] = density->val[cell_id] * phys_cst->gravity[0];
    dfext[cell_id][0] = 0.;
    f_ext[cell_id][1] = density->val[cell_id] * phys_cst->gravity[1];
    dfext[cell_id][1] = 0.;
    f_ext[cell_id][2] = density->val[cell_id] * phys_cst->gravity[2];
    dfext[cell_id][2] = 0;
  }

  /* Solving
   * ======= */

  cs_real_t *dam = nullptr;
  CS_MALLOC_HD(dam, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  cs_real_t *xam = nullptr;
  CS_MALLOC_HD(xam, m->n_i_faces, cs_real_t, cs_alloc_mode);

  cs_real_t *rhs = nullptr;
  CS_MALLOC_HD(rhs, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  cs_real_t *dpvar = nullptr;
  CS_MALLOC_HD(dpvar, m->n_cells_with_ghosts, cs_real_t, cs_alloc_mode);

  for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {
    dpvar[cell_id] = 0.;
    dam[cell_id] = 0.;
    rhs[cell_id] = 0.;
  }

  cs_real_t inf_norm = 1.;

  /* Loop to compute pressure profile */
  for (int sweep = 0; sweep < eqp_p->nswrsm && inf_norm > eqp_p->epsrsm; sweep++) {
    //FIXME 100 or nswrsm

    /* Update previous values of pressure for the convergence test */
    cs_field_current_to_previous(f);

    _hydrostatic_pressure_compute(f_ext,
                                  dfext,
                                  f->val,
                                  f->name,
                                  z_min,
                                  p_ground,
                                  i_massflux,
                                  b_massflux,
                                  i_viscm,
                                  b_viscm,
                                  dam,
                                  xam,
                                  dpvar,
                                  rhs);

    /* L infinity residual computation and forcing update */
    inf_norm = 0.;
    for (cs_lnum_t cell_id = 0; cell_id < m->n_cells; cell_id++) {
      inf_norm = fmax(fabs(f->val[cell_id] - f->val_pre[cell_id])/ps, inf_norm);

      /* Boussinesq hypothesis: do not update adiabatic temperature profile */
      if (idilat > 0) {
        temp->val[cell_id] =   potemp->val[cell_id]
                             * pow((f->val[cell_id]/ps), rscp);
      }

      /* f_ext = rho^k * g */
      cs_real_t rho_k = f->val[cell_id] / (rair * temp->val[cell_id]);
      density->val[cell_id] = rho_k;

      f_ext[cell_id][0] = rho_k * phys_cst->gravity[0];
      f_ext[cell_id][1] = rho_k * phys_cst->gravity[1];
      f_ext[cell_id][2] = rho_k * phys_cst->gravity[2];
    }

    if (cs_log_default_is_active()) {
      cs_parall_max(1, CS_REAL_TYPE, &inf_norm);
      bft_printf
        (_("Meteo profiles: iterative process to compute hydrostatic pressure\n"
           "  sweep %d, L infinity norm (delta p) / ps =%e\n"), sweep, inf_norm);
    }
  }

  /* Free memory */
  CS_FREE_HD(dpvar);
  CS_FREE_HD(rhs);
  CS_FREE_HD(i_viscm);
  CS_FREE_HD(b_viscm);
  CS_FREE_HD(xam);
  CS_FREE_HD(dam);
  CS_FREE_HD(f_ext);
  CS_FREE_HD(dfext);
  CS_FREE_HD(b_massflux);
  CS_FREE_HD(i_massflux);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function set the file name of the meteo file.
 *
 * \param[in] file_name  name of the file.
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_set_meteo_file_name(const char *file_name)
{
  if (file_name == nullptr) {
    return;
  }

  CS_REALLOC(_atmo_option.meteo_file_name,
             strlen(file_name) + 1,
             char);

  sprintf(_atmo_option.meteo_file_name, "%s", file_name);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief 1D Radiative scheme - Solar data + zenithal angle)
 *
 * Compute:
 *   - zenithal angle
 *   - solar contant (with correction for earth - solar length)
 *   - albedo if above the sea
 *   (Use analytical formulae of Paltrige and Platt
 *              dev.in atm. science no 5)
 * \param[in]   latitude    latitude
 * \param[in]   longitude   longitude
 * \param[in]   squant      start day in the year
 * \param[in]   utc         Universal time (hour)
 * \param[in]   sea_id      sea index
 * \param[out]  albedo      albedo
 * \param[out]  za          zenithal angle
 * \param[out]  muzero      cosin of zenithal angle
 * \param[out]  omega       solar azimut angle
 * \param[out]  fo          solar constant
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_compute_solar_angles(cs_real_t latitude,
                             cs_real_t longitude,
                             cs_real_t squant,
                             cs_real_t utc,
                             int       sea_id,
                             cs_real_t *albedo,
                             cs_real_t *za,
                             cs_real_t *muzero,
                             cs_real_t *omega,
                             cs_real_t *fo)
{
  /* 1 - initialisations */
  *fo = 1370.;

  /* conversions sexagesimal-decimal */

  cs_real_t flat = latitude  *cs_math_pi/180.;
  cs_real_t flong = longitude * 4. / 60.;

  cs_real_t t00 = 2. * cs_math_pi * squant/365.;

  /* 2 - compute declinaison (maximum error < 3 mn) */

  cs_real_t decl
    =   0.006918 - 0.399912*cos(t00) + 0.070257*sin(t00)
      - 0.006758*cos(2.*t00) + 0.000907*sin(2.*t00) - 0.002697*cos(3.*t00)
      + 0.001480*sin(3.*t00);

  /* 3 - compute local solar hour
   * equation du temps     erreur maxi    35 secondes
   */

  cs_real_t eqt
    = (  0.000075 + 0.001868*cos(t00) - 0.032077*sin(t00)
       - 0.014615*cos(2.*t00) - 0.040849*sin(2.*t00)) * 12./cs_math_pi;

  cs_real_t local_time = utc + flong + eqt;

  /* Transformation local_time-radians */

  /* On retire cs_math_pi et on prend le modulo 2pi du resultat */
  cs_real_t hr = (local_time - 12.)*cs_math_pi/12.;
  if (local_time < 12.)
    hr = (local_time + 12.)*cs_math_pi/12.;

  /* 4 - compute of cosinus of the zenithal angle */

  *muzero = sin(decl)*sin(flat) + cos(decl)*cos(flat)*cos(hr);

  *za = acos(*muzero);

  /* 5 - compute solar azimut */
  *omega = 0.;
  if (cs::abs(sin(*za)) > cs_math_epzero) {
    /* Cosinus of the zimut angle */
    cs_real_t co = (sin(decl)*cos(flat)-cos(decl)*sin(flat)*cos(hr))/sin(*za);
    *omega = acos(co);
    if (local_time > 12.)
      *omega = 2. * cs_math_pi - acos(co);
  }
  *omega -= cs_glob_atmo_option->domain_orientation * cs_math_pi / 180.;

  /* 5 - compute albedo at sea which depends on the zenithal angle */

  if (sea_id == 1) {
    cs_real_t ho = acos(*muzero);
    ho = 180.*(cs_math_pi/2. - ho)/cs_math_pi;
    if (ho < 8.5)
      ho = 8.5;
    if (ho > 60.)
      ho = 60.;
    *albedo = 3./ho;
  }

 /* 6 - Compute solar constant
    distance correction earth-sun
    corfo=(r0/r)**2
    precision better than e-04 */

  cs_real_t corfo
    =   1.000110 + 0.034221*cos(t00) + 0.001280*sin(t00)
      + 0.000719*cos(2.*t00) + 0.000077*sin(2.*t00);
  *fo *= corfo;

   /* Correction for very low zenithal angle */
   /* Optical air mass
    * cf. Kasten, F., Young, A.T., 1989. Revised optical air mass tables and
    * approximation formula.
    *
    * Note: old formula (LH74)
    * m = 35.0/sqrt(1224.0*muzero*muzero + 1.0) */
#if 1
   if (*muzero > 0)
     *muzero += 0.50572 * pow(96.07995-180./cs_math_pi * *za, -1.6364);
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Print the atmospheric module options to setup.log.
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_log_setup(void)
{
  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_OFF)
    return;

  cs_log_printf(CS_LOG_SETUP,
                _("\n"
                  "Atmospheric module options\n"
                  "--------------------------\n\n"));

  switch(cs_glob_physical_model_flag[CS_ATMOSPHERIC]) {
    case CS_ATMO_CONSTANT_DENSITY:
      /* Constant density */
      cs_log_printf(CS_LOG_SETUP,
                  _("  Constant density\n\n"));
      break;
    case CS_ATMO_DRY:
      /* Dry */
      cs_log_printf(CS_LOG_SETUP,
                  _("  Dry atmosphere\n\n"));
      break;
    case CS_ATMO_HUMID:
      /* Humid */
      cs_log_printf(CS_LOG_SETUP,
                  _("  Humid atmosphere\n\n"));
      break;
    default:
      break;
  }

  if (cs_glob_atmo_option->compute_z_ground > 0)
    cs_log_printf(CS_LOG_SETUP,
        _("  Compute ground elevation\n\n"));

  if (cs_glob_atmo_option->open_bcs_treatment > 0)
    cs_log_printf(CS_LOG_SETUP,
        _("  Impose open BCs with momentum source terms\n"));

  if (cs_glob_atmo_option->open_bcs_treatment == 2)
    cs_log_printf(CS_LOG_SETUP,
        _("  and impose profiles at ingoing faces\n\n"));

  /* CUT */
  cs_log_printf
    (CS_LOG_SETUP,
     _("\n"
       "  Starting Coordinated Universal Time (CUT):\n"
       "    Year:      %4d\n"
       "    Quant:     %4d\n"
       "    Hour:      %4d\n"
       "    Min:       %4d\n"
       "    Sec:       %4f\n\n"),
     cs_glob_atmo_option->syear,
     cs_glob_atmo_option->squant,
     cs_glob_atmo_option->shour,
     cs_glob_atmo_option->smin,
     cs_glob_atmo_option->ssec);

  /* Centre of the domain latitude */
  cs_log_printf
    (CS_LOG_SETUP,
     _("  Domain center:\n"
       "    latitude:  %6f\n"
       "    longitude: %6f\n"
       "    x center (in Lambert-93) : %6f\n"
       "    y center (in Lambert-93) : %6f\n\n"),
     cs_glob_atmo_option->latitude,
     cs_glob_atmo_option->longitude,
     cs_glob_atmo_option->x_l93,
     cs_glob_atmo_option->y_l93);

  if (cs_glob_atmo_option->meteo_profile == 1) {
    cs_log_printf
      (CS_LOG_SETUP,
       _("  Large scale Meteo file: %s\n\n"),
       cs_glob_atmo_option->meteo_file_name);
  }

  if (cs_glob_atmo_option->meteo_profile == 2) {
    cs_log_printf
      (CS_LOG_SETUP,
       _("  Large scale Meteo profile info:\n"
         "    roughness: %12f [m]\n"
         "    LMO inv:   %12f [m^-1]\n"
         "    ustar0:    %12f [m/s]\n"
         "    uref:      %12f [m/s]\n"
         "    zref:      %12f [m]\n"
         "    angle:     %12f [°]\n"
         "    P sea:     %12f [Pa]\n"
         "    T0:        %12f [K]\n"
         "    Tstar:     %12f [K]\n"
         "    BL height: %12f [m]\n"
         "    phim_s:    %s\n"
         "    phih_s:    %s\n"
         "    phim_u:    %s\n"
         "    phih_u:    %s\n\n"),
       cs_glob_atmo_option->meteo_z0,
       cs_glob_atmo_option->meteo_dlmo,
       cs_glob_atmo_option->meteo_ustar0,
       cs_glob_atmo_option->meteo_uref,
       cs_glob_atmo_option->meteo_zref,
       cs_glob_atmo_option->meteo_angle,
       cs_glob_atmo_option->meteo_psea,
       cs_glob_atmo_option->meteo_t0,
       cs_glob_atmo_option->meteo_tstar,
       cs_glob_atmo_option->meteo_zi,
       _univ_fn_name[cs_glob_atmo_option->meteo_phim_s],
       _univ_fn_name[cs_glob_atmo_option->meteo_phih_s],
       _univ_fn_name[cs_glob_atmo_option->meteo_phim_u],
       _univ_fn_name[cs_glob_atmo_option->meteo_phih_u]);
  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Reads the meteo profile data for the atmospheric
 *
 * \param[in]  mode     0: reading for dimensions and starting time only
 *                      1: reading actual meteo data
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_read_meteo_profile(int mode)
{
  cs_atmo_option_t *at_opt = &_atmo_option;
  cs_fluid_properties_t *fp = cs_get_glob_fluid_properties();

  int ih2o = 0;
  int itp = -1;
  char line[256];
  const char *name = at_opt->meteo_file_name;

  FILE *file = fopen(name, "r");

  // flag to take into account the humidity
  if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID)
    ih2o = 1;


  if (mode == 1)
    bft_printf("Reading meteo profiles data\n");
  else
    bft_printf("Reading dimensions for meteo profiles\n");

  while (fgets(line, 256, file) != nullptr) {

    itp ++;

    /* Read the comments
     * ------------------ */
    char *s = line;
    char fisrt_caracter = *s;
    while(fisrt_caracter == '/') {
      fgets(line, 256, file);
      char *_s = line;
      fisrt_caracter = *_s;
    }

    /* Read the time of the profile
       ---------------------------- */

    /* clean line */
    for (int i = 0; line[i] != '\0'; i++) {
      if (line[i] == ',')
        line[i] = ' ';
      if (line[i] == 'd' || line[i] == 'D')
        line[i] = 'e';
    }

    cs_real_t seconde = -1.0;
    int year = -1, quant = -1, hour = -1, minute = -1;
    sscanf(line, "%d %d %d %d %lf", &year, &quant, &hour, &minute, &seconde);

    if (seconde < 0.0 || quant > 366)
      cs_parameters_error
        (CS_ABORT_DELAYED,
         _("Meteorological profile data"),
         _("Error in the date of meteo profile file:\n"
           "Check the format (integers, real) for the date."));

    /* if the date and time are not completed in cs_user_model
     * the date and time of the first meteo profile are taken as the
     * starting time of the simulation */
    if (at_opt->syear < 0.0) {
      at_opt->syear  = year;
      at_opt->squant = quant;
      at_opt->shour  = hour;
      at_opt->smin   = minute;
      at_opt->ssec   = seconde;
    }

    if (mode == 1) {
      /* Compute the julian day for the starting day of the simulation
       * (julian day at 12h) */
      const cs_real_t sjday
        = at_opt->squant + ( ( 1461 * (at_opt->syear + 4800 + (1 - 14) / 12))/4
                             + (367 * (1 - 2 - 12 * ((1 - 14) / 12))) / 12
                             - (3 * ((at_opt->syear + 4900 + (1-14)/12) / 100))/4
                             + 1 - 32075 ) - 1;
      const cs_real_t jday
        = quant + (  (1461*(year+4800+(1-14)/12))/4
                     + (367*(1-2-12*((1-14) / 12))) / 12
                     - (3 * ((year+4900+(1-14)/12)/100)) / 4
                     +  1-32075) - 1;

      at_opt->time_met[itp] = (jday - sjday)*86400.0
                            + (hour - at_opt->shour)*3600.0
                            + (  minute - at_opt->smin)*60.0
                              + (seconde - at_opt->ssec);

      if (itp > 0)
        if (at_opt->time_met[itp] < at_opt->time_met[itp-1])
          cs_parameters_error
            (CS_ABORT_IMMEDIATE,
             _("Error in the meteo profile file:\n"),
             _("check that the chronogical order of"
               "the profiles are respected"));

    }

    /* Read the position of the profile
       -------------------------------- */
    fgets(line, 256, file);
    s = line;
    fisrt_caracter = *s;
    while(fisrt_caracter == '/') {
      fgets(line, 256, file);
      char *_s = line;
      fisrt_caracter = *_s;
    }

    if (mode == 1) {
      /* clean line */
      for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == ',')
          line[i] = ' ';
        if (line[i] == 'd' || line[i] == 'D')
          line[i] = 'e';
      }
      sscanf(line, "%lf %lf", &at_opt->xyp_met[3*itp],
                              &at_opt->xyp_met[1 + 3*itp]);
    }

    /* Read the sea-level pressure
       --------------------------- */
    fgets(line, 256, file);
    s = line;
    fisrt_caracter = *s;
    while(fisrt_caracter == '/') {
      fgets(line, 256, file);
      char *_s = line;
      fisrt_caracter = *_s;
    }

    cs_real_t pres = -1.0;
    sscanf(line, "%lf", &pres);
    if (itp == 0)
      fp->p0 = pres;
    if (mode == 1)
      at_opt->xyp_met[2 + 3*itp] = pres;

    /* Read the temperature and humidity profiles
       ------------------------------------------ */
    fgets(line, 256, file);
    s = line;
    fisrt_caracter = *s;
    while(fisrt_caracter == '/') {
      fgets(line, 256, file);
      char *_s = line;
      fisrt_caracter = *_s;
    }

    if (mode == 0) {
      cs_real_t zzmax, temp, qv;
      sscanf(line, " %d", &at_opt->met_1d_nlevels_t);
      for (int ii = 0; ii < at_opt->met_1d_nlevels_t; ii++) {
        fgets(line, 256, file);
        /* clean line */
        for (int i = 0; line[i] != '\0'; i++) {
          if (line[i] == ',')
            line[i] = ' ';
          if (line[i] == 'd' || line[i] == 'D')
            line[i] = 'e';
        }
        sscanf(line, " %lf %lf %lf", &zzmax, &temp, &qv);
        if(ii == 0 && itp == 0) {
          fp->t0 = temp + cs_physical_constants_celsius_to_kelvin;
          const cs_real_t rhum = fp->r_pg_cnst*(1.0+(fp->rvsra-1.0)*qv*ih2o);
          fp->ro0 = fp->p0 / fp->t0 /rhum;
        }
      }

      /* Computes met_1d_nlevels_max_t */
      at_opt->met_1d_nlevels_max_t = at_opt->met_1d_nlevels_t;
      if (at_opt->radiative_model_1d == 1) {
        const cs_real_t ztop = 11000.0;
        while(zzmax <= (ztop-1000.0)) {
          zzmax += 1000.0;
          at_opt->met_1d_nlevels_max_t += 1;
        }
      }

    }

    else {

      for (int ii = 0; ii < at_opt->met_1d_nlevels_t; ii++) {

        fgets(line, 256, file);
        /* clean line */
        for (int i = 0; line[i] != '\0'; i++) {
          if (line[i] == ',')
            line[i] = ' ';
          if (line[i] == 'd' || line[i] == 'D')
            line[i] = 'e';
        }

        //  Altitude, temperature, humidite
        if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID) {
          sscanf(line, " %le %le %le %le", &at_opt->z_temp_met[ii],
                 &at_opt->temp_met[ii + itp*at_opt->met_1d_nlevels_t],
                 &at_opt->qw_met[ii + itp*at_opt->met_1d_nlevels_t],
                 &at_opt->ndrop_met[ii + itp*at_opt->met_1d_nlevels_t]);
        }
        else {
          sscanf(line, " %le %le %le", &at_opt->z_temp_met[ii],
                 &at_opt->temp_met[ii + itp*at_opt->met_1d_nlevels_t],
                 &at_opt->qw_met[ii + itp*at_opt->met_1d_nlevels_t]);
        }
        // Initialize p0, rho0 and theta0 at the first level
        if (itp == 0 && ii == 0) {
          const cs_real_t qv = at_opt->qw_met[0];
          fp->t0 = at_opt->temp_met[0] + cs_physical_constants_celsius_to_kelvin;
          const cs_real_t rhum = fp->r_pg_cnst*(1.0+(fp->rvsra-1.0)*qv*ih2o);
          fp->ro0 = fp->p0 / fp->t0 /rhum;
        }

        // Check the unity of the specific humidity (kg/kg) when used
        if (   at_opt->qw_met[ii + itp*at_opt->met_1d_nlevels_t] > 0.1
            || at_opt->qw_met[ii + itp*at_opt->met_1d_nlevels_t] < 0.0 )
          cs_parameters_error
            (CS_ABORT_IMMEDIATE,
             _("Meteo profile file"),
             _("the values for the specific humidity are not realistic;"
               " check the unit (kg/kg)."));
        if (cs_glob_physical_model_flag[CS_ATMOSPHERIC] == CS_ATMO_HUMID)
          if (at_opt->ndrop_met[ii + itp*at_opt->met_1d_nlevels_t] < 0.0)
            cs_parameters_error
              (CS_ABORT_IMMEDIATE,
               _("Meteo profile file"),
               _("Number of droplets read < 0."));

      } // end for

      /* If 1D radiative model, complete the
         temperatureand humidity profiles up to 11000m */
      if (at_opt->radiative_model_1d == 1) {
        int ii = at_opt->met_1d_nlevels_t - 1;
        cs_real_t tlkelv, dum;
        const cs_real_t ztop  = 11000.00;
        const cs_real_t p_ref = 101325.0;
        const cs_real_t t_ref = 288.1500;
        cs_real_t zzmax = ((int)at_opt->z_temp_met[ii]/1000)*1000.0;
        while(zzmax <= (ztop-1000.0)) {
          ii ++;
          zzmax += 1000.0;
          at_opt->z_temp_met[ii] = zzmax;
          // standard temperature profile above the domaine
          int _size = at_opt->met_1d_nlevels_t-1+itp*at_opt->met_1d_nlevels_max_t;
          cs_real_t ttmet = at_opt->temp_met[_size]
                          + cs_physical_constants_celsius_to_kelvin;
          if (at_opt->hydrostatic_pressure_model == 0)
            cs_atmo_profile_std(at_opt->z_temp_met[at_opt->met_1d_nlevels_max_t - 1],
                                fp->p0,
                                ttmet,
                                at_opt->z_temp_met[ii],
                                &dum,
                                &tlkelv,
                                &dum);
          else
            cs_atmo_profile_std(0.0,
                                p_ref,
                                t_ref,
                                at_opt->z_temp_met[ii],
                                &dum,
                                &tlkelv,
                                &dum);
          at_opt->temp_met[ii + itp*at_opt->met_1d_nlevels_max_t]
            = tlkelv - cs_physical_constants_celsius_to_kelvin;
          if (at_opt->qv_profile == 0)
            at_opt->qw_met[ii + itp*at_opt->met_1d_nlevels_max_t] = 0.0;
          else
            at_opt->qw_met[ii + itp*at_opt->met_1d_nlevels_max_t]
              = at_opt->qw_met[_size]
              * exp((  at_opt->z_temp_met[at_opt->met_1d_nlevels_t-1]
                      -at_opt->z_temp_met[ii])/2.5e3);

          at_opt->ndrop_met[ii + itp*at_opt->met_1d_nlevels_max_t] = 0.0;
        } // end while

      }

    } // if (mode == 1)

    if (mode == 1) {

      _compute_hydro_pressure_profile(itp, ih2o, fp, at_opt);

      _compute_pot_temperature_density_profile(itp, ih2o, fp, at_opt);
    }

    /* Read the velocity profile
       ------------------------- */
    fgets(line, 256, file);
    s = line;
    fisrt_caracter = *s;
    while(fisrt_caracter == '/') {
      fgets(line, 256, file);
      char *_s = line;
      fisrt_caracter = *_s;
    }

    /* clean line */
    for (int i = 0; line[i] != '\0'; i++) {
      if (line[i] == ',')
        line[i] = ' ';
      if (line[i] == 'd' || line[i] == 'D')
        line[i] = 'e';
    }

    if (mode == 0) {
      sscanf(line, " %d", &at_opt->met_1d_nlevels_d);
      for (int ii = 0; ii < at_opt->met_1d_nlevels_d; ii++)
        fgets(line, 256, file);

      if (at_opt->met_1d_nlevels_d < 2 || at_opt->met_1d_nlevels_t < 2)
        cs_parameters_error
          (CS_ABORT_DELAYED,
           _("Meteo profile file"),
           _("The number of temperature or velocity measurements "
             "must be larger than 2."));
    }
    else {
      const int nbmetd = at_opt->met_1d_nlevels_d;
      for (int ii = 0; ii < nbmetd; ii++) {
        fgets(line, 256, file);
        for (int i = 0; line[i] != '\0'; i++) {
          if (line[i] == ',')
            line[i] = ' ';
          if (line[i] == 'd' || line[i] == 'D')
            line[i] = 'e';
        }
        sscanf(line, " %le %le  %le %le %le",
               &at_opt->z_dyn_met[ii],
               &at_opt->u_met[ii + itp*nbmetd],
               &at_opt->v_met[ii + itp*nbmetd],
               &at_opt->ek_met[ii + itp*nbmetd],
               &at_opt->ep_met[ii + itp*nbmetd]);
      }
    }

    /* Logging
       ------- */
    if (mode == 1) {
      const int date[5] = {year, quant, hour, minute, (int)seconde};
      _log_meteo_profile(itp, date, at_opt);
    }

  } // end loop

  if (mode == 0)
    at_opt->met_1d_ntimes = itp + 1;

  fclose(file);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Deallocate arrays for atmo module
 */
/*----------------------------------------------------------------------------*/

void
cs_atmo_finalize(void)
{
  CS_FREE(_atmo_option.meteo_file_name);
  CS_FREE(_atmo_option.z_dyn_met);
  CS_FREE(_atmo_option.z_temp_met);
  CS_FREE(_atmo_option.xyp_met);
  CS_FREE(_atmo_option.u_met);
  CS_FREE(_atmo_option.v_met);
  CS_FREE(_atmo_option.w_met);
  CS_FREE(_atmo_option.time_met);
  CS_FREE(_atmo_option.hyd_p_met);
  CS_FREE(_atmo_option.pot_t_met);
  CS_FREE(_atmo_option.ek_met);
  CS_FREE(_atmo_option.ep_met);
  CS_FREE(_atmo_option.temp_met);
  CS_FREE(_atmo_option.rho_met);
  CS_FREE(_atmo_option.qw_met);
  CS_FREE(_atmo_option.ndrop_met);
  CS_FREE(_atmo_option.dpdt_met);
  CS_FREE(_atmo_option.mom_met);
  CS_FREE(_atmo_option.mom_cs);

  CS_FREE(_atmo_option.rad_1d_xy);
  CS_FREE(_atmo_option.rad_1d_z);
  CS_FREE(_atmo_option.rad_1d_acinfe);
  CS_FREE(_atmo_option.rad_1d_dacinfe);
  CS_FREE(_atmo_option.rad_1d_aco2);
  CS_FREE(_atmo_option.rad_1d_aco2s);
  CS_FREE(_atmo_option.rad_1d_daco2);
  CS_FREE(_atmo_option.rad_1d_daco2s);
  CS_FREE(_atmo_option.rad_1d_acsup);
  CS_FREE(_atmo_option.rad_1d_acsups);
  CS_FREE(_atmo_option.rad_1d_dacsup);
  CS_FREE(_atmo_option.rad_1d_dacsups);
  CS_FREE(_atmo_option.rad_1d_tauzq);
  CS_FREE(_atmo_option.rad_1d_tauz);
  CS_FREE(_atmo_option.rad_1d_zq);
  CS_FREE(_atmo_option.rad_1d_zray);
  CS_FREE(_atmo_option.rad_1d_ir_div);
  CS_FREE(_atmo_option.rad_1d_sol_div);
  CS_FREE(_atmo_option.rad_1d_iru);
  CS_FREE(_atmo_option.rad_1d_ird);
  CS_FREE(_atmo_option.rad_1d_solu);
  CS_FREE(_atmo_option.rad_1d_sold);
  CS_FREE(_atmo_option.rad_1d_albedo0);
  CS_FREE(_atmo_option.rad_1d_emissi0);
  CS_FREE(_atmo_option.rad_1d_temp0);
  CS_FREE(_atmo_option.rad_1d_theta0);
  CS_FREE(_atmo_option.rad_1d_qw0);
  CS_FREE(_atmo_option.rad_1d_p0);
  CS_FREE(_atmo_option.rad_1d_rho0);
  CS_FREE(_atmo_option.rad_1d_aerosols);
  CS_FREE(_atmo_option.rad_1d_fn);
  CS_FREE(_atmo_option.rad_1d_nc);
  CS_FREE(_atmo_option.rad_1d_qv);
  CS_FREE(_atmo_option.rad_1d_ql);
  CS_FREE(_atmo_option.rad_1d_qw);

  CS_FREE(_atmo_option.soil_cat_r1);
  CS_FREE(_atmo_option.soil_cat_r2);
  CS_FREE(_atmo_option.soil_cat_vegeta);
  CS_FREE(_atmo_option.soil_cat_albedo);
  CS_FREE(_atmo_option.soil_cat_emissi);
  CS_FREE(_atmo_option.soil_cat_roughness);
  CS_FREE(_atmo_option.soil_cat_w1);
  CS_FREE(_atmo_option.soil_cat_w2);
  CS_FREE(_atmo_option.soil_cat_thermal_inertia);
  CS_FREE(_atmo_option.soil_cat_thermal_roughness);
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
