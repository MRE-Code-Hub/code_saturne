#ifndef CS_INTERNAL_COUPLING_H
#define CS_INTERNAL_COUPLING_H

/*============================================================================
 * Internal coupling: coupling for one instance of code_saturne
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
 * PLE library headers
 *----------------------------------------------------------------------------*/

#include <ple_locator.h>

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "base/cs_defs.h"

#include "base/cs_base.h"
#include "base/cs_dispatch.h"
#include "base/cs_field.h"
#include "alge/cs_matrix_assembler.h"
#include "mesh/cs_mesh.h"
#include "mesh/cs_mesh_quantities.h"
#include "base/cs_parameters.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Macro definitions
 *============================================================================*/

/*============================================================================
 * Type definitions
 *============================================================================*/

/* Internal coupling structure definition */

typedef struct {

  /* Id */
  int id;

  /* Locator + tag for exchanging variables */
  ple_locator_t   *locator;
  int             *c_tag;

  /* Selection criteria for coupled domains */
  char  *cells_criteria;
  char  *faces_criteria;

  char *interior_faces_group_name;
  char *exterior_faces_group_name;

  /* Associated zone ids */

  int   n_volume_zones;
  int  *volume_zone_ids;

  cs_lnum_t  n_local; /* Number of faces */
  cs_lnum_t *faces_local; /* Coupling boundary faces, numbered 0..n-1 */

  cs_lnum_t  n_distant; /* Number of faces in faces_distant */
  cs_lnum_t *faces_distant; /* Distant boundary faces associated with locator */

  /* Face i is coupled in this entity if coupled_faces[i] = true */
  bool *coupled_faces;

  /* IJ vectors */
  cs_real_3_t *ci_cj_vect;

} cs_internal_coupling_t;

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return number of defined internal couplings.
 *
 * \return  number of internal couplings
 */
/*----------------------------------------------------------------------------*/

