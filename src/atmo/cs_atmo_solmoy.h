#ifndef __CS_ATMO_SOLMOY_H__
#define __CS_ATMO_SOLMOY_H__
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_defs.h"
#include "base/cs_physical_constants.h"
#include "atmo/cs_atmo.h"
#include "base/cs_field.h"
#include "mesh/cs_mesh.h"
#include "atmo/cs_air_props.h"
#include "base/cs_math.h"
#include "alge/cs_divergence.h"
#include "bft/bft_mem.h"
#include "bft/bft_error.h"
#include "bft/bft_printf.h"
#include "base/cs_parall.h"

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*----------------------------------------------------------------------------*/
/*!
 * \file cs_atsoil.h
 *
 * \brief Header for the atmospheric soil module.
 */
/*----------------------------------------------------------------------------*/

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize ground level parameters from land use.
 */
/*----------------------------------------------------------------------------*/

void
cs_solmoy(void);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* __CS_ATMO_SOLMOY__ */
