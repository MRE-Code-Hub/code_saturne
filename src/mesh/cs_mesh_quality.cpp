/*============================================================================
 * Compute several mesh quality criteria.
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
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_error.h"
#include "bft/bft_printf.h"

#include "alge/cs_blas.h"
#include "base/cs_interface.h"
#include "base/cs_math.h"
#include "base/cs_mem.h"
#include "mesh/cs_mesh.h"
#include "mesh/cs_mesh_quantities.h"
#include "base/cs_post.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "mesh/cs_mesh_quality.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

#define CS_MESH_QUALITY_N_SUBS  10

#undef _CROSS_PRODUCT_3D
#undef _DOT_PRODUCT_3D
#undef _MODULE_3D
#undef _COSINE_3D

#define _CROSS_PRODUCT_3D(cross_v1_v2, v1, v2) ( \
 cross_v1_v2[0] = v1[1]*v2[2] - v1[2]*v2[1],   \
 cross_v1_v2[1] = v1[2]*v2[0] - v1[0]*v2[2],   \
 cross_v1_v2[2] = v1[0]*v2[1] - v1[1]*v2[0]  )

#define _DOT_PRODUCT_3D(v1, v2) ( \
 v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2])

#define _MODULE_3D(v) \
 sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])

#define _COSINE_3D(v1, v2) (\
 _DOT_PRODUCT_3D(v1, v2) / (_MODULE_3D(v1) * _MODULE_3D(v2)) )

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Compute the minimum and the maximum of a vector (locally).
 *
 * parameters:
 *   n_vals    <-- local number of elements
 *   var       <-- pointer to vector
 *   min       --> minimum
 *   max       --> maximum
 *----------------------------------------------------------------------------*/

static void
_compute_local_minmax(cs_lnum_t        n_vals,
                      const cs_real_t  var[],
                      cs_real_t       *min,
                      cs_real_t       *max)
{
  cs_lnum_t  i;
  cs_real_t  _min = DBL_MAX, _max = -DBL_MAX;

  for (i = 0; i < n_vals; i++) {
    _min = cs::min(_min, var[i]);
    _max = cs::max(_max, var[i]);
  }

  *min = _min;
  *max = _max;
}

/*----------------------------------------------------------------------------
 * Display the distribution of values of a real vector.
 *
 * parameters:
 *   n_steps <-- number of histogram steps
 *   var_min <-- minimum variable value
 *   var_max <-- maximum variable value
 *   count   <-> count for each histogram slice (size: n_steps)
 *               local values in, global values out
 *----------------------------------------------------------------------------*/

static void
_display_histograms(int        n_steps,
                    cs_real_t  var_min,
                    cs_real_t  var_max,
                    cs_gnum_t  count[])
{
  int  i, j;
  double var_step;

#if defined(HAVE_MPI)

  if (cs_glob_n_ranks > 1) {

    cs_gnum_t _g_count[CS_MESH_QUALITY_N_SUBS];
    cs_gnum_t *g_count = _g_count;

    if (n_steps > CS_MESH_QUALITY_N_SUBS)
      CS_MALLOC(g_count, n_steps, cs_gnum_t);

    MPI_Allreduce(count, g_count, n_steps, CS_MPI_GNUM, MPI_SUM,
                  cs_glob_mpi_comm);

    for (i = 0; i < n_steps; i++)
      count[i] = g_count[i];

    if (n_steps > CS_MESH_QUALITY_N_SUBS)
      CS_FREE(g_count);

  }

#endif

  /* Print base min, max, and increment */

  bft_printf(_("    minimum value =         %10.5e\n"), (double)var_min);
  bft_printf(_("    maximum value =         %10.5e\n\n"), (double)var_max);

  var_step = cs::abs(var_max - var_min) / n_steps;

  if (cs::abs(var_max - var_min) > 0.) {

    /* Number of elements in each subdivision */

    for (i = 0, j = 1; i < n_steps - 1; i++, j++)
      bft_printf("    %3d : [ %10.5e ; %10.5e [ = %10llu\n",
                 i+1, var_min + i*var_step, var_min + j*var_step,
                 (unsigned long long)(count[i]));

    bft_printf("    %3d : [ %10.5e ; %10.5e ] = %10llu\n",
               n_steps,
               var_min + (n_steps - 1)*var_step,
               var_max,
               (unsigned long long)(count[n_steps - 1]));

  }

}

/*----------------------------------------------------------------------------
 * Display the distribution of values of a real vector on cells or
 * boundary faces.
 *
 * parameters:
 *   n_vals     <-- number of values
 *   var        <-- pointer to vector (size: n_vals)
 *----------------------------------------------------------------------------*/

