#ifndef CS_HALO_H
#define CS_HALO_H

/*============================================================================
 * Structure and function headers handling with ghost cells
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

#include "base/cs_defs.h"
#include "base/cs_base.h"
#include "base/cs_base_accel.h"
#include "base/cs_interface.h"
#include "base/cs_rank_neighbors.h"

#include "fvm/fvm_periodicity.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*============================================================================
 * Macro definitions
 *============================================================================*/

/*=============================================================================
 * Type definitions
 *============================================================================*/

/*!> Halo type */

typedef enum {

  CS_HALO_STANDARD,   /*!< standard halo */
  CS_HALO_EXTENDED,   /*!< extended halo (vertex-adjacent cells) */
  CS_HALO_N_TYPES

} cs_halo_type_t;

/* Halo communication mode */

typedef enum {

  CS_HALO_COMM_P2P,      /*!< non-blocking point-to-point communication */
  CS_HALO_COMM_RMA_GET   /*!< MPI-3 one-sided with get semantics and
                           active target synchronization */

} cs_halo_comm_mode_t;

/* Structure for halo management */
/* ----------------------------- */

typedef struct {

  int       n_c_domains;     /* Number of communicating domains. */
  int       n_transforms;    /* Number of periodic transformations */

  int       *c_domain_rank;  /* List of communicating ranks */

  const fvm_periodicity_t * periodicity; /* Pointer to periodicity
                                            structure describing transforms */

  int       n_rotations;     /* Number of periodic transformations
                                involving rotations */

  cs_lnum_t  n_local_elts;   /* Number of local elements */

  /* send_halo features : send to distant ranks */

  cs_lnum_t  n_send_elts[2];   /* Numer of ghost elements in send_list
                                n_elts[0] = standard elements
                                n_elts[1] = extended + standard elements */

  cs_lnum_t  *send_list;       /* List of local elements in distant halos
                                  (0 to n-1 numbering) */

  cs_lnum_t  *send_index;      /* Index on send_list
                                  Size = 2*n_c_domains + 1. For each rank, we
                                  have an index for standard halo and one
                                  for extended halo. */

  cs_lnum_t  *send_perio_lst;  /* For each transformation and for each type of
                                  halo on each communicating rank, we store
                                  2 values:
                                   - start index,
                                   - number of elements. */

  cs_lnum_t   std_send_block_size;  /* Send block size for packing */
  cs_lnum_t   n_std_send_blocks;    /* Number of send blocks for packing with
                                       standard exchange */
  cs_lnum_t  *std_send_blocks;      /* Block start and end info for packing
                                       with standard exchange */

  /* halo features : receive from distant ranks */

  cs_lnum_t  n_elts[2];       /* Numer of ghost elements in halo
                                 n_elts[0] = standard elements
                                 n_elts[1] = extended + standard elements */

  cs_lnum_t  *index;        /* Index on halo sections;
                               Size = 2*n_c_domains + 1. For each rank, we
                               have an index for the standard halo and one
                               for the extended halo. */

  cs_lnum_t  *perio_lst;    /* For each transformation and for each type of halo
                               on each communicating rank, we store 2 values:
                                 - start index,
                                 - number of elements. */

  /* Organization of perio_lst:

         -------------------------------------------------
    T1:  |   |   |   |   |   |   |   |   |   |   |   |   |
         -------------------------------------------------
          idx  n  idx  n  idx  n  idx  n  idx  n  idx  n
          ______  ______  ______  ______  ______  ______
           std     ext     std     ext     std     ext
           ___________     ___________     ___________
             rank 0          rank 1          rank 2

         -------------------------------------------------
    T2:  |   |   |   |   |   |   |   |   |   |   |   |   |
         -------------------------------------------------
          idx  n  idx  n  idx  n  idx  n  idx  n  idx  n
          ______  ______  ______  ______  ______  ______
           std     ext     std     ext     std     ext
           ___________     ___________     ___________
             rank 0          rank 1          rank 2

         -------------------------------------------------
    T3:  |   |   |   |   |   |   |   |   |   |   |   |   |
         -------------------------------------------------
          idx  n  idx  n  idx  n  idx  n  idx  n  idx  n
          ______  ______  ______  ______  ______  ______
           std     ext     std     ext     std     ext
           ___________     ___________     ___________
             rank 0          rank 1          rank 2

  etc...

  */

#if defined(HAVE_MPI)

  MPI_Group   c_domain_group;    /* Group of connected domains */
  cs_lnum_t  *c_domain_s_shift;  /* Target buffer shift for distant
                                    ranks using one-sided get */
#endif

} cs_halo_t;

