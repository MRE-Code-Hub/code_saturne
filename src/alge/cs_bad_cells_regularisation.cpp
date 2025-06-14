/*============================================================================
 * Regulation on bad cells.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_printf.h"

#include "alge/cs_blas.h"
#include "base/cs_boundary_conditions.h"
#include "base/cs_halo.h"
#include "base/cs_halo_perio.h"
#include "base/cs_math.h"
#include "base/cs_mem.h"
#include "mesh/cs_mesh.h"
#include "mesh/cs_mesh_bad_cells.h"
#include "mesh/cs_mesh_quantities.h"
#include "base/cs_parall.h"
#include "base/cs_parameters.h"
#include "alge/cs_sles_default.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "alge/cs_bad_cells_regularisation.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Regularisation on bad cells for scalars
 *
 * \param[in, out]  var  variable to regularize.
 */
/*----------------------------------------------------------------------------*/

void
cs_bad_cells_regularisation_scalar(cs_real_t *var)
{
  const cs_mesh_t *mesh = cs_glob_mesh;
  cs_mesh_quantities_t *mq = cs_glob_mesh_quantities;

  if (!(cs_glob_mesh_quantities_flag & CS_BAD_CELLS_REGULARISATION))
    return;

  cs_lnum_t n_cells_ext = mesh->n_cells_with_ghosts;
  cs_lnum_t n_cells = mesh->n_cells;
  cs_lnum_t n_i_faces = mesh->n_i_faces;
  const cs_lnum_2_t *i_face_cells = mesh->i_face_cells;

  const cs_real_t *surfn = mq->i_face_surf;
  auto *dist = mq->i_dist;
  auto *volume  = mq->cell_vol;

  cs_real_t *xam, *dam, *rhs;

  double varmin = 1.e20;
  double varmax =-1.e20;

  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++)
    if (!(mq->bad_cell_flag[cell_id] & CS_BAD_CELL_TO_REGULARIZE)) {
      varmin = cs::min(varmin, var[cell_id]);
      varmax = cs::max(varmax, var[cell_id]);
    }

  cs_parall_min(1, CS_DOUBLE, &varmin);
  cs_parall_max(1, CS_DOUBLE, &varmax);

  CS_MALLOC(xam, n_i_faces, cs_real_t);
  CS_MALLOC(dam, n_cells_ext, cs_real_t);
  CS_MALLOC(rhs, n_cells_ext, cs_real_t);

  for (cs_lnum_t cell_id = 0; cell_id < n_cells_ext; cell_id++) {
    dam[cell_id] = 0.;
    rhs[cell_id] = 0.;
  }

  for (cs_lnum_t face_id = 0; face_id < n_i_faces; face_id++) {
    cs_lnum_t cell_id1 = i_face_cells[face_id][0];
    cs_lnum_t cell_id2 = i_face_cells[face_id][1];
    xam[face_id] = 0.;

    double surf = surfn[face_id];
    double vol = 0.5 * (volume[cell_id1] + volume[cell_id2]);
    surf = cs::max(surf, 0.1*vol/dist[face_id]);
    double ssd = surf / dist[face_id];

    dam[cell_id1] += ssd;
    dam[cell_id2] += ssd;

    if (   mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE
        && mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      xam[face_id] = -ssd;
    }
    else if (mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE) {
      rhs[cell_id1] += ssd * var[cell_id2];
      rhs[cell_id2] += ssd * var[cell_id2];
    }
    else if (mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      rhs[cell_id2] += ssd * var[cell_id1];
      rhs[cell_id1] += ssd * var[cell_id1];
    }
    else {
      rhs[cell_id1] += ssd * var[cell_id1];
      rhs[cell_id2] += ssd * var[cell_id2];
    }
  }

  cs_real_t rnorm = sqrt(cs_gdot(n_cells, rhs, rhs));

  /*  Solver residual */
  double ressol = 0.;
  int niterf = 0;

  cs_real_t epsilp = 1.e-12;

  cs_sles_solve_native(-1, /* f_id */
                       "potential_regularisation_scalar",
                       true, /* symmetric */
                       1, /* db_size */
                       1, /* eb_size */
                       (cs_real_t *)dam,
                       xam,
                       epsilp,
                       rnorm,
                       &niterf,
                       &ressol,
                       (cs_real_t *)rhs,
                       (cs_real_t *)var);

  bft_printf("Solving %s: N iter: %d, Res: %12.5e, Norm: %12.5e\n",
                 "potential_regularisation_scalar", niterf, ressol, rnorm);
  //FIXME use less!
  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++) {
    var[cell_id] = cs::min(var[cell_id], varmax);
    var[cell_id] = cs::max(var[cell_id], varmin);
  }

  if (mesh->halo != nullptr)
    cs_halo_sync_var(mesh->halo, CS_HALO_STANDARD, var);

  /* Free solver setup */
  cs_sles_free_native(-1, /* f_id*/
                      "potential_regularisation_scalar");

  /* Free memory */
  CS_FREE(xam);
  CS_FREE(dam);
  CS_FREE(rhs);

  return;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Regularisation on bad cells for vectors
 *
 * \param[in, out]  var  variable to regularize.
 */
