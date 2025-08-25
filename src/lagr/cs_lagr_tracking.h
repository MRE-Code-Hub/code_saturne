#ifndef __CS_LAGR_TRACKING_H__
#define __CS_LAGR_TRACKING_H__

/*============================================================================
 * Functions and types for the Lagrangian module
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

#include "lagr/cs_lagr_particle.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Macro definitions
 *============================================================================*/

/*============================================================================
 * Type definitions
 *============================================================================*/

/* State where a particle can be.
   (order is chosen so as to make tests simpler;
   inside domain first, outside after) */

typedef enum {
  CS_LAGR_PART_TO_SYNC,
  CS_LAGR_PART_TO_SYNC_NEXT,
  CS_LAGR_PART_TREATED,
  CS_LAGR_PART_STUCK,
  CS_LAGR_PART_MERGED,
  CS_LAGR_PART_OUT,
  CS_LAGR_PART_ERR
} cs_lagr_tracking_state_t;

/*=============================================================================
 * Global variables
 *============================================================================*/

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize particle tracking subsystem
 */
/*----------------------------------------------------------------------------*/

void
cs_lagr_tracking_initialize(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Integrate or not SDEs associated tot the particle and apply one
 * trajectography step to track the displacement.
 *
 * \param[in]     visc_length     viscous layer thickness
 * \param[in,out] particle_range  start and past-the-end ids of tracked
 * \param[in,out] particle_range  start and past-the-end ids of tracked
 * \param[in]     resol_sde       true  the sdes will be resolved
 *                                false only the trajectography step is done
 *
 */
/*----------------------------------------------------------------------------*/

void
cs_lagr_integ_track_particles(const cs_real_t  visc_length[],
                              cs_lnum_t        particle_range[2],
                              const bool       resol_sde);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Finalize Lagrangian module.
 */
/*----------------------------------------------------------------------------*/

void
cs_lagr_tracking_finalize(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Determine the number of the closest wall face from the particle
 *        as well as the corresponding wall normal distance (y_p^+)
 *
 * Used for the deposition model.
 *
 * \param[in]   p_set        pointer to particle set
 * \param[in]   p_id         particle id
 * \param[in]   visc_length  viscous layer thickness
 * \param[out]  yplus        associated yplus value
 * \param[out]  face_id      associated neighbor wall face, or -1
 */
/*----------------------------------------------------------------------------*/

 void
 cs_lagr_test_wall_cell(const cs_lagr_particle_set_t   *p_set,
                        cs_lnum_t                       p_id,
                        const cs_real_t                 visc_length[],
                        cs_real_t                      *yplus,
                        cs_lnum_t                      *face_id);

/*----------------------------------------------------------------------------*/


END_C_DECLS

#endif /* __CS_LAGR_TRACKING_H__ */