/*! Structure to maintain halo exchange state */

typedef struct _cs_halo_state_t  cs_halo_state_t;

/*=============================================================================
 * Global static variables
 *============================================================================*/

END_C_DECLS
#ifdef __cplusplus

/*=============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Create a halo structure given an interface set.
 *
 * \param[in]  ifs  pointer to a cs_interface_set structure
 *
 * \return  pointer to created cs_halo_t structure
 */
/*----------------------------------------------------------------------------*/

cs_halo_t *
cs_halo_create(const cs_interface_set_t  *ifs);

/*----------------------------------------------------------------------------*/
/*
 * \brief Ready halo for use.
 *
 * This function should be called after building a halo using the
 * cs_halo_create_function and defined locally.
 * It is called automatically by cs_halo_create_from_ref and
 * cs_halo_create_from_rank_neigbors so does not need to be called again
 * using these functions.
 *
 * \param[in]  halo  pointer to halo structure
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_create_complete(cs_halo_t  *halo);

/*----------------------------------------------------------------------------*/
/*
 * \brief Create a halo structure, given a reference halo.
 *
 * \param[in]  ref  pointer to reference halo
 *
 * \return  pointer to created cs_halo_t structure
 */
/*----------------------------------------------------------------------------*/

cs_halo_t *
cs_halo_create_from_ref(const cs_halo_t  *ref);

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------*/
/*
 * \brief Create a halo structure from distant element distant ranks and ids.
 *
 * \param[in]  rn              associated rank neighbors info
 * \param[in]  n_local_elts    number of elements for local rank
 * \param[in]  n_distant_elts  number of distant elements for local rank
 * \param[in]  elt_rank_idx    distant element rank index in rank neighbors,
 *                             ordered by rank (size: n_distant_elts)
 * \param[in]  elt_id          distant element id (at distant rank),
 *                             ordered by rank (size: n_distant_elts)
 * \param[in]  elt_tr_id       distant element transform id (-1 for
 *                             non-periodic elements), null if non-periodic
 * \param[in]  periodicity     optional periodicity, or null
 *
 * \return  pointer to created cs_halo_t structure
 */
/*----------------------------------------------------------------------------*/

cs_halo_t *
cs_halo_create_from_rank_neighbors
(
  const cs_rank_neighbors_t  *rn,
  cs_lnum_t                   n_local_elts,
  cs_lnum_t                   n_distant_elts,
  const int                   elt_rank_idx[],
  const cs_lnum_t             elt_id[],
  const int16_t               elt_tr_id[],
  const fvm_periodicity_t    *periodicity
);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Create a halo structure from distant element distant ranks and ids.
 *
 * \param[in]  rn              associated rank neighbors info
 * \param[in]  n_local_elts    number of elements for local rank
 * \param[in]  n_distant_elts  number of distant elements for local rank
 * \param[in]  elt_rank_idx    distant element rank index in rank neighbors,
 *                             ordered by rank (size: n_distant_elts)
 * \param[in]  elt_tr_id       distant element transform id, or null
 *                             (-1 for non-periodic elements)
 * \param[in]  elt_id          distant element id (at distant rank),
 *                             ordered by rank (size: n_distant_elts)
 *
 * \return  pointer to created cs_halo_t structure
 */
