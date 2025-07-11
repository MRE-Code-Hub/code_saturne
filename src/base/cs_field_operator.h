#ifndef CS_FIELD_OPERATOR_H
#define CS_FIELD_OPERATOR_H

/*============================================================================
 * Field based algebraic operators.
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

#include "base/cs_defs.h"
#include "base/cs_field.h"
#include "alge/cs_gradient.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Macro definitions
 *============================================================================*/

/*============================================================================
 * Type definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Field values interpolation type
 *----------------------------------------------------------------------------*/

typedef enum {

  CS_FIELD_INTERPOLATE_MEAN,      /* mean element value (P0 interpolation) */
  CS_FIELD_INTERPOLATE_GRADIENT   /* mean + gradient correction (pseudo-P1) */

} cs_field_interpolate_t;

/*=============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Compute cell gradient of scalar field or component of vector or
 * tensor field.
 *
 * parameters:
 *   f              <-- pointer to field
 *   use_previous_t <-- should we use values from the previous time step ?
 *   inc            <-- if 0, solve on increment; 1 otherwise
 *   grad           --> gradient
 *----------------------------------------------------------------------------*/

void
cs_field_gradient_scalar(const cs_field_t  *f,
                         bool               use_previous_t,
                         int                inc,
                         cs_real_3_t       *grad);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute cell gradient of scalar array using parameters associated
 *         with a given field.
 *
 * \param[in]       f_id           associated field id
 * \param[in]       inc            if 0, solve on increment; 1 otherwise
 * \param[in]       bc_coeffs      boundary condition structure
 * \param[in, out]  var            gradient's base variable
 * \param[out]      grad           gradient
 */
/*----------------------------------------------------------------------------*/

void
cs_field_gradient_scalar_array(int                         f_id,
                               int                         inc,
                               const cs_field_bc_coeffs_t *bc_coeffs,
                               cs_real_t                   var[],
                               cs_real_3_t                 grad[]);

/*----------------------------------------------------------------------------
 * Compute cell gradient of scalar field or component of vector or
 * tensor field.
 *
 * parameters:
 *   f              <-- pointer to field
 *   use_previous_t <-- should we use values from the previous time step ?
 *   inc            <-- if 0, solve on increment; 1 otherwise
 *   hyd_p_flag     <-- flag for hydrostatic pressure
 *   f_ext          <-- exterior force generating the hydrostatic pressure
 *   grad           --> gradient
 *----------------------------------------------------------------------------*/

void
cs_field_gradient_potential(const cs_field_t  *f,
                            bool               use_previous_t,
                            int                inc,
                            int                hyd_p_flag,
                            cs_real_3_t        f_ext[],
                            cs_real_3_t       *grad);

/*----------------------------------------------------------------------------
 * Compute cell gradient of scalar field or component of vector or
 * tensor field.
 *
 * parameters:
 *   f              <-- pointer to field
 *   use_previous_t <-- should we use values from the previous time step ?
 *   inc            <-- if 0, solve on increment; 1 otherwise
 *   grad           --> gradient
 *----------------------------------------------------------------------------*/

void
cs_field_gradient_vector(const cs_field_t  *f,
                         bool               use_previous_t,
                         int                inc,
                         cs_real_33_t      *grad);

/*----------------------------------------------------------------------------
 * Compute cell gradient of tensor field.
 *
 * parameters:
 *   f              <-- pointer to field
 *   use_previous_t <-- should we use values from the previous time step ?
 *   inc            <-- if 0, solve on increment; 1 otherwise
 *   grad           --> gradient
 *----------------------------------------------------------------------------*/

void
cs_field_gradient_tensor(const cs_field_t  *f,
                         bool               use_previous_t,
                         int                inc,
                         cs_real_63_t      *grad);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the values of a scalar field at boundary face I' positions.
 *
 * \param[in]       f               pointer to field
 * \param[in]       use_previous_t  should we use values from the previous
 *                                  time step ?
 * \param[in]       inc             if 0, solve on increment; 1 otherwise
 * \param[out]      grad            gradient
 */
/*----------------------------------------------------------------------------*/

