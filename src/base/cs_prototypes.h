#ifndef __CS_PROTOTYPES_H__
#define __CS_PROTOTYPES_H__

/*============================================================================
 * Prototypes for Fortran functions and subroutines callable from C
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

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_base.h"
#include "base/cs_field.h"
#include "base/cs_mobile_structures.h"
#include "base/cs_probe.h"
#include "base/cs_time_control.h"
#include "base/cs_volume_zone.h"
#include "cdo/cs_domain.h"
#include "mesh/cs_mesh.h"
#include "mesh/cs_mesh_bad_cells.h"
#include "mesh/cs_mesh_quantities.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*============================================================================
 * Macro definitions
 *============================================================================*/

/*=============================================================================
 * Fortran function/subroutine prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Add field indexes associated with a new non-user solved variable,
 * with default options
 *
 * parameters:
 *   f_id <--   field id
 *
 * returns:
 *   variable number for defined field
 *----------------------------------------------------------------------------*/

int
cs_add_variable_field_indexes(int  f_id);

/*----------------------------------------------------------------------------
 * Add field indexes associated with a new non-user solved variable,
 * with default options
 *
 * parameters:
 *   f_id <--   field id
 *
 * returns:
 *   scalar number for defined field
 *----------------------------------------------------------------------------*/

int
cs_add_model_field_indexes(int  f_id);

/*----------------------------------------------------------------------------*/
/*! \brief  Prepare for the cressman interpolation of the variables
 *
 * \param[in]   the_time          current time
 */
/*----------------------------------------------------------------------------*/

void
cs_summon_cressman(cs_real_t the_time);

/*----------------------------------------------------------------------------
 * Return Lagrangian model status.
 *
 * parameters:
 *   model_flag   --> 0 without Lagrangian, 1 or 2 with Lagrangian
 *   restart_flag --> 1 for Lagrangian restart, 0 otherwise
 *   frozen_flag  --> 1 for frozen Eulerian flow, 0 otherwise
 *----------------------------------------------------------------------------*/

void
cs_lagr_status(int  *model_flag,
               int  *restart_flag,
               int  *frozen_flag);

/*----------------------------------------------------------------------------*/
/*!
  * \brief Initialize all field BC coefficients.
  */
/*----------------------------------------------------------------------------*/

void
cs_field_map_and_init_bcs(void);

/*----------------------------------------------------------------------------*/
/*!
  * \brief Update the convective mass flux before the Navier Stokes equations
  *        (prediction and correction steps).
  *
  * This function computes a potential \f$ \varia \f$ solving the equation:
  * \f[
  * D \left( \Delta t, \varia \right) = \divs \left( \rho \vect{u}^n\right)
  *                                   - \Gamma^n
  *                                   + \dfrac{\rho^n - \rho^{n-1}}{\Delta t}
  * \f]
  * This potential is then used to update the mass flux as follows:
  * \f[
  *  \dot{m}^{n+\frac{1}{2}}_\ij = \dot{m}^{n}_\ij
  *                               - \Delta t \grad_\fij \varia \cdot \vect{S}_\ij
  * \f]
  *
  *  \param[in]  dt time step (per cell)
  */
/*----------------------------------------------------------------------------*/

void
cs_mass_flux_prediction(cs_real_t  dt[]);

/*----------------------------------------------------------------------------
 * Compute source terms for specific physical model scalars.
 *----------------------------------------------------------------------------*/

void
cs_physical_model_source_terms(int         iscal,
                               cs_real_t   st_imp[],
                               cs_real_t   st_exp[]);

/*----------------------------------------------------------------------------*/
/*!
  * \brief Update the convective mass flux before the Navier Stokes equations
  * (prediction and correction steps).
  *
  * This function computes a potential \f$ \varia \f$ solving the equation:
  * \f[
  * D \left( \Delta t, \varia \right) = \divs \left( \rho \vect{u}^n\right)
  *                                   - \Gamma^n
  *                                   + \dfrac{\rho^n - \rho^{n-1}}{\Delta t}
  * \f]
  * This potential is then used to update the mass flux as follows:
  * \f[
  *  \dot{m}^{n+\frac{1}{2}}_\ij = \dot{m}^{n}_\ij
  *                               - \Delta t \grad_\fij \varia \cdot \vect{S}_\ij
  * \f]
  *
  *  \param[in]     ncesmp        number of cells with mass source term
  *  \param[in]     icetsm        index of cells with mass source term
  *  \param[in]     dt            time step (per cell)
  */