static void
_histogram(cs_lnum_t        n_vals,
           const cs_real_t  var[])
{
  cs_lnum_t  i;
  int        j, k;

  cs_real_t  step;
  cs_real_t  max, min, _max, _min;

  cs_gnum_t count[CS_MESH_QUALITY_N_SUBS];
  const int  n_steps = CS_MESH_QUALITY_N_SUBS;

  assert (sizeof(double) == sizeof(cs_real_t));

  /* Compute global min and max */

  _compute_local_minmax(n_vals, var, &_min, &_max);

  /* Default initialization */

  min = _min;
  max = _max;

#if defined(HAVE_MPI)

  if (cs_glob_n_ranks > 1) {
    MPI_Allreduce(&_min, &min, 1, CS_MPI_REAL, MPI_MIN,
                  cs_glob_mpi_comm);

    MPI_Allreduce(&_max, &max, 1, CS_MPI_REAL, MPI_MAX,
                  cs_glob_mpi_comm);
  }

#endif

  /* Define axis subdivisions */

  for (j = 0; j < n_steps; j++)
    count[j] = 0;

  if (cs::abs(max - min) > 0.) {

    step = cs::abs(max - min) / n_steps;

    /* Loop on values */

    for (i = 0; i < n_vals; i++) {

      /* Associated subdivision */

      for (j = 0, k = 1; k < n_steps; j++, k++) {
        if (var[i] < min + k*step)
          break;
      }
      count[j] += 1;

    }

  }

  _display_histograms(n_steps, min, max, count);
}

/*----------------------------------------------------------------------------
 * Display the distribution of values of a real vector on interior faces.
 *
 * parameters:
 *   mesh <-- pointer to mesh structure
 *   var  <-- pointer to vector (size: mesh->n_i_faces)
 *----------------------------------------------------------------------------*/

static void
_int_face_histogram(const cs_mesh_t  *mesh,
                    const cs_real_t   var[])
{
  cs_lnum_t  i;
  int        j, k;

  cs_real_t  step;
  cs_real_t  max, min, _max, _min;

  cs_gnum_t count[CS_MESH_QUALITY_N_SUBS];
  const int  n_steps = CS_MESH_QUALITY_N_SUBS;

  assert (sizeof (double) == sizeof (cs_real_t));

  /* Compute global min and max */

  _compute_local_minmax(mesh->n_i_faces, var, &_min, &_max);

  /* Default initialization */

  min = _min;
  max = _max;

#if defined(HAVE_MPI)

  if (cs_glob_n_ranks > 1) {
    MPI_Allreduce(&_min, &min, 1, CS_MPI_REAL, MPI_MIN,
                  cs_glob_mpi_comm);

    MPI_Allreduce(&_max, &max, 1, CS_MPI_REAL, MPI_MAX,
                  cs_glob_mpi_comm);
  }

#endif

  /* Define axis subdivisions */

  for (j = 0; j < n_steps; j++)
    count[j] = 0;

  if (cs::abs(max - min) > 0.) {

    step = cs::abs(max - min) / n_steps;

    /* Loop on faces */

    for (i = 0; i < mesh->n_i_faces; i++) {

      if (mesh->i_face_cells[i][0] >= mesh->n_cells)
        continue;

      /* Associated subdivision */

      for (j = 0, k = 1; k < n_steps; j++, k++) {
        if (var[i] < min + k*step)
          break;
      }
      count[j] += 1;

    }

  }

  _display_histograms(n_steps, min, max, count);
}

/*----------------------------------------------------------------------------
 * Compute weighting coefficient and center offsetting coefficient
 * for internal faces.
 *
 * parameters:
 *   mesh             <-- pointer to mesh structure.
 *   mesh_quantities  <-- pointer to mesh quantities structures.
 *   weighting        <-> array for weigthing coefficient.
 *   offsetting       <-> array for offsetting coefficient.
 *----------------------------------------------------------------------------*/