/*----------------------------------------------------------------------------*/

cs_halo_t *
cs_halo_create_from_rank_neighbors
(
  const cs_rank_neighbors_t  *rn,
  cs_lnum_t                   n_local_elts,
  cs_lnum_t                   n_distant_elts,
  const int                   elt_rank_idx[],
  const cs_lnum_t             elt_id[]
);

#endif /* HAVE_MPI */

/*----------------------------------------------------------------------------*/
/*!
 * brief Destroy a halo structure.
 *
 * \param[in, out]  halo  pointer to pointer to cs_halo structure to destroy.
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_destroy(cs_halo_t  **halo);

/*----------------------------------------------------------------------------*/

#endif // __cplusplus
BEGIN_C_DECLS

/*----------------------------------------------------------------------------*/
/*!
 * \brief Create a halo state structure.
 *
 * \return  pointer to created cs_halo_state_t structure.
 */
/*----------------------------------------------------------------------------*/

cs_halo_state_t *
cs_halo_state_create(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Destroy a halo state structure.
 *
 * \param[in, out]  halo_state  pointer to pointer to cs_halo_state
 *                              structure to destroy.
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_state_destroy(cs_halo_state_t  **halo_state);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get pointer to default halo state structure.
 *
 * \return]  halo  pointer to pointer to cs_halo structure to destroy.
 */
/*----------------------------------------------------------------------------*/