/*----------------------------------------------------------------------------*/

void
cs_prediction_mass_flux(cs_lnum_t  ncesmp,
                        cs_lnum_t  icetsm[],
                        cs_real_t  dt[]);

/*============================================================================
 *  User function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Data Entry of the 1D wall thermal module.
 *----------------------------------------------------------------------------*/

void
cs_user_1d_wall_thermal(int iappel);

/*----------------------------------------------------------------------------
 * Fill in vertical profiles of atmospheric properties prior to solving
 * 1D radiative transfers.
 *----------------------------------------------------------------------------*/

void
cs_user_atmo_1d_rad_prf(cs_real_t   preray[],
                        cs_real_t   temray[],
                        cs_real_t   romray[],
                        cs_real_t   qvray[],
                        cs_real_t   qlray[],
                        cs_real_t   ncray[],
                        cs_real_t   aeroso[]);

/*----------------------------------------------------------------------------
 * Data Entry of the wall condensation module
 *----------------------------------------------------------------------------*/

void
cs_user_wall_condensation(int iappel);

/*----------------------------------------------------------------------------
 * Setup boundary conditions to be applied.
 *----------------------------------------------------------------------------*/

void
cs_user_boundary_conditions_setup(cs_domain_t  *domain);

/*----------------------------------------------------------------------------
 * This function is called at each time step for boundary conditions.
 *----------------------------------------------------------------------------*/

void
cs_user_boundary_conditions(cs_domain_t  *domain,
                            int           bc_type[]);

/*----------------------------------------------------------------------------
 * Boundary conditions for (Arbitrary Lagrangian Eulerian).
 *----------------------------------------------------------------------------*/

