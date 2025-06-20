/*============================================================================
 * Initialization and handling of Fortran-related mechanisms
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/*----------------------------------------------------------------------------
 * PLE library headers
 *----------------------------------------------------------------------------*/

#include <ple_defs.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_error.h"
#include "bft/bft_printf.h"

#include "base/cs_base.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "base/cs_base_fortran.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*============================================================================
 * Local Macro Definitions
 *============================================================================*/

/*============================================================================
 * Local Type Definitions
 *============================================================================*/

/*============================================================================
 *  Global variables
 *============================================================================*/

static FILE  *_bft_printf_file = NULL;

/*============================================================================
 * Prototypes for Fortran subroutines
 *============================================================================*/

/* Flush standard output */

extern void cs_f_flush_logs(void);

/* Print a message to standard output */

extern void
cs_f_print(char  *cs_buf_print,
           int   *msgsize);

/* Gain main log file name */

extern void
cs_f_base_log_name(int        len,
                   char      *path);

/* Initialize Fortran log files */

extern void
cs_f_open_run_log(const int  *infecr,    /* <-- value to assign to nfecra */
                  const int  *isuppr,    /* <-- supress output if 1 */
                  const int  *ierror);   /* --> error code (0 if sucess) */

/* Close log handled by Fortran: (CLose LIsting) */

extern void
cs_f_close_run_log(void);

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Print a message to standard output
 *----------------------------------------------------------------------------*/

static int
_bft_printf_c(const char     *const format,
              va_list         arg_ptr)
{
  if (_bft_printf_file != NULL)
    return vfprintf(_bft_printf_file, format, arg_ptr);
  else
    return 0;
}

/*----------------------------------------------------------------------------
 * Print a message to standard output
 *----------------------------------------------------------------------------*/

static int
_bft_printf_f(const char     *const format,
              va_list         arg_ptr)
{
  int  msgsize;

  /* Buffer for printing from C code: print to a character string, which will
     be printed to file by Fortran code.
     If Fortran output is completely replaced by C output in the future,
     we will be able to remove this step, but it is currently necessary
     so as to ensure that the same output files may be used and output
     remains ordered. */

#undef CS_BUF_PRINT_F_SIZE
#define CS_BUF_PRINT_F_SIZE 16384

  static char cs_buf_print_f[CS_BUF_PRINT_F_SIZE];

  /* Write to buffer */

#if (__STDC_VERSION__ < 199901L)
  msgsize = vsprintf (cs_buf_print_f, format, arg_ptr);
#else
  msgsize = vsnprintf (cs_buf_print_f, CS_BUF_PRINT_F_SIZE, format, arg_ptr);
#endif

  if (msgsize == -1 || msgsize > CS_BUF_PRINT_F_SIZE - 1) {
    fprintf(stderr,
            _("Fatal error: bft_printf() called on a message of size %d\n"
              "whereas the print buffer is of size %d."),
            msgsize, CS_BUF_PRINT_F_SIZE);

    /* Try to force segmentation fault (to call signal handlers);
       as stack has most likely been corrupted, this is the most
       "similar" error that allows for portable handling. */
    {
      int *_force_err = NULL;
      *_force_err = 0;
    }
    cs_exit(EXIT_FAILURE);
  }

  /* Effective output by Fortran code */

  cs_f_print(cs_buf_print_f, &msgsize);

  return msgsize;
}

/*----------------------------------------------------------------------------
 * Flush log output buffer
 *----------------------------------------------------------------------------*/

static int
_bft_printf_flush(void)
{
  if (_bft_printf_file != NULL)
    return fflush(_bft_printf_file);
  else if (bft_printf_proxy_get() == _bft_printf_f)
    cs_f_flush_logs();
  return 0;
}

/*----------------------------------------------------------------------------
 * Close C output log file.
 *----------------------------------------------------------------------------*/

