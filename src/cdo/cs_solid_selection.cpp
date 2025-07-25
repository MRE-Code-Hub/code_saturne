/*============================================================================
 * Manage the list of solid cells and associated helper functions
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
#include <string.h>

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_mem.h"

#include "base/cs_array.h"
#include "base/cs_parall.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "cdo/cs_solid_selection.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Additional doxygen documentation
 *============================================================================*/

/*!
  \file cs_solid_selection.cpp

  \brief Structure and functions handling the list of solid cells
         Useful for Navier-Stokes, thermal module or the solidification module

*/

/*=============================================================================
 * Local macro definitions
 *============================================================================*/

#define CS_SOLID_SELECTION_DBG 0

/*============================================================================
 * Type definitions
 *============================================================================*/

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*============================================================================
 * Static variables
 *============================================================================*/

cs_solid_selection_t *_cs_solid = nullptr;

/*============================================================================
 * Private function prototypes
 *============================================================================*/

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Retrieve the information related to the list of solid cells
 *        If this structure does not exist, there is an initialization.
 *
 * \return a pointer to a cs_solid_selection_t structure
 */
/*----------------------------------------------------------------------------*/

cs_solid_selection_t *
cs_solid_selection_get(void)
{
  if (_cs_solid == nullptr) {
    CS_MALLOC(_cs_solid, 1, cs_solid_selection_t);

    _cs_solid->n_cells   = 0;
    _cs_solid->n_g_cells = 0;
    _cs_solid->cell_ids  = nullptr;

    _cs_solid->cell_is_solid = nullptr;
    _cs_solid->face_is_solid = nullptr;
  }

  return _cs_solid;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Synchronize the solid selection
 *
 * \param[in] connect    pointer to a cs_cdo_connect_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_solid_selection_sync(const cs_cdo_connect_t *connect)
{
  if (_cs_solid == nullptr)
    cs_solid_selection_get(); // Allocate a structure by default

  /* Parallel synchronization of the global number of solid cells */

  _cs_solid->n_g_cells = _cs_solid->n_cells;
  cs::parall::sum(_cs_solid->n_g_cells);

  if (_cs_solid->n_g_cells > 0) {
    /* Tag cells and faces */

    if (_cs_solid->cell_is_solid == nullptr)
      CS_MALLOC(_cs_solid->cell_is_solid, connect->n_cells, bool);

    if (_cs_solid->face_is_solid == nullptr)
      CS_MALLOC(_cs_solid->face_is_solid, connect->n_faces[0], bool);

    /* Set to false all cells and all faces */

    cs_array_bool_fill_false(connect->n_cells, _cs_solid->cell_is_solid);
    cs_array_bool_fill_false(connect->n_faces[0], _cs_solid->face_is_solid);

    /* Tag cells associated to a solid cell and its related faces */

    const cs_adjacency_t *c2f = connect->c2f;

    for (cs_lnum_t i = 0; i < _cs_solid->n_cells; i++) {
      const cs_lnum_t c_id = _cs_solid->cell_ids[i];

      _cs_solid->cell_is_solid[c_id] = true;

      for (cs_lnum_t j = c2f->idx[c_id]; j < c2f->idx[c_id + 1]; j++)
        _cs_solid->face_is_solid[c2f->ids[j]] = true;

    } /* Loop on solid cells */

    /* Synchronize face tags */

    if (connect->face_ifs != nullptr) {
      assert(sizeof(bool) == sizeof(char));
      cs_interface_set_max(connect->face_ifs,
                           connect->n_faces[0],
                           1,       /* stride */
                           false,   /* interlace (not useful here) */
                           CS_CHAR, /* boolean */
                           _cs_solid->face_is_solid);
    }

  } /* n_g_cells > 0 */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Free the structure storing the information related to the list of
 *        solid cells.
 */
/*----------------------------------------------------------------------------*/

void
cs_solid_selection_free(void)
{
  if (_cs_solid == nullptr)
    return;

  CS_FREE(_cs_solid->cell_is_solid);
  CS_FREE(_cs_solid->face_is_solid);
  CS_FREE(_cs_solid->cell_ids);
  CS_FREE(_cs_solid);
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
