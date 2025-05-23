#ifndef __CS_DBG_H__
#define __CS_DBG_H__

/*============================================================================
 * General functions or variables for the INNOV module
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

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include <float.h>

#include "base/cs_base.h"
#include "cdo/cs_cdo_bc.h"
#include "cdo/cs_cdo_local.h"
#include "base/cs_defs.h"
#include "cdo/cs_equation_param.h"
#include "base/cs_math.h"
#include "alge/cs_matrix.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*============================================================================
 * Macro definitions
 *============================================================================*/

/*============================================================================
 * Type definitions
 *============================================================================*/

/*============================================================================
 * Global variables
 *============================================================================*/

/*============================================================================
 * Static inline function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief   Check if there is no invalid setting for a homogeneous Dirichlet
 *
 * \param[in]  fname      name of the calling function
 * \param[in]  csys       pointer to a cs_cell_mesh_t  structure
 */
/*----------------------------------------------------------------------------*/

static inline void
cs_dbg_check_hmg_dirichlet_cw(const char           *fname,
                              const cs_cell_sys_t  *csys)
{
  for (short int i = 0; i < csys->n_dofs; i++) {
    if (csys->dof_flag[i] & CS_CDO_BC_HMG_DIRICHLET)
      if (fabs(csys->dir_values[i]) > 100*DBL_MIN)
        bft_error(__FILE__, __LINE__, 0,
                  " %s: Invalid value for a homogeneous Dirichlet condition",
                  fname);
  }
}

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief   Function used to select which element deserves a dump or specific
 *          treatment during a debugging stage
 *
 * \param[in]  eqp      pointer to a cs_equation_param_t structure
 * \param[in]  cm       pointer to a cs_cell_mesh_t structure
 * \param[in]  csys     pointer to a cs_cell_sys_t structure
 */
/*----------------------------------------------------------------------------*/

bool
cs_dbg_cw_test(const cs_equation_param_t   *eqp,
               const cs_cell_mesh_t        *cm,
               const cs_cell_sys_t         *csys);

/*----------------------------------------------------------------------------*/
/*!
 * \brief   Print an array.
 *          Print into the file f if given otherwise open a new file named
 *          fname if given otherwise print into the standard output
 *          The usage of threshold allows one to compare more easier arrays.
 *          without taking into account numerical roundoff.
 *
 * \param[in]  fp         pointer to a file structure or null
 * \param[in]  fname      filename or null
 * \param[in]  thd        threshold (below this value --> set 0)
 * \param[in]  n_elts     size of the array
 * \param[in]  array      list of values to dump
 * \param[in]  n_cols     print array with n_cols columns
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_array_fprintf(FILE             *fp,
                     const char       *fname,
                     cs_real_t         thd,
                     cs_lnum_t         n_elts,
                     const cs_real_t   array[],
                     int               n_cols);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  In debug mode, print into a file the solution and its right-hand
 *         side
 *
 * \param[in] eqname     name of the related equation
 * \param[in] id         id number
 * \param[in] level      level of debug
 * \param[in] sol        solution array
 * \param[in] rhs        rhs array
 * \param[in] size       size of the array to print
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_fprintf_system(const char        *eqname,
                      int                id,
                      int                level,
                      const cs_real_t   *sol,
                      const cs_real_t   *rhs,
                      cs_lnum_t          size);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  In debug mode, dump an array of double into the log
 *
 * \param[in] header     header message to write
 * \param[in] size       number of elements in array
 * \param[in] array      pointer to the array of values
 * \param[in] n_cols     print array with n_cols columns
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_darray_to_listing(const char        *header,
                         const cs_lnum_t    size,
                         const cs_real_t    array[],
                         int                n_cols);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  In debug mode, dump an array of integer into the log
 *
 * \param[in] header     header message to write
 * \param[in] size       number of elements in array
 * \param[in] array      pointer to the array of values
 * \param[in] n_cols     print array with n_cols columns
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_iarray_to_listing(const char        *header,
                         const cs_lnum_t    size,
                         const cs_lnum_t    array[],
                         int                n_cols);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  In debug mode, dump a linear system. Case of scalar-valued entries.
 *
 * \param[in] name      name of the equation related to the current system
 * \param[in] matrix    pointer to the matrix to dump
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_dump_local_scalar_msr_matrix(const char          *name,
                                    const cs_matrix_t   *matrix);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Print a linear system. Case of scalar-valued entries.
 *
 * \param[in] name         name of the equation related to the current system
 * \param[in] matrix       pointer to the matrix to dump
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_print_local_scalar_msr_matrix(const char          *name,
                                     const cs_matrix_t   *matrix);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  In debug mode, dump a linear system stored in a MSR format into the
 *         listing file (should be a small system)
 *
 * \param[in] eqname     name of the equation related to the current system
 * \param[in] size       number of elements in array
 * \param[in] verbosity  level of display
 * \param[in] x          solution array
 * \param[in] b          right-hand side
 * \param[in] row_index  index on row entries (column id and extra-diag values)
 * \param[in] col_id     list of column id
 * \param[in] xval       array of extra-diagonal values
 * \param[in] dval       array of diagonal values
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_dump_msr_system(const char        *eqname,
                       cs_lnum_t          size,
                       int                verbosity,
                       const cs_real_t    x[],
                       const cs_real_t    b[],
                       const cs_lnum_t    row_index[],
                       const cs_lnum_t    col_id[],
                       const cs_real_t    xval[],
                       const cs_real_t    dval[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Binary dump (matrix, rhs and solution) of a matrix, its right-hand
 *        side and the solution array
 *
 * \param[in] basename  name of the system
 * \param[in] matrix    matrix to dump
 * \param[in] b         right-hand side to dump or null
 * \param[in] x         solution array to dump or null
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_binary_dump_system(const char         *basename,
                          const cs_matrix_t  *matrix,
                          const cs_real_t    *rhs,
                          const cs_real_t    *sol);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* __CS_DBG_H__ */
