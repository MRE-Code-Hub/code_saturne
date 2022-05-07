/*============================================================================
 * Sparse Linear Equation Solvers using CUDA
 *============================================================================*/

/*
  This file is part of code_saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2022 EDF S.A.

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

#include "cs_defs.h"

/*----------------------------------------------------------------------------
 * Standard library headers
 *----------------------------------------------------------------------------*/

#include <assert.h>
#include <algorithm>
#include <list>

#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

#if defined(HAVE_CUBLAS)
#include <cublas_v2.h>
#endif

#include <cooperative_groups.h>
#if (CUDART_VERSION >= 11000)
#include <cooperative_groups/reduce.h>
#endif
namespace cg = cooperative_groups;

// #include "mpi-ext.h"

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft_error.h"
#include "bft_mem.h"
#include "bft_printf.h"
#include "cs_base.h"
#include "cs_base_accel.h"
#include "cs_blas_cuda.h"
#include "cs_log.h"
#include "cs_matrix.h"
#include "cs_matrix_priv.h"
#include "cs_matrix_spmv_cuda.h"
#include "cs_sles_it.h"
#include "cs_sles_pc.h"
#include "cs_sles_it_priv.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_sles_it_cuda.h"

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

 /* SIMD unit size to ensure SIMD alignement (based on warp size) */

#define CS_SIMD_SIZE(s) (((s-1)/32+1)*32)
#define CS_BLOCKSIZE 256

/* Use graph capture ? */

#if (CUDART_VERSION > 9020)
#define _USE_GRAPH 1
#else
#define _USE_GRAPH 0
#endif

/*----------------------------------------------------------------------------
 * Compatibility macro for __ldg (load from generic memory) intrinsic,
 * forcing load from read-only texture cache.
 *
 * This was not available in (very old) CUDA architectures.
 *----------------------------------------------------------------------------*/

#if __CUDA_ARCH__ < 350
#define __ldg(ptr) *(ptr);
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Kernel for sum reduction within a warp (for warp size 32).
 *
 * \param[in, out]  stmp  shared value to reduce
 * \param[in, out]  tid   thread id
 */
/*----------------------------------------------------------------------------*/

template <size_t blockSize, typename T>
__device__ static void __forceinline__
_warp_reduce_sum(volatile T  *stmp,
                 size_t       tid)
{
  if (blockSize >= 64) stmp[tid] += stmp[tid + 32];
  if (blockSize >= 32) stmp[tid] += stmp[tid + 16];
  if (blockSize >= 16) stmp[tid] += stmp[tid +  8];
  if (blockSize >=  8) stmp[tid] += stmp[tid +  4];
  if (blockSize >=  4) stmp[tid] += stmp[tid +  2];
  if (blockSize >=  2) stmp[tid] += stmp[tid +  1];
}

/*----------------------------------------------------------------------------
 * Compute Vx <- Vx - (A-diag).Rk and residue for Jacobi solver.
 *
 * parameters:
 *   n_rows    <-- number of rows
 *   ad_inv    <-- inverse of diagonal
 *   ad        <-- diagonal
 *   rhs       <-- right hand side
 *   vx        <-> solution at current iteration
 *   rk        <-> solution at previous iteraton
 *   sum_block --> contribution to residue
 *----------------------------------------------------------------------------*/

