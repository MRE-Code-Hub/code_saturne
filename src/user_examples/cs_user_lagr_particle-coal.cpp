/*============================================================================
 * Methods for lagrangian module
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

/*============================================================================
 * Functions dealing with particle tracking
 *============================================================================*/

#include "base/cs_defs.h"

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <assert.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "cs_headers.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*============================================================================
 * Global variables
 *============================================================================*/

/*============================================================================
 * Local (user defined) function definitions
 *============================================================================*/

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * User function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief User modification of newly injected particles.
 *
 * This function is called after the initialization of the new particles in
 * order to modify them according to new particle profiles (injection
 * profiles, position of the injection point, statistical weights,
 * correction of the diameter if the standard-deviation option is activated).
 *
 * This function is called for each injection zone and set. Particles
 * with ids between \c pset->n_particles and \c n_elts are initialized
 * but may be modified by this function.
 *
 * \param[in,out]  particles         particle set
 * \param[in]      zis               zone injection set data
 * \param[in]      particle_range    start and past-the-end ids of new particles
 *                                   for this zone and class
 * \param[in]      particle_face_id  face ids of new particles if zone is
 *                                   a boundary, null otherwise
 * \param[in]      visc_length       viscous layer thickness
 *                                   (size: number of mesh boundary faces)
 */
/*----------------------------------------------------------------------------*/

void
cs_user_lagr_in(cs_lagr_particle_set_t             *particles,
                const cs_lagr_injection_set_t      *zis,
                const cs_lnum_t                     particle_range[2],
                [[maybe_unused]] const cs_lnum_t    particle_face_id[],
                [[maybe_unused]] const cs_real_t    visc_length[])
{

  /*! [lagr_inj_example_coal] */

  const cs_coal_model_t  *cm = cs_glob_coal_model;

  /* Changes to selected attributes to define coal
     --------------------------------------------- */

  constexpr int n_max_layers = 5;

  const int  n_layers = cs_glob_lagr_model->n_temperature_layers;
  assert(n_layers <= n_max_layers);

  int        coal_id = 0;
  cs_real_t  cp = zis->cp;
  cs_real_t  water_mass_f = 0.0, density = 0;
  cs_real_t  initial_diameter = 0, shrinking_diameter = 0;

  cs_real_t  temperature[n_max_layers];
  cs_real_t  coal_mass_fraction[n_max_layers];
  cs_real_t  coke_density[n_max_layers];
  cs_real_t  coke_mass_fraction[n_max_layers];

  for (int l_id = 0; l_id < n_layers; l_id++) {
    temperature[l_id] = 0;
    coal_mass_fraction[l_id] = 0;
    coke_density[l_id] = 0;
    coke_mass_fraction[l_id] = 0;
  }

  /* Determine coal properties based on injection zone and set
     --------------------------------------------------------- */

  if (zis->zone_id == 1 && zis->set_id == 0) {

    coal_id = 0;

    density
      =   cm->xashch[coal_id] * cm->rho0ch[coal_id]
        +    (1.0 - cm->xwatch[coal_id] - cm->xashch[coal_id])
           * cm->rho0ch[coal_id]
           * (1.0 - (cm->y1ch[coal_id] + cm->y2ch[coal_id]) / 2.0);

    cp = cm->cp2ch[coal_id]; /* specific heat */
    water_mass_f = 0.0;      /* water mass fraction */

    for (int l_id = 0; l_id < n_layers; l_id++) {

      /* temperature profile (in degrees C) */
      temperature[l_id] = 800 - 273.15;

      /* reactive coal mass fraction */
      coal_mass_fraction[l_id] = 0.;

      /* coke density after pyrolysis */
      coke_density[l_id]
        =   (1.0 - cm->xwatch[coal_id] - cm->xashch[coal_id])
          * cm->rho0ch[coal_id]
        * (1.0 - (cm->y1ch[coal_id] + cm->y2ch[coal_id]) / 2.0);

      /* coke mass fraction */
      coke_mass_fraction[l_id] = coke_density[l_id] / density;

    }

    /* coke diameter */
    shrinking_diameter = zis->diameter;

    /* initial particle diameter */
    initial_diameter = zis->diameter;

  }

  /* Now set newly injected particle values
     -------------------------------------- */

  for (cs_lnum_t p_id = particle_range[0]; p_id < particle_range[1]; p_id++) {

    /* specific heat */
    cs_lagr_particles_set_real(particles, p_id, CS_LAGR_CP, cp);

    /* water mass fraction in the particle */
    cs_lagr_particles_set_real(particles, p_id, CS_LAGR_WATER_MASS, water_mass_f);

    cs_real_t diam = cs_lagr_particles_get_real(particles, p_id, CS_LAGR_DIAMETER);
    cs_real_t mass = density * cs_math_pi/6 * (diam*diam*diam);

    cs_lagr_particles_set_real(particles, p_id, CS_LAGR_MASS, mass);
    cs_lagr_particles_set_real(particles, p_id, CS_LAGR_WATER_MASS,
                               water_mass_f * mass);

    cs_real_t *particle_temperature
      = (cs_real_t *)cs_lagr_particles_attr(particles, p_id, CS_LAGR_TEMPERATURE);
    cs_real_t *particle_coal_mass
      = (cs_real_t *)cs_lagr_particles_attr(particles, p_id, CS_LAGR_COAL_MASS);
    cs_real_t *particle_coke_mass
      = (cs_real_t *)cs_lagr_particles_attr(particles, p_id, CS_LAGR_COKE_MASS);
    cs_real_t *particle_coal_density
      = (cs_real_t *)cs_lagr_particles_attr(particles, p_id, CS_LAGR_COAL_DENSITY);

    for (int l_id = 0; l_id < n_layers; l_id++) {
      particle_temperature[l_id] = temperature[l_id];
      particle_coal_mass[l_id] = coal_mass_fraction[l_id] * mass / n_layers;
      particle_coke_mass[l_id] = coke_mass_fraction[l_id] * mass / n_layers;
      particle_coal_density[l_id] = coke_density[l_id];
    }

    cs_lagr_particles_set_real(particles, p_id, CS_LAGR_SHRINKING_DIAMETER,
                               shrinking_diameter);
    cs_lagr_particles_set_real(particles, p_id, CS_LAGR_INITIAL_DIAMETER,
                               initial_diameter);
  }
  /*! [lagr_inj_example_coal] */
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
