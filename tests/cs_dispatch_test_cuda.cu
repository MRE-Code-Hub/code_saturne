/*============================================================================
 * Dispatch test, CUDA implementations.
 *============================================================================*/

/*
  This file is part of code_saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2024 EDF S.A.

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

#include <iostream>

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_dispatch.h"

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*----------------------------------------------------------------------------*/

/*=============================================================================
 * Local macros
 *============================================================================*/

/*=============================================================================
 * Local definitions
 *============================================================================*/

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*=============================================================================
 * Semi-private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Test dispatch class.
 *----------------------------------------------------------------------------*/

void
cs_dispatch_test_cuda(void)
{
  const cs_lnum_t n = 100;

  int device_id;
  cudaGetDevice(&device_id);

  cudaStream_t stream;
  cudaStreamCreate(&stream);

  //cs_dispatch_context ctx(cs_device_context(stream), {});
  cs_dispatch_context ctx(cs_device_context(stream), {});

  cs_real_t *a0, *a1;
  cs_alloc_mode_t amode = CS_ALLOC_HOST_DEVICE_SHARED;
  CS_MALLOC_HD(a0, n, cs_real_t, amode);
  CS_MALLOC_HD(a1, n, cs_real_t, amode);

  unsigned int blocksize = 64;
  unsigned int gridsize
    = (unsigned int)ceil((double)n / blocksize);

  for (int i = 0; i < 3; i++) {

    if (i == 1) {
      static_cast<cs_device_context&>(ctx).set_n_min_for_gpu(200);
      static_cast<cs_host_context&>(ctx).set_n_min_for_cpu_threads(200);
      //  ctx_cuda.set_n_min_for_gpu(200);
    }
    else if (i == 2) {
      static_cast<cs_device_context&>(ctx).set_n_min_for_gpu(20);
    }

    ctx.parallel_for(n, CS_HOST_DEVICE_FUNCTOR(=, (cs_lnum_t ii), {
      cs_lnum_t c_id = ii;
#ifdef __CUDA_ARCH__   // Test to know whether we are on GPU or CPU...
      a0[ii] = c_id*0.1;
#else
      a0[ii] = -c_id*0.1;
#endif
      a1[ii] = cos(a0[ii]);
    }));

    cudaStreamSynchronize(stream);

    for (cs_lnum_t ii = 0; ii < n/10; ii++) {
      std::cout << ii << " " << a0[ii] << " " << a1[ii] << std::endl;
    }
  }

  std::cout << "device_id " << device_id << std::endl;

  CS_FREE_HD(a0);
  CS_FREE_HD(a1);
}