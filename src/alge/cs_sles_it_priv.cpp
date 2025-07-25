/*============================================================================
 * Sparse Linear Equation Solvers
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

#include <chrono>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#if defined(HAVE_MPI)
#include <mpi.h>
#endif

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_base_accel.h"
#include "base/cs_dispatch.h"
#include "base/cs_math.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "alge/cs_sles_it_priv.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Additional doxygen documentation
 *============================================================================*/

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * LU factorization of dense 3*3 matrix.
 *
 * parameters:
 *   i        <-- block id
 *   ad       <-- diagonal part of linear equation matrix
 *   ad_inv   --> inverse of the diagonal part of linear equation matrix
 *----------------------------------------------------------------------------*/

CS_F_HOST_DEVICE inline void
_fact_lu33(cs_lnum_t         i,
           const cs_real_t  *ad,
           cs_real_t        *ad_inv)
{
  const cs_real_t *restrict  _ad = &ad[9*i];
  cs_real_t _ad_inv[9];

  _ad_inv[0] = _ad[0];
  _ad_inv[1] = _ad[1];
  _ad_inv[2] = _ad[2];

  _ad_inv[3] = _ad[3]/_ad[0];
  _ad_inv[4] = _ad[4] - _ad_inv[3]*_ad[1];
  _ad_inv[5] = _ad[5] - _ad_inv[3]*_ad[2];

  _ad_inv[6] = _ad[6]/_ad[0];
  _ad_inv[7] = (_ad[7] - _ad_inv[6]*_ad[1])/_ad_inv[4];
  _ad_inv[8] = _ad[8] - _ad_inv[6]*_ad[2] - _ad_inv[7]*_ad_inv[5];

  cs_real_t *restrict ad_inv_i = &ad_inv[9*i];
  for (cs_lnum_t j = 0; j < 9; j++)
    ad_inv_i[j] = _ad_inv[j];
}

/*----------------------------------------------------------------------------
 * LU factorization of dense matrix.
 *
 * parameters:
 *   i        <-- block id
 *   ad       <-- diagonal part of linear equation matrix
 *   ad_inv   --> inverse of the diagonal part of linear equation matrix
 *----------------------------------------------------------------------------*/

CS_F_HOST_DEVICE inline void
_fact_lu(cs_lnum_t         i,
         const int         db_size,
         const cs_real_t  *ad,
         cs_real_t        *ad_inv)
{
  cs_real_t *restrict _ad_inv = &ad_inv[db_size*db_size*i];
  const cs_real_t *restrict  _ad = &ad[db_size*db_size*i];

  _ad_inv[0] = _ad[0];
  // ad_inv(1,j) = ad(1,j)
  // ad_inv(j,1) = ad(j,1)/a(1,1)
  for (cs_lnum_t ii = 1; ii < db_size; ii++) {
    _ad_inv[ii] = _ad[ii];
    _ad_inv[ii*db_size] = _ad[ii*db_size]/_ad[0];
  }
  // ad_inv(i,i) = ad(i,i) - Sum( ad_inv(i,k)*ad_inv(k,i)) k=1 to i-1
  for (cs_lnum_t ii = 1; ii < db_size - 1; ii++) {
    _ad_inv[ii + ii*db_size] = _ad[ii + ii*db_size];
    for (cs_lnum_t kk = 0; kk < ii; kk++) {
      _ad_inv[ii + ii*db_size] -=  _ad_inv[ii*db_size + kk]
                                  *_ad_inv[kk*db_size + ii];
    }

    for (cs_lnum_t jj = ii + 1; jj < db_size; jj++) {
      _ad_inv[ii*db_size + jj] = _ad[ii*db_size + jj];
      _ad_inv[jj*db_size + ii] =   _ad[jj*db_size + ii]
                                 / _ad_inv[ii*db_size + ii];
      for (cs_lnum_t kk = 0; kk < ii; kk++) {
        _ad_inv[ii*db_size + jj] -=  _ad_inv[ii*db_size + kk]
                                    *_ad_inv[kk*db_size + jj];
        _ad_inv[jj*db_size + ii] -=  _ad_inv[jj*db_size + kk]
                                    *_ad_inv[kk*db_size + ii]
                                    /_ad_inv[ii*db_size + ii];
      }
    }
  }
  _ad_inv[db_size*db_size -1] = _ad[db_size*db_size - 1];
  for (cs_lnum_t kk = 0; kk < db_size - 1; kk++) {
    _ad_inv[db_size*db_size - 1] -=  _ad_inv[(db_size-1)*db_size + kk]
                                    *_ad_inv[kk*db_size + db_size -1];
  }
}

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize or reset convergence info structure.
 *        At this stage, the initial residual is set to HUGE_VAL, as it is
 *        unknown.
 *
 * \param[in, out] convergence   convergence info structure
 * \param[in]      solver_name   solver name
 * \param[in]      verbosity     verbosity level
 * \param[in]      n_iter_max    maximum number of iterations
 * \param[in]      precision     precision limit
 * \param[in]      r_norm        residual normalization
 * \param[in, out] residual      initial residual
 */
/*----------------------------------------------------------------------------*/

