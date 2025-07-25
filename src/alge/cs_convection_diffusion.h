#ifndef __CS_CONVECTION_DIFFUSION_H__
#define __CS_CONVECTION_DIFFUSION_H__

/*============================================================================
 * Convection-diffusion operators.
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
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_mem.h"

#include "base/cs_base.h"
#include "base/cs_dispatch.h"
#include "base/cs_halo.h"
#include "base/cs_math.h"
#include "mesh/cs_mesh_quantities.h"
#include "cdo/cs_equation_param.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Macro definitions
 *============================================================================*/

/*============================================================================
 * Type definition
 *============================================================================*/

/*----------------------------------------------------------------------------
 * NVD/TVD Advection Scheme
 *----------------------------------------------------------------------------*/

typedef enum {

  CS_NVD_GAMMA      = 0,      /* GAMMA            */
  CS_NVD_SMART      = 1,      /* SMART            */
  CS_NVD_CUBISTA    = 2,      /* CUBISTA          */
  CS_NVD_SUPERBEE   = 3,      /* SUPERBEE         */
  CS_NVD_MUSCL      = 4,      /* MUSCL            */
  CS_NVD_MINMOD     = 5,      /* MINMOD           */
  CS_NVD_CLAM       = 6,      /* CLAM             */
  CS_NVD_STOIC      = 7,      /* STOIC            */
  CS_NVD_OSHER      = 8,      /* OSHER            */
  CS_NVD_WASEB      = 9,      /* WASEB            */
  CS_NVD_VOF_HRIC   = 10,     /* M-HRIC for VOF   */
  CS_NVD_VOF_CICSAM = 11,     /* M-CICSAM for VOF */
  CS_NVD_VOF_STACS  = 12,     /* STACS for VOF    */
  CS_NVD_N_TYPES    = 13      /* number of NVD schemes */

} cs_nvd_type_t;

/*============================================================================
 *  Global variables
 *============================================================================*/

/*============================================================================
 * Public function prototypes for Fortran API
 *============================================================================*/

/*=============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Return pointer to slope test indicator field values if active.
 *
 * parameters:
 *   f_id  <-- field id (or -1)
 *   eqp   <-- equation parameters
 *
 * return:
 *   pointer to local values array, or NULL;
 *----------------------------------------------------------------------------*/

cs_real_t *
cs_get_v_slope_test(int                        f_id,
                    const cs_equation_param_t  eqp);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the beta blending coefficient of the
 * beta limiter (ensuring preservation of a given min/max pair of values).
 *
 * \param[in]     f_id         field id
 * \param[in]     inc          "not an increment" flag
 * \param[in]     rovsdt       rho * volume / dt
 */
/*----------------------------------------------------------------------------*/