cs_halo_state_t *
cs_halo_state_get_default(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Compute required size for packing send data into dense buffer.
 *
 * \param[in]   halo        pointer to halo structure
 * \param[in]   data_type   data type
 * \param[in]   stride       number of (interlaced) values by entity
 *
 * \return  required size, in bytes
 */
/*----------------------------------------------------------------------------*/

static inline size_t
cs_halo_pack_size(const cs_halo_t  *halo,
                  cs_datatype_t     data_type,
                  int               stride)
{
  size_t elt_size = cs_datatype_size[data_type]*stride;
  size_t pack_size = halo->n_send_elts[CS_HALO_EXTENDED] * elt_size;

  return pack_size;
}

/*----------------------------------------------------------------------------
 * Apply local cells renumbering to a halo
 *
 * parameters:
 *   halo        <-- pointer to halo structure
 *   new_cell_id <-- array indicating old -> new cell id (0 to n-1)
 *---------------------------------------------------------------------------*/

void
cs_halo_renumber_cells(cs_halo_t        *halo,
                       const cs_lnum_t   new_cell_id[]);

/*----------------------------------------------------------------------------
 * Apply ghost cells renumbering to a halo
 *
 * parameters:
 *   halo        <-- pointer to halo structure
 *   old_cell_id <-- array indicating new -> old cell id (0 to n-1)
 *---------------------------------------------------------------------------*/

void
cs_halo_renumber_ghost_cells(cs_halo_t        *halo,
                             const cs_lnum_t   old_cell_id[]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize halo state prior to packing halo data to send.
 *
 * A local state handler may be provided, or the default state handler will
 * be used.
 *
 * This function is included in \ref cs_halo_sync_pack, but may be called
 * separately for specific implementations, such as for accelerator devices.
 *
 * A local state and/or buffer may be provided, or the default (global) state
 * and buffer will be used. If provided explicitely,
 * the buffer must be of sufficient size.
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       data_type   data type
 * \param[in]       stride      number of (interlaced) values by entity
 * \param[out]      send_buf    pointer to send buffer, null for global buffer
 * \param[in, out]  hs          pointer to halo state, null for global state
 *
 * \return  pointer to halo send buffer
 */
/*----------------------------------------------------------------------------*/

void *
cs_halo_sync_pack_init_state(const cs_halo_t  *halo,
                             cs_halo_type_t    sync_mode,
                             cs_datatype_t     data_type,
                             int               stride,
                             void             *send_buf,
                             cs_halo_state_t  *hs);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Pack halo data to send into dense buffer.
 *
 * A local state handler may be provided, or the default state handler will
 * be used.
 *
 * A local state and/or buffer may be provided, or the default (global) state
 * and buffer will be used. If provided explicitely,
 * the buffer must be of sufficient size.
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       data_type   data type
 * \param[in]       stride      number of (interlaced) values by entity
 * \param[in]       val         pointer to variable value array
 * \param[out]      send_buf    pointer to send buffer, null for global buffer
 * \param[in, out]  hs          pointer to halo state, null for global state
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_sync_pack(const cs_halo_t  *halo,
                  cs_halo_type_t    sync_mode,
                  cs_datatype_t     data_type,
                  int               stride,
                  void             *val,
                  void             *send_buf,
                  cs_halo_state_t  *hs);

#if defined(HAVE_ACCEL)

/*----------------------------------------------------------------------------*/
/*!
 * \brief Pack halo data to send into dense buffer on accelerator device.
 *
 * A local state handler may be provided, or the default state handler will
 * be used.
 *
 * A local state and/or buffer may be provided, or the default (global) state
 * and buffer will be used. If provided explicitely,
 * the buffer must be of sufficient size.
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       data_type   data type
 * \param[in]       stride      number of (interlaced) values by entity
 * \param[in]       val         pointer to variable value array (on device)
 * \param[out]      send_buf    pointer to send buffer (on device),
 *                              null for global buffer
 * \param[in, out]  hs          pointer to halo state, null for global state
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_sync_pack_d(const cs_halo_t  *halo,
                    cs_halo_type_t    sync_mode,
                    cs_datatype_t     data_type,
                    int               stride,
                    void             *val,
                    void             *send_buf,
                    cs_halo_state_t  *hs);

#endif /* defined(HAVE_ACCEL) */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Launch update array of values in case of parallelism or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * The cs_halo_sync_pack function should have been called before this function,
 * using the same hs argument.
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       val         pointer to variable value array
 * \param[in, out]  hs          pointer to halo state, null for global state
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_sync_start(const cs_halo_t  *halo,
                   void             *val,
                   cs_halo_state_t  *hs);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Wait for completion of update array of values in case of
 *  parallelism or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * The cs_halo_sync_start function should have been called before this function,
 * using the same hs argument.
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       val         pointer to variable value array
 * \param[in, out]  hs          pointer to halo state, null for global state
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_sync_wait(const cs_halo_t  *halo,
                  void             *val,
                  cs_halo_state_t  *hs);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Update array of values in case of parallelism or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \param[in]   halo        pointer to halo structure
 * \param[in]   sync_mode   synchronization mode (standard or extended)
 * \param[in]   data_type   data type
 * \param[in]   stride      number of (interlaced) values by entity
 * \param[in]   val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_sync(const cs_halo_t  *halo,
             cs_halo_type_t    sync_mode,
             cs_datatype_t     data_type,
             int               stride,
             void             *val);

#if defined(HAVE_ACCEL)

/*----------------------------------------------------------------------------*/
/*!
 * \brief Update array of values on device in case of parallelism
 *        or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \param[in]   halo        pointer to halo structure
 * \param[in]   sync_mode   synchronization mode (standard or extended)
 * \param[in]   data_type   data type
 * \param[in]   stride      number of (interlaced) values by entity
 * \param[in]   val         pointer to variable value array (on device)
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_sync_d(const cs_halo_t  *halo,
               cs_halo_type_t    sync_mode,
               cs_datatype_t     data_type,
               int               stride,
               void             *val);

#endif /* defined(HAVE_ACCEL) */

/*----------------------------------------------------------------------------
 * Update array of any type of halo values in case of parallelism or
 * periodicity.
 *
 * Data is untyped; only its size is given, so this function may also
 * be used to synchronize interleaved multidimendsional data, using
 * size = element_size*dim (assuming a homogeneous environment, at least
 * as far as data encoding goes).
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * parameters:
 *   halo      <-- pointer to halo structure
 *   sync_mode <-- synchronization mode (standard or extended)
 *   size      <-- size of each element
 *   num       <-> pointer to local number value array
 *----------------------------------------------------------------------------*/

void
cs_halo_sync_untyped(const cs_halo_t  *halo,
                     cs_halo_type_t    sync_mode,
                     size_t            size,
                     void             *val);

/*----------------------------------------------------------------------------
 * Update array of integer halo values in case of parallelism or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * parameters:
 *   halo      <-- pointer to halo structure
 *   sync_mode <-- synchronization mode (standard or extended)
 *   num       <-> pointer to local number value array
 *----------------------------------------------------------------------------*/

void
cs_halo_sync_num(const cs_halo_t  *halo,
                 cs_halo_type_t    sync_mode,
                 cs_lnum_t         num[]);

/*----------------------------------------------------------------------------
 * Update array of variable (floating-point) halo values in case of
 * parallelism or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * parameters:
 *   halo      <-- pointer to halo structure
 *   sync_mode <-- synchronization mode (standard or extended)
 *   var       <-> pointer to variable value array
 *----------------------------------------------------------------------------*/

void
cs_halo_sync_var(const cs_halo_t  *halo,
                 cs_halo_type_t    sync_mode,
                 cs_real_t         var[]);

/*----------------------------------------------------------------------------
 * Update array of strided variable (floating-point) halo values in case
 * of parallelism or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * parameters:
 *   halo      <-- pointer to halo structure
 *   sync_mode <-- synchronization mode (standard or extended)
 *   var       <-> pointer to variable value array
 *   stride    <-- number of (interlaced) values by entity
 *----------------------------------------------------------------------------*/

void
cs_halo_sync_var_strided(const cs_halo_t  *halo,
                         cs_halo_type_t    sync_mode,
                         cs_real_t         var[],
                         int               stride);

/*----------------------------------------------------------------------------
 * Return MPI_Barrier usage flag.
 *
 * returns:
 *   true if MPI barriers are used after posting receives and before posting
 *   sends, false otherwise
 *---------------------------------------------------------------------------*/

bool
cs_halo_get_use_barrier(void);

/*----------------------------------------------------------------------------
 * Set MPI_Barrier usage flag.
 *
 * parameters:
 *   use_barrier <-- true if MPI barriers should be used after posting
 *                   receives and before posting sends, false otherwise.
 *---------------------------------------------------------------------------*/

void
cs_halo_set_use_barrier(bool use_barrier);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get default communication mode for halo exchange.
 *
 * \return  allocation mode
 */
/*----------------------------------------------------------------------------*/

cs_halo_comm_mode_t
cs_halo_get_comm_mode(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set default communication mode for halo exchange.
 *
 * \param[in]  mode  allocation mode to set
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_set_comm_mode(cs_halo_comm_mode_t  mode);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get default host/device allocation mode for message packing arrays.
 *
 * \return  allocation mode
 */
/*----------------------------------------------------------------------------*/

cs_alloc_mode_t
cs_halo_get_buffer_alloc_mode(void);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set default host/device allocation mode for message packing arrays.
 *
 * \param[in]  mode  allocation mode to set
 */
/*----------------------------------------------------------------------------*/

void
cs_halo_set_buffer_alloc_mode(cs_alloc_mode_t  mode);

/*----------------------------------------------------------------------------
 * Dump a cs_halo_t structure.
 *
 * parameters:
 *   halo           <--  pointer to cs_halo_t struture
 *   print_level    <--  0 only dimensions and indexes are printed, else (1)
 *                       everything is printed
 *---------------------------------------------------------------------------*/

void
cs_halo_dump(const cs_halo_t  *halo,
             int               print_level);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#if defined(__cplusplus)

/*=============================================================================
 * Public C++ functions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*
 * \brief Update array of values in case of parallelism or periodicity.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \tparam[in]      Stride      number of (interlaced) values by entity
 * \tparam[in]      T           value type
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       on_device   run on accelerated device if possible
 * \param[in, out]  val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

template <typename T>
void
cs_halo_sync(const cs_halo_t  *halo,
             cs_halo_type_t    sync_mode,
             bool              on_device,
             T                 val[]);

template <int Stride, typename T>
void
cs_halo_sync(const cs_halo_t  *halo,
             cs_halo_type_t    sync_mode,
             bool              on_device,
             T                 val[][Stride]);

/*----------------------------------------------------------------------------*/
/*
 * \brief Update array of values in case of parallelism or periodicity,
 *        using the standard neighborhood.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \tparam[in]      Stride      number of (interlaced) values by entity
 * \tparam[in]      T           value type
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       on_device   run on accelerated device if possible
 * \param[in, out]  val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

template <typename T>
void
cs_halo_sync(const cs_halo_t  *halo,
             bool              on_device,
             T                 val[]);

template <int Stride, typename T>
void
cs_halo_sync(const cs_halo_t  *halo,
             bool              on_device,
             T                 val[][Stride]);

/*----------------------------------------------------------------------------*/
/*
 * \brief Update ghost cell values of a spatial vector field,
 *        including rotational periodicity if present.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \tparam[in]      T           value type
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       on_device   run on accelerated device if possible
 * \param[in, out]  val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

template <typename T>
void
cs_halo_sync_r(const cs_halo_t  *halo,
               cs_halo_type_t    sync_mode,
               bool              on_device,
               T                 val[][3]);

/*----------------------------------------------------------------------------*/
/*
 * \brief Update ghost cell values of a spatial vector field,
 *        including rotational periodicity if present,
 *        using the standard neighborhood.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \tparam[in]      T           value type
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       on_device   run on accelerated device if possible
 * \param[in, out]  val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

template <typename T>
void
cs_halo_sync_r(const cs_halo_t  *halo,
               bool              on_device,
               T                 val[][3]);

/*----------------------------------------------------------------------------*/
/*
 * \brief Update ghost cell values of a symmetric tensor field,
 *        including rotational periodicity if present.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \tparam[in]      T           value type
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       on_device   run on accelerated device if possible
 * \param[in, out]  val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

template <typename T>
void
cs_halo_sync_r(const cs_halo_t  *halo,
               cs_halo_type_t    sync_mode,
               bool              on_device,
               T                 val[][6]);

/*----------------------------------------------------------------------------*/
/*
 * \brief Update ghost cell values of a symmetric tensor field,
 *        including rotational periodicity if present,
 *        using the standard neighborhood.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \tparam[in]      T           value type
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       on_device   run on accelerated device if possible
 * \param[in, out]  val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

template <typename T>
void
cs_halo_sync_r(const cs_halo_t  *halo,
               bool              on_device,
               T                 val[][6]);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Update ghost cell values of a non-symmetric tensor field,
 *        including rotational periodicity if present.
 *
 * This function aims at copying main values from local elements
 * (id between 1 and n_local_elements) to ghost elements on distant ranks
 * (id between n_local_elements + 1 to n_local_elements_with_halo).
 *
 * \tparam[in]      T           value type
 *
 * \param[in]       halo        pointer to halo structure
 * \param[in]       sync_mode   synchronization mode (standard or extended)
 * \param[in]       on_device   run on accelerated device if possible
 * \param[in, out]  val         pointer to variable value array
 */
/*----------------------------------------------------------------------------*/

template <typename T>
void
cs_halo_sync_r(const cs_halo_t  *halo,
               cs_halo_type_t    sync_mode,
               bool              on_device,
               T                 val[][3][3]);

/*----------------------------------------------------------------------------*/

#endif // __cplusplus

#endif /* CS_HALO_H */