void
cs_sles_it_convergence_init(cs_sles_it_convergence_t  *convergence,
                            const char                *solver_name,
                            int                        verbosity,
                            unsigned                   n_iter_max,
                            double                     precision,
                            double                     r_norm,
                            double                    *residual)
{
  *residual = HUGE_VAL;  /* Unknown at this stage */

  convergence->name = solver_name;
  convergence->verbosity = verbosity;

  convergence->n_iterations = 0;
  convergence->n_iterations_max = n_iter_max;

  convergence->precision = precision;
  convergence->r_norm = r_norm;
  convergence->residual = *residual;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Setup context for iterative linear solver.
 *        This function is common to most solvers/smoothers
 *
 * \param[in, out] c                 pointer to solver context info
 * \param[in]      name              pointer to system name
 * \param[in]      a                 matrix
 * \param[in]      verbosity         verbosity level
 * \param[in]      diag_block_size   diagonal block size
 * \param[in]      block_nn_inverse  if diagonal block size is 3 or 6, compute
 *                                   inverse of block if true, inverse of block
 *                                   diagonal otherwise
 */
/*----------------------------------------------------------------------------*/

void
cs_sles_it_setup_priv(cs_sles_it_t       *c,
                      const char         *name,
                      const cs_matrix_t  *a,
                      int                 verbosity,
                      int                 diag_block_size,
                      bool                block_nn_inverse)
{
  cs_sles_it_setup_t *sd = c->setup_data;

  cs_alloc_mode_t amode = cs_matrix_get_alloc_mode(a);

  if (sd == nullptr) {
    CS_MALLOC(c->setup_data, 1, cs_sles_it_setup_t);
    sd = c->setup_data;
    sd->ad_inv = nullptr;
    sd->_ad_inv = nullptr;
    sd->pc_context = nullptr;
    sd->pc_apply = nullptr;
  }

  sd->n_rows = cs_matrix_get_n_rows(a) * diag_block_size;

  sd->initial_residual = -1;

  const cs_sles_it_t  *s = c->shared;

  if (c->pc != nullptr) {

    if (s != nullptr) {
      if (s->setup_data == nullptr)
        s = nullptr;
    }

    if (s == nullptr)
      cs_sles_pc_setup(c->pc,
                       name,
                       a,
                       c->on_device,
                       verbosity);

    sd->pc_context = cs_sles_pc_get_context(c->pc);
    sd->pc_apply = cs_sles_pc_get_apply_func(c->pc);

  }

  /* Setup diagonal inverse for Jacobi and Gauss-Seidel */

  else if (block_nn_inverse) {

    if (s != nullptr) {
      if (s->setup_data == nullptr)
        s = nullptr;
      else if (s->setup_data->ad_inv == nullptr)
        s = nullptr;
    }

    if (s != nullptr) {
      sd->ad_inv = s->setup_data->ad_inv;
      CS_FREE_HD(sd->_ad_inv);
    }
    else {

      const cs_lnum_t n_rows = sd->n_rows;
      const cs_lnum_t ad_inv_size = (block_nn_inverse) ?
        n_rows*diag_block_size : n_rows;

      CS_MALLOC_HD(sd->_ad_inv, ad_inv_size, cs_real_t, amode);

      sd->ad_inv = sd->_ad_inv;

      std::chrono::high_resolution_clock::time_point t_start;
      if (cs_glob_timer_kernels_flag > 0)
        t_start = std::chrono::high_resolution_clock::now();

      const cs_real_t  *restrict ad = cs_matrix_get_diagonal(a);

      cs_dispatch_context  ctx;
      ctx.set_use_gpu(c->on_device && amode > CS_ALLOC_HOST);

      cs_real_t *ad_inv = sd->_ad_inv;

      if (diag_block_size == 1) {

        ctx.parallel_for(n_rows, [=] CS_F_HOST_DEVICE (cs_lnum_t i) {
          ad_inv[i] = 1.0 / ad[i];
        });

      }
      else {

        const cs_lnum_t  n_blocks = sd->n_rows / diag_block_size;

        switch(diag_block_size) {
        case 3:
          {
            const cs_real_33_t *b_ad
              = reinterpret_cast<const cs_real_33_t *>(ad);
            cs_real_33_t *b_ad_inv
              = reinterpret_cast<cs_real_33_t *>(ad_inv);
            ctx.parallel_for(n_blocks, [=] CS_F_HOST_DEVICE (cs_lnum_t i) {
              cs_math_matrix_gauss_inverse(b_ad[i], b_ad_inv[i]);
            });
          }
          break;

        case 6:
          {
            const cs_real_66_t *b_ad
              = reinterpret_cast<const cs_real_66_t *>(ad);
            cs_real_66_t *b_ad_inv
              = reinterpret_cast<cs_real_66_t *>(ad_inv);
            ctx.parallel_for(n_blocks, [=] CS_F_HOST_DEVICE (cs_lnum_t i) {
              cs_math_matrix_gauss_inverse(b_ad[i], b_ad_inv[i]);
            });
          }
          break;

        default:
          ctx.parallel_for(n_blocks, [=] CS_F_HOST_DEVICE (cs_lnum_t i) {
            _fact_lu(i, diag_block_size, ad, ad_inv);
          });
        }

      }

      ctx.wait();
      if (c->on_device && ctx.use_gpu() == false)
        cs_sync_h2d(sd->_ad_inv);

      if (cs_glob_timer_kernels_flag > 0) {
        std::chrono::high_resolution_clock::time_point
          t_stop = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds elapsed
          = std::chrono::duration_cast
          <std::chrono::microseconds>(t_stop - t_start);
        printf("%d: %s (fact lu, %d)", cs_glob_rank_id, __func__,
               diag_block_size);
        printf(", total = %lld\n", (long long)elapsed.count());
      }

    }

  }
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*----------------------------------------------------------------------------*/

END_C_DECLS