static void
_compute_weighting_offsetting(const cs_mesh_t             *mesh,
                              const cs_mesh_quantities_t  *mesh_quantities,
                              cs_real_t                    weighting[],
                              cs_real_t                    offsetting[])
{
  cs_lnum_t  i, face_id, cell1, cell2;
  cs_real_t  cell_center1[3], cell_center2[3];
  cs_real_t  face_center[3], face_normal[3];
  cs_real_t  v0[3], v1[3], v2[3];

  double  coef0 = 0, coef1 = 0, coef2 = 0;

  const cs_lnum_t  dim = mesh->dim;

  /* Compute weighting coefficient */
  /*-------------------------------*/

  /* Loop on internal faces */

  for (face_id = 0; face_id < mesh->n_i_faces; face_id++) {

    /* Get local number of the cells in contact with the face */

    cell1 = mesh->i_face_cells[face_id][0];
    cell2 = mesh->i_face_cells[face_id][1];

    /* Get information on mesh quantities */

    for (i = 0; i < dim; i++) {

      /* Center of gravity for each cell */

      cell_center1[i] = mesh_quantities->cell_cen[cell1][i];
      cell_center2[i] = mesh_quantities->cell_cen[cell2][i];

      /* Face center coordinates */

      face_center[i] = mesh_quantities->i_face_cog[face_id][i];

      /* Surface vector (orthogonal to the face) */

      face_normal[i] = mesh_quantities->i_face_normal[face_id*dim + i];

    }

    /* Compute weighting coefficient with two approaches. Keep the max value. */

    for (i = 0; i < dim; i++) {

      v0[i] = cell_center2[i] - cell_center1[i];
      v1[i] = face_center[i] - cell_center1[i];
      v2[i] = cell_center2[i] - face_center[i];

    }

    coef0 = _DOT_PRODUCT_3D(v0, face_normal);
    coef1 = _DOT_PRODUCT_3D(v1, face_normal)/coef0;
    coef2 = _DOT_PRODUCT_3D(v2, face_normal)/coef0;

    weighting[face_id] = cs::max(coef1, coef2);

    /* Compute center offsetting coefficient */
    /*---------------------------------------*/

    for (i = 0; i < dim; i++) {
      v1[i] = mesh_quantities->dofij[face_id][i];
      v2[i] = mesh_quantities->i_face_normal[face_id*3 + i];
    }
    double of_s = _MODULE_3D(v1) * _MODULE_3D(v2);

    offsetting[cell1] = cs::min(offsetting[cell1],
        1. - pow(of_s / fabs(mesh_quantities->cell_vol[cell1]), 1./3.));
    offsetting[cell2] = cs::min(offsetting[cell2],
        1. - pow(of_s / fabs(mesh_quantities->cell_vol[cell2]), 1./3.));

  } /* End of loop on faces */

}

/*----------------------------------------------------------------------------
 * Compute angle between face normal and segment based on centers of the
 * adjacent cells. Evaluates a level of non-orthogonality.
 *
 * parameters:
 *   mesh             <-- pointer to mesh structure.
 *   mesh_quantities  <-- pointer to mesh quantities structures.
 *   i_face_ortho     <-> array for internal faces.
 *   b_face_ortho     <-> array for border faces.
 *----------------------------------------------------------------------------*/

static void
_compute_orthogonality(const cs_mesh_t             *mesh,
                       const cs_mesh_quantities_t  *mesh_quantities,
                       cs_real_t                    i_face_ortho[],
                       cs_real_t                    b_face_ortho[])
{
  cs_lnum_t  i, face_id, cell1, cell2;
  double  cos_alpha;
  cs_real_t  cell_center1[3], cell_center2[3];
  cs_real_t  face_center[3];
  cs_real_t  face_normal[3], vect[3];

  const double  rad_to_deg = 180. / acos(-1.);
  const cs_lnum_t  dim = mesh->dim;

  /* Loop on internal faces */
  /*------------------------*/

  for (face_id = 0; face_id < mesh->n_i_faces; face_id++) {

    /* Get local number of the cells beside the face */

    cell1 = mesh->i_face_cells[face_id][0];
    cell2 = mesh->i_face_cells[face_id][1];

    /* Get information on mesh quantities */

    for (i = 0; i < dim; i++) {

      /* Center of gravity for each cell */

      cell_center1[i] = mesh_quantities->cell_cen[cell1][i];
      cell_center2[i] = mesh_quantities->cell_cen[cell2][i];

      /* Surface vector (orthogonal to the face) */

      face_normal[i] = mesh_quantities->i_face_normal[face_id*dim + i];

    }

    /* Compute angle which evaluates the non-orthogonality. */

    for (i = 0; i < dim; i++)
      vect[i] = cell_center2[i] - cell_center1[i];

    cos_alpha = _COSINE_3D(vect, face_normal);
    cos_alpha = cs::abs(cos_alpha);
    cos_alpha = cs::min(cos_alpha, 1);

    if (cos_alpha < 1.)
      i_face_ortho[face_id] = acos(cos_alpha) * rad_to_deg;
    else
      i_face_ortho[face_id] = 0.;

  } /* End of loop on internal faces */

  /* Loop on border faces */
  /*----------------------*/

  for (face_id = 0; face_id < mesh->n_b_faces; face_id++) {

    /* Get local number of the cell beside the face */

    cell1 = mesh->b_face_cells[face_id];

    /* Get information on mesh quantities */

    for (i = 0; i < dim; i++) {

      /* Center of gravity of the cell */

      cell_center1[i] = mesh_quantities->cell_cen[cell1][i];

      /* Face center coordinates */

      face_center[i] = mesh_quantities->b_face_cog[face_id][i];

      /* Surface vector (orthogonal to the face) */

      face_normal[i] = mesh_quantities->b_face_normal[face_id*dim + i];

    }

    /* Compute alpha: angle wich evaluate the difference with orthogonality. */

    for (i = 0; i < dim; i++)
      vect[i] = face_center[i] - cell_center1[i];

    cos_alpha = _COSINE_3D(vect, face_normal);
    cos_alpha = cs::abs(cos_alpha);
    cos_alpha = cs::min(cos_alpha, 1);

    if (cos_alpha < 1.)
      b_face_ortho[face_id] = acos(cos_alpha) * rad_to_deg;
    else
      b_face_ortho[face_id] = 0.;

  } /* End of loop on border faces */

}