template <size_t blockSize>
__global__ static void
_jacobi_compute_vx_and_residue(cs_lnum_t          n_rows,
                               const cs_real_t   *restrict ad_inv,
                               const cs_real_t   *restrict ad,
                               const cs_real_t   *rhs,
                               cs_real_t         *restrict vx,
                               cs_real_t         *restrict rk,
                               double            *sum_block)
{
  __shared__ double sdata[blockSize];

  cs_lnum_t ii = blockIdx.x*blockDim.x + threadIdx.x;
  size_t tid = threadIdx.x;

  if (ii < n_rows) {
    vx[ii] = (rhs[ii]-vx[ii])*ad_inv[ii];
    double r = ad[ii] * (vx[ii]-rk[ii]);
    sdata[tid] = r*r;
    rk[ii] = vx[ii];
  }
  else
    sdata[tid] = 0.0;

  __syncthreads();

  if (blockSize >= 1024) {
    if (tid < 512) {sdata[tid] += sdata[tid + 512];} __syncthreads();
  }
  if (blockSize >= 512) {
    if (tid < 256) {sdata[tid] += sdata[tid + 256];} __syncthreads();
  }
  if (blockSize >= 256) {
    if (tid < 128) {sdata[tid] += sdata[tid + 128];} __syncthreads();
  }
  if (blockSize >= 128) {
    if (tid <  64) {sdata[tid] += sdata[tid +  64];} __syncthreads();
  }

  if (tid < 32) _warp_reduce_sum<blockSize>(sdata, tid);

  // Output: b_res for this block
  if (tid == 0) sum_block[blockIdx.x] = sdata[0];
}

/*----------------------------------------------------------------------------
 * Block Jacobi utilities.
 * Compute forward and backward to solve an LU 3*3 system.
 *
 * parameters:
 *   mat   <-- 3*3*dim matrix
 *   x     <-> 1st part of RHS (c - b) in, solution out
 *   c     --> 2nd part of RHS (c - b)
 *----------------------------------------------------------------------------*/

__device__ static void  __forceinline__
_fw_and_bw_lu33_cuda(const cs_real_t  mat[],
                     cs_real_t        x[restrict],
                     const cs_real_t  c[restrict])
{
  cs_real_t  aux[3];

  aux[0] = (c[0] - x[0]);
  aux[1] = (c[1] - x[1]) - aux[0]*mat[3];
  aux[2] = (c[2] - x[2]) - aux[0]*mat[6] - aux[1]*mat[7];

  x[2] = aux[2]/mat[8];
  x[1] = (aux[1] - mat[5]*x[2])/mat[4];
  x[0] = (aux[0] - mat[1]*x[1] - mat[2]*x[2])/mat[0];
}

/*----------------------------------------------------------------------------
 * Compute Vx <- Vx - (A-diag).Rk and residue for Jacobi with
 * 3x3 block-diagonal matrix.
 *
 * parameters:
 *   n_b_rows  <-- number of block rows
 *   ad_inv    <-- inverse of diagonal
 *   ad        <-- diagonal
 *   rhs       <-- right hand side
 *   vx        <-> 1st part of RHS (c - b) in, solution at current iteration
 *   rk        <-> solution at previous iteraton
 *   sum_block --> contribution to residue
 *----------------------------------------------------------------------------*/

template <size_t blockSize>
__global__ static void
_block_3_jacobi_compute_vx_and_residue(cs_lnum_t          n_b_rows,
                                       const cs_real_t   *restrict ad_inv,
                                       const cs_real_t   *restrict ad,
                                       const cs_real_t   *rhs,
                                       cs_real_t         *restrict vx,
                                       cs_real_t         *restrict rk,
                                       double            *sum_block)
{
  __shared__ cs_real_t sdata[blockSize];

  cs_lnum_t ii = blockIdx.x*blockDim.x + threadIdx.x;
  size_t tid = threadIdx.x;
  sdata[tid] = 0.0;

  if (ii < n_b_rows) {
    _fw_and_bw_lu33_cuda(ad_inv + 9*ii, vx + 3*ii, rhs + 3*ii);

    #pragma unroll
    for (cs_lnum_t jj = 0; jj < 3; jj++) {
      double r = 0.0;
      #pragma unroll
      for (cs_lnum_t kk = 0; kk < 3; kk++)
        r +=  ad[ii*9 + jj*3 + kk] * (vx[ii*3 + kk] - rk[ii*3 + kk]);

      sdata[tid] += (r*r);
    }

    #pragma unroll
    for (cs_lnum_t kk = 0; kk < 3; kk++)
      rk[ii*3 + kk] = vx[ii*3 + kk];
  }

  __syncthreads();

  if (blockSize >= 1024) {
    if (tid < 512) {sdata[tid] += sdata[tid + 512];} __syncthreads();
  }
  if (blockSize >= 512) {
    if (tid < 256) {sdata[tid] += sdata[tid + 256];} __syncthreads();
  }
  if (blockSize >= 256) {
    if (tid < 128) {sdata[tid] += sdata[tid + 128];} __syncthreads();
  }
  if (blockSize >= 128) {
    if (tid <  64) {sdata[tid] += sdata[tid +  64];} __syncthreads();
  }

  if (tid < 32) _warp_reduce_sum<blockSize>(sdata, tid);

  // Output: b_res for this block
  if (tid == 0) sum_block[blockIdx.x] = sdata[0];
}

