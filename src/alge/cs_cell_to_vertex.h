#ifndef __CS_CELL_TO_VERTEX_H__
#define __CS_CELL_TO_VERTEX_H__

/*============================================================================
 * Cell to vertex interpolation.
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
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_base.h"

/*----------------------------------------------------------------------------*/

/*=============================================================================
 * Local Macro definitions
 *============================================================================*/

BEGIN_C_DECLS

/*============================================================================
 * Type definition
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Cell to vertex computation method
 *----------------------------------------------------------------------------*/

typedef enum {

  CS_CELL_TO_VERTEX_UNWEIGHTED,          /*!< Uniform (constant) weights */
  CS_CELL_TO_VERTEX_SHEPARD,             /*!< Shepard interpolation
                                           (weights by inverse distance) */
  CS_CELL_TO_VERTEX_LR                   /*!< Linear regression
                                           (least-squares) */

} cs_cell_to_vertex_type_t;

/*============================================================================
 *  Global variables
 *============================================================================*/

/* Short names for cell to vertex methods */

extern const char *cs_cell_to_vertex_type_name[];

/*=============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Free cell to vertex interpolation weights.
 *
 * This will force subsequent calls to rebuild those weights if needed.
 */
/*----------------------------------------------------------------------------*/

void
cs_cell_to_vertex_free(void);

END_C_DECLS

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Interpolate cell values to vertex values.
 *
 * \param[in]       method            interpolation method
 * \param[in]       verbosity         verbosity level
 * \param[in]       var_dim           variable dimension
 * \param[in]       ignore_rot_perio  if true, ignore periodicity of rotation
 * \param[in]       c_weight          cell weight, or NULL
 * \param[in]       c_var             base cell-based variable
 * \param[in]       b_var             base boundary-face values, or NULL
 * \param[out]      v_var             vertex-based variable
 */
/*----------------------------------------------------------------------------*/

template <cs_lnum_t stride>
void
cs_cell_to_vertex(cs_cell_to_vertex_type_t   method,
                  int                        verbosity,
                  bool                       ignore_rot_perio,
                  const cs_real_t            c_weight[],
                  const cs_real_t            c_var[],
                  const cs_real_t            b_var[],
                  cs_real_t                  v_var[]);

/*----------------------------------------------------------------------------*/

#endif /* __CS_CELL_TO_VERTEX__ */