/*----------------------------------------------------------------------------*/

void
cs_bad_cells_regularisation_vector(cs_real_3_t  *var,
                                   int           boundary_projection)
{
  const cs_mesh_t *mesh = cs_glob_mesh;
  cs_mesh_quantities_t *mq = cs_glob_mesh_quantities;

  if (!(cs_glob_mesh_quantities_flag & CS_BAD_CELLS_REGULARISATION))
    return;

  cs_lnum_t n_cells_ext = mesh->n_cells_with_ghosts;
  cs_lnum_t n_cells = mesh->n_cells;
  cs_lnum_t n_i_faces = mesh->n_i_faces;
  cs_lnum_t n_b_faces = mesh->n_b_faces;
  const cs_lnum_2_t *i_face_cells = mesh->i_face_cells;
  const cs_lnum_t *b_face_cells = mesh->b_face_cells;

  const cs_real_t *surfn = mq->i_face_surf;
  const cs_real_t *b_face_surf = mq->b_face_surf;
  double *dist = mq->i_dist;
  double *distbr = mq->b_dist;
  double *volume  = mq->cell_vol;

  const cs_nreal_3_t *b_face_u_normal = mq->b_face_u_normal;

  cs_real_33_t *dam;
  cs_real_3_t *rhs;
  cs_real_t *xam;

#if 1
  double varmin[3] = {1.e20, 1.e20, 1.e20};
  double varmax[3] = {-1.e20, -1.e20,-1.e20};

  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++)
    if (!(mq->bad_cell_flag[cell_id] & CS_BAD_CELL_TO_REGULARIZE)) {
      for (int i = 0; i < 3; i++) {
        varmin[i] = cs::min(varmin[i], var[cell_id][i]);
        varmax[i] = cs::max(varmax[i], var[cell_id][i]);
      }
    }

  for (int i = 0; i < 3; i++) {
    cs_parall_min(1, CS_DOUBLE, &varmin[i]);
    cs_parall_max(1, CS_DOUBLE, &varmax[i]);
  }