/*----------------------------------------------------------------------------
 * Block Jacobi utilities.
 * Compute forward and backward to solve an LU system.
 *
 * parameters:
 *   mat     <-- p*p*dim matrix
 *   db_size <-- block (p) size
 *   x       <-> 1st part of RHS (c - b) in, solution out
 *   c       --> 2nd part of RHS (c - b)
 *----------------------------------------------------------------------------*/

__device__ static void  __forceinline__
_fw_and_bw_lu_cuda(const cs_real_t  mat[],
                   cs_lnum_t        db_size,
                   cs_real_t        x[restrict],
                   const cs_real_t  c[restrict])
{
  assert(db_size <= 9);
  cs_real_t aux[9];

  /* forward */
  for (cs_lnum_t ii = 0; ii < db_size; ii++) {
    aux[ii] = (c[ii] - x[ii]);
    for (cs_lnum_t jj = 0; jj < ii; jj++) {
      aux[ii] -= aux[jj]*mat[ii*db_size + jj];
    }
  }

  /* backward */
  for (cs_lnum_t ii = db_size - 1; ii >= 0; ii-=1) {
    x[ii] = aux[ii];
    for (cs_lnum_t jj = db_size - 1; jj > ii; jj-=1) {
      x[ii] -= x[jj]*mat[ii*db_size + jj];
    }
    x[ii] /= mat[ii*(db_size + 1)];
  }
}

/*----------------------------------------------------------------------------
 * Compute Vx <- Vx - (A-diag).Rk and residue for Jacobi with
 * 3x3 block-diagonal matrix.
 *
 * parameters:
 *   n_b_rows  <-- number of block rows
 *   ad_inv    <-- inverse of diagonal
 *   ad        <-- diagonal
 *   rhs       <-- right hand side
 *   vx        <-> 1st part of RHS (c - b) in, solution at current iteration
 *   rk        <-> solution at previous iteraton
 *   sum_block --> contribution to residue
 *----------------------------------------------------------------------------*/

template <size_t blockSize>
__global__ static void
_block_jacobi_compute_vx_and_residue(cs_lnum_t          n_b_rows,
                                     cs_lnum_t          db_size,
                                     const cs_real_t   *restrict ad_inv,
                                     const cs_real_t   *restrict ad,
                                     const cs_real_t   *rhs,
                                     cs_real_t         *restrict vx,
                                     cs_real_t         *restrict rk,
                                     double            *sum_block)
{
  __shared__ cs_real_t sdata[blockSize];

  cs_lnum_t ii = blockIdx.x*blockDim.x + threadIdx.x;
  size_t tid = threadIdx.x;
  sdata[tid] = 0.0;

  cs_lnum_t n = db_size;
  cs_lnum_t nn = db_size*db_size;

  if (ii < n_b_rows) {
    _fw_and_bw_lu_cuda(ad_inv + nn*ii, n, vx + n*ii, rhs + n*ii);

    #pragma unroll
    for (cs_lnum_t jj = 0; jj < n; jj++) {
      double r = 0.0;
      #pragma unroll
      for (cs_lnum_t kk = 0; kk < n; kk++)
        r +=  ad[ii*nn + jj*n + kk] * (vx[ii*n + kk] - rk[ii*n + kk]);

      sdata[tid] += (r*r);
    }

    #pragma unroll
    for (cs_lnum_t kk = 0; kk < n; kk++)
      rk[ii*n + kk] = vx[ii*n + kk];
  }

  __syncthreads();

  if (blockSize >= 1024) {
    if (tid < 512) {sdata[tid] += sdata[tid + 512];} __syncthreads();
  }
  if (blockSize >= 512) {
    if (tid < 256) {sdata[tid] += sdata[tid + 256];} __syncthreads();
  }
  if (blockSize >= 256) {
    if (tid < 128) {sdata[tid] += sdata[tid + 128];} __syncthreads();
  }
  if (blockSize >= 128) {
    if (tid <  64) {sdata[tid] += sdata[tid +  64];} __syncthreads();
  }

  if (tid < 32) _warp_reduce_sum<blockSize>(sdata, tid);

  // Output: b_res for this block
  if (tid == 0) sum_block[blockIdx.x] = sdata[0];
}

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Solution of A.vx = Rhs using Jacobi.
 *
 * On entry, vx is considered initialized.
 *
 * parameters:
 *   c               <-- pointer to solver context info
 *   a               <-- linear equation matrix
 *   diag_block_size <-- diagonal block size
 *   rotation_mode   <-- halo update option for rotational periodicity
 *   convergence     <-- convergence information structure
 *   rhs             <-- right hand side
 *   vx              <-> system solution
 *   aux_size        <-- number of elements in aux_vectors (in bytes)
 *   aux_vectors     --- optional working area (allocation otherwise)
 *
 * returns:
 *   convergence state
 *----------------------------------------------------------------------------*/