void
cs_field_gradient_boundary_iprime_scalar(const cs_field_t  *f,
                                         bool               use_previous_t,
                                         cs_lnum_t          n_faces,
                                         const cs_lnum_t   *face_ids,
                                         cs_real_t          var_iprime[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the values of a vector field at boundary face I' positions.
 *
 * \param[in]       f               pointer to field
 * \param[in]       use_previous_t  should we use values from the previous
 *                                  time step ?
 * \param[in]       inc             if 0, solve on increment; 1 otherwise
 * \param[out]      grad            gradient
 */
/*----------------------------------------------------------------------------*/

void
cs_field_gradient_boundary_iprime_vector(const cs_field_t  *f,
                                         bool               use_previous_t,
                                         cs_lnum_t          n_faces,
                                         const cs_lnum_t   *face_ids,
                                         cs_real_3_t        var_iprime[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute the values of a symmetric tensor field at
 *        boundary face I' positions.
 *
 * \param[in]       f               pointer to field
 * \param[in]       use_previous_t  should we use values from the previous
 *                                  time step ?
 * \param[in]       inc             if 0, solve on increment; 1 otherwise
 * \param[out]      grad            gradient
 */
/*----------------------------------------------------------------------------*/

void
cs_field_gradient_boundary_iprime_tensor(const cs_field_t  *f,
                                         bool               use_previous_t,
                                         cs_lnum_t          n_faces,
                                         const cs_lnum_t   *face_ids,
                                         cs_real_6_t        var_iprime[]);

/*----------------------------------------------------------------------------
 * Interpolate field values at a given set of points.
 *
 * parameters:
 *   f                  <-- pointer to field
 *   interpolation_type <-- interpolation type
 *   n_points           <-- number of points at which interpolation
 *                          is required
 *   point_location     <-- location of points in mesh elements
 *                          (based on the field location)
 *   point_coords       <-- point coordinates
 *   val                --> interpolated values
 *----------------------------------------------------------------------------*/

void
cs_field_interpolate(cs_field_t              *f,
                     cs_field_interpolate_t   interpolation_type,
                     cs_lnum_t                n_points,
                     const cs_lnum_t          point_location[],
                     const cs_real_3_t        point_coords[],
                     cs_real_t               *val);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Find local extrema of a given scalar field at each cell
 *
 * This assumes the field values have been synchronized.
 *
 * \param[in]       f_id        scalar field id
 * \param[in]       halo_type   halo type
 * \param[in, out]  local_max   local maximum value
 * \param[in, out]  local_min   local minimum value
 */
/*----------------------------------------------------------------------------*/

void
cs_field_local_extrema_scalar(int              f_id,
                              cs_halo_type_t   halo_type,
                              cs_real_t       *local_max,
                              cs_real_t       *local_min);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Shift field values in order to set its spatial average to a given
 * value.
 *
 * \param[in]   f   pointer to field
 * \param[in]   va  real value of volume average to be set
 */
/*----------------------------------------------------------------------------*/

void
cs_field_set_volume_average(cs_field_t     *f,
                            const cs_real_t mean);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Synchronize current parallel and periodic field values.
 *
 * This function currently only upates fields based on CS_MESH_LOCATION_CELLS.
 *
 * \param[in, out]   f           pointer to field
 * \param[in]        halo_type   halo type
 */
/*----------------------------------------------------------------------------*/

void
cs_field_synchronize(cs_field_t      *f,
                     cs_halo_type_t   halo_type);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#ifdef __cplusplus

/*----------------------------------------------------------------------------*/
/*!
 * \brief Synchronize current parallel and periodic field values.
 *
 * This function currently only upates fields based on CS_MESH_LOCATION_CELLS.
 *
 * \param[in, out]   f           pointer to field
 * \param[in]        halo_type   halo type
 * \param[in]        on_device   specify whether sync is done on device
 */
/*----------------------------------------------------------------------------*/

void
cs_field_synchronize(cs_field_t            *f,
                     cs_halo_type_t         halo_type,
                     bool                   on_device);

#endif // __cplusplus

/*----------------------------------------------------------------------------*/

#endif /* CS_FIELD_OPERATOR_H */