#endif

  CS_MALLOC(xam, n_i_faces, cs_real_t);
  CS_MALLOC(dam, n_cells_ext, cs_real_33_t);
  CS_MALLOC(rhs, n_cells_ext, cs_real_3_t);

  for (cs_lnum_t cell_id = 0; cell_id < n_cells_ext; cell_id++) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        dam[cell_id][i][j] = 0.;
      }
      rhs[cell_id][i] = 0.;
    }
  }

  for (cs_lnum_t face_id = 0; face_id < n_i_faces; face_id++) {
    cs_lnum_t cell_id1 = i_face_cells[face_id][0];
    cs_lnum_t cell_id2 = i_face_cells[face_id][1];
    xam[face_id] = 0.;

    //FIXME useful?
    double surf = surfn[face_id];
    double vol = 0.5 * (volume[cell_id1] + volume[cell_id2]);
    surf = cs::max(surf, 0.1*vol/dist[face_id]);
    double ssd = surf / dist[face_id];

    for (int i = 0; i < 3; i++) {
      dam[cell_id1][i][i] += ssd;
      dam[cell_id2][i][i] += ssd;
    }

    if (   mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE
        && mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      xam[face_id] = -ssd;
    }
    else if (mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE) {
      for (int i = 0; i < 3; i++) {
        rhs[cell_id1][i] += ssd * var[cell_id2][i];
        rhs[cell_id2][i] += ssd * var[cell_id2][i];
      }
    }
    else if (mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      for (int i = 0; i < 3; i++) {
        rhs[cell_id2][i] += ssd * var[cell_id1][i];
        rhs[cell_id1][i] += ssd * var[cell_id1][i];
      }
    }
    else {
      for (int i = 0; i < 3; i++) {
        rhs[cell_id1][i] += ssd * var[cell_id1][i];
        rhs[cell_id2][i] += ssd * var[cell_id2][i];
      }
    }
  }

  /* Boundary projection... should be consistent with BCs... */
  if (boundary_projection == 1) {
    for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {
      if (cs_glob_bc_type[face_id] == CS_SMOOTHWALL ||
          cs_glob_bc_type[face_id] == CS_ROUGHWALL  ||
          cs_glob_bc_type[face_id] == CS_SYMMETRY     ) {
        cs_lnum_t cell_id = b_face_cells[face_id];
        if (mq->bad_cell_flag[cell_id] & CS_BAD_CELL_TO_REGULARIZE) {
          double ssd = b_face_surf[face_id] / distbr[face_id];
          for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
              double nn =   b_face_u_normal[face_id][i]
                          * b_face_u_normal[face_id][j];
              dam[cell_id][i][j] += ssd * nn;
            }
          }
        }
      }
    }
  }

  cs_real_t rnorm = sqrt(cs_gdot(3*n_cells,
                                 (const cs_real_t *)rhs,
                                 (const cs_real_t *)rhs));

  /*  Solver residual */
  double ressol = 0.;
  int niterf = 0;

  cs_real_t epsilp = 1.e-12;

  cs_sles_solve_native(-1, /* f_id */
                       "potential_regularisation_vector",
                       true, /* symmetric */
                       3, /* db_size */
                       1, /* eb_size */
                       (cs_real_t *)dam,
                       xam,
                       epsilp,
                       rnorm,
                       &niterf,
                       &ressol,
                       (cs_real_t *)rhs,
                       (cs_real_t *)var);

  bft_printf("Solving %s: N iter: %d, Res: %12.5e, Norm: %12.5e\n",
                 "potential_regularisation_vector", niterf, ressol, rnorm);

  //FIXME useless clipping? the matrix is min/max preserving..
#if 1
  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++) {
    for (int i = 0; i < 3; i++) {
      var[cell_id][i] = cs::min(var[cell_id][i], varmax[i]);
      var[cell_id][i] = cs::max(var[cell_id][i], varmin[i]);
    }
  }
#endif

  if (mesh->halo != nullptr) {
    cs_halo_sync_var_strided(mesh->halo, CS_HALO_STANDARD, (cs_real_t *)var, 3);

    if (mesh->n_init_perio > 0)
      cs_halo_perio_sync_var_vect(mesh->halo,
                                  CS_HALO_STANDARD,
                                  (cs_real_t *)var,
                                  3);
  }

  /* Free solver setup */
  cs_sles_free_native(-1, /* f_id*/
                      "potential_regularisation_vector");

  /* Free memory */
  CS_FREE(xam);
  CS_FREE(dam);
  CS_FREE(rhs);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Regularisation on bad cells for symmetric tensors.
 *
 * \param[in, out]  var  variable to regularize.
 */
/*----------------------------------------------------------------------------*/

void
cs_bad_cells_regularisation_sym_tensor(cs_real_6_t  *var,
                                       int           boundary_projection)
{
  const cs_mesh_t *mesh = cs_glob_mesh;
  cs_mesh_quantities_t *mq = cs_glob_mesh_quantities;

  if (!(cs_glob_mesh_quantities_flag & CS_BAD_CELLS_REGULARISATION))
    return;

  cs_lnum_t n_cells_ext = mesh->n_cells_with_ghosts;
  cs_lnum_t n_cells = mesh->n_cells;
  cs_lnum_t n_i_faces = mesh->n_i_faces;
  const cs_lnum_2_t *i_face_cells = mesh->i_face_cells;

  const cs_real_t *surfn = mq->i_face_surf;
  double *dist = mq->i_dist;
  double *volume  = mq->cell_vol;

  cs_real_66_t *dam;
  cs_real_6_t *rhs;
  cs_real_t *xam;
#if 1
  double varmin[6] = { 1.e20,  1.e20, 1.e20,  1.e20,  1.e20, 1.e20};
  double varmax[6] = {-1.e20, -1.e20,-1.e20, -1.e20, -1.e20,-1.e20};

  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++)
    if (!(mq->bad_cell_flag[cell_id] & CS_BAD_CELL_TO_REGULARIZE)) {
      for (int i = 0; i < 6; i++) {
        varmin[i] = cs::min(varmin[i], var[cell_id][i]);
        varmax[i] = cs::max(varmax[i], var[cell_id][i]);
      }
    }

  for (int i = 0; i < 6; i++) {
    cs_parall_min(1, CS_DOUBLE, &varmin[i]);
    cs_parall_max(1, CS_DOUBLE, &varmax[i]);
  }
