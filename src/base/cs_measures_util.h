#ifndef __CS_MEASURES_H__
#define __CS_MEASURES_H__

/*============================================================================
 * Field management.
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

#if defined(HAVE_MPI)
#include <mpi.h>
#endif

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_defs.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*----------------------------------------------------------------------------
 * Definition of the measures set structure
 *----------------------------------------------------------------------------*/

typedef struct {

  const char         *name;                /* Name */
  int                 id;                  /* Measures set id */
  int                 type;                /* Measures set type flag
                                              not yet used */
  int                 dim;                 /* Measures set dimension */
  int                *comp_ids;            /* index of components of measures if
                                              dim > 1 */
  cs_lnum_t           nb_measures;         /* Number of measures */
  cs_lnum_t           nb_measures_max;     /* Dynamic reallocation parameter */
  bool                interleaved;         /* Is measures set interleaved ? */
  int                *is_cressman;         /* Is measure cressman interpolated?
                                               0: no
                                                1: yes */
  int                *is_interpol;         /* Is measure taken into account for
                                              interpolation ?
                                                0: no
                                                1: yes */
  cs_real_t          *coords;              /* measures coordinates */
  cs_real_t          *measures;            /* measures values */
  cs_real_t          *inf_radius;          /* influence radius */

} cs_measures_set_t;

typedef struct {

  const char         *name;                /* Name */
  int                 id;                  /* Grid id */
  cs_lnum_t           nb_points;           /* Number of grid points */
  bool                is_connect;          /* Is connectivity computed? */
  cs_real_t          *coords;              /* measures coordinates */
  cs_lnum_t          *cell_connect;        /* Mesh -> grid connectivity */
  int                *rank_connect;        /* Rank location */

} cs_interpol_grid_t;

/*----------------------------------------------------------------------------
 * Interpolate mesh field on interpol grid structure.
 *
 * This function is deprecated because it take 1 value of the 3D field to
 * the 1D grid...
 *
 * parameters:
 *   ig                   <-- pointer to the interpolation grid structure
 *   values_to_interpol   <-- field on mesh (size = n_cells)
 *   interpolated_values  --> interpolated values on the interpolation grid
 *                            structure (size = ig->nb_point)
 *----------------------------------------------------------------------------*/

void
cs_interpol_field_on_grid_deprecated(cs_interpol_grid_t *ig,
                                     const cs_real_t    *values_to_interpol,
                                     cs_real_t          *interpoled_values);

/*----------------------------------------------------------------------------
 * Interpolate mesh field on interpol grid structure.
 *
 * parameters:
 *   ig                   <-- pointer to the interpolation grid structure
 *   values_to_interpol   <-- field on mesh (size = n_cells)
 *   interpolated_values  --> interpolated values on the interpolation grid
 *                            structure (size = ig->nb_point)
 *----------------------------------------------------------------------------*/

void
cs_interpol_field_on_grid(cs_interpol_grid_t         *ig,
                          const cs_real_t            *values_to_interpol,
                          cs_real_t                  *interpoled_values);

/*----------------------------------------------------------------------------
 * Create an interpolation grid descriptor.
 *
 * For measures set with a dimension greater than 1, components are interleaved.
 *
 * parameters:
 *   name        <-- grid name
 *
 * returns:
 *   pointer to new interpolation grid.
 *
 *----------------------------------------------------------------------------*/

cs_interpol_grid_t *
cs_interpol_grid_create(const char   *name);

/*----------------------------------------------------------------------------
 * Create interpolation grid.
 *
 * parameters:
 *   ig        <-- pointer to an interpol grid structure
 *   nb_points <-- number of considered points
 *   coord     <-- coordonates of considered points
 *----------------------------------------------------------------------------*/

void
cs_interpol_grid_init(cs_interpol_grid_t    *ig,
                      const cs_lnum_t        nb_points,
                      const cs_real_t       *coords);

/*----------------------------------------------------------------------------
 * Create a measures set descriptor.
 *
 * For measures set with a dimension greater than 1, components are interleaved.
 *
 * parameters:
 *   name        <-- measures set name
 *   type_flag   <-- mask of field property and category values (not used yet)
 *   dim         <-- measure set dimension (number of components)
 *   interleaved <-- if dim > 1, indicate if field is interleaved
 *
 * returns:
 *   pointer to new measures set.
 *
 *----------------------------------------------------------------------------*/

cs_measures_set_t *
cs_measures_set_create(const char   *name,
                       int           type_flag,
                       int           dim,
                       bool          interleaved);