void
cs_beta_limiter_building(int              f_id,
                         int              inc,
                         const cs_real_t  rovsdt[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add the explicit part of the convection/diffusion terms of a
 * standard transport equation of a scalar field \f$ \varia \f$.
 *
 * More precisely, the right hand side \f$ Rhs \f$ is updated as
 * follows:
 * \f[
 * Rhs = Rhs - \sum_{\fij \in \Facei{\celli}}      \left(
 *        \dot{m}_\ij \left( \varia_\fij - \varia_\celli \right)
 *      - \mu_\fij \gradv_\fij \varia \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * Warning:
 * - \f$ Rhs \f$ has already been initialized before calling bilsc2!
 * - mind the sign minus
 *
 * Please refer to the
 * <a href="../../theory.pdf#bilsc2"><b>bilsc2</b></a> section of the
 * theory guide for more informations.
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          field id (or -1)
 * \param[in]     eqp           equation parameters
 * \param[in]     icvflb        global indicator of boundary convection flux
 *                               - 0 upwind scheme at all boundary faces
 *                               - 1 imposed flux at some boundary faces
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imasac        take mass accumulation into account?
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     pvara         solved variable (previous time step)
 * \param[in]     icvfli        boundary face indicator array of convection flux
 *                               - 0 upwind scheme
 *                               - 1 imposed flux
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_massflux    mass flux at interior faces
 * \param[in]     b_massflux    mass flux at boundary faces
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in,out] rhs           right hand side \f$ \vect{Rhs} \f$
 * \param[in,out] i_flux        interior flux (or nullptr)
 * \param[in,out] b_flux        boundary flux (or nullptr)
 */
/*----------------------------------------------------------------------------*/

void
cs_convection_diffusion_scalar(int                         idtvar,
                               int                         f_id,
                               const cs_equation_param_t   eqp,
                               int                         icvflb,
                               int                         inc,
                               int                         imasac,
                               cs_real_t                  *pvar,
                               const cs_real_t            *pvara,
                               const int                   icvfli[],
                               const cs_field_bc_coeffs_t *bc_coeffs,
                               const cs_real_t             i_massflux[],
                               const cs_real_t             b_massflux[],
                               const cs_real_t             i_visc[],
                               const cs_real_t             b_visc[],
                               cs_real_t                  *rhs,
                               cs_real_2_t                 i_flux[],
                               cs_real_t                   b_flux[]);

/*----------------------------------------------------------------------------*/
/*
 * \brief Update face flux with convection contribution of a standard transport
 * equation of a scalar field \f$ \varia \f$.
 *
 * \f[
 * C_\ij = \dot{m}_\ij \left( \varia_\fij - \varia_\celli \right)
 * \f]
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          field id (or -1)
 * \param[in]     eqp           equation parameters
 * \param[in]     icvflb        global indicator of boundary convection flux
 *                               - 0 upwind scheme at all boundary faces
 *                               - 1 imposed flux at some boundary faces
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imasac        take mass accumulation into account?
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     pvara         solved variable (previous time step)
 * \param[in]     icvfli        boundary face indicator array of convection flux
 *                               - 0 upwind scheme
 *                               - 1 imposed flux
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_massflux    mass flux at interior faces
 * \param[in]     b_massflux    mass flux at boundary faces
 * \param[in,out] i_conv_flux   scalar convection flux at interior faces
 * \param[in,out] b_conv_flux   scalar convection flux at boundary faces
 */
/*----------------------------------------------------------------------------*/

void
cs_face_convection_scalar(int                         idtvar,
                          int                         f_id,
                          const cs_equation_param_t   eqp,
                          int                         icvflb,
                          int                         inc,
                          int                         imasac,
                          cs_real_t                  *pvar,
                          const cs_real_t            *pvara,
                          const int                   icvfli[],
                          const cs_field_bc_coeffs_t *bc_coeffs,
                          const cs_real_t             i_massflux[],
                          const cs_real_t             b_massflux[],
                          cs_real_2_t                 i_conv_flux[],
                          cs_real_t                   b_conv_flux[]);

/*----------------------------------------------------------------------------*/
/*
 * \brief Add the explicit part of the convection/diffusion terms of a transport
 *  equation of a vector field \f$ \vect{\varia} \f$.
 *
 * More precisely, the right hand side \f$ \vect{Rhs} \f$ is updated as
 * follows:
 * \f[
 *  \vect{Rhs} = \vect{Rhs} - \sum_{\fij \in \Facei{\celli}}      \left(
 *         \dot{m}_\ij \left( \vect{\varia}_\fij - \vect{\varia}_\celli \right)
 *       - \mu_\fij \gradt_\fij \vect{\varia} \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * Remark:
 * if ivisep = 1, then we also take \f$ \mu \transpose{\gradt\vect{\varia}}
 * + \lambda \trace{\gradt\vect{\varia}} \f$, where \f$ \lambda \f$ is
 * the secondary viscosity, i.e. usually \f$ -\frac{2}{3} \mu \f$.
 *
 * Warning:
 * - \f$ \vect{Rhs} \f$ has already been initialized before calling bilsc!
 * - mind the sign minus
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          index of the current variable
 * \param[in]     eqp           equation parameters
 * \param[in]     icvflb        global indicator of boundary convection flux
 *                               - 0 upwind scheme at all boundary faces
 *                               - 1 imposed flux at some boundary faces
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     ivisep        indicator to take \f$ \divv
 *                               \left(\mu \gradt \transpose{\vect{a}} \right)
 *                               -2/3 \grad\left( \mu \dive \vect{a} \right)\f$
 *                               - 1 take into account,
 *                               - 0 otherwise
 * \param[in]     imasac        take mass accumulation into account?
 * \param[in]     pvar          solved velocity (current time step)
 * \param[in]     pvara         solved velocity (previous time step)
 * \param[in]     icvfli        boundary face indicator array of convection flux
 *                               - 0 upwind scheme
 *                               - 1 imposed flux
 * \param[in]     bc_coeffs_v   boundary conditions structure for the variable
 * \param[in]     bc_coeffs_solve_v   sweep loop boundary conditions structure
 * \param[in]     i_massflux    mass flux at interior faces
 * \param[in]     b_massflux    mass flux at boundary faces
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     i_secvis      secondary viscosity at interior faces
 * \param[in]     b_secvis      secondary viscosity at boundary faces
 * \param[in]     i_pvar        velocity at interior faces
 * \param[in]     b_pvar        velocity at boundary faces
 * \param[in,out] rhs           right hand side \f$ \vect{Rhs} \f$
 */
/*----------------------------------------------------------------------------*/

void
cs_convection_diffusion_vector(int                         idtvar,
                               int                         f_id,
                               const cs_equation_param_t   eqp,
                               int                         icvflb,
                               int                         inc,
                               int                         ivisep,
                               int                         imasac,
                               cs_real_3_t                *pvar,
                               const cs_real_3_t          *pvara,
                               const int                   icvfli[],
                               const cs_field_bc_coeffs_t *bc_coeffs_v,
                               const cs_bc_coeffs_solve_t *bc_coeffs_solve_v,
                               const cs_real_t             i_massflux[],
                               const cs_real_t             b_massflux[],
                               const cs_real_t             i_visc[],
                               const cs_real_t             b_visc[],
                               const cs_real_t             i_secvis[],
                               const cs_real_t             b_secvis[],
                               cs_real_3_t                *i_pvar,
                               cs_real_3_t                *b_pvar,
                               cs_real_3_t                *rhs);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add the explicit part of the convection/diffusion terms of a transport
 *  equation of a tensor field \f$ \tens{\varia} \f$.
 *
 * More precisely, the right hand side \f$ \tens{Rhs} \f$ is updated as
 * follows:
 * \f[
 *  \tens{Rhs} = \tens{Rhs} - \sum_{\fij \in \Facei{\celli}}      \left(
 *         \dot{m}_\ij \left( \tens{\varia}_\fij - \tens{\varia}_\celli \right)
 *       - \mu_\fij \gradt_\fij \tens{\varia} \cdot \tens{S}_\ij  \right)
 * \f]
 *
 * Warning:
 * - \f$ \tens{Rhs} \f$ has already been initialized before calling bilsc!
 * - mind the sign minus
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          index of the current variable
 * \param[in]     eqp           equation parameters
 * \param[in]     icvflb        global indicator of boundary convection flux
 *                               - 0 upwind scheme at all boundary faces
 *                               - 1 imposed flux at some boundary faces
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imasac        take mass accumulation into account?
 * \param[in]     pvar          solved velocity (current time step)
 * \param[in]     pvara         solved velocity (previous time step)
 * \param[in]     bc_coeffs_ts  boundary condition structure for the variable
 * \param[in]     bc_coeffs_solve_ts   sweep loop boundary conditions structure
 * \param[in]     i_massflux    mass flux at interior faces
 * \param[in]     b_massflux    mass flux at boundary faces
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in,out] rhs           right hand side \f$ \tens{Rhs} \f$
 */
/*----------------------------------------------------------------------------*/

void
cs_convection_diffusion_tensor(int                          idtvar,
                               int                          f_id,
                               const cs_equation_param_t    eqp,
                               int                          icvflb,
                               int                          inc,
                               int                          imasac,
                               cs_real_6_t                 *pvar,
                               const cs_real_6_t           *pvara,
                               const cs_field_bc_coeffs_t  *bc_coeffs_ts,
                               const cs_bc_coeffs_solve_t  *bc_coeffs_solve_ts,
                               const cs_real_t              i_massflux[],
                               const cs_real_t              b_massflux[],
                               const cs_real_t              i_visc[],
                               const cs_real_t              b_visc[],
                               cs_real_6_t                 *rhs);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add the explicit part of the convection/diffusion terms of a transport
 * equation of a scalar field \f$ \varia \f$ such as the temperature.
 *
 * More precisely, the right hand side \f$ Rhs \f$ is updated as
 * follows:
 * \f[
 * Rhs = Rhs + \sum_{\fij \in \Facei{\celli}}      \left(
 *        C_p\dot{m}_\ij \varia_\fij
 *      - \lambda_\fij \gradv_\fij \varia \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * Warning:
 * \f$ Rhs \f$ has already been initialized before calling bilsct!
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          index of the current variable
 * \param[in]     eqp           equation parameters)
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imasac        take mass accumulation into account?
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     pvara         solved variable (previous time step)
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_massflux    mass flux at interior faces
 * \param[in]     b_massflux    mass flux at boundary faces
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     xcpp          array of specific heat (\f$ C_p \f$)
 * \param[in,out] rhs           right hand side \f$ \vect{Rhs} \f$
 */
/*----------------------------------------------------------------------------*/

void
cs_convection_diffusion_thermal(int                         idtvar,
                                int                         f_id,
                                const cs_equation_param_t   eqp,
                                int                         inc,
                                int                         imasac,
                                cs_real_t                 * pvar,
                                const cs_real_t           * pvara,
                                const cs_field_bc_coeffs_t *bc_coeffs,
                                const cs_real_t             i_massflux[],
                                const cs_real_t             b_massflux[],
                                const cs_real_t             i_visc[],
                                const cs_real_t             b_visc[],
                                const cs_real_t             xcpp[],
                                cs_real_t                 * rhs);

/*----------------------------------------------------------------------------*/
/*
 * \brief Add the explicit part of the diffusion terms with a symmetric tensor
 * diffusivity for a transport equation of a scalar field \f$ \varia \f$.
 *
 * More precisely, the right hand side \f$ Rhs \f$ is updated as
 * follows:
 * \f[
 * Rhs = Rhs - \sum_{\fij \in \Facei{\celli}}      \left(
 *      - \tens{\mu}_\fij \gradv_\fij \varia \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * Warning:
 * - \f$ Rhs \f$ has already been initialized before
 *   calling cs_anisotropic_diffusion_scalar!
 * - mind the sign minus
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          index of the current variable
 * \param[in]     eqp           equation parameters
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     pvara         solved variable (previous time step)
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     viscel        symmetric cell tensor \f$ \tens{\mu}_\celli \f$
 * \param[in]     weighf        internal face weight between cells i j in case
 *                               of tensor diffusion
 * \param[in]     weighb        boundary face weight for cells i in case
 *                               of tensor diffusion
 * \param[in,out] rhs           right hand side \f$ \vect{Rhs} \f$
 */
/*----------------------------------------------------------------------------*/

void
cs_anisotropic_diffusion_scalar(int                         idtvar,
                                int                         f_id,
                                const cs_equation_param_t   eqp,
                                int                         inc,
                                cs_real_t                 * pvar,
                                const cs_real_t           * pvara,
                                const cs_field_bc_coeffs_t *bc_coeffs,
                                const cs_real_t             i_visc[],
                                const cs_real_t             b_visc[],
                                cs_real_6_t               * viscel,
                                const cs_real_2_t           weighf[],
                                const cs_real_t             weighb[],
                                cs_real_t                 * rhs);

/*-----------------------------------------------------------------------------*/
/*
 * \brief Add explicit part of the terms of diffusion by a left-multiplying
 * symmetric tensorial diffusivity for a transport equation of a vector field
 * \f$ \vect{\varia} \f$.
 *
 * More precisely, the right hand side \f$ \vect{Rhs} \f$ is updated as
 * follows:
 * \f[
 * \vect{Rhs} = \vect{Rhs} - \sum_{\fij \in \Facei{\celli}}      \left(
 *      - \gradt_\fij \vect{\varia} \tens{\mu}_\fij  \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * Remark:
 * if ivisep = 1, then we also take \f$ \mu \transpose{\gradt\vect{\varia}}
 * + \lambda \trace{\gradt\vect{\varia}} \f$, where \f$ \lambda \f$ is
 * the secondary viscosity, i.e. usually \f$ -\frac{2}{3} \mu \f$.
 *
 * Warning:
 * - \f$ \vect{Rhs} \f$ has already been initialized before calling the present
 *   function
 * - mind the sign minus
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          index of the current variable
 * \param[in]     eqp           equation parameters
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     ivisep        indicator to take \f$ \divv
 *                               \left(\mu \gradt \transpose{\vect{a}} \right)
 *                               -2/3 \grad\left( \mu \dive \vect{a} \right)\f$
 *                               - 1 take into account,
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     pvara         solved variable (previous time step)
 * \param[in]     bc_coeffs_v   boundary conditions structure for the variable
 * \param[in]     bc_coeffs_solve_v   sweep loop boundary conditions structure
 * \param[in]     i_visc        \f$ \tens{\mu}_\fij \dfrac{S_\fij}{\ipf\jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     i_secvis      secondary viscosity at interior faces
 * \param[in,out] rhs           right hand side \f$ \vect{Rhs} \f$
 */
/*----------------------------------------------------------------------------*/

void
cs_anisotropic_left_diffusion_vector(int                         idtvar,
                                     int                         f_id,
                                     const cs_equation_param_t   eqp,
                                     int                         inc,
                                     int                         ivisep,
                                     cs_real_3_t                *pvar,
                                     const cs_real_3_t          *pvara,
                                     const cs_field_bc_coeffs_t *bc_coeffs_v,
                                     const cs_bc_coeffs_solve_t *bc_coeffs_solve_v,
                                     const cs_real_33_t          i_visc[],
                                     const cs_real_t             b_visc[],
                                     const cs_real_t             i_secvis[],
                                     cs_real_3_t                *rhs);

/*-----------------------------------------------------------------------------*/
/*
 * \brief Add explicit part of the terms of diffusion by a right-multiplying
 * symmetric tensorial diffusivity for a transport equation of a vector field
 * \f$ \vect{\varia} \f$.
 *
 * More precisely, the right hand side \f$ \vect{Rhs} \f$ is updated as
 * follows:
 * \f[
 * \vect{Rhs} = \vect{Rhs} - \sum_{\fij \in \Facei{\celli}}      \left(
 *      - \gradt_\fij \vect{\varia} \tens{\mu}_\fij  \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * Warning:
 * - \f$ \vect{Rhs} \f$ has already been initialized before calling the present
 *   function
 * - mind the sign minus
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          index of the current variable
 * \param[in]     eqp           equation parameters
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     pvara         solved variable (previous time step)
 * \param[in]     bc_coeffs_v   boundary condition structure for the variable
 * \param[in]     bc_coeffs_solve_v   sweep loop boundary conditions structure
 * \param[in]     i_visc        \f$ \tens{\mu}_\fij \dfrac{S_\fij}{\ipf\jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     viscel        symmetric cell tensor \f$ \tens{\mu}_\celli \f$
 * \param[in]     weighf        internal face weight between cells i j in case
 *                               of tensor diffusion
 * \param[in]     weighb        boundary face weight for cells i in case
 *                               of tensor diffusion
 * \param[in,out] rhs           right hand side \f$ \vect{Rhs} \f$
 */
/*----------------------------------------------------------------------------*/

void
cs_anisotropic_right_diffusion_vector(int                          idtvar,
                                      int                          f_id,
                                      const cs_equation_param_t    eqp,
                                      int                          inc,
                                      cs_real_3_t                 *pvar,
                                      const cs_real_3_t           *pvara,
                                      const cs_field_bc_coeffs_t  *bc_coeffs_v,
                                      const cs_bc_coeffs_solve_t  *bc_coeffs_solve_v,
                                      const cs_real_t              i_visc[],
                                      const cs_real_t              b_visc[],
                                      cs_real_6_t                 *viscel,
                                      const cs_real_2_t            weighf[],
                                      const cs_real_t              weighb[],
                                      cs_real_3_t                 *rhs);

/*----------------------------------------------------------------------------*/
/*
 * \brief Add the explicit part of the diffusion terms with a symmetric tensor
 * diffusivity for a transport equation of a scalar field \f$ \varia \f$.
 *
 * More precisely, the right hand side \f$ Rhs \f$ is updated as
 * follows:
 * \f[
 * Rhs = Rhs - \sum_{\fij \in \Facei{\celli}}      \left(
 *      - \tens{\mu}_\fij \gradv_\fij \varia \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * Warning:
 * - \f$ Rhs \f$ has already been initialized before
 *   calling cs_anisotropic_diffusion_scalar!
 * - mind the sign minus
 *
 * \param[in]     idtvar        indicator of the temporal scheme
 * \param[in]     f_id          index of the current variable
 * \param[in]     eqp           equation parameters
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     pvara         solved variable (previous time step)
 * \param[in]     bc_coeffs_ts  boundary condition structure for the variable
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     viscel        symmetric cell tensor \f$ \tens{\mu}_\celli \f$
 * \param[in]     weighf        internal face weight between cells i j in case
 *                               of tensor diffusion
 * \param[in]     weighb        boundary face weight for cells i in case
 *                               of tensor diffusion
 * \param[in,out] rhs           right hand side \f$ \vect{Rhs} \f$
 */
/*----------------------------------------------------------------------------*/

void
cs_anisotropic_diffusion_tensor(int                          idtvar,
                                int                          f_id,
                                const cs_equation_param_t    eqp,
                                int                          inc,
                                cs_real_6_t                 *pvar,
                                const cs_real_6_t           *pvara,
                                const cs_field_bc_coeffs_t  *bc_coeffs_ts,
                                const cs_bc_coeffs_solve_t  *bc_coeffs_solve_ts,
                                const cs_real_t              i_visc[],
                                const cs_real_t              b_visc[],
                                cs_real_6_t                 *viscel,
                                const cs_real_2_t            weighf[],
                                const cs_real_t              weighb[],
                                cs_real_6_t                 *rhs);

/*----------------------------------------------------------------------------*/
/*
 * \brief Update the face mass flux with the face pressure (or pressure
 * increment, or pressure double increment) gradient.
 *
 * \f[
 * \dot{m}_\ij = \dot{m}_\ij
 *             - \Delta t \grad_\fij \delta p \cdot \vect{S}_\ij
 * \f]
 *
 * Please refer to the
 * <a href="../../theory.pdf#cs_face_diffusion_potential">
     <b>cs_face_diffusion_potential/cs_diffusion_potential</b></a>
 * section of the theory guide for more information.
 *
 * \param[in]     f_id          field id (or -1)
 * \param[in]     m             pointer to mesh
 * \param[in]     fvq           pointer to finite volume quantities
 * \param[in]     init          indicator
 *                               - 1 initialize the mass flux to 0
 *                               - 0 otherwise
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imrgra        indicator
 *                               - 0 iterative gradient
 *                               - 1 least squares gradient
 * \param[in]     nswrgp        number of reconstruction sweeps for the
 *                               gradients
 * \param[in]     imligp        clipping gradient method
 *                               - < 0 no clipping
 *                               - = 0 thank to neighbooring gradients
 *                               - = 1 thank to the mean gradient
 * \param[in]     iphydp        hydrostatic pressure indicator
 * \param[in]     iwgrp         indicator
 *                               - 1 weight gradient by vicosity*porosity
 *                               - weighting determined by field options
 * \param[in]     iwarnp        verbosity
 * \param[in]     epsrgp        relative precision for the gradient
 *                               reconstruction
 * \param[in]     climgp        clipping coeffecient for the computation of
 *                               the gradient
 * \param[in]     frcxt         body force creating the hydrostatic pressure
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     visel         viscosity by cell
 * \param[in,out] i_massflux    mass flux at interior faces
 * \param[in,out] b_massflux    mass flux at boundary faces
 */
/*----------------------------------------------------------------------------*/

void
cs_face_diffusion_potential(const int                   f_id,
                            const cs_mesh_t            *m,
                            cs_mesh_quantities_t       *fvq,
                            int                         init,
                            int                         inc,
                            int                         imrgra,
                            int                         nswrgp,
                            int                         imligp,
                            int                         iphydp,
                            int                         iwgrp,
                            int                         iwarnp,
                            double                      epsrgp,
                            double                      climgp,
                            cs_real_3_t                *frcxt,
                            cs_real_t                  *pvar,
                            const cs_field_bc_coeffs_t *bc_coeffs,
                            const cs_real_t             i_visc[],
                            const cs_real_t             b_visc[],
                            cs_real_t                  *visel,
                            cs_real_t                  *i_massflux,
                            cs_real_t                  *b_massflux);

/*----------------------------------------------------------------------------*/
/*
 * \brief Add the explicit part of the pressure gradient term to the mass flux
 * in case of anisotropic diffusion of the pressure field \f$ P \f$.
 *
 * More precisely, the mass flux side \f$ \dot{m}_\fij \f$ is updated as
 * follows:
 * \f[
 * \dot{m}_\fij = \dot{m}_\fij -
 *              \left( \tens{\mu}_\fij \gradv_\fij P \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * \param[in]     f_id          field id (or -1)
 * \param[in]     m             pointer to mesh
 * \param[in]     fvq           pointer to finite volume quantities
 * \param[in]     init           indicator
 *                               - 1 initialize the mass flux to 0
 *                               - 0 otherwise
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imrgra        indicator
 *                               - 0 iterative gradient
 *                               - 1 least squares gradient
 * \param[in]     nswrgp        number of reconstruction sweeps for the
 *                               gradients
 * \param[in]     imligp        clipping gradient method
 *                               - < 0 no clipping
 *                               - = 0 thank to neighbooring gradients
 *                               - = 1 thank to the mean gradient
 * \param[in]     ircflp        indicator
 *                               - 1 flux reconstruction,
 *                               - 0 otherwise
 * \param[in]     ircflb        indicator
 *                               - 1 boundary flux reconstruction,
 *                               - 0 otherwise
 * \param[in]     iphydp        indicator
 *                               - 1 hydrostatic pressure taken into account
 *                               - 0 otherwise
 * \param[in]     iwgrp         indicator
 *                               - 1 weight gradient by vicosity*porosity
 *                               - weighting determined by field options
 * \param[in]     iwarnp        verbosity
 * \param[in]     epsrgp        relative precision for the gradient
 *                               reconstruction
 * \param[in]     climgp        clipping coeffecient for the computation of
 *                               the gradient
 * \param[in]     frcxt         body force creating the hydrostatic pressure
 * \param[in]     pvar          solved variable (pressure)
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     viscel        symmetric cell tensor \f$ \tens{\mu}_\celli \f$
 * \param[in]     weighf        internal face weight between cells i j in case
 *                               of tensor diffusion
 * \param[in]     weighb        boundary face weight for cells i in case
 *                               of tensor diffusion
 * \param[in,out] i_massflux    mass flux at interior faces
 * \param[in,out] b_massflux    mass flux at boundary faces
 */
/*----------------------------------------------------------------------------*/

void
cs_face_anisotropic_diffusion_potential(const int                   f_id,
                                        const cs_mesh_t            *m,
                                        cs_mesh_quantities_t       *fvq,
                                        int                         init,
                                        int                         inc,
                                        int                         imrgra,
                                        int                         nswrgp,
                                        int                         imligp,
                                        int                         ircflp,
                                        int                         ircflb,
                                        int                         iphydp,
                                        int                         iwgrp,
                                        int                         iwarnp,
                                        double                      epsrgp,
                                        double                      climgp,
                                        cs_real_3_t                *frcxt,
                                        cs_real_t                  *pvar,
                                        const cs_field_bc_coeffs_t *bc_coeffs,
                                        const cs_real_t             i_visc[],
                                        const cs_real_t             b_visc[],
                                        cs_real_6_t                *viscel,
                                        const cs_real_2_t           weighf[],
                                        const cs_real_t             weighb[],
                                        cs_real_t                  *i_massflux,
                                        cs_real_t                  *b_massflux);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Update the cell mass flux divergence with the face pressure (or
 * pressure increment, or pressure double increment) gradient.
 *
 * \f[
 * \dot{m}_\ij = \dot{m}_\ij
 *             - \sum_j \Delta t \grad_\fij p \cdot \vect{S}_\ij
 * \f]
 *
 * \param[in]     f_id          field id (or -1)
 * \param[in]     m             pointer to mesh
 * \param[in]     fvq           pointer to finite volume quantities
 * \param[in]     init          indicator
 *                               - 1 initialize the mass flux to 0
 *                               - 0 otherwise
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imrgra        indicator
 *                               - 0 iterative gradient
 *                               - 1 least squares gradient
 * \param[in]     nswrgp        number of reconstruction sweeps for the
 *                               gradients
 * \param[in]     imligp        clipping gradient method
 *                               - < 0 no clipping
 *                               - = 0 thank to neighbooring gradients
 *                               - = 1 thank to the mean gradient
 * \param[in]     iphydp        hydrostatic pressure indicator
 * \param[in]     iwarnp        verbosity
 * \param[in]     iwgrp         indicator
 *                               - 1 weight gradient by vicosity*porosity
 *                               - weighting determined by field options
 * \param[in]     epsrgp        relative precision for the gradient
 *                               reconstruction
 * \param[in]     climgp        clipping coeffecient for the computation of
 *                               the gradient
 * \param[in]     frcxt         body force creating the hydrostatic pressure
 * \param[in]     pvar          solved variable (current time step)
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     visel         viscosity by cell
 * \param[in,out] diverg        mass flux divergence
 */
/*----------------------------------------------------------------------------*/

void
cs_diffusion_potential(const int                   f_id,
                       const cs_mesh_t            *m,
                       cs_mesh_quantities_t       *fvq,
                       int                         init,
                       int                         inc,
                       int                         imrgra,
                       int                         nswrgp,
                       int                         imligp,
                       int                         iphydp,
                       int                         iwgrp,
                       int                         iwarnp,
                       double                      epsrgp,
                       double                      climgp,
                       cs_real_3_t                *frcxt,
                       cs_real_t                  *pvar,
                       const cs_field_bc_coeffs_t *bc_coeffs,
                       const cs_real_t             i_visc[],
                       const cs_real_t             b_visc[],
                       cs_real_t                   visel[],
                       cs_real_t                  *diverg);

/*----------------------------------------------------------------------------*/
/*
 * \brief Add the explicit part of the divergence of the mass flux due to the
 * pressure gradient (analog to cs_anisotropic_diffusion_scalar).
 *
 * More precisely, the divergence of the mass flux side
 * \f$ \sum_{\fij \in \Facei{\celli}} \dot{m}_\fij \f$ is updated as follows:
 * \f[
 * \sum_{\fij \in \Facei{\celli}} \dot{m}_\fij
 *  = \sum_{\fij \in \Facei{\celli}} \dot{m}_\fij
 *  - \sum_{\fij \in \Facei{\celli}}
 *    \left( \tens{\mu}_\fij \gradv_\fij P \cdot \vect{S}_\ij  \right)
 * \f]
 *
 * \param[in]     f_id          field id (or -1)
 * \param[in]     m             pointer to mesh
 * \param[in]     fvq           pointer to finite volume quantities
 * \param[in]     init           indicator
 *                               - 1 initialize the mass flux to 0
 *                               - 0 otherwise
 * \param[in]     inc           indicator
 *                               - 0 when solving an increment
 *                               - 1 otherwise
 * \param[in]     imrgra        indicator
 *                               - 0 iterative gradient
 *                               - 1 least squares gradient
 * \param[in]     nswrgp        number of reconstruction sweeps for the
 *                               gradients
 * \param[in]     imligp        clipping gradient method
 *                               - < 0 no clipping
 *                               - = 0 thank to neighbooring gradients
 *                               - = 1 thank to the mean gradient
 * \param[in]     ircflp        indicator
 *                               - 1 flux reconstruction,
 *                               - 0 otherwise
 * \param[in]     ircflb        indicator
 *                               - 1 boundary flux reconstruction,
 *                               - 0 otherwise
 * \param[in]     iphydp        indicator
 *                               - 1 hydrostatic pressure taken into account
 *                               - 0 otherwise
 * \param[in]     iwgrp         indicator
 *                               - 1 weight gradient by vicosity*porosity
 *                               - weighting determined by field options
 * \param[in]     iwarnp        verbosity
 * \param[in]     epsrgp        relative precision for the gradient
 *                               reconstruction
 * \param[in]     climgp        clipping coeffecient for the computation of
 *                               the gradient
 * \param[in]     frcxt         body force creating the hydrostatic pressure
 * \param[in]     pvar          solved variable (pressure)
 * \param[in]     bc_coeffs     boundary condition structure for the variable
 * \param[in]     i_visc        \f$ \mu_\fij \dfrac{S_\fij}{\ipf \jpf} \f$
 *                               at interior faces for the r.h.s.
 * \param[in]     b_visc        \f$ \mu_\fib \dfrac{S_\fib}{\ipf \centf} \f$
 *                               at border faces for the r.h.s.
 * \param[in]     viscel        symmetric cell tensor \f$ \tens{\mu}_\celli \f$
 * \param[in]     weighf        internal face weight between cells i j in case
 *                               of tensor diffusion
 * \param[in]     weighb        boundary face weight for cells i in case
 *                               of tensor diffusion
 * \param[in,out] diverg        divergence of the mass flux
 */
/*----------------------------------------------------------------------------*/

void
cs_anisotropic_diffusion_potential(const int                   f_id,
                                   const cs_mesh_t            *m,
                                   cs_mesh_quantities_t       *fvq,
                                   int                         init,
                                   int                         inc,
                                   int                         imrgra,
                                   int                         nswrgp,
                                   int                         imligp,
                                   int                         ircflp,
                                   int                         ircflb,
                                   int                         iphydp,
                                   int                         iwgrp,
                                   int                         iwarnp,
                                   double                      epsrgp,
                                   double                      climgp,
                                   cs_real_3_t                *frcxt,
                                   cs_real_t                  *pvar,
                                   const cs_field_bc_coeffs_t *bc_coeffs,
                                   const cs_real_t             i_visc[],
                                   const cs_real_t             b_visc[],
                                   cs_real_6_t                *viscel,
                                   const cs_real_2_t           weighf[],
                                   const cs_real_t             weighb[],
                                   cs_real_t                  *diverg);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#ifdef __cplusplus

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the upwind gradient used in the slope tests.
 *
 * This function assumes the input gradient and pvar values have already
 * been synchronized.
 *
 * \param[in]     f_id         field id
 * \param[in]     ctx          Reference to dispatch context
 * \param[in]     inc          Not an increment flag
 * \param[in]     grad         standard gradient
 * \param[out]    grdpa        upwind gradient
 * \param[in]     pvar         values
 * \param[in]     bc_coeffs    boundary condition structure for the variable
 * \param[in]     i_massflux   mass flux at interior faces
 */
/*----------------------------------------------------------------------------*/

void
cs_slope_test_gradient(int                         f_id,
                       cs_dispatch_context        &ctx,
                       int                         inc,
                       const cs_real_3_t          *grad,
                       cs_real_3_t                *grdpa,
                       const cs_real_t            *pvar,
                       const cs_field_bc_coeffs_t *bc_coeffs,
                       const cs_real_t            *i_massflux);

/*----------------------------------------------------------------------------*/
/*
 * \brief Compute the upwind gradient used in the slope tests.
 *
 * template parameters:
 *   stride        1 for scalars, 3 for vectors, 6 for symmetric tensors
 *
 * This function assumes the input gradient and pvar values have already
 * been synchronized.
 *
 * \param[in]     ctx          Reference to dispatch context
 * \param[in]     grad         standard gradient
 * \param[out]    grdpa        upwind gradient
 * \param[in]     pvar         values
 * \param[in]     val_f        face values for gradient
 * \param[in]     i_massflux   mass flux at interior faces
 */
/*----------------------------------------------------------------------------*/

template <cs_lnum_t stride>
void
cs_slope_test_gradient_strided
  (cs_dispatch_context         &ctx,
   const cs_real_t              grad[][stride][3],
   cs_real_t                  (*restrict grdpa)[stride][3],
   const cs_real_t              pvar[][stride],
   const cs_real_t              val_f[][stride],
   const cs_real_t             *i_massflux);

/*----------------------------------------------------------------------------*/
/*
 * \brief Compute the upwind gradient used in the pure SOLU schemes
 *        (observed in the litterature).
 *
 * \param[in]     f_id         field index
 * \param[in]     ctx          Reference to dispatch context
 * \param[in]     inc          Not an increment flag
 * \param[in]     halo_type    halo type
 * \param[in]     bc_coeffs    boundary condition structure for the variable
 * \param[in]     i_massflux   mass flux at interior faces
 * \param[in]     b_massflux   mass flux at boundary faces
 * \param[in]     pvar         values
 * \param[out]    grdpa        upwind gradient
 */
/*----------------------------------------------------------------------------*/

void
cs_upwind_gradient(const int                     f_id,
                   cs_dispatch_context          &ctx,
                   const int                     inc,
                   const cs_halo_type_t          halo_type,
                   const cs_field_bc_coeffs_t   *bc_coeffs,
                   const cs_real_t               i_massflux[],
                   const cs_real_t               b_massflux[],
                   const cs_real_t              *pvar,
                   cs_real_3_t                  *grdpa);

/*----------------------------------------------------------------------------
 * Compute the local cell Courant number as the maximum of all cell face based
 * Courant number at each cell.
 *
 * parameters:
 *   f           <-- pointer to field
 *   ctx         <-- reference to dispatch context
 *   courant     --> cell Courant number
 */
/*----------------------------------------------------------------------------*/

void
cs_cell_courant_number(const cs_field_t     *f,
                       cs_dispatch_context  &ctx,
                       cs_real_t            *courant);

/*----------------------------------------------------------------------------*/

#endif /* cplusplus */

#endif /* __CS_CONVECTION_DIFFUSION_H__ */
