#ifndef __CS_LAGR_CLOGGING_H__
#define __CS_LAGR_CLOGGING_H__

/*============================================================================
 * Clogging modeling.
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
#include "lagr/cs_lagr_particle.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*============================================================================
 * Type definitions
 *============================================================================*/

typedef struct {

  cs_real_t   water_permit;
  cs_real_t   ionic_strength;
  cs_real_t   jamming_limit;
  cs_real_t   min_porosity;
  cs_real_t   diam_mean;
  cs_real_t   valen;
  cs_real_t   phi_p;
  cs_real_t   phi_s;

  cs_real_t*  temperature;
  cs_real_t*  debye_length;
  cs_real_t   cstham;
  cs_real_t   csthpp;
  cs_real_t   lambda_vdw;

} cs_lagr_clogging_param_t;

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Clogging initialization.
 *
 * - Retrieve various parameters for storing in global structure.
 * - Compute and store the Debye screening length
 *----------------------------------------------------------------------------*/

void
cloginit(const cs_real_t   *water_permit,
         const cs_real_t   *ionic_strength,
         const cs_real_t   *jamming_limit,
         const cs_real_t   *min_porosity,
         const cs_real_t   *diam_mean,
         const cs_real_t    temperature[],
         const cs_real_t   *valen,
         const cs_real_t   *phi_p,
         const cs_real_t   *phi_s,
         const cs_real_t   *cstham,
         const cs_real_t   *csthpp,
         const cs_real_t   *lambda_vdw
         );

/*----------------------------------------------------------------------------
 * Clogging finalization.
 *
 * Deallocate the arrays storing temperature and Debye length.
 *----------------------------------------------------------------------------*/

void
cs_lagr_clogging_finalize(void);

/*----------------------------------------------------------------------------
 * Clogging:
 *
 * - Compute the number of deposited particles in contact with the depositing
 *   particle
 * - Re-compute the energy barrier if this number is greater than zero
 *
 * parameters:
 *   particle         <-- pointer to particle data
 *   attr_map         <-- pointer to attribute map
 *   iel              <-- id of cell where the particle is
 *   face_area        <-- area of face
 *   energy_barrier   <-> energy barrier
 *   surface_coverage <-> surface coverage
 *   limit            <-> jamming limit
 *   mporos           <-> minimum porosity
 *
 * returns:
 *   number of deposited particles in contact with the depositing particle
 *----------------------------------------------------------------------------*/

int
cs_lagr_clogging_barrier(const void                     *particle,
                         const cs_lagr_attribute_map_t  *attr_map,
                         cs_lnum_t                       iel,
                         cs_real_t                      *energy_barrier,
                         cs_real_t                      *surface_coverage,
                         cs_real_t                      *limit,
                         cs_real_t                      *mporos);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* __CS_LAGR_CLOGGING_H__ */

