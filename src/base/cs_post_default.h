#ifndef __CS_POST_DEFAULT_H__
#define __CS_POST_DEFAULT_H__

/*============================================================================
 * Post-processing management
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

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_defs.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*============================================================================
 * Macro definitions
 *============================================================================*/

/*============================================================================
 * Local type definitions
 *============================================================================*/

/*=============================================================================
 * Global variables
 *============================================================================*/

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Output post-processing meshes using associated writers.
 *----------------------------------------------------------------------------*/

void
cs_post_default_write_meshes(void);

/*----------------------------------------------------------------------------
 * Loop on post-processing meshes to output variables
 *----------------------------------------------------------------------------*/

void
cs_post_default_write_variables(void);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* __CS_POST_DEFAULT_H__ */