#endif

  CS_MALLOC(xam, n_i_faces, cs_real_t);
  CS_MALLOC(dam, n_cells_ext, cs_real_66_t);
  CS_MALLOC(rhs, n_cells_ext, cs_real_6_t);

  for (cs_lnum_t cell_id = 0; cell_id < n_cells_ext; cell_id++) {
    for (int i = 0; i < 6; i++) {
      for (int j = 0; j < 6; j++) {
        dam[cell_id][i][j] = 0.;
      }
      rhs[cell_id][i] = 0.;
    }
  }

  for (cs_lnum_t face_id = 0; face_id < n_i_faces; face_id++) {
    cs_lnum_t cell_id1 = i_face_cells[face_id][0];
    cs_lnum_t cell_id2 = i_face_cells[face_id][1];
    xam[face_id] = 0.;

    //FIXME useful?
    double surf = surfn[face_id];
    double vol = 0.5 * (volume[cell_id1] + volume[cell_id2]);
    surf = cs::max(surf, 0.1*vol/dist[face_id]);
    double ssd = surf / dist[face_id];

    for (int i = 0; i < 6; i++) {
      dam[cell_id1][i][i] += ssd;
      dam[cell_id2][i][i] += ssd;
    }

    if (   mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE
        && mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      xam[face_id] = -ssd;
    }
    else if (mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE) {
      for (int i = 0; i < 6; i++) {
        rhs[cell_id1][i] += ssd * var[cell_id2][i];
        rhs[cell_id2][i] += ssd * var[cell_id2][i];
      }
    }
    else if (mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      for (int i = 0; i < 6; i++) {
        rhs[cell_id2][i] += ssd * var[cell_id1][i];
        rhs[cell_id1][i] += ssd * var[cell_id1][i];
      }
    }
    else {
      for (int i = 0; i < 6; i++) {
        rhs[cell_id1][i] += ssd * var[cell_id1][i];
        rhs[cell_id2][i] += ssd * var[cell_id2][i];
      }
    }
  }

  /* Boundary projection... should be consistent with BCs... */
#if 0
  if (boundary_projection == 1) {
    cs_lnum_t n_b_faces = mesh->n_b_faces;
    const cs_lnum_t *b_face_cells = mesh->b_face_cells;
    const cs_nreal_3_t *b_face_u_normal = mq->b_face_u_normal;
    const cs_real_t *b_face_surf = mq->b_face_surf;
    double *distbr = mq->b_dist;

    for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {
      if (cs_glob_bc_type[face_id] == CS_SMOOTHWALL ||
          cs_glob_bc_type[face_id] == CS_ROUGHWALL  ||
          cs_glob_bc_type[face_id] == CS_SYMMETRY     ) {
        cs_lnum_t cell_id = b_face_cells[face_id];
        if (mq->bad_cell_flag[cell_id] & CS_BAD_CELL_TO_REGULARIZE) {
          double ssd = b_face_surf[face_id] / distbr[face_id];
          for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
              double nn =   b_face_u_normal[face_id][i]
                          * b_face_u_normal[face_id][j];
              //TODO ???    dam[cell_id][i][j] += ssd * nn;
            }
          }
        }
      }
    }
  }