int
cs_internal_coupling_n_couplings(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define coupling volume using given selection criteria.
 *
 * Then, this volume must be separated from the rest of the domain with a wall.
 *
 * \param[in]  criteria_cells  criteria for the first group of cells
 * \param[in]  criteria_faces  criteria for faces to be joined
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_add(const char  criteria_cells[],
                         const char  criteria_faces[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define coupling volume using given criteria. Then, this volume will
 * be separated from the rest of the domain with thin walls.
 *
 * \param[in]  criteria_cells  criteria for the first group of cells
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_add_volume(const char criteria_cells[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define coupling volume using a cs_zone_t. Then, this volume will
 * be separated from the rest of the domain with thin walls.
 *
 * \param[in]  z  pointer to cs_volume_zone_t
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_add_volume_zone(const cs_zone_t *z);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define coupling volume using given cs_zone_t. Then, this volume will
 * be separated from the rest of the domain with thin walls.
 *
 * \param[in]  n_zones   number of associated volume zones
 * \param[in]  zone_ids  ids of associated volume zones
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_add_volume_zones(int        n_zones,
                                      const int  zone_ids[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Define internal coupling volume boundary group names.
 *
 * This is used only for internal couplings based on a separation of volumes
 * (cs_internal_coupling_add_volume, cs_internal_coupling_add_volume_zone,
 * cs_internal_coupling_add_volume_zones).
 *
 * The interior name is used for faces adjacent to the main volume, and
 * the exterio name for faces adjacent to the selected (exterior) volume.
 *
 * This allows filtering faces on each side of the boundary in a simpler manner.
 *
 * \param[in, out] cpl             pointer to mesh structure to modify
 * \param[in]      criteria_cells  criteria for the first group of cells
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_add_boundary_groups(cs_internal_coupling_t  *cpl,
                                         const char              *interior_name,
                                         const char              *exterior_name);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Impose wall BCs to internal coupled faces if not yet defined.
 *
 * \param[in, out] bc_type       face boundary condition type
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_bcs(int         bc_type[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Destruction of all internal coupling related structures.
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_finalize(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return the coupling associated with a given coupling_id.
 *
 * \param[in]  coupling_id  associated with a coupling entity
 *
 * \return pointer to associated coupling structure
 */
/*----------------------------------------------------------------------------*/

cs_internal_coupling_t *
cs_internal_coupling_by_id(int coupling_id);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Exchange quantities from distant to local
 * (update local using distant).
 *
 * \param[in]  cpl     pointer to coupling entity
 * \param[in]  stride  stride (e.g. 1 for double, 3 for interleaved coordinates)
 * \param[in]  distant distant values, size coupling->n_distant
 * \param[out] local   local values, size coupling->n_local
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_exchange_var(const cs_internal_coupling_t  *cpl,
                                  int                            stride,
                                  cs_real_t                      distant[],
                                  cs_real_t                      local[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Exchange variable between groups using cell id.
 *
 * \param[in]  cpl     pointer to coupling entity
 * \param[in]  stride  number of values (non interlaced) by entity
 * \param[in]  tab     variable exchanged
 * \param[out] local   local data
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_exchange_by_cell_id(const cs_internal_coupling_t  *cpl,
                                         int                            stride,
                                         const cs_real_t                tab[],
                                         cs_real_t                      local[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Exchange variable between groups using face id.
 *
 * \param[in]  cpl     pointer to coupling entity
 * \param[in]  stride  number of values (non interlaced) by entity
 * \param[in]  tab     variable exchanged
 * \param[out] local   local data
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_exchange_by_face_id(const cs_internal_coupling_t  *cpl,
                                         int                            stride,
                                         const cs_real_t                tab[],
                                         cs_real_t                      local[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Setup internal coupling related parameters.
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_setup(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize internal coupling related structures.
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_initialize(void);

/*----------------------------------------------------------------------------
 * Addition to matrix-vector product in case of internal coupling.
 *
 * parameters:
 *   exclude_diag <-- extra diagonal flag
 *   f            <-- associated field pointer
 *   x            <-- vector x in m * x = y
 *   y            <-> vector y in m * x = y
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_spmv_contribution(bool               exclude_diag,
                                       const cs_field_t  *f,
                                       const cs_real_t            *x,
                                       cs_real_t                  *y);

/*----------------------------------------------------------------------------
 * Add coupling term coordinates to matrix assembler.
 *
 * parameters:
 *   coupling_id
 *   r_g_id   <-- global row ids (per cell)
 *   ma       <-> matrix assembler
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_matrix_add_ids(int                     coupling_id,
                                    const cs_gnum_t        *r_g_id,
                                    cs_matrix_assembler_t  *ma);

/*----------------------------------------------------------------------------
 * Add coupling terms to matrix values assembly.
 *
 * parameters:
 *   f        <-- associated field
 *   db_size  <-- diagonal block size
 *   eb_size  <-- extra-diagonal block size
 *   r_g_id   <-- global row ids (per cell)
 *   mav      <-> matrix values assembler
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_matrix_add_values(const cs_field_t              *f,
                                       cs_lnum_t                      db_size,
                                       cs_lnum_t                      eb_size,
                                       const cs_gnum_t                r_g_id[],
                                       cs_matrix_assembler_values_t  *mav);

/*----------------------------------------------------------------------------
 * Return pointers to coupling components
 *
 * parameters:
 *   cpl             <-- pointer to coupling entity
 *   n_local         --> null or pointer to component n_local
 *   faces_local     --> null or pointer to component faces_local
 *   n_distant       --> null or pointer to component n_distant
 *   faces_distant   --> null or pointer to component faces_distant
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_coupled_faces(const cs_internal_coupling_t  *cpl,
                                   cs_lnum_t                     *n_local,
                                   const cs_lnum_t               *faces_local[],
                                   cs_lnum_t                     *n_distant,
                                   const cs_lnum_t               *faces_distant[]);

/*----------------------------------------------------------------------------
 * Log information about a given internal coupling entity
 *
 * parameters:
 *   cpl <-- pointer to coupling entity
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_log(const cs_internal_coupling_t  *cpl);

/*----------------------------------------------------------------------------
 * Print informations about all coupling entities
 *
 * parameters:
 *   cpl <-- pointer to coupling entity
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_dump(void);

/*----------------------------------------------------------------------------
 * Add preprocessing operations required by coupling volume using given
 * criteria.
 *
 * The volume is separated from the rest of the domain with inserted
 * boundaries.
 *
 * parameters:
 *   mesh           <-> pointer to mesh structure to modify
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_preprocess(cs_mesh_t   *mesh);

/*----------------------------------------------------------------------------
 * Define face to face mappings for internal couplings.
 *
 * parameters:
 *   mesh           <-> pointer to mesh structure to modify
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_map(cs_mesh_t   *mesh);

/*----------------------------------------------------------------------------
 * Define coupling entity using given criteria.
 *
 * parameters:
 *   f_id       <-- id of the field
 *----------------------------------------------------------------------------*/

void
cs_internal_coupling_add_entity(int        f_id);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Update internal coupling coefficients of the field of the
 * given id using given boundary exchange coefficients passed by face id.
 *
 * \param[in] f     pointer to field
 * \param[in] hbnd  boundary exchange coefficients passed by face id
 */
/*----------------------------------------------------------------------------*/

void
cs_ic_field_set_exchcoeff(const cs_field_t  *f,
                          const cs_real_t   *hbnd);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get distant data using face id at all coupling faces for a given
 * field id.
 *
 * \param[in]  field_id    field id
 * \param[in]  stride      number of values (interlaced) by entity
 * \param[in]  tab_distant exchanged data by face id
 * \param[out] tab_local   local data by face id
 */
/*----------------------------------------------------------------------------*/

void
cs_ic_field_dist_data_by_face_id(const int         field_id,
                                 int               stride,
                                 const cs_real_t   tab_distant[],
                                 cs_real_t         tab_local[]);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#if defined(__cplusplus)

/*=============================================================================
 * Public C++ functions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute scalar boundary condition coefficients for internal coupling.
 *
 * \param[in]     ctx              reference to dispatch context
 * \param[in]     bc_coeffs        associated BC coefficients structure
 * \param[in]     cpl              structure associated with internal coupling
 * \param[in]     halo_type        halo type
 * \param[in]     w_stride         stride for weighting coefficient
 * \param[in]     clip_coeff       clipping coefficient
 * \param[out]    bc_coeffs        boundary condition structure
 * \param[in]     var              gradient's base variable
 * \param[in]     c_weight         weighted gradient coefficient variable,
 *                                 or null
 */
/*----------------------------------------------------------------------------*/

void
cs_internal_coupling_update_bc_coeffs_s
(
 cs_dispatch_context           &ctx,
 const cs_field_bc_coeffs_t    *bc_coeffs,
 const cs_internal_coupling_t  *cpl,
 cs_halo_type_t                 halo_type,
 int                            w_stride,
 double                         clip_coeff,
 const cs_real_t               *var,
 const cs_real_t               *c_weight
);

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Update vector boundary condition coefficients for internal coupling.
 *
 * \param[in]      ctx              reference to dispatch context
 * \param[in]      bc_coeffs_v      boundary condition structure
 * \param[in]      cpl              structure associated with internal coupling
 * \param[in]      halo_type        halo type
 * \param[in]      clip_coeff       clipping coefficient
 * \param[in]      df_limiter       diffusion limiter array
 * \param[in]      var              gradient's base variable
 * \param[in]      c_weight         weighted gradient coefficient variable,
 *                                  or nullptr
 */
/*----------------------------------------------------------------------------*/

template <cs_lnum_t stride>
void
cs_internal_coupling_update_bc_coeffs_strided
(
 cs_dispatch_context           &ctx,
 const cs_field_bc_coeffs_t    *bc_coeffs_v,
 const cs_internal_coupling_t  *cpl,
 cs_halo_type_t                 halo_type,
 double                         clip_coeff,
 cs_real_t                     *df_limiter,
 const cs_real_t                var[][stride],
 const cs_real_t               *c_weight
);

/*----------------------------------------------------------------------------*/

#endif // __cplusplus

#endif /* CS_INTERNAL_COUPLING_H */