cs_sles_convergence_state_t
cs_sles_it_cuda_jacobi(cs_sles_it_t              *c,
                       const cs_matrix_t         *a,
                       cs_lnum_t                  diag_block_size,
                       cs_sles_it_convergence_t  *convergence,
                       const cs_real_t           *rhs,
                       cs_real_t                 *restrict vx,
                       size_t                     aux_size,
                       void                      *aux_vectors)
{
  cs_sles_convergence_state_t cvg= CS_SLES_ITERATING;
  unsigned n_iter = 0;

  int device_id = cs_get_device_id();

  cudaStream_t stream1, stream2;
  cudaStreamCreate(&stream1);
  cudaStreamCreate(&stream2);

  const cs_lnum_t n_cols = cs_matrix_get_n_columns(a) * diag_block_size;

  size_t vec_size = n_cols * sizeof(cs_real_t);

  cs_alloc_mode_t amode_vx = cs_check_device_ptr(vx);
  cs_alloc_mode_t amode_rhs = cs_check_device_ptr(rhs);

  if (amode_vx == CS_ALLOC_HOST_DEVICE_SHARED)
    cudaMemPrefetchAsync(vx, vec_size, device_id, stream2);

  if (amode_rhs == CS_ALLOC_HOST_DEVICE_SHARED)
    cudaMemPrefetchAsync(rhs, vec_size, device_id, stream1);
  else if (amode_rhs == CS_ALLOC_HOST) {
    cudaMemPrefetchAsync(rhs, vec_size, device_id, stream1);
  }

  const cs_real_t  *restrict ad;
  {
    void *d_val_p = const_cast<cs_real_t *>(cs_matrix_get_diagonal(a));
    ad = (const cs_real_t  *restrict)cs_get_device_ptr(d_val_p);
  }

  double residue, res2;

  double *res;
  CS_MALLOC_HD(res, 1, double, CS_ALLOC_HOST_DEVICE_SHARED);

  /* Allocate or map work arrays
     --------------------------- */

  assert(c->setup_data != NULL);

  const cs_real_t *restrict ad_inv = c->setup_data->ad_inv;
  const cs_lnum_t n_rows = c->setup_data->n_rows;

  cs_real_t *_aux_vectors = NULL;
  {
    const cs_lnum_t n_cols = cs_matrix_get_n_columns(a) * diag_block_size;
    const size_t n_wa = 1;
    const size_t wa_size = CS_SIMD_SIZE(n_cols);

    if (   aux_vectors == NULL
        || cs_check_device_ptr(aux_vectors) == CS_ALLOC_HOST
        || aux_size/sizeof(cs_real_t) < (wa_size * n_wa)) {
#if defined(MPIX_CUDA_AWARE_SUPPORT) && MPIX_CUDA_AWARE_SUPPORT
      cudaMalloc(&_aux_vectors, wa_size * n_wa *sizeof(cs_real_t));
#else
      cudaMallocManaged(&_aux_vectors, wa_size * n_wa *sizeof(cs_real_t));
#endif
    }
    else
      _aux_vectors = (cs_real_t *)aux_vectors;
  }

  cs_real_t *restrict rk = _aux_vectors;

  cudaMemcpyAsync(rk, vx, n_rows*sizeof(cs_real_t),
                  cudaMemcpyDeviceToDevice, stream2);

  const unsigned int blocksize = 256;

  unsigned int gridsize = (unsigned int)ceil((double)n_rows/blocksize);

  double  *sum_block;
  CS_MALLOC_HD(sum_block, gridsize, double, CS_ALLOC_DEVICE);

  cs_matrix_spmv_cuda_set_stream(stream2);

#if _USE_GRAPH == 1

  /* Build graph for a portion of kernels used here. */

  cudaGraph_t graph;
  cudaGraphExec_t graph_exec = NULL;
  cudaGraphNode_t graph_node;
  cudaStreamBeginCapture(stream2, cudaStreamCaptureModeGlobal);

  /* Compute Vx <- Vx - (A-diag).Rk and residue. */

  _jacobi_compute_vx_and_residue<blocksize><<<gridsize, blocksize, 0, stream2>>>
    (n_rows, ad_inv, ad, rhs, vx, rk, sum_block);
  cs_blas_cuda_reduce_single_block<blocksize><<<1, blocksize, 0, stream2>>>
    (gridsize, sum_block, res);

  cudaStreamEndCapture(stream2, &graph);
  cudaError_t status = cudaGraphInstantiate(&graph_exec, graph, &graph_node,
                                            nullptr, 0);
  assert(status == cudaSuccess);

#endif /* _USE_GRAPH == 1 */

  /* Current iteration
     ----------------- */

  while (cvg == CS_SLES_ITERATING) {

    n_iter += 1;

    /* SpmV done outside of graph capture as halo synchronization
       currently synchonizes separate streams and may not be captured. */

    cs_matrix_vector_multiply_partial_d(a, CS_MATRIX_SPMV_E, rk, vx);

#if _USE_GRAPH == 1

    cudaGraphLaunch(graph_exec, stream2);

#else

    /* Compute Vx <- Vx - (A-diag).Rk and residue. */

    _jacobi_compute_vx_and_residue<blocksize><<<gridsize, blocksize, 0, stream2>>>
      (n_rows, ad_inv, ad, rhs, vx, rk, sum_block);
    cs_blas_cuda_reduce_single_block<blocksize><<<1, blocksize, 0, stream2>>>
      (gridsize, sum_block, res);

#endif /* _USE_GRAPH */

    cudaStreamSynchronize(stream2);
    res2 = *res;

#if defined(HAVE_MPI)

    if (c->comm != MPI_COMM_NULL) {
      double _sum;
      MPI_Allreduce(&res2, &_sum, 1, MPI_DOUBLE, MPI_SUM, c->comm);
      res2 = _sum;
    }

#endif /* defined(HAVE_MPI) */

    residue = sqrt(res2); /* Actually, residue of previous iteration */

    /* Convergence test */
    if (n_iter == 1)
      c->setup_data->initial_residue = residue;
    cvg = cs_sles_it_convergence_test(c, n_iter, residue, convergence);

  }

#if _USE_GRAPH == 1

  cudaGraphDestroy(graph);
  cudaGraphExecDestroy(graph_exec);

#endif

  if (_aux_vectors != (cs_real_t *)aux_vectors)
    cudaFree(_aux_vectors);

  CS_FREE_HD(res);
  CS_FREE_HD(sum_block);

  cs_matrix_spmv_cuda_set_stream(0);

  cudaStreamDestroy(stream2);
  cudaStreamDestroy(stream1);

  return cvg;
}

