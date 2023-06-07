/*============================================================================
 * High-level metadata related to CDO/HHO schemes
 *============================================================================*/

/*
  This file is part of code_saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2023 EDF S.A.

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
 * Standard C library headers
 *----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "cs_log.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_param_cdo.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

/*============================================================================
 * Local structure definitions
 *============================================================================*/

/*=============================================================================
 * Local Macro definitions
 *============================================================================*/

/*============================================================================
 * Static global variables
 *============================================================================*/

cs_param_cdo_mode_t  cs_glob_param_cdo_mode = CS_PARAM_CDO_MODE_OFF;

/*============================================================================
 * Prototypes for functions intended for use only by Fortran wrappers.
 * (descriptions follow, with function bodies).
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Set the CDO mode in the Fortran part
 *
 * parameters:
 *   mode <-- -1: no CDO, 1: with CDO, 2: CDO only
 *----------------------------------------------------------------------------*/

extern void
cs_f_set_cdo_mode(int  mode);

/*============================================================================
 * Private function definitions
 *============================================================================*/


/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*=============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set the global variable storing the mode of activation to apply to
 *        CDO/HHO schemes. Deprecated way to set the CDO mode.
 */
/*----------------------------------------------------------------------------*/

void
cs_param_cdo_mode_set(cs_param_cdo_mode_t   mode)
{
  cs_glob_param_cdo_mode = mode;
  cs_f_set_cdo_mode(mode);  /* Set the CDO mode in the FORTRAN part */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get the mode of activation for the CDO/HHO schemes.
 *
 * \return the mode of activation for the CDO/HHO module
 */
/*----------------------------------------------------------------------------*/

cs_param_cdo_mode_t
cs_param_cdo_mode_get(void)
{
  return cs_glob_param_cdo_mode;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Print a welcome message indicating what is the current CDO status
 */
/*----------------------------------------------------------------------------*/

void
cs_param_cdo_log(void)
{
  switch (cs_glob_param_cdo_mode) {

  case CS_PARAM_CDO_MODE_ONLY:
    cs_log_printf(CS_LOG_DEFAULT,
                  "\n -msg- CDO/HHO module is activated *** Experimental ***"
                  "\n -msg- CDO/HHO module is in a stand-alone mode\n");
    break;

  case CS_PARAM_CDO_MODE_WITH_FV:
    cs_log_printf(CS_LOG_DEFAULT,
                  "\n -msg- CDO/HHO module is activated *** Experimental ***"
                  "\n -msg- CDO/HHO module with FV schemes mode\n");
    break;

  default:
  case CS_PARAM_CDO_MODE_OFF:
    cs_log_printf(CS_LOG_DEFAULT,
                  "\n -msg- CDO/HHO module is not activated\n");
    break;

  } /* Switch on CDO mode */
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