#endif

  cs_real_t rnorm = sqrt(cs_gdot(6*n_cells,
                                 (const cs_real_t *)rhs,
                                 (const cs_real_t *)rhs));

  /*  Solver residual */
  double ressol = 0.;
  int niterf = 0;

  cs_real_t epsilp = 1.e-12;

  cs_sles_solve_native(-1, /* f_id */
                       "potential_regularisation_sym_tensor",
                       true, /* symmetric */
                       6, /* db_size */
                       1, /* eb_size */
                       (cs_real_t *)dam,
                       xam,
                       epsilp,
                       rnorm,
                       &niterf,
                       &ressol,
                       (cs_real_t *)rhs,
                       (cs_real_t *)var);

  bft_printf("Solving %s: N iter: %d, Res: %12.5e, Norm: %12.5e\n",
                 "potential_regularisation_sym_tensor", niterf, ressol, rnorm);
  //FIXME useless clipping? the matrix is min/max preserving..
#if 1
  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++) {
    for (int i = 0; i < 6; i++) {
      var[cell_id][i] = cs::min(var[cell_id][i], varmax[i]);
      var[cell_id][i] = cs::max(var[cell_id][i], varmin[i]);
    }
  }
#endif

  //FIXME periodicity of rotation
  if (mesh->halo != nullptr)
    cs_halo_sync_var_strided(mesh->halo, CS_HALO_STANDARD, (cs_real_t *)var, 6);

  /* Free solver setup */
  cs_sles_free_native(-1, /* f_id*/
                      "potential_regularisation_sym_tensor");

  /* Free memory */
  CS_FREE(xam);
  CS_FREE(dam);
  CS_FREE(rhs);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Regularisation on bad cells for tensors
 *
 * \param[in, out]  var  variable to regularize.
 */
/*----------------------------------------------------------------------------*/

void
cs_bad_cells_regularisation_tensor(cs_real_9_t  *var,
                                   int           boundary_projection)
{
  const cs_mesh_t *mesh = cs_glob_mesh;
  cs_mesh_quantities_t *mq = cs_glob_mesh_quantities;

  if (!(cs_glob_mesh_quantities_flag & CS_BAD_CELLS_REGULARISATION))
    return;

  cs_lnum_t n_cells_ext = mesh->n_cells_with_ghosts;
  cs_lnum_t n_cells = mesh->n_cells;
  cs_lnum_t n_i_faces = mesh->n_i_faces;
  const cs_lnum_2_t *i_face_cells = mesh->i_face_cells;

  const cs_real_t *surfn = mq->i_face_surf;
  double *dist = mq->i_dist;
  double *volume  = mq->cell_vol;

  cs_real_99_t *dam;
  cs_real_9_t *rhs;
  cs_real_t *xam;
#if 1
  double varmin[9] = { 1.e20,  1.e20, 1.e20,  1.e20,  1.e20, 1.e20,  1.e20,  1.e20, 1.e20};
  double varmax[9] = {-1.e20, -1.e20,-1.e20, -1.e20, -1.e20,-1.e20, -1.e20, -1.e20,-1.e20};

  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++)
    if (!(mq->bad_cell_flag[cell_id] & CS_BAD_CELL_TO_REGULARIZE)) {
      for (int i = 0; i < 9; i++) {
        varmin[i] = cs::min(varmin[i], var[cell_id][i]);
        varmax[i] = cs::max(varmax[i], var[cell_id][i]);
      }
    }

  for (int i = 0; i < 9; i++) {
    cs_parall_min(1, CS_DOUBLE, &varmin[i]);
    cs_parall_max(1, CS_DOUBLE, &varmax[i]);
  }