/*----------------------------------------------------------------------------
 * (re)Allocate and fill in a measures set structure with an array of measures.
 *
 * parameters:
 *   ms               <-- pointer to the measures set
 *   nb_measures      <-- number of measures
 *   is_cressman      <-- for each measure cressman interpolation is:
 *                          0: not used
 *                          1: used
 *   is_interpol      <-- for the interpolation on mesh, each measure is
 *                          0: not taken into account
 *                          1: taken into account
 *   measures_coords  <-- measures spaces coordonates
 *   measures         <-- measures values (associated to coordinates)
 *   influence_radius <-- influence radius for interpolation (xyz interleaved)
 *----------------------------------------------------------------------------*/

void
cs_measures_set_map_values(cs_measures_set_t       *ms,
                           const cs_lnum_t          nb_measures,
                           const int               *is_cressman,
                           const int               *is_interpol,
                           const cs_real_t         *measures_coords,
                           const cs_real_t         *measures,
                           const cs_real_t         *influence_radius);

/*----------------------------------------------------------------------------
 * Add new measures to an existing measures set (already declared and
 * allocated).
 *
 * parameters:
 *   ms               <-- pointer to the existing measures set
 *   nb_measures      <-- number of new measures
 *   is_cressman      <-- for each new measure cressman interpolation is:
 *                          0: not used
 *                          1: used
 *   is_interpol      <-- for the interpolation on mesh, each new measure is
 *                          0: not taken into account
 *                          1: taken into account
 *   measures_coords  <-- new measures spaces coordonates
 *   measures         <-- new measures values (associated to coordonates)
 *   influence_radius <-- influence radius for interpolation (xyz interleaved)
 *----------------------------------------------------------------------------*/

void
cs_measures_set_add_values(cs_measures_set_t       *ms,
                           const cs_lnum_t          nb_measures,
                           const int               *is_cressman,
                           const int               *is_interpol,
                           const cs_real_t         *measures_coords,
                           const cs_real_t         *measures,
                           const cs_real_t         *influence_radius);

/*----------------------------------------------------------------------------
 * Compute a Cressman interpolation on the global mesh.
 *
 * parameters:
 *   ms                   <-- pointer to the measures set structure
 *                            (values to interpolate)
 *   interpolated_values  --> interpolated values on the global mesh
 *                            (size = n_cells or nb_faces)
 *   id_type              <-- parameter:
 *                              1: interpolation on volumes
 *                              2: interpolation on boundary faces
 *----------------------------------------------------------------------------*/

void
cs_cressman_interpol(cs_measures_set_t         *ms,
                     cs_real_t                 *interpolated_values,
                     int                        id_type);

/*----------------------------------------------------------------------------
 * Return a pointer to a measures set based on its id.
 *
 * This function requires that a measures set of the given id is defined.
 *
 * parameters:
 *  id <-- measures set id
 *
 * return:
 *   pointer to the measures set structure
 *
 *----------------------------------------------------------------------------*/

cs_measures_set_t  *
cs_measures_set_by_id(int  id);

/*----------------------------------------------------------------------------
 * Return a pointer to a grid based on its id.
 *
 * This function requires that a grid of the given id is defined.
 *
 * parameters:
 *  id <-- grid id
 *
 * return:
 *   pointer to the grid structure
 *
 *----------------------------------------------------------------------------*/

cs_interpol_grid_t  *
cs_interpol_grid_by_id(int  id);

/*----------------------------------------------------------------------------
 * Return a pointer to a measure set based on its name.
 *
 * This function requires that a measure set of the given name is defined.
 *
 * parameters:
 * name <-- measure set name
 *
 * return:
 *   pointer to the measures set structure
 *----------------------------------------------------------------------------*/

cs_measures_set_t  *
cs_measures_set_by_name(const char  *name);

/*----------------------------------------------------------------------------
 * Return a pointer to a grid based on its name.
 *
 * This function requires that a grid of the given name is defined.
 *
 * parameters:
 * name <-- grid name
 *
 * return:
 *   pointer to the grid structure
 *----------------------------------------------------------------------------*/

cs_interpol_grid_t  *
cs_interpol_grid_by_name(const char  *name);

/*----------------------------------------------------------------------------
 * Destroy all defined measures sets.
 *----------------------------------------------------------------------------*/

void
cs_measures_sets_destroy(void);

/*----------------------------------------------------------------------------
 * Destroy all defined grids.
 *----------------------------------------------------------------------------*/

void
cs_interpol_grids_destroy(void);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* __CS_MEASURES_H__ */
