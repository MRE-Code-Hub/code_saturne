/*============================================================================
 * Save mesh Preprocessor data
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

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(HAVE_MPI)
#include <mpi.h>
#endif

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_error.h"
#include "bft/bft_printf.h"

#include "fvm/fvm_io_num.h"
#include "fvm/fvm_periodicity.h"

#include "base/cs_base.h"
#include "base/cs_block_dist.h"
#include "base/cs_io.h"
#include "base/cs_mem.h"
#include "mesh/cs_mesh.h"
#include "mesh/cs_mesh_builder.h"
#include "mesh/cs_mesh_to_builder.h"
#include "base/cs_parall.h"
#include "base/cs_part_to_block.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "mesh/cs_mesh_save.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Type Definitions
 *============================================================================*/

/* Directory name separator
   (historically, '/' for Unix/Linux, '\' for Windows, ':' for Mac
   but '/' should work for all on modern systems) */

#define DIR_SEPARATOR '/'

/*============================================================================
 * Static global variables
 *============================================================================*/

/*============================================================================
 *  Global variables
 *============================================================================*/

/*=============================================================================
 * Private function definitions
 *============================================================================*/

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Save a mesh as preprocessor data.
 *
 * parameters:
 *   mesh     <-- pointer to mesh structure
 *   mb       <-- pointer to optional mesh builder structure, or nullptr
 *   path     <-- optional directory name for output, or nullptr for default
 *                (directory automatically created if necessary)
 *   filename <-- file name
 *----------------------------------------------------------------------------*/

void
cs_mesh_save(cs_mesh_t          *mesh,
             cs_mesh_builder_t  *mb,
             const char         *path,
             const char         *filename)
{
  cs_file_access_t  method;
  int  block_rank_step = 1;
  size_t block_min_size = 0;
  long  echo = CS_IO_ECHO_OPEN_CLOSE;

  cs_io_t  *pp_out = nullptr;

  cs_mesh_builder_t  *_mb = nullptr;

  bool transfer = false;

#if defined(HAVE_MPI)

  MPI_Info  hints;
  MPI_Comm  block_comm, comm;

  cs_file_get_default_comm(&block_rank_step,
                           &block_comm, &comm);
  block_min_size = cs_parall_get_min_coll_buf_size();


  assert(comm == cs_glob_mpi_comm || comm == MPI_COMM_NULL);

#endif

  const cs_gnum_t n_g_faces = mesh->n_g_i_faces + mesh->n_g_b_faces;

  /* Use existing mesh_builder_t structure, or create a temporary one */

  if (mb == nullptr) {
    _mb = cs_mesh_builder_create();
  }
  else {
    _mb = mb;
    transfer = true;
  }

  cs_mesh_builder_define_block_dist(_mb,
                                    cs_glob_rank_id,
                                    cs_glob_n_ranks,
                                    block_rank_step,
                                    block_min_size,
                                    mesh->n_g_cells,
                                    n_g_faces,
                                    mesh->n_g_vertices);

  /* Open file for output */
  size_t  ldir = 0, lname = strlen(filename);

  const char  *name = filename;
  char *_name = nullptr;

  if (path != nullptr)
    ldir = strlen(path);

  if (ldir > 0) {

    if (cs_glob_rank_id < 1) {
      if (cs_file_mkdir_default(path) != 0)
        bft_error(__FILE__, __LINE__, 0,
                  _("The %s directory cannot be created"), path);
    }

#if defined(HAVE_MPI)
    if (cs_glob_n_ranks > 1)
      MPI_Barrier(cs_glob_mpi_comm);
#endif

    CS_MALLOC(_name, ldir + lname + 2, char);
    sprintf(_name, "%s%c%s",
            path, DIR_SEPARATOR, filename);
    name = _name;
  }

#if defined(HAVE_MPI)
  cs_file_get_default_access(CS_FILE_MODE_WRITE, &method, &hints);
  pp_out = cs_io_initialize(name,
                            "Face-based mesh definition, R0",
                            CS_IO_MODE_WRITE,
                            method,
                            echo,
                            hints,
                            block_comm,
                            comm);
#else
  cs_file_get_default_access(CS_FILE_MODE_WRITE, &method);
  pp_out = cs_io_initialize(name,
                            "Face-based mesh definition, R0",
                            CS_IO_MODE_WRITE,
                            method,
                            echo);
#endif

  CS_FREE(_name);

  /* Write data */
  /*------------*/

  /* Main mesh data */

  cs_mesh_to_builder(mesh, _mb, transfer, pp_out);

  if (mb == nullptr)
    cs_mesh_builder_destroy(&_mb);

  /* Close file */

  cs_io_finalize(&pp_out);
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