#endif

  CS_MALLOC(xam, n_i_faces, cs_real_t);
  CS_MALLOC(dam, n_cells_ext, cs_real_99_t);
  CS_MALLOC(rhs, n_cells_ext, cs_real_9_t);

  for (cs_lnum_t cell_id = 0; cell_id < n_cells_ext; cell_id++) {
    for (int i = 0; i < 9; i++) {
      for (int j = 0; j < 9; j++) {
        dam[cell_id][i][j] = 0.;
      }
      rhs[cell_id][i] = 0.;
    }
  }

  for (cs_lnum_t face_id = 0; face_id < n_i_faces; face_id++) {
    cs_lnum_t cell_id1 = i_face_cells[face_id][0];
    cs_lnum_t cell_id2 = i_face_cells[face_id][1];
    xam[face_id] = 0.;

    //FIXME useful?
    double surf = surfn[face_id];
    double vol = 0.5 * (volume[cell_id1] + volume[cell_id2]);
    surf = cs::max(surf, 0.1*vol/dist[face_id]);
    double ssd = surf / dist[face_id];

    for (int i = 0; i < 9; i++) {
      dam[cell_id1][i][i] += ssd;
      dam[cell_id2][i][i] += ssd;
    }

    if (   mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE
        && mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      xam[face_id] = -ssd;
    }
    else if (mq->bad_cell_flag[cell_id1] & CS_BAD_CELL_TO_REGULARIZE) {
      for (int i = 0; i < 9; i++) {
        rhs[cell_id1][i] += ssd * var[cell_id2][i];
        rhs[cell_id2][i] += ssd * var[cell_id2][i];
      }
    }
    else if (mq->bad_cell_flag[cell_id2] & CS_BAD_CELL_TO_REGULARIZE) {
      for (int i = 0; i < 9; i++) {
        rhs[cell_id2][i] += ssd * var[cell_id1][i];
        rhs[cell_id1][i] += ssd * var[cell_id1][i];
      }
    }
    else {
      for (int i = 0; i < 9; i++) {
        rhs[cell_id1][i] += ssd * var[cell_id1][i];
        rhs[cell_id2][i] += ssd * var[cell_id2][i];
      }
    }
  }

#if 0
  /* Boundary projection... should be consistent with BCs... */
  if (boundary_projection == 1) {
    const cs_lnum_t n_b_faces = mesh->n_b_faces;
    const cs_real_t *distbr = mq->b_dist;
    const cs_real_t *b_face_surf = mq->b_face_surf;
    const cs_nreal_3_t *b_face_u_normal = mq->b_face_u_normal;
    const cs_lnum_t *b_face_cells = mesh->b_face_cells;
    for (cs_lnum_t face_id = 0; face_id < n_b_faces; face_id++) {
      if (cs_glob_bc_type[face_id] == CS_SMOOTHWALL ||
          cs_glob_bc_type[face_id] == CS_ROUGHWALL  ||
          cs_glob_bc_type[face_id] == CS_SYMMETRY     ) {
        cs_lnum_t cell_id = b_face_cells[face_id];
        if (mq->bad_cell_flag[cell_id] & CS_BAD_CELL_TO_REGULARIZE) {
          double ssd = b_face_surf[face_id] / distbr[face_id];
          for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
              double nn =   b_face_u_normal[face_id][i]
                          * b_face_u_normal[face_id][j];
//TODO ???              dam[cell_id][i][j] += ssd * nn;
            }
          }
        }
      }
    }
  }
#endif

  cs_real_t rnorm = sqrt(cs_gdot(9*n_cells,
                                 (const cs_real_t *)rhs,
                                 (const cs_real_t *)rhs));

  /*  Solver residual */
  double ressol = 0.;
  int niterf = 0;

  cs_real_t epsilp = 1.e-12;

  cs_sles_solve_native(-1, /* f_id */
                       "potential_regularisation_tensor",
                       true, /* symmetric */
                       9, /* db_size */
                       1, /* eb_size */
                       (cs_real_t *)dam,
                       xam,
                       epsilp,
                       rnorm,
                       &niterf,
                       &ressol,
                       (cs_real_t *)rhs,
                       (cs_real_t *)var);

  bft_printf("Solving %s: N iter: %d, Res: %12.5e, Norm: %12.5e\n",
                 "potential_regularisation_tensor", niterf, ressol, rnorm);
  //FIXME useless clipping? the matrix is min/max preserving..
#if 1
  for (cs_lnum_t cell_id = 0; cell_id < n_cells; cell_id++) {
    for (int i = 0; i < 9; i++) {
      var[cell_id][i] = cs::min(var[cell_id][i], varmax[i]);
      var[cell_id][i] = cs::max(var[cell_id][i], varmin[i]);
    }
  }
#endif

  //FIXME periodicity of rotation
  if (mesh->halo != nullptr)
    cs_halo_sync_var_strided(mesh->halo, CS_HALO_STANDARD, (cs_real_t *)var, 9);

  /* Free solver setup */
  cs_sles_free_native(-1, /* f_id*/
                      "potential_regularisation_tensor");

  /* Free memory */
  CS_FREE(xam);
  CS_FREE(dam);
  CS_FREE(rhs);
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