static void
_close_c_log_file(void)
{
  if (_bft_printf_file != NULL && _bft_printf_file != stdout) {
    fclose(_bft_printf_file);
    _bft_printf_file = NULL;
  }
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions for Fortran API
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Get log name file information.
 *
 * When log file output is suppressed, it returns the name of the
 * bit bucket file ("/dev/null")
 *
 * parameters:
 *   len     <-- maximum string length
 *   path    --> string
 *----------------------------------------------------------------------------*/

void
cs_f_base_log_name(int        len,
                   char      *path)
{
  size_t name_l;

  size_t l = len;
  const char *name = cs_base_bft_printf_name();

  if (cs_base_bft_printf_suppressed())
#if defined(WIN32) || defined(_WIN32)
    name = "NUL";
#else
    name = "/dev/null";
#endif

  name_l = strlen(name);
  if (name_l <= l) {
    size_t i;
    memcpy(path, name, name_l);
    for (i = name_l; i < l; i++)
      path[i] = '\0';
  }
  else
    bft_error(__FILE__, __LINE__, 0,
              _("Path passed to cslogname too short for: %s"), name);
}

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Replace default bft_printf() mechanism with internal mechanism.
 *
 * This variant is designed to allow switching from C to Fortran output,
 * whithout disabling regular C stdout output when switched to Fortran.
 *
 * This allows redirecting or suppressing logging for different ranks.
 *
 * parameters:
 *   log_name    <-- base file name for log, or NULL for stdout
 *   rn_log_flag <-- redirection for ranks > 0 log:
 *                   false:  to "/dev/null" (suppressed)
 *                   true: redirected to <log_name>_n*.log" file;
 *----------------------------------------------------------------------------*/

void
cs_base_fortran_bft_printf_set(const char  *log_name,
                               bool         rn_log_flag)
{
  const char *name = NULL;
  bool suppress = false;
  int  infecr = 6, isuppr = 0, ierror = 0;

  /* C output */

  cs_base_bft_printf_init(log_name, rn_log_flag);

  name = cs_base_bft_printf_name();
  suppress = cs_base_bft_printf_suppressed();

  if (suppress == false) {

    /* Allow bypassing this with environment variable to accommodate
       some debug habits */

    const char *p = getenv("CS_LOG_TO_STDOUT");
    if (p != NULL) {
      if (atoi(p) > 0)
        name = NULL;
    }

    /* In standard case, redirect if possible */

    if (name != NULL) {

      _bft_printf_file = fopen(name, "w");

      if (_bft_printf_file == NULL)
        bft_error(__FILE__, __LINE__, errno,
                  _("It is impossible to open the default output "
                    "file:\n%s"), name);

    }

    else
      _bft_printf_file = stdout;

  }

  /* Fortran output */

  if (suppress) {
    infecr = 9;
    isuppr = 1;
#if defined(WIN32) || defined(_WIN32)
    name = "NUL";
#else
    name = "/dev/null";
#endif
  }

  cs_f_open_run_log(&infecr, &isuppr, &ierror);

  if (ierror != 0)
    bft_error(__FILE__, __LINE__, 0,
              _("Error opening file \"%s\" from Fortran."), name);

  /* Set function pointers */

  bft_printf_proxy_set(_bft_printf_c);
  bft_printf_flush_proxy_set(_bft_printf_flush);
  ple_printf_function_set(_bft_printf_c);

  /* Close C and Fortran log files upon standard or error exit routines
     (switch back to C mode in pre-exit stage to ensure flushing,
     close C file at exit). */

  cs_base_atexit_set(cs_base_fortran_bft_printf_to_c);
  atexit(_close_c_log_file);
}

/*----------------------------------------------------------------------------
 * Switch bft_printf() mechanism to C output.
 *
 * This function may only be called after cs_base_fortran_bft_printf_set()
 *----------------------------------------------------------------------------*/

void
cs_base_fortran_bft_printf_to_c(void)
{
  const char *name = cs_base_bft_printf_name();

  if (name != NULL) {

    cs_f_close_run_log();

    if (_bft_printf_file == NULL) {

      _bft_printf_file = fopen(name, "a");

      if (_bft_printf_file == NULL)
        bft_error(__FILE__, __LINE__, errno,
                  _("It is impossible to re-open the default output "
                    "file:\n%s"), name);

    }

  }

  /* Set function pointers */

  bft_printf_proxy_set(_bft_printf_c);
  ple_printf_function_set(_bft_printf_c);
}

/*----------------------------------------------------------------------------
 * Switch bft_printf() mechanism to Fortran output.
 *
 * This function may only be called after cs_base_fortran_bft_printf_set()
 *----------------------------------------------------------------------------*/

void
cs_base_fortran_bft_printf_to_f(void)
{
  const char *name = cs_base_bft_printf_name();

  if (name != NULL) {

    int nfecra = 9, isuppr = 0, ierror = 0;

    /* Close C output */

    int retval = fclose(_bft_printf_file);

    if (retval != 0)
      bft_error(__FILE__, __LINE__, 0,
                _("Error closing file \"%s\":\n\n"
                  "  %s"), name, strerror(errno));
    _bft_printf_file = NULL;

    /* Open Fortran output */

    if (cs_base_bft_printf_suppressed())
      isuppr = 1;

    cs_f_open_run_log(&nfecra, &isuppr, &ierror);

    if (ierror != 0) {
      bft_error(__FILE__, __LINE__, 0,
                _("Error opening file \"%s\" from Fortran."), name);
    }

  }

  /* Set function pointers */

  bft_printf_proxy_set(_bft_printf_f);
  ple_printf_function_set(_bft_printf_f);
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