/*----------------------------------------------------------------------------
 * Solution of A.vx = Rhs using block Jacobi.
 *
 * On entry, vx is considered initialized.
 *
 * parameters:
 *   c               <-- pointer to solver context info
 *   a               <-- linear equation matrix
 *   diag_block_size <-- diagonal block size
 *   rotation_mode   <-- halo update option for rotational periodicity
 *   convergence     <-- convergence information structure
 *   rhs             <-- right hand side
 *   vx              <-> system solution
 *   aux_size        <-- number of elements in aux_vectors (in bytes)
 *   aux_vectors     --- optional working area (allocation otherwise)
 *
 * returns:
 *   convergence state
 *----------------------------------------------------------------------------*/

cs_sles_convergence_state_t
cs_sles_it_cuda_block_jacobi(cs_sles_it_t              *c,
                             const cs_matrix_t         *a,
                             cs_lnum_t                  diag_block_size,
                             cs_sles_it_convergence_t  *convergence,
                             const cs_real_t           *rhs,
                             cs_real_t                 *restrict vx,
                             size_t                     aux_size,
                             void                      *aux_vectors)
{
  cs_sles_convergence_state_t cvg= CS_SLES_ITERATING;
  unsigned n_iter = 0;

  int device_id = cs_get_device_id();

  cudaStream_t stream1, stream2;
  cudaStreamCreate(&stream1);
  cudaStreamCreate(&stream2);

  const cs_lnum_t n_cols = cs_matrix_get_n_columns(a) * diag_block_size;

  size_t vec_size = n_cols * sizeof(cs_real_t);

  cs_alloc_mode_t amode_vx = cs_check_device_ptr(vx);
  cs_alloc_mode_t amode_rhs = cs_check_device_ptr(rhs);

  if (amode_vx == CS_ALLOC_HOST_DEVICE_SHARED)
    cudaMemPrefetchAsync(vx, vec_size, device_id, stream2);

  if (amode_rhs == CS_ALLOC_HOST_DEVICE_SHARED)
    cudaMemPrefetchAsync(rhs, vec_size, device_id, stream1);
  else if (amode_rhs == CS_ALLOC_HOST) {
    cudaMemPrefetchAsync(rhs, vec_size, device_id, stream1);
  }

  const cs_real_t  *restrict ad;
  {
    void *d_val_p = const_cast<cs_real_t *>(cs_matrix_get_diagonal(a));
    ad = (const cs_real_t  *restrict)cs_get_device_ptr(d_val_p);
  }

  double residue, res2;

  double *res;
  CS_MALLOC_HD(res, 1, double, CS_ALLOC_HOST_DEVICE_SHARED);

  /* Allocate or map work arrays
     --------------------------- */

  assert(c->setup_data != NULL);

  const cs_real_t *restrict ad_inv = c->setup_data->ad_inv;
  const cs_lnum_t n_rows = c->setup_data->n_rows;

  cs_real_t *_aux_vectors = NULL;
  {
    const cs_lnum_t n_cols = cs_matrix_get_n_columns(a) * diag_block_size;
    const size_t n_wa = 1;
    const size_t wa_size = CS_SIMD_SIZE(n_cols);

    if (   aux_vectors == NULL
        || cs_check_device_ptr(aux_vectors) == CS_ALLOC_HOST
        || aux_size/sizeof(cs_real_t) < (wa_size * n_wa)) {
#if defined(MPIX_CUDA_AWARE_SUPPORT) && MPIX_CUDA_AWARE_SUPPORT
      cudaMalloc(&_aux_vectors, wa_size * n_wa *sizeof(cs_real_t));
#else
      cudaMallocManaged(&_aux_vectors, wa_size * n_wa *sizeof(cs_real_t));
#endif
    }
    else
      _aux_vectors = (cs_real_t *)aux_vectors;
  }

  cs_real_t *restrict rk = _aux_vectors;

  cudaMemcpyAsync(rk, vx, n_rows*sizeof(cs_real_t),
                  cudaMemcpyDeviceToDevice, stream2);

  const unsigned int blocksize = 256;
  cs_lnum_t n_b_rows = n_rows / diag_block_size;

  unsigned int gridsize = (unsigned int)ceil((double)n_b_rows/blocksize);

  double  *sum_block;
  CS_MALLOC_HD(sum_block, gridsize, double, CS_ALLOC_DEVICE);

  cs_matrix_spmv_cuda_set_stream(stream2);

#if _USE_GRAPH == 1

  /* Build graph for a portion of kernels used here. */

  cudaGraph_t graph;
  cudaGraphExec_t graph_exec = NULL;
  cudaGraphNode_t graph_node;
  cudaStreamBeginCapture(stream2, cudaStreamCaptureModeGlobal);

  /* Compute Vx <- Vx - (A-diag).Rk and residue. */

  if (diag_block_size == 3)
    _block_3_jacobi_compute_vx_and_residue
      <blocksize><<<gridsize, blocksize, 0, stream2>>>
      (n_b_rows, ad_inv, ad, rhs, vx, rk, sum_block);
  else
    _block_jacobi_compute_vx_and_residue
      <blocksize><<<gridsize, blocksize, 0, stream2>>>
      (n_b_rows, diag_block_size, ad_inv, ad, rhs, vx, rk, sum_block);

  cs_blas_cuda_reduce_single_block<blocksize><<<1, blocksize, 0, stream2>>>
    (gridsize, sum_block, res);

  cudaStreamEndCapture(stream2, &graph);
  cudaError_t status = cudaGraphInstantiate(&graph_exec, graph, &graph_node,
                                            nullptr, 0);
  assert(status == cudaSuccess);

#endif /* _USE_GRAPH == 1 */

  /* Current iteration
     ----------------- */

  while (cvg == CS_SLES_ITERATING) {

    n_iter += 1;

    /* SpmV done outside of graph capture as halo synchronization
       currently synchonizes separate streams and may not be captured. */

    cs_matrix_vector_multiply_partial_d(a, CS_MATRIX_SPMV_E, rk, vx);

#if _USE_GRAPH == 1

    cudaGraphLaunch(graph_exec, stream2);

#else

    /* Compute Vx <- Vx - (A-diag).Rk and residue. */

    if (diag_block_size == 3)
      _block_3_jacobi_compute_vx_and_residue
        <blocksize><<<gridsize, blocksize, 0, stream2>>>
        (n_b_rows, ad_inv, ad, rhs, vx, rk, sum_block);
    else
      _block_jacobi_compute_vx_and_residue
        <blocksize><<<gridsize, blocksize, 0, stream2>>>
        (n_b_rows, diag_block_size, ad_inv, ad, rhs, vx, rk, sum_block);
    cs_blas_cuda_reduce_single_block<blocksize><<<1, blocksize, 0, stream2>>>
      (gridsize, sum_block, res);

#endif /* _USE_GRAPH */

    cudaStreamSynchronize(stream2);
    res2 = *res;

#if defined(HAVE_MPI)

    if (c->comm != MPI_COMM_NULL) {
      double _sum;
      MPI_Allreduce(&res2, &_sum, 1, MPI_DOUBLE, MPI_SUM, c->comm);
      res2 = _sum;
    }

#endif /* defined(HAVE_MPI) */

    residue = sqrt(res2); /* Actually, residue of previous iteration */

    /* Convergence test */
    if (n_iter == 1)
      c->setup_data->initial_residue = residue;
    cvg = cs_sles_it_convergence_test(c, n_iter, residue, convergence);

  }

#if _USE_GRAPH == 1

  cudaGraphDestroy(graph);
  cudaGraphExecDestroy(graph_exec);

#endif

  if (_aux_vectors != (cs_real_t *)aux_vectors)
    cudaFree(_aux_vectors);

  CS_FREE_HD(sum_block);
  CS_FREE_HD(res);

  cs_matrix_spmv_cuda_set_stream(0);

  cudaStreamDestroy(stream2);
  cudaStreamDestroy(stream1);

  return cvg;
}

/*----------------------------------------------------------------------------*/

END_C_DECLS