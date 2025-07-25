/*============================================================================
 * General functions or variables for the CDO module
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
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_mem.h"
#include "bft/bft_error.h"

#include "base/cs_log.h"
#include "alge/cs_matrix_util.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "cdo/cs_dbg.h"

/*----------------------------------------------------------------------------*/
BEGIN_C_DECLS

/*=============================================================================
 * Global variables
 *============================================================================*/

/*=============================================================================
 * Local static variables
 *============================================================================*/

/*============================================================================
 * Private function prototypes
 *============================================================================*/

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
cs_dbg_cw_test(const cs_equation_param_t *eqp,
               const cs_cell_mesh_t      *cm,
               const cs_cell_sys_t       *csys)
{
  CS_UNUSED(eqp);

#if 1 /* First example: Look for debug information related the cell number 0   \
       */
  if (cm != nullptr)
    if (cm->c_id == 0)
      return true;
  if (csys != nullptr)
    if (csys->c_id == 0)
      return true;
#endif

#if 0 /* Second example: Only search debug information for a given equation    \
         and a requested vertex */

  /* Vertex number 441 belongs to the current cell ? */
  const cs_lnum_t  target_vertex_id = 441;
  const short int _v = cs_cell_mesh_get_v(target_vertex_id, cm);
  if (_v < 0)
    return false;

  if (eqp != nullptr)
    if (strcmp(eqp->name, "Tracer1") == 0)
      return true;
#endif

#if 0 /* Third example: Look for the cells which have a previous DoF value     \
         greater than 0.06 for the face 1 */
  const cs_lnum_t target_face_id = 1;
  const short int _f = cs_cell_mesh_get_f(target_face_id, cm);

  if (csys != nullptr && _f > -1) {
    if (csys->val_n[_f] > 0.06)
      return true;
  }
#endif

  return false; /* Default behavior */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief   Print an array.
 *          Print into the file f if given otherwise open a new file named
 *          fname if given otherwise print into the standard output
 *          The usage of threshold allows one to compare more easier arrays
 *          without taking into account numerical roundoff.
 *
 * \param[in]  fp         pointer to a file structure or nullptr
 * \param[in]  fname      filename or nullptr
 * \param[in]  thd        threshold (below this value --> set 0)
 * \param[in]  n_elts     size of the array
 * \param[in]  array      list of values to dump
 * \param[in]  n_cols     print array with n_cols columns
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_array_fprintf(FILE           *fp,
                     const char     *fname,
                     cs_real_t       thd,
                     cs_lnum_t       n_elts,
                     const cs_real_t array[],
                     int             n_cols)
{
  FILE *fout = stdout;
  if (fp != nullptr)
    fout = fp;
  else if (fname != nullptr) {
    fout = fopen(fname, "w");
  }

  fprintf(fout, "array %p\n", (const void *)array);

  if (array == nullptr)
    return;

  if (n_cols < 1)
    n_cols = 1;
  int n_rows = n_elts / n_cols;

  for (cs_lnum_t i = 0; i < n_rows; i++) {
    for (cs_lnum_t j = i * n_cols; j < (i + 1) * n_cols; j++) {
      if (fabs(array[j]) < thd)
        fprintf(fout, " % -8.5e", 0.);
      else
        fprintf(fout, " % -8.5e", array[j]);
    }
    fprintf(fout, "\n");
  }

  if (n_rows * n_cols < n_elts) {
    for (cs_lnum_t j = n_rows * n_cols; j < n_elts; j++) {
      if (fabs(array[j]) < thd)
        fprintf(fout, " % -8.5e", 0.);
      else
        fprintf(fout, " % -8.5e", array[j]);
    }
    fprintf(fout, "\n");
  }

  if (fout != stdout && fout != fp)
    fclose(fout);
}

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
cs_dbg_fprintf_system(const char      *eqname,
                      int              id,
                      int              level,
                      const cs_real_t *sol,
                      const cs_real_t *rhs,
                      cs_lnum_t        size)
{
  int   len      = strlen(eqname) + strlen("-sol-.log") + 4 + 1;
  char *filename = nullptr;

  CS_MALLOC(filename, len, char);

  sprintf(filename, "%s-sol-%04d.log", eqname, id);
  if (sol != nullptr && level > 4)
    cs_dbg_array_fprintf(nullptr, filename, 1e-16, size, sol, 6);

  sprintf(filename, "%s-rhs-%04d.log", eqname, id);
  if (rhs != nullptr && level > 5)
    cs_dbg_array_fprintf(nullptr, filename, 1e-16, size, rhs, 6);

  CS_FREE(filename);
}

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
cs_dbg_darray_to_listing(const char     *header,
                         const cs_lnum_t size,
                         const cs_real_t array[],
                         int             n_cols)
{
  cs_log_printf(CS_LOG_DEFAULT, "\nDUMP>> %s\n", header);

  if (n_cols < 1)
    n_cols = 1;
  int n_rows = size / n_cols;

  for (cs_lnum_t i = 0; i < n_rows; i++) {
    for (cs_lnum_t j = i * n_cols; j < (i + 1) * n_cols; j++)
      cs_log_printf(CS_LOG_DEFAULT, " (%04ld) % 6.4e", (long)j, array[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }

  if (n_rows * n_cols < size) {
    for (cs_lnum_t j = n_rows * n_cols; j < size; j++)
      cs_log_printf(CS_LOG_DEFAULT, " (%04ld) % 6.4e", (long)j, array[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }
}

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
cs_dbg_iarray_to_listing(const char     *header,
                         const cs_lnum_t size,
                         const cs_lnum_t array[],
                         int             n_cols)
{
  cs_log_printf(CS_LOG_DEFAULT, "\nDUMP>> %s\n", header);

  if (n_cols < 1)
    n_cols = 1;
  int n_rows = size / n_cols;

  for (cs_lnum_t i = 0; i < n_rows; i++) {
    for (cs_lnum_t j = i * n_cols; j < (i + 1) * n_cols; j++)
      cs_log_printf(CS_LOG_DEFAULT, " (%04ld) % 6ld", (long)j, (long)array[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }

  if (n_rows * n_cols < size) {
    for (cs_lnum_t j = n_rows * n_cols; j < size; j++)
      cs_log_printf(CS_LOG_DEFAULT, " (%04ld) % 6ld", (long)j, (long)array[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  In debug mode, dump a linear system. Case of scalar-valued entries.
 *
 * \param[in] name      name of the equation related to the current system
 * \param[in] matrix    pointer to the matrix to dump
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_dump_local_scalar_msr_matrix(const char *name, const cs_matrix_t *matrix)
{
  if (cs_matrix_get_type(matrix) != CS_MATRIX_MSR)
    return;

  cs_log_printf(
    CS_LOG_DEFAULT, "\nDUMP MSR MATRIX FOR THE EQUATION >> %s\n", name);

  const cs_lnum_t  size = cs_matrix_get_n_rows(matrix);
  const cs_lnum_t *row_index, *col_id;
  const cs_real_t *d_val, *x_val;

  cs_matrix_get_msr_arrays(matrix, &row_index, &col_id, &d_val, &x_val);

  for (cs_lnum_t i = 0; i < size; i++) {

    const cs_lnum_t *idx = row_index + i;

    cs_log_printf(CS_LOG_DEFAULT, "%4ld |D|% -6.4e |E", (long)i, d_val[i]);
    for (cs_lnum_t j = idx[0]; j < idx[1]; j++)
      cs_log_printf(
        CS_LOG_DEFAULT, "|% -6.4e c%4ld", x_val[j], (long)col_id[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");

  } /* Loop on rows */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Print a linear system. Case of scalar-valued entries.
 *
 * \param[in] name         name of the equation related to the current system
 * \param[in] matrix       pointer to the matrix to dump
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_print_local_scalar_msr_matrix(const char        *name,
                                     const cs_matrix_t *matrix)
{
  if (cs_matrix_get_type(matrix) != CS_MATRIX_MSR)
    return;

  FILE *f = fopen(name, "w");

  const cs_lnum_t  size = cs_matrix_get_n_rows(matrix);
  const cs_lnum_t *row_index, *col_id;
  const cs_real_t *d_val, *x_val;

  fprintf(f, "%d, %d\n", size, size);

  cs_matrix_get_msr_arrays(matrix, &row_index, &col_id, &d_val, &x_val);

  for (cs_lnum_t i = 0; i < size; i++) {

    const cs_lnum_t *idx = row_index + i;

    fprintf(f, "%d %d %16.14e\n", i, i, d_val[i]);
    for (cs_lnum_t j = idx[0]; j < idx[1]; j++)
      fprintf(f, "%d %d %16.14e\n", i, col_id[j], x_val[j]);

  } /* Loop on rows */
}

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
cs_dbg_dump_msr_system(const char     *eqname,
                       cs_lnum_t       size,
                       int             verbosity,
                       const cs_real_t x[],
                       const cs_real_t b[],
                       const cs_lnum_t row_index[],
                       const cs_lnum_t col_id[],
                       const cs_real_t xval[],
                       const cs_real_t dval[])
{
  cs_log_printf(
    CS_LOG_DEFAULT, "\nDUMP LINEAR SYSTEM FOR THE EQUATION >> %s\n", eqname);

  int n_dump_cols = 8;
  int n_dump_rows = size / n_dump_cols;

  cs_log_printf(CS_LOG_DEFAULT, " >> SOLUTION\n");
  for (cs_lnum_t i = 0; i < n_dump_rows; i++) {
    for (cs_lnum_t j = i * n_dump_cols; j < (i + 1) * n_dump_cols; j++)
      cs_log_printf(CS_LOG_DEFAULT, "%4ld % -6.4e |", (long)j, x[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }
  if (n_dump_rows * n_dump_cols < size) {
    for (cs_lnum_t j = n_dump_rows * n_dump_cols; j < size; j++)
      cs_log_printf(CS_LOG_DEFAULT, "%4ld % -6.4e |", (long)j, x[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }

  cs_log_printf(CS_LOG_DEFAULT, " >> RIGHT-HAND SIDE\n");
  for (cs_lnum_t i = 0; i < n_dump_rows; i++) {
    for (cs_lnum_t j = i * n_dump_cols; j < (i + 1) * n_dump_cols; j++)
      cs_log_printf(CS_LOG_DEFAULT, "%4ld % -6.4e |", (long)j, b[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }
  if (n_dump_rows * n_dump_cols < size) {
    for (cs_lnum_t j = n_dump_rows * n_dump_cols; j < size; j++)
      cs_log_printf(CS_LOG_DEFAULT, "%4ld % -6.4e |", (long)j, b[j]);
    cs_log_printf(CS_LOG_DEFAULT, "\n");
  }

  if (verbosity > 2) {

    cs_log_printf(CS_LOG_DEFAULT, " >> DIAGONAL ENTRIES\n");
    for (cs_lnum_t i = 0; i < n_dump_rows; i++) {
      for (cs_lnum_t j = i * n_dump_cols; j < (i + 1) * n_dump_cols; j++)
        cs_log_printf(CS_LOG_DEFAULT, "%4ld % -6.4e |", (long)j, dval[j]);
      cs_log_printf(CS_LOG_DEFAULT, "\n");
    }
    if (n_dump_rows * n_dump_cols < size) {
      for (cs_lnum_t j = n_dump_rows * n_dump_cols; j < size; j++)
        cs_log_printf(CS_LOG_DEFAULT, "%4ld % -6.4e |", (long)j, dval[j]);
      cs_log_printf(CS_LOG_DEFAULT, "\n");
    }

    cs_log_printf(CS_LOG_DEFAULT, " >> EXTRA-DIAGONAL ENTRIES\n");
    for (cs_lnum_t i = 0; i < size; i++) {

      const cs_lnum_t *idx       = row_index + i;
      const cs_lnum_t *_col      = col_id + idx[0];
      const cs_real_t *_val      = xval + idx[0];
      const int        n_entries = idx[1] - idx[0];

      int _n_cols = 6;
      int _n_rows = n_entries / _n_cols;

      for (cs_lnum_t ii = 0; ii < _n_rows; ii++) {
        cs_log_printf(CS_LOG_DEFAULT, "ROW%4ld >> ", (long)i);
        for (cs_lnum_t jj = ii * _n_cols; jj < (ii + 1) * _n_cols; jj++)
          cs_log_printf(
            CS_LOG_DEFAULT, "%4ld: % -6.4e |", (long)_col[jj], _val[jj]);
        cs_log_printf(CS_LOG_DEFAULT, "\n");
      }
      if (_n_rows * _n_cols < n_entries) {
        cs_log_printf(CS_LOG_DEFAULT, "ROW%4ld >> ", (long)i);
        for (cs_lnum_t jj = _n_rows * _n_cols; jj < n_entries; jj++)
          cs_log_printf(
            CS_LOG_DEFAULT, "%4ld: % -6.4e |", (long)_col[jj], _val[jj]);
        cs_log_printf(CS_LOG_DEFAULT, "\n");
      }
    }

  } /* verbosity */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Binary dump (matrix, rhs and solution) of a matrix, its right-hand
 *        side and the solution array
 *
 * \param[in] basename  name of the system
 * \param[in] matrix    matrix to dump
 * \param[in] b         right-hand side to dump
 * \param[in] x         solution array to dump
 */
/*----------------------------------------------------------------------------*/

void
cs_dbg_binary_dump_system(const char        *basename,
                          const cs_matrix_t *matrix,
                          const cs_real_t   *rhs,
                          const cs_real_t   *sol)
{
  if (matrix == nullptr)
    return;

  const cs_lnum_t n_rows          = cs_matrix_get_n_rows(matrix);
  const cs_lnum_t diag_block_size = cs_matrix_get_diag_block_size(matrix);

  int   len  = strlen(basename) + 4;
  char *name = nullptr;

  CS_MALLOC(name, len + 1, char);

  sprintf(name, "%s_mat", basename);
  cs_matrix_dump(matrix, name);

  if (rhs != nullptr) {

    sprintf(name, "%s_rhs", basename);
    cs_matrix_dump_vector(n_rows, diag_block_size, rhs, name);
  }

  if (sol != nullptr) {

    sprintf(name, "%s_sol", basename);
    cs_matrix_dump_vector(n_rows, diag_block_size, sol, name);
  }

  CS_FREE(name);
}

/*----------------------------------------------------------------------------*/
END_C_DECLS