/*----------------------------------------------------------------------------
 * Evaluate face warping angle.
 *
 * parameters:
 *   idx_start       <-- first vertex index
 *   idx_end         <-- last vertex index
 *   face_vertex_id  <-- face -> vertices connectivity
 *   face_normal     <-- face normal
 *   vertex_coords   <-- vertices coordinates
 *   face_warping    --> face warping angle
 *----------------------------------------------------------------------------*/

static void
_get_face_warping(cs_lnum_t        idx_start,
                  cs_lnum_t        idx_end,
                  const cs_real_t  face_normal[],
                  const cs_lnum_t  face_vertex_id[],
                  const cs_real_t  vertex_coords[],
                  double           face_warping[])
{
  cs_lnum_t  i, idx, vertex_id1, vertex_id2;
  double  edge_cos_alpha;
  cs_real_t  vect[3];

  double  cos_alpha = 0.;

  const int  dim = 3;
  const double  rad_to_deg = 180. / acos(-1.);

  /* Loop on edges */

  for (idx = idx_start; idx < idx_end - 1; idx++) {

    vertex_id1 = face_vertex_id[idx];
    vertex_id2 = face_vertex_id[idx + 1];

    /* Get vertex coordinates */

    for (i = 0; i < dim; i++)
      vect[i] =  vertex_coords[vertex_id2*dim + i]
               - vertex_coords[vertex_id1*dim + i];

    edge_cos_alpha = _COSINE_3D(vect, face_normal);
    edge_cos_alpha = cs::abs(edge_cos_alpha);
    cos_alpha = cs::max(cos_alpha, edge_cos_alpha);

  }

  /* Last edge */

  vertex_id1 = face_vertex_id[idx_end - 1];
  vertex_id2 = face_vertex_id[idx_start];

  /* Get vertex coordinates */

  for (i = 0; i < dim; i++)
    vect[i] =  vertex_coords[vertex_id2*dim + i]
             - vertex_coords[vertex_id1*dim + i];

  edge_cos_alpha = _COSINE_3D(vect, face_normal);
  edge_cos_alpha = cs::abs(edge_cos_alpha);
  cos_alpha = cs::max(cos_alpha, edge_cos_alpha);
  cos_alpha = cs::min(cos_alpha, 1.);

  *face_warping = 90. - acos(cos_alpha) * rad_to_deg;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define the cell -> faces connectivity
 *
 * \param[in]  mesh        pointer to a cs_mesh_t structure
 * \param[in]  p_c2f_idx   pointer to the array of indexes
 * \param[in]  p_c2f_lst   pointer to the list of face ids
 */
/*----------------------------------------------------------------------------*/

static void
_build_c2f(const cs_mesh_t   *mesh,
           cs_lnum_t         *p_c2f_idx[],
           cs_lnum_t         *p_c2f_ids[])
{
  int  idx_size = 0;
  int  *cell_shift = nullptr;
  cs_lnum_t  *c2f_idx = nullptr;
  cs_lnum_t  *c2f_ids = nullptr;

  const int  n_cells = mesh->n_cells;
  const int  n_i_faces = mesh->n_i_faces;
  const int  n_b_faces = mesh->n_b_faces;

  CS_MALLOC(c2f_idx, n_cells + 1, cs_lnum_t);
  CS_MALLOC(cell_shift, n_cells, int);

# pragma omp parallel for if (n_cells > CS_THR_MIN)
  for (cs_lnum_t i = 0; i < n_cells; i++)
    cell_shift[i] = c2f_idx[i] = 0;
  c2f_idx[n_cells] = 0;

  for (cs_lnum_t i = 0; i < n_b_faces; i++) {
    c2f_idx[mesh->b_face_cells[i]+1] += 1;
    idx_size += 1;
  }

  for (cs_lnum_t i = 0; i < n_i_faces; i++) {

    const int  c1_id = mesh->i_face_cells[i][0];
    const int  c2_id = mesh->i_face_cells[i][1];

    if (c1_id < n_cells) // cell owned by the local rank
      c2f_idx[c1_id+1] += 1, idx_size += 1;
    if (c2_id < n_cells) // cell owned by the local rank
      c2f_idx[c2_id+1] += 1, idx_size += 1;

  }

  for (cs_lnum_t i = 0; i < n_cells; i++)
    c2f_idx[i+1] += c2f_idx[i];

  assert(c2f_idx[n_cells] == idx_size);

  CS_MALLOC(c2f_ids, idx_size, cs_lnum_t);

  for (cs_lnum_t f_id = 0; f_id < n_i_faces; f_id++) {

    const cs_lnum_t  c1_id = mesh->i_face_cells[f_id][0];
    const cs_lnum_t  c2_id = mesh->i_face_cells[f_id][1];

    if (c1_id < n_cells) { /* Don't want ghost cells */

      const cs_lnum_t  shift = c2f_idx[c1_id] + cell_shift[c1_id];
      c2f_ids[shift] = f_id;
      cell_shift[c1_id] += 1;

    }

    if (c2_id < n_cells) { /* Don't want ghost cells */

      const cs_lnum_t  shift = c2f_idx[c2_id] + cell_shift[c2_id];
      c2f_ids[shift] = f_id;
      cell_shift[c2_id] += 1;

    }

  } /* End of loop on internal faces */

  for (cs_lnum_t  f_id = 0; f_id < n_b_faces; f_id++) {

    const cs_lnum_t  c_id = mesh->b_face_cells[f_id];
    const cs_lnum_t  shift = c2f_idx[c_id] + cell_shift[c_id];

    c2f_ids[shift] = n_i_faces + f_id;
    cell_shift[c_id] += 1;

  } /* End of loop on border faces */

  /* Free memory */
  CS_FREE(cell_shift);

  /* Return pointers */
  *p_c2f_idx = c2f_idx;
  *p_c2f_ids = c2f_ids;
}

/*----------------------------------------------------------------------------
 * Compute cellwise the warping error
 *  Id = 1/|c| \sum_(f \in F_c) x_f \otimes \vect{f}
 * Froebinus norm is used to get a scalar-valued quantity
 *
 * parameters:
 *   mesh             <-- pointer to mesh structure.
 *   mesh_quantities  <-- pointer to mesh quantities structures.
 *   warp_error       <-> array of values to compute
 *----------------------------------------------------------------------------*/

static void
_compute_warp_error(const cs_mesh_t              *mesh,
                    const cs_mesh_quantities_t   *mesh_quantities,
                    cs_real_t                     warp_error[])
{
  const cs_real_t  *vol = mesh_quantities->cell_vol;

  cs_lnum_t  *c2f_ids = nullptr;
  cs_lnum_t  *c2f_idx = nullptr;

  /* Build cell -> face connectivity */
  _build_c2f(mesh, &c2f_idx, &c2f_ids);

  for (cs_lnum_t c_id = 0; c_id < mesh->n_cells; c_id++) {

    const cs_real_t  invvol_c = 1/vol[c_id];
    const cs_real_t  *xc = mesh_quantities->cell_cen[c_id];

    cs_real_33_t   tens = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };

    for (cs_lnum_t i = c2f_idx[c_id]; i < c2f_idx[c_id+1]; i++) {

      cs_lnum_t  f_id = c2f_ids[i];
      cs_real_t  *xf = nullptr, *surf = nullptr;
      int  sgn = 1;

      if (f_id < mesh->n_i_faces) {

        const cs_lnum_t  c2_id = mesh->i_face_cells[f_id][1];
        if (c_id == c2_id)
          sgn = -1;

        xf = mesh_quantities->i_face_cog[f_id];
        surf = mesh_quantities->i_face_normal + 3*f_id;

      }
      else {

        f_id -= mesh->n_i_faces; // Border face
        xf = mesh_quantities->b_face_cog[f_id];
        surf = mesh_quantities->b_face_normal + 3*f_id;

      }

      for (int ki = 0; ki < 3; ki++)
        for (int kj = 0; kj < 3; kj++)
          tens[ki][kj] += sgn*(xf[ki] - xc[ki]) * surf[kj];

    } // Loop on face cells

    for (int ki = 0; ki < 3; ki++)
      for (int kj = 0; kj < 3; kj++)
        tens[ki][kj] *= invvol_c;

    warp_error[c_id] =
      fabs(cs_math_33_determinant((const cs_real_t (*)[3])tens) - 1.);

  } // Loop on cells

  /* Free memory */
  CS_FREE(c2f_ids);
  CS_FREE(c2f_idx);
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Evaluate face warping angle for internal and border faces..
 *
 * parameters:
 *   mesh             <-- pointer to a cs_mesh_t structure
 *   i_face_normal    <-- internal face normal
 *   b_face_normal    <-- border face normal
 *   i_face_warping   --> face warping angle for internal faces
 *   b_face_warping   --> face warping angle for border faces
 *
 * Returns:
 *----------------------------------------------------------------------------*/

void
cs_mesh_quality_compute_warping(const cs_mesh_t    *mesh,
                                const cs_real_t     i_face_normal[],
                                const cs_real_t     b_face_normal[],
                                cs_real_t           i_face_warping[],
                                cs_real_t           b_face_warping[])
{
  cs_lnum_t  i, face_id, idx_start, idx_end;
  cs_real_t  this_face_normal[3];

  const cs_lnum_t  dim = mesh->dim;
  const cs_lnum_t  *i_face_vtx_idx = mesh->i_face_vtx_idx;

  assert(dim == 3);

  /* Compute warping for internal faces */
  /*------------------------------------*/

  for (face_id = 0; face_id < mesh->n_i_faces; face_id++) {

    /* Get normal to the face */

    for (i = 0; i < dim; i++)
      this_face_normal[i] = i_face_normal[face_id*dim + i];

    /* Evaluate warping for each edge of face. Keep the max. */

    idx_start = i_face_vtx_idx[face_id];
    idx_end = i_face_vtx_idx[face_id + 1];

    _get_face_warping(idx_start,
                      idx_end,
                      this_face_normal,
                      mesh->i_face_vtx_lst,
                      mesh->vtx_coord,
                      &(i_face_warping[face_id]));

  } /* End of loop on internal faces */

  /* Compute warping for border faces */
  /*----------------------------------*/

  cs_mesh_quality_compute_b_face_warping(mesh,
                                         b_face_normal,
                                         b_face_warping);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Evaluate face warping angle for boundary faces..
 *
 * \param[in]   mesh            pointer to a cs_mesh_t structure
 * \param[in]   b_face_normal   boundary face normal
 * \param[out]  b_face_warping  face warping angle for boundary faces
 */
/*----------------------------------------------------------------------------*/

void
cs_mesh_quality_compute_b_face_warping(const cs_mesh_t  *mesh,
                                       const cs_real_t   b_face_normal[],
                                       cs_real_t         b_face_warping[])
{
  const cs_lnum_t  dim = mesh->dim;
  const cs_lnum_t  *b_face_vtx_idx = mesh->b_face_vtx_idx;

  assert(dim == 3);

  for (cs_lnum_t face_id = 0; face_id < mesh->n_b_faces; face_id++) {

    /* Evaluate warping for each edge */

    cs_lnum_t idx_start = b_face_vtx_idx[face_id];
    cs_lnum_t idx_end = b_face_vtx_idx[face_id + 1];

    _get_face_warping(idx_start,
                      idx_end,
                      b_face_normal + (3*face_id),
                      mesh->b_face_vtx_lst,
                      mesh->vtx_coord,
                      &(b_face_warping[face_id]));

  }
}

/*----------------------------------------------------------------------------
 * Compute mesh quality indicators
 *
 * parameters:
 *   mesh             <-- pointer to mesh structure.
 *   mesh_quantities  <-- pointer to mesh quantities structure.
 *----------------------------------------------------------------------------*/

void
cs_mesh_quality(const cs_mesh_t             *mesh,
                const cs_mesh_quantities_t  *mesh_quantities)
{
  cs_lnum_t  i;

  bool  compute_volume = true;
  bool  compute_weighting = true;
  bool  compute_orthogonality = true;
  bool  compute_warping = true;
  bool  compute_thickness = true;
  bool  compute_warp_error = true;

  double  *working_array = nullptr;

  const cs_lnum_t  n_i_faces = mesh->n_i_faces;
  const cs_lnum_t  n_b_faces = mesh->n_b_faces;
  const cs_lnum_t  n_cells = mesh->n_cells;
  const cs_lnum_t  n_cells_wghosts = mesh->n_cells_with_ghosts;

  const cs_time_step_t *ts = cs_glob_time_step;

  /* Check input data */

  assert(mesh_quantities->i_face_normal != nullptr || mesh->n_i_faces == 0);
  assert(mesh_quantities->i_face_cog != nullptr || mesh->n_i_faces == 0);
  assert(mesh_quantities->cell_cen != nullptr);
  assert(mesh_quantities->cell_vol != nullptr);

  /* Assume resulting fields will be exported to postprocessing meshes */

  /* TODO
     For the moment, we export the mesh at this stage; this should be moved
     once PSTEMA has been moved from CALTRI to an earlier step. */

  cs_post_activate_writer(0, true);

  bool post_fields = false;

  if (cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_SURFACE, 0) != 0)
    post_fields = true;

  if (cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_BOUNDARY, 0) != 0)
    post_fields = true;

  if (post_fields)
    cs_post_write_meshes(ts);

  /* Evaluate mesh quality criteria */
  /*--------------------------------*/

  /*--------------*/
  /* Face warping */
  /*--------------*/

  if (compute_warping == true) {

    double  *i_face_warping = nullptr, *b_face_warping = nullptr;

    CS_MALLOC(working_array, n_i_faces + n_b_faces, double);

    for (i = 0; i < n_i_faces + n_b_faces; i++)
      working_array[i] = 0.;

    i_face_warping = working_array;
    b_face_warping = working_array + n_i_faces;

    cs_mesh_quality_compute_warping(mesh,
                                    mesh_quantities->i_face_normal,
                                    mesh_quantities->b_face_normal,
                                    i_face_warping,
                                    b_face_warping);

    /* Display histograms */

    if (mesh->n_g_i_faces > 0) {
      bft_printf(_("\n  Histogram of the interior faces warping:\n\n"));
      _int_face_histogram(mesh, i_face_warping);
    }

    if (mesh->n_g_b_faces > 0) {
      bft_printf(_("\n  Histogram of the boundary faces warping:\n\n"));
      _histogram(n_b_faces, b_face_warping);
    }

    /* Post processing */

    int mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_SURFACE, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Face_Warp",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        nullptr,
                        i_face_warping,
                        b_face_warping,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_SURFACE,
                                                   mesh_id);
    }

    mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_BOUNDARY, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Face_Warp",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        nullptr,
                        nullptr,
                        b_face_warping,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_BOUNDARY,
                                                   mesh_id);
    }

    CS_FREE(working_array);

  } /* End of face warping treatment */

  /*----------------------------------------------*/
  /* Weighting and center offsetting coefficients */
  /*----------------------------------------------*/

  if (compute_weighting == true) {

    double  *weighting = nullptr, *offsetting = nullptr;

    /* Only defined on internal faces */

    CS_MALLOC(working_array, n_i_faces + n_cells_wghosts, double);

    weighting = working_array;
    offsetting = working_array + n_i_faces;

    for (i = 0; i < n_cells_wghosts; i++)
      offsetting[i] = 1.;

    _compute_weighting_offsetting(mesh,
                                  mesh_quantities,
                                  weighting,
                                  offsetting);

    /* Display histograms */

    if (mesh->n_g_i_faces > 0) {
      bft_printf(_("\n  Histogram of the interior faces "
                   "weighting coefficient:\n\n"));
      _int_face_histogram(mesh, weighting);
    }

    bft_printf(_("\n  Histogram of the cells "
                 "off-centering coefficient:\n\n"));
    _histogram(n_cells, offsetting);

    /* Post processing */

    int mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_SURFACE, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Weighting coefficient",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        nullptr,
                        weighting,
                        nullptr,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_SURFACE, mesh_id);
    }

    mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_VOLUME, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Offset",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        offsetting,
                        nullptr,
                        nullptr,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_VOLUME, mesh_id);
    }

    CS_FREE(working_array);

  } /* End of off-setting and weighting treatment */

  /*---------------------*/
  /* Angle orthogonality */
  /*---------------------*/

  if (compute_orthogonality == true) {

    double  *i_face_ortho = nullptr, *b_face_ortho = nullptr;

    CS_MALLOC(working_array, n_i_faces + n_b_faces, double);

    for (i = 0; i < n_i_faces + n_b_faces; i++)
      working_array[i] = 0.;

    i_face_ortho = working_array;
    b_face_ortho = working_array + n_i_faces;

    _compute_orthogonality(mesh,
                           mesh_quantities,
                           i_face_ortho,
                           b_face_ortho);

    /* Display histograms */

    if (mesh->n_g_i_faces > 0) {
      bft_printf(_("\n  Histogram of the interior faces "
                   "non-orthogonality coefficient (in degrees):\n\n"));
      _int_face_histogram(mesh, i_face_ortho);
    }

    if (mesh->n_g_b_faces > 0) {
      bft_printf(_("\n  Histogram of the boundary faces "
                   "non-orthogonality coefficient (in degrees):\n\n"));
      _histogram(n_b_faces, b_face_ortho);
    }

    /* Post processing */

    int mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_SURFACE, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Non_Ortho",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        nullptr,
                        i_face_ortho,
                        b_face_ortho,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_SURFACE,
                                                   mesh_id);
    }

    mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_BOUNDARY, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Non_Ortho",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        nullptr,
                        nullptr,
                        b_face_ortho,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_BOUNDARY,
                                                   mesh_id);
    }

    CS_FREE(working_array);

  } /* End of non-orthogonality treatment */

  /*-------------*/
  /* Cell volume */
  /*-------------*/

  if (compute_volume == true) {

    /* Display histograms */

    bft_printf(_("\n  Histogram of cell volumes:\n\n"));
    _histogram(n_cells, mesh_quantities->cell_vol);

    /* Post processing */

    int mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_VOLUME, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Cell_Volume",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        mesh_quantities->cell_vol,
                        nullptr,
                        nullptr,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_VOLUME, mesh_id);
    }

  } /* End of cell volume treatment */

  /*-------------------------*/
  /* boundary cell thickness */
  /*-------------------------*/

  if (compute_thickness == true && mesh->n_g_b_faces > 0) {

    cs_real_t *b_thickness;
    CS_MALLOC(b_thickness, mesh->n_b_faces, cs_real_t);

    cs_mesh_quantities_b_thickness_f(mesh, mesh_quantities, 0, b_thickness);

    /* Display histograms */

    bft_printf(_("\n  Histogram of boundary cell thickness:\n\n"));
    _histogram(mesh->n_b_faces, b_thickness);

    /* Post processing */

    int mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_BOUNDARY, 0);

    while (mesh_id != 0) {
      cs_post_write_var(CS_POST_MESH_BOUNDARY,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Boundary_Cell_Thickness",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        nullptr,
                        nullptr,
                        b_thickness,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_BOUNDARY,
                                                   mesh_id);
    }

    CS_FREE(b_thickness);

  } /* End of boundary cell thickness treatment */

  /*---------------*/
  /* warping error */
  /*---------------*/

  if (compute_warp_error == true) {

    cs_real_t  *warp_error = nullptr;
    CS_MALLOC(warp_error, mesh->n_cells ,cs_real_t);

    _compute_warp_error(mesh, mesh_quantities, warp_error);

    /* Display histograms */

    bft_printf(_("\n  Histogram of the cellwise warping error :\n\n"));
    _histogram(n_cells, warp_error);

    /* Post processing */

    int mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_VOLUME, 0);

    while (mesh_id != 0) {
      cs_post_write_var(mesh_id,
                        CS_POST_WRITER_ALL_ASSOCIATED,
                        "Warp_Error",
                        1,
                        false,
                        true,
                        CS_POST_TYPE_cs_real_t,
                        warp_error,
                        nullptr,
                        nullptr,
                        ts);

      mesh_id = cs_post_mesh_find_next_with_cat_id(CS_POST_MESH_VOLUME, mesh_id);
    }

    double  l2_error = cs_gres(mesh->n_cells,
                               mesh_quantities->cell_vol,
                               warp_error,
                               warp_error);

    if (l2_error > 0) /* May be < 0 in case of negative volumes */
      bft_printf(" L2-error norm induced by warping : %5.3e\n", sqrt(l2_error));

    CS_FREE(warp_error);

  } /* End of cell volume treatment */

}

/*----------------------------------------------------------------------------*/

/* Delete local macros */

#undef _CROSS_PRODUCT_3D
#undef _DOT_PRODUCT_3D
#undef _MODULE_3D
#undef _COSINE_3D

/*----------------------------------------------------------------------------*/

END_C_DECLS