void
cs_user_boundary_conditions_ale(cs_domain_t  *domain,
                                int           bc_type[],
                                int           ale_bc_type[],
                                int           impale[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize variables.
 *
 * This function is called at beginning of the computation
 * (restart or not) before the time step loop.
 *
 * This is intended to initialize or modify (when restarted)
 * variable and time step values.

 * \param[in, out]  domain   pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_extra_operations_initialize(cs_domain_t     *domain);

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called at the end of each time step.
 *
 * It has a very general purpose, although it is recommended to handle
 * mainly postprocessing or data-extraction type operations.
 *
 * \param[in, out]  domain   pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_extra_operations(cs_domain_t     *domain);

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called at the end of the calculation.
 *
 * It has a very general purpose, although it is recommended to handle
 * mainly postprocessing or data-extraction type operations.

 * \param[in, out]  domain   pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_extra_operations_finalize(cs_domain_t     *domain);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Definition of internal mobile structures and corresponding initial
 *        conditions (initial displacement and velocity ).
 *
 * \param[in]       is_restart         indicate if computation is restarted
 * \param[in]       n_structs          number of mobile structures
 * \param[in, out]  plot;              monitoring format mask
 *                                       0: no plot
 *                                       1: plot to text (.dat) format
 *                                       2: plot to .csv format
 *                                       3: plot to both formats
 * \param[in, out]  plot_time_control  plot time output frequency control
 * \param[in, out]  aexxst             coefficient for predicted displacement
 * \param[in, out]  bexxst             coefficient for predicted displacement
 * \param[in, out]  cfopre             coefficient for predicted force
 * \param[in, out]  xstr0              initial displacement per structure
 * \param[in, out]  vstr0              initial velocity per structure
 * \param[in, out]  xstreq             displacement of initial mesh relative to
 *                                     structures position at equilibrium
 *
 * \param[in, out]  plot_time_control  time control associated to plotting
 */
/*----------------------------------------------------------------------------*/

void
cs_user_fsi_structure_define(int                 is_restart,
                             int                 n_structs,
                             int                *plot,
                             cs_time_control_t  *plot_time_control,
                             cs_real_t          *aexxst,
                             cs_real_t          *bexxst,
                             cs_real_t          *cfopre,
                             cs_real_t           xstr0[][3],
                             cs_real_t           vstr0[][3],
                             cs_real_t           xstreq[][3]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Time-based settings for internal mobile structures.
 *
 * \param[in]       n_structs  number of mobile structures
 * \param[in]       ts         time step structure
 * \param[in]       xstreq     displacement of initial mesh rel. to equilibrium
 * \param[in]       xstr       structural displacement
 * \param[in]       vstr       structural velocity
 * \param[in, out]  xmstru     matrix of structural mass
 * \param[in, out]  xcstru     matrix of structural friction
 * \param[in, out]  xkstru     matrix of structural stiffness
 * \param[in, out]  forstr     forces acting on structures (take forces)
 * \param[in, out]  dtstr      structural time step
 */
/*----------------------------------------------------------------------------*/

void
cs_user_fsi_structure_values(int                    n_structs,
                             const cs_time_step_t  *ts,
                             const cs_real_t        xstreq[][3],
                             const cs_real_t        xstr[][3],
                             const cs_real_t        vstr[][3],
                             cs_real_t              xmstru[][3][3],
                             cs_real_t              xcstru[][3][3],
                             cs_real_t              xkstru[][3][3],
                             cs_real_t              forstr[][3],
                             cs_real_t              dtstr[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define structure numbers for faces associated with internal
 *        or external (code_aster) structures.
 *
 * Structure numbers associated to a given face have the following values:
 * - -i where coupled to  i-th (1-to n) external (code_aster) structure.
 * - 0 where not coupled with an internal or external structure.
 * - i  where coupled to  i-th (1-to n) internal (mass-spring) structure.
 *
 * \param[in, out]  domain         pointer to a cs_domain_t structure
 * \param[in, out]  structure_num  structure id associated to each face
 * \param[in, out]  structure_typ  structure type associated to each face
 */
/*----------------------------------------------------------------------------*/

void
cs_user_fsi_structure_num(cs_domain_t               *domain,
                          int                        structure_num[],
                          cs_mobile_structure_type_t structure_typ[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute convergence state of the external structures
 *        (code_aster excluded).
 *
 * Compute converge status and residual of the external coupling
 * Convergence status: 0 - external coupling has not converged
 *                     1 - external coupling has converged
 *
 * \param[in, out]  domain         pointer to a cs_domain_t structure
 * \param[out]      cvg_status     convergence status
 * \param[out]      residual       value of the residual
 */
/*----------------------------------------------------------------------------*/

void
cs_user_fsi_external_cvg(const cs_domain_t *domain,
                         const cs_real_t    epsilon,
                         int               *cvg_status,
                         cs_real_t         *residual);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute displacement fluid boundary for the external structures
 *        (code_aster excluded).
 *
 * \param[in]  domain         pointer to a cs_domain_t structure
 * \param[in]  b_stress       pointer to boundary stress array
 * \param[in, out]  disaple   pointer to mesh_displacement array
 */
/*----------------------------------------------------------------------------*/

void
cs_user_fsi_external_displacement(const cs_domain_t *domain,
                                  const cs_real_3_t *b_stress,
                                  cs_real_3_t       *disaple);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute GUI-defined head losses for a given volume zone.
 *
 * Head loss tensor coefficients for each cell are organized as follows:
 * cku11, cku22, cku33, cku12, cku13, cku23.
 *
 * \param[in]       zone  pointer to zone structure
 * \param[in, out]  cku   head loss coefficients
 */
/*----------------------------------------------------------------------------*/

void
cs_user_head_losses(const cs_zone_t *zone, cs_real_t cku[][6]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called one time step to initialize problem.
 *
 * \param[in, out]  domain   pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_initialization(cs_domain_t     *domain);

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called at startup and allows overloading the GUI
 * defined initialization functions.
 *
 * This function is called before the beginning of the computation
 * allowing an overload of the GUI defined initialization (called just
 * after cs_gui_initial_conditions).
 *
 * \param[in, out]  domain   pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_initial_conditions(cs_domain_t  *domain);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define internal coupling options.
 *
 * Options are usually defined using cs_internal_coupling_add_entity.
 */
/*----------------------------------------------------------------------------*/

void
cs_user_internal_coupling(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define volumes as internal coupling zones.
 *
 * These zones will be separated from the rest of the domain using automatically
 * defined thin walls.
 *
 * \param[in, out] mesh  pointer to a cs_mesh_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_internal_coupling_add_volumes(cs_mesh_t  *mesh);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define volumesi from separated meshes as internal coupling zones.
 *
 * These zones must be disjoint and the face selection criteria must be
 * specified.
 *
 * \param[in, out]  mesh  pointer to a cs_mesh_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_internal_coupling_from_disjoint_meshes(cs_mesh_t  *mesh);

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called each time step to define physical properties.
 *
 * \param[in, out]   domain    pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_physical_properties(cs_domain_t  *domain);

/*----------------------------------------------------------------------------*/
/*!
 * \brief User definition of enthalpy to temperature conversion.
 *
 * This allows overwriting the solver defaults if necessary.
 *
 * This function may be called on a per-zone basis, so as to allow different
 * conversion relations in zones representing solids or different fluids.
 *
 * \param[in, out]  domain   pointer to a cs_domain_t structure
 * \param[in]       z        zone (volume or boundary) applying to current call
 * \param[in]       z_local  if true, h and t arrays are defined in a compact
 *                           (contiguous) manner for this zone only;
 *                           if false, h and t are defined on the zone's parent
 *                           location (usually all cells or boundary faces)
 * \param[in]       h        enthalpy values
 * \param[in, out]  t        temperature values
 */
/*----------------------------------------------------------------------------*/

void
cs_user_physical_properties_h_to_t(cs_domain_t      *domain,
                                   const cs_zone_t  *z,
                                   bool              z_local,
                                   const cs_real_t   h[],
                                   cs_real_t         t[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief User definition of temperature to enthalpy conversion.
 *
 * This allows overwriting the solver defaults if necessary.
 *
 * This function may be called on a per-zone basis, so as to allow different
 * conversion relations in zones representing solids or different fluids.
 *
 * \param[in, out]  domain   pointer to a cs_domain_t structure
 * \param[in]       z        zone (volume or boundary) applying to current call
 * \param[in]       z_local  if true, h and t arrays are defined in a compact
 *                           (contiguous) manner for this zone only;
 *                           if false, h and t are defined on the zone's parent
 *                           location (usually all cells or boundary faces)
 * \param[in]       h        temperature values
 * \param[in, out]  t        enthalpy values
 */
/*----------------------------------------------------------------------------*/

void
cs_user_physical_properties_t_to_h(cs_domain_t      *domain,
                                   const cs_zone_t  *z,
                                   bool              z_local,
                                   const cs_real_t   t[],
                                   cs_real_t         h[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief User function to define a custom law for the thermodynamic pressure.
 *
 * Allows to define a custom law for the constant uniform thermodynamic
 * pressure (whenn \ref cs_velocity_pressure_model_t::idilat = 3 or
 * \ref cs_fluid_properties_t::ipthrm = 1).
 *
 * The density is then updated
 * (in \ref cs_compute_thermo_pressure_density.c) as:
 * \f[\rho^{n+1} =\rho^{n} \cdot \frac{P_{th}^{n+1}}{P_{th}^{n}}\f].
 *
 * \param[in, out]  td_p  Updated value of the thermodynamic pressure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_physical_properties_td_pressure(cs_real_t  *td_p);

/*----------------------------------------------------------------------------*/
/*!
 * \brief User modification of the turbulence viscosity
 *
 * \param[in, out]   domain      pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_physical_properties_turb_viscosity(cs_domain_t      *domain);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Additional user-defined source terms for variable equations
 *   (momentum, scalars, turbulence...).
 *
 * \param[in, out]  domain   pointer to a cs_domain_t structure
 * \param[in]       f_id     field id of the variable
 * \param[out]      st_exp   explicit source term
 * \param[out]      st_imp   implicit part of the source term
 */
/*----------------------------------------------------------------------------*/

void
cs_user_source_terms(cs_domain_t  *domain,
                     int           f_id,
                     cs_real_t    *st_exp,
                     cs_real_t    *st_imp);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the porosity (volume factor \f$ \epsilon \f$
 *        when the porosity model is activated.
 *        (\ref cs_glob_porous_model > 0).
 *
 * This function is called at the begin of the simulation only.
 *
 * \param[in, out]   domain    pointer to a cs_domain_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_user_porosity(cs_domain_t  *domain);

/*----------------------------------------------------------------------------
 * Define mesh joinings.
 *----------------------------------------------------------------------------*/

void
cs_user_join(void);

/*----------------------------------------------------------------------------
 * Define linear solver options.
 *
 * This function is called at the setup stage, once user and most model-based
 * fields are defined.
 *----------------------------------------------------------------------------*/

void
cs_user_linear_solvers(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define or modify output user parameters.
 * For CDO schemes, specify the elements such as properties, advection fields,
 * user-defined equations and modules which have been previously added.
 */
/*----------------------------------------------------------------------------*/

void
cs_user_finalize_setup(cs_domain_t *domain);

/*----------------------------------------------------------------------------
 * Tag bad cells within the mesh based on geometric criteria.
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_bad_cells_tag(cs_mesh_t             *mesh,
                           cs_mesh_quantities_t  *mesh_quantities);

/*----------------------------------------------------------------------------
 * Define restart mode for mesh input and preprocessing.
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_restart_mode(void);

/*----------------------------------------------------------------------------
 * Define mesh files to read and optional associated transformations.
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_input(void);

/*----------------------------------------------------------------------------
 * Modifiy geometry and mesh.
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_modify(cs_mesh_t  *mesh);

/*----------------------------------------------------------------------------
 * Insert boundary wall into a mesh.
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_boundary(cs_mesh_t  *mesh);

/*----------------------------------------------------------------------------
 * Mesh smoothing.
 *
 * parameters:
 *   mesh <-> pointer to mesh structure to smoothe
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_smoothe(cs_mesh_t  *mesh);

/*----------------------------------------------------------------------------
 * Enable or disable mesh saving.
 *
 * By default, mesh is saved when modified.
 *
 * parameters:
 *   mesh <-> pointer to mesh structure
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_save(cs_mesh_t  *mesh);

/*----------------------------------------------------------------------------
 * Set options for cutting of warped faces
 *
 * parameters:
 *   mesh <-> pointer to mesh structure to smoothe
 *----------------------------------------------------------------------------*/

void
cs_user_mesh_warping(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Apply partial modifications to the mesh after the preprocessing
 *        and initial postprocessing mesh building stage.
 *
 * \param[in,out] mesh  pointer to a cs_mesh_t structure
 * \param[in,out] mesh_quantities pointer to a cs_mesh_quantities_t structure
*/
/*----------------------------------------------------------------------------*/

void
cs_user_mesh_modify_partial(cs_mesh_t             *mesh,
                            cs_mesh_quantities_t  *mesh_quantities);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define a cartesian mesh.
*/
/*----------------------------------------------------------------------------*/

void
cs_user_mesh_cartesian_define(void);

/*----------------------------------------------------------------------------
 * Select physical model options, including user fields.
 *
 * This function is called at the earliest stages of the data setup.
 *----------------------------------------------------------------------------*/

void
cs_user_model(void);

/*----------------------------------------------------------------------------
 * Define advanced mesh numbering options.
 *----------------------------------------------------------------------------*/

void
cs_user_numbering(void);

/*----------------------------------------------------------------------------
 * Define parallel IO settings.
 *----------------------------------------------------------------------------*/

void
cs_user_parallel_io(void);

/*----------------------------------------------------------------------------
 * Define advanced partitioning options.
 *----------------------------------------------------------------------------*/

void
cs_user_partition(void);

/*----------------------------------------------------------------------------
 * User function for input of radiative transfer module options.
 *
 * Deprecated Use cs_user_model instead.
 *----------------------------------------------------------------------------*/

void
cs_user_radiative_transfer_parameters(void);

/*----------------------------------------------------------------------------
 * Define sparse matrix tuning options.
 *----------------------------------------------------------------------------*/

void
cs_user_matrix_tuning(void);

/*----------------------------------------------------------------------------
 * Define or modify general numerical and physical user parameters.
 *
 * At the calling point of this function, most model-related most variables
 * and other fields have been defined, so specific settings related to those
 * fields may be set here.
 *----------------------------------------------------------------------------*/

void
cs_user_parameters(cs_domain_t *domain);

/*-----------------------------------------------------------------------------
 * User subroutine for input of radiative transfer boundary conditions
 *----------------------------------------------------------------------------*/

void
cs_user_radiative_transfer_bcs(cs_domain_t      *domain,
                               const int         bc_type[],
                               int               isothp[],
                               cs_real_t        *tmin,
                               cs_real_t        *tmax,
                               cs_real_t        *tx,
                               const cs_real_t   dt[],
                               const cs_real_t   thwall[],
                               const cs_real_t   qincid[],
                               cs_real_t         hfcnvp[],
                               cs_real_t         flcnvp[],
                               cs_real_t         xlamp[],
                               cs_real_t         epap[],
                               cs_real_t         epsp[],
                               cs_real_t         textp[]);

/*----------------------------------------------------------------------------
 * Define periodic faces.
 *----------------------------------------------------------------------------*/

void
cs_user_periodicity(void);

/*----------------------------------------------------------------------------
 * Define post-processing writers.
 *
 * The default output format and frequency may be configured, and additional
 * post-processing writers allowing outputs in different formats or with
 * different format options and output frequency than the main writer may
 * be defined.
 *----------------------------------------------------------------------------*/

void
cs_user_postprocess_writers(void);

/*-----------------------------------------------------------------------------
 * Define monitoring probes and profiles. A profile is seen as a set of probes.
 *----------------------------------------------------------------------------*/

void
cs_user_postprocess_probes(void);

/*----------------------------------------------------------------------------
 * Define post-processing meshes.
 *
 * The main post-processing meshes may be configured, and additional
 * post-processing meshes may be defined as a subset of the main mesh's
 * cells or faces (both interior and boundary).
 *----------------------------------------------------------------------------*/

void
cs_user_postprocess_meshes(void);

/*----------------------------------------------------------------------------
 * User function for output of values on a post-processing mesh.
 *----------------------------------------------------------------------------*/

void
cs_user_postprocess_values(const char            *mesh_name,
                           int                    mesh_id,
                           int                    cat_id,
                           cs_probe_set_t        *probes,
                           cs_lnum_t              n_cells,
                           cs_lnum_t              n_i_faces,
                           cs_lnum_t              n_b_faces,
                           cs_lnum_t              n_vertices,
                           const cs_lnum_t        cell_list[],
                           const cs_lnum_t        i_face_list[],
                           const cs_lnum_t        b_face_list[],
                           const cs_lnum_t        vertex_list[],
                           const cs_time_step_t  *ts);

/*----------------------------------------------------------------------------
 * Override default frequency or calculation end based output.
 *
 * This allows fine-grained control of activation or deactivation,
 *
 * parameters:
 *   nt_max_abs <-- maximum time step number
 *   nt_cur_abs <-- current time step number
 *   t_cur_abs  <-- absolute time at the current time step
 *----------------------------------------------------------------------------*/

void
cs_user_postprocess_activate(int     nt_max_abs,
                             int     nt_cur_abs,
                             double  t_cur_abs);

/*----------------------------------------------------------------------------
 * Absorption coefficient for radiative module
 *----------------------------------------------------------------------------*/

void
cs_user_rad_transfer_absorption(const int  bc_type[],
                                cs_real_t  ck[]);

/*----------------------------------------------------------------------------
 * Compute the net radiation flux
 *----------------------------------------------------------------------------*/

void
cs_user_rad_transfer_net_flux(const int        itypfb[],
                              const cs_real_t  twall[],
                              const cs_real_t  qincid[],
                              const cs_real_t  xlam[],
                              const cs_real_t  epa[],
                              const cs_real_t  eps[],
                              const cs_real_t  ck[],
                              cs_real_t        net_flux[]);

/*----------------------------------------------------------------------------
 * Set user solver.
 *----------------------------------------------------------------------------*/

int
cs_user_solver_set(void);

/*----------------------------------------------------------------------------
 * Main call to user solver.
 *----------------------------------------------------------------------------*/

void
cs_user_solver(const cs_mesh_t             *mesh,
               const cs_mesh_quantities_t  *mesh_quantities);

/*----------------------------------------------------------------------------
 * Define couplings with other instances of code_saturne.
 *----------------------------------------------------------------------------*/

void
cs_user_saturne_coupling(void);

/*----------------------------------------------------------------------------
 * Define couplings with SYRTHES code.
 *----------------------------------------------------------------------------*/

void
cs_user_syrthes_coupling(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute a volume exchange coefficient for SYRTHES couplings.
 *
 * \param[in]   coupling_id   Syrthes coupling id
 * \param[in]   syrthes_name  name of associated Syrthes instance
 * \param[in]   n_elts        number of associated cells
 * \param[in]   elt_ids       associated cell ids
 * \param[out]  h_vol         associated exchange coefficient (size: n_elts)
 */
/*----------------------------------------------------------------------------*/

void
cs_user_syrthes_coupling_volume_h(int               coupling_id,
                                  const char       *syrthes_name,
                                  cs_lnum_t         n_elts,
                                  const cs_lnum_t   elt_ids[],
                                  cs_real_t         h_vol[]);

/*----------------------------------------------------------------------------
 * Define couplings with CATHARE code.
 *----------------------------------------------------------------------------*/

void
cs_user_cathare_coupling(void);

/*----------------------------------------------------------------------------
 * Define time moments.
 *----------------------------------------------------------------------------*/

void
cs_user_time_moments(void);


/*----------------------------------------------------------------------------
 * Define time tables.
 *----------------------------------------------------------------------------*/

void
cs_user_time_table(void);

/*----------------------------------------------------------------------------
 * Define rotor/stator model.
 *----------------------------------------------------------------------------*/

void
cs_user_turbomachinery(void);

/*----------------------------------------------------------------------------
 * Define rotor axes, associated cells, and rotor/stator faces.
 *----------------------------------------------------------------------------*/

void
cs_user_turbomachinery_rotor(void);

/*----------------------------------------------------------------------------
 * Define rotation velocity of rotor.
 *----------------------------------------------------------------------------*/

void
cs_user_turbomachinery_set_rotation_velocity(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define volume and surface zones.
 */
/*----------------------------------------------------------------------------*/

void
cs_user_zones(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define scaling parameter for electric model
*/
/*----------------------------------------------------------------------------*/

void
cs_user_scaling_elec(const cs_mesh_t             *mesh,
                     const cs_mesh_quantities_t  *mesh_quantities,
                     cs_real_t                   *dt);

/*----------------------------------------------------------------------------
 * Computation of the relaxation time-scale to equilibrium in the frame of
 * the homogeneous two-phase model.
 *----------------------------------------------------------------------------*/

void
cs_user_hgn_thermo_relax_time(const cs_mesh_t *mesh,
                              const cs_real_t *alpha_eq,
                              const cs_real_t *y_eq,
                              const cs_real_t *z_eq,
                              const cs_real_t *ei,
                              const cs_real_t *v,
                              cs_real_t       *relax_tau);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define ParaMEDMEM coupling(s)
 */
/*----------------------------------------------------------------------------*/

void
cs_user_paramedmem_define_couplings(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define coupled meshes
 */
/*----------------------------------------------------------------------------*/

void
cs_user_paramedmem_define_meshes(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define fields to couple with ParaMEDMEM
 */
/*----------------------------------------------------------------------------*/

void
cs_user_paramedmem_define_fields(void);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* __CS_PROTOTYPES_H__ */
