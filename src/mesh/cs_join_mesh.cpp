/*============================================================================
 * Manipulation of a cs_join_mesh_t structure
 *===========================================================================*/

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
 *---------------------------------------------------------------------------*/

#include <assert.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*----------------------------------------------------------------------------
 *  Local headers
 *---------------------------------------------------------------------------*/

#include "bft/bft_printf.h"

#include "fvm/fvm_io_num.h"
#include "fvm/fvm_nodal.h"
#include "fvm/fvm_nodal_from_desc.h"
#include "fvm/fvm_nodal_order.h"

#include "base/cs_all_to_all.h"
#include "base/cs_math.h"
#include "base/cs_mem.h"
#include "base/cs_order.h"
#include "base/cs_search.h"
#include "mesh/cs_join_post.h"
#include "mesh/cs_join_set.h"
#include "mesh/cs_join_util.h"
#include "base/cs_parall.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *---------------------------------------------------------------------------*/

#include "mesh/cs_join_mesh.h"

/*---------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*============================================================================
 * Macro and type definitions
 *===========================================================================*/

/*============================================================================
 * Private function definitions
 *===========================================================================*/

/*----------------------------------------------------------------------------
 * Return a string related to a state
 *
 * parameters:
 *   state <-- state to display
 *
 * returns:
 *   related string
 *---------------------------------------------------------------------------*/

static const char *
_print_state(cs_join_state_t  state)
{
  if (state == CS_JOIN_STATE_UNDEF)
    return "UDF";
  else if (state == CS_JOIN_STATE_NEW)
    return "NEW";
  else if (state == CS_JOIN_STATE_ORIGIN)
    return "ORI";
  else if (state == CS_JOIN_STATE_PERIO)
    return "PER";
  else if (state == CS_JOIN_STATE_MERGE)
    return "MRG";
  else if (state == CS_JOIN_STATE_PERIO_MERGE)
    return "PMG";
  else if (state == CS_JOIN_STATE_SPLIT)
    return "SPL";
  else
    return "ERR";
}

/*----------------------------------------------------------------------------
 * Compute the cross product of two vectors.
 *
 * parameters:
 *   v1 <-- first vector
 *   v2 <-- second vector
 *
 * returns:
 *   the resulting cross product (v1 x v2)
 *---------------------------------------------------------------------------*/

inline static void
_cross_product(const double  v1[],
               const double  v2[],
               double        result[])
{
  result[0] = v1[1] * v2[2] - v2[1] * v1[2];
  result[1] = v2[0] * v1[2] - v1[0] * v2[2];
  result[2] = v1[0] * v2[1] - v2[0] * v1[1];
}

/*----------------------------------------------------------------------------
 * Compute the dot product of two vectors.
 *
 * parameters:
 *   v1 <-- first vector
 *   v2 <-- second vector
 *
 * returns:
 *   the resulting dot product (v1.v2)
 *---------------------------------------------------------------------------*/

inline static double
_dot_product(const double  v1[],
             const double  v2[])
{
  int  i;
  double  result = 0.0;

  for (i = 0; i < 3; i++)
    result += v1[i] * v2[i];

  return result;
}

/*----------------------------------------------------------------------------
 * Get the norm of a vector.
 *
 * parameters:
 *  v      <->  vector to work with.
 *---------------------------------------------------------------------------*/

inline static double
_norm(double   v[])
{
  return  sqrt(_dot_product(v, v));
}

/*----------------------------------------------------------------------------
 * Compute the distance between two vertices.
 *
 * parameters:
 *   id         <-- local id of the vertex to deal with
 *   quantities <-- array keeping edge vector and its length
 *
 * returns:
 *   sine in radians between edges sharing vertex id
 *---------------------------------------------------------------------------*/

inline static double
_compute_sine(int           id,
              const double  quantities[])
{
  int  i;

  double  sine;
  double  norm_a, norm_b, a[3], b[3], c[3];

  for (i = 0; i < 3; i++) {
    a[i] = -quantities[4*id+i];
    b[i] = quantities[4*(id+1)+i];
  }

  norm_a = quantities[4*id+3];
  norm_b = quantities[4*(id+1)+3];

  _cross_product(a, b, c);

  sine = _norm(c) / (norm_a * norm_b);

  return sine;
}

/*----------------------------------------------------------------------------
 * Compute the distance between two vertices.
 *
 * parameters:
 *   a <-- coordinates of the first vertex.
 *   b <-- coordinates of the second vertex.
 *
 * returns:
 *   distance between a and b.
 *---------------------------------------------------------------------------*/

inline static double
_compute_length(const double  a[3],
                const double  b[3])
{
  double  length;

  length = sqrt(  (b[0] - a[0])*(b[0] - a[0])
                + (b[1] - a[1])*(b[1] - a[1])
                + (b[2] - a[2])*(b[2] - a[2]));

  return length;
}

/*----------------------------------------------------------------------------
 * Compute tolerance (mode 2)
 * tolerance = min[ edge length * sine(v1v2) * fraction]
 *
 * parameters:
 *   vertex_coords    <--  coordinates of vertices.
 *   vertex_tolerance <->  local tolerance affected to each vertex and
 *                         to be updated
 *   n_faces          <--  number of selected faces
 *   face_lst         <--  list of faces selected to compute the tolerance
 *   f2v_idx          <--  "face -> vertex" connect. index
 *   f2v_lst          <--  "face -> vertex" connect. list
 *   fraction         <--  parameter used to compute the tolerance
 *---------------------------------------------------------------------------*/

static void
_compute_tolerance2(const cs_real_t   vtx_coords[],
                    double            vtx_tolerance[],
                    const cs_lnum_t   n_faces,
                    const cs_lnum_t   face_lst[],
                    const cs_lnum_t   f2v_idx[],
                    const cs_lnum_t   f2v_lst[],
                    double            fraction)
{
  int  i, j, k, coord;
  double  tolerance, sine;
  double  a[3], b[3];

  int   n_max_face_vertices = 0;
  int  *face_connect = nullptr;
  double  *edge_quantities = nullptr;

  for (i = 0; i < n_faces; i++) {
    int  fid = face_lst[i] - 1;
    n_max_face_vertices = cs::max(n_max_face_vertices,
                                  f2v_idx[fid+1] - f2v_idx[fid]);
  }

  CS_MALLOC(face_connect, n_max_face_vertices + 1, int);
  CS_MALLOC(edge_quantities, 4 * (n_max_face_vertices + 1), double);

  for (i = 0; i < n_faces; i++) {

    int  face_id = face_lst[i] - 1;
    int  start = f2v_idx[face_id];
    int  end = f2v_idx[face_id + 1];
    int  n_face_vertices = end - start;

    /* Keep face connect */

    for (k = 0, j = start; j < end; j++, k++)
      face_connect[k] = f2v_lst[j];
    face_connect[k] = f2v_lst[start];

    /* Keep edge lengths and edge vectors:
        - edge_quantities[4*k+0..2] = edge vector
        - edge_quantities[4*k+3] = edge length */

    for (k = 0; k < n_face_vertices; k++) {

      for (coord = 0; coord < 3; coord++) {
        a[coord] = vtx_coords[3*face_connect[k] + coord];
        b[coord] = vtx_coords[3*face_connect[k+1] + coord];
        edge_quantities[4*(k+1)+coord] = b[coord] - a[coord];
      }
      edge_quantities[4*(k+1)+3] = _compute_length(a, b);

    }

    for (coord = 0; coord < 4; coord++)
      edge_quantities[coord] = edge_quantities[4*k+coord];

    /* Loop on the vertices of the face to update tolerance on
       each vertex */

    for (k = 0; k < n_face_vertices; k++) {

      int  vid = face_connect[k];

      tolerance = fraction * cs::min(edge_quantities[4*k+3],
                                     edge_quantities[4*(k+1)+3]);
      sine = _compute_sine(k, edge_quantities);

      vtx_tolerance[vid] = cs::min(vtx_tolerance[vid], sine*tolerance);

    }

  } /* End of loop on faces */

  CS_FREE(face_connect);
  CS_FREE(edge_quantities);
}

/*----------------------------------------------------------------------------
 * Compute tolerance (mode 1)
 * tolerance = shortest edge length * fraction
 *
 * parameters:
 *   vertex_coords    <--  coordinates of vertices.
 *   vertex_tolerance <->  local tolerance affected to each vertex and
 *                         to be updated
 *   n_faces          <--  number of selected faces
 *   face_lst         <--  list of faces selected to compute the tolerance
 *   face_vtx_idx     <--  "face -> vertex" connect. index
 *   face_vtx_lst     <--  "face -> vertex" connect. list
 *   fraction         <--  parameter used to compute the tolerance
 *---------------------------------------------------------------------------*/

static void
_compute_tolerance1(const cs_real_t   vtx_coords[],
                    double            vtx_tolerance[],
                    const cs_lnum_t   n_faces,
                    const cs_lnum_t   face_lst[],
                    const cs_lnum_t   face_vtx_idx[],
                    const cs_lnum_t   face_vtx_lst[],
                    double            fraction)
{
  cs_lnum_t  i, j, k, start, end, face_id, vtx_id1, vtx_id2;
  double  length, tolerance;
  double  a[3], b[3];

  for (i = 0; i < n_faces; i++) {

    face_id = face_lst[i] - 1;
    start = face_vtx_idx[face_id];
    end = face_vtx_idx[face_id + 1];

    /* Loop on the vertices of the face */

    for (j = start; j < end - 1; j++) {

      vtx_id1 = face_vtx_lst[j];
      vtx_id2 = face_vtx_lst[j+1];

      for (k = 0; k < 3; k++) {
        a[k] = vtx_coords[3*vtx_id1 + k];
        b[k] = vtx_coords[3*vtx_id2 + k];
      }

      length = _compute_length(a, b);
      tolerance = length * fraction;
      vtx_tolerance[vtx_id1] = cs::min(vtx_tolerance[vtx_id1], tolerance);
      vtx_tolerance[vtx_id2] = cs::min(vtx_tolerance[vtx_id2], tolerance);

    }

    /* Case end - start */

    vtx_id1 = face_vtx_lst[end-1];
    vtx_id2 = face_vtx_lst[start];

    for (k = 0; k < 3; k++) {
      a[k] = vtx_coords[3*vtx_id1 + k];
      b[k] = vtx_coords[3*vtx_id2 + k];
    }

    length = _compute_length(a, b);
    tolerance = length * fraction;
    vtx_tolerance[vtx_id1] = cs::min(vtx_tolerance[vtx_id1], tolerance);
    vtx_tolerance[vtx_id2] = cs::min(vtx_tolerance[vtx_id2], tolerance);

  } /* End of loop on faces */
}

/*----------------------------------------------------------------------------
 * Define for each vertex a tolerance which is the radius of the
 * sphere in which the vertex can be fused with another vertex.
 * This tolerance is computed from the given list of faces (interior or border)
 *
 * parameters:
 *   param            <--  set of user-defined parameters for the joining
 *   vertex_coords    <--  coordinates of vertices.
 *   n_faces          <--  number of selected faces
 *   face_lst         <--  list of faces selected to compute the tolerance
 *   face_vtx_idx     <--  "face -> vertex" connect. index
 *   face_vtx_lst     <--  "face -> vertex" connect. list
 *   vertex_tolerance <->  local tolerance affected to each vertex and
 *                         to be updated
 *---------------------------------------------------------------------------*/

static void
_get_local_tolerance(cs_join_param_t  param,
                     const cs_real_t  vtx_coords[],
                     const cs_lnum_t  n_faces,
                     const cs_lnum_t  face_lst[],
                     const cs_lnum_t  face_vtx_idx[],
                     const cs_lnum_t  face_vtx_lst[],
                     double           vtx_tolerance[])
{
  if (param.tcm % 10 == 1) {

    /* tol = min(edge length * fraction) */

    _compute_tolerance1(vtx_coords,
                        vtx_tolerance,
                        n_faces,
                        face_lst,
                        face_vtx_idx,
                        face_vtx_lst,
                        param.fraction);

  }
  else if (param.tcm % 10 == 2) {

    /* tol = min(edge length * sin(v1v2) * fraction) */

    _compute_tolerance2(vtx_coords,
                        vtx_tolerance,
                        n_faces,
                        face_lst,
                        face_vtx_idx,
                        face_vtx_lst,
                        param.fraction);

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              "  Tolerance computation mode (%d) is not defined\n",
              param.tcm);
}

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Exchange local vertex tolerances to get a global vertex tolerance.
 *
 * parameters:
 *   n_vertices        <-- number of local selected vertices
 *   select_vtx_io_num <-- fvm_io_num_t structure for the selected vertices
 *   vertex_data       <-> data associated to each selected vertex
 *---------------------------------------------------------------------------*/

static void
_get_global_tolerance(cs_lnum_t            n_vertices,
                      const fvm_io_num_t  *select_vtx_io_num,
                      cs_join_vertex_t     vtx_data[])
{
  double  *g_vtx_tolerance = nullptr, *part_tolerance = nullptr;
  cs_gnum_t  n_g_vertices = fvm_io_num_get_global_count(select_vtx_io_num);
  const cs_gnum_t  *part_gnum = fvm_io_num_get_global_num(select_vtx_io_num);

  MPI_Comm  mpi_comm = cs_glob_mpi_comm;
  const int  local_rank = cs::max(cs_glob_rank_id, 0);
  const int  n_ranks = cs_glob_n_ranks;

  cs_block_dist_info_t
    bi = cs_block_dist_compute_sizes(local_rank,
                                     n_ranks,
                                     1,
                                     0,
                                     n_g_vertices);

  cs_all_to_all_t
    *d = cs_all_to_all_create_from_block(n_vertices,
                                         0, /* flags */
                                         part_gnum,
                                         bi,
                                         mpi_comm);

  /* Send the global numbering for each vertex */

  cs_gnum_t *block_gnum = cs_all_to_all_copy_array(d,
                                                   1,
                                                   false, /* reverse */
                                                   part_gnum);

  cs_lnum_t n_recv = cs_all_to_all_n_elts_dest(d);

  /* Send the vertex tolerance for each vertex */

  CS_MALLOC(part_tolerance, n_vertices, double);

  for (cs_lnum_t i = 0; i < n_vertices; i++)
    part_tolerance[i] = vtx_data[i].tolerance;

  double *recv_tolerance = cs_all_to_all_copy_array(d,
                                                    1,
                                                    false, /* reverse */
                                                    part_tolerance);

  /* Define the global tolerance array */

  CS_MALLOC(g_vtx_tolerance, bi.block_size, double);

  for (cs_lnum_t i = 0; i < bi.block_size; i++)
    g_vtx_tolerance[i] = DBL_MAX;

  const cs_gnum_t  first_vtx_gnum = bi.gnum_range[0];

  for (cs_lnum_t i = 0; i < n_recv; i++) {
    cs_lnum_t vtx_id = block_gnum[i] - first_vtx_gnum;
    g_vtx_tolerance[vtx_id] = cs::min(g_vtx_tolerance[vtx_id], recv_tolerance[i]);
  }

  /* Replace local vertex tolerance by the new computed global tolerance */

  for (cs_lnum_t i = 0; i < n_recv; i++) {
    cs_lnum_t vtx_id = block_gnum[i] - first_vtx_gnum;
    recv_tolerance[i] = g_vtx_tolerance[vtx_id];
  }

  CS_FREE(g_vtx_tolerance);

  cs_all_to_all_copy_array(d,
                           CS_DOUBLE,
                           1,
                           true, /* reverse */
                           recv_tolerance,
                           part_tolerance);

  for (cs_lnum_t i = 0; i < n_vertices; i++)
    vtx_data[i].tolerance = part_tolerance[i];

  /* Free memory */

  CS_FREE(part_tolerance);
  CS_FREE(recv_tolerance);
  CS_FREE(block_gnum);

  cs_all_to_all_destroy(&d);
}
#endif /* HAVE_MPI */

/*----------------------------------------------------------------------------
 * Define for each vertex a tolerance which is the radius of the
 * sphere in which the vertex can be fused with another vertex.
 * This tolerance is computed from the given list of faces (interior or border)
 *
 * parameters:
 *   param        <-- set of user-defined parameters for the joining
 *   selection    <-- pointer to a struct. keeping selected entities
 *   b_f2v_idx    <-- border "face -> vertex" connectivity index
 *   b_f2v_lst    <-- border "face -> vertex" connectivity
 *   i_f2v_idx    <-- interior "face -> vertex" connectivity index
 *   i_f2v_lst    <-- interior "face -> vertex" connectivity
 *   n_vertices   <-- number of vertices in the parent mesh
 *   vtx_coord    <-- coordinates of vertices in parent mesh
 *   vtx_gnum     <-- global numbering of vertices
 *
 * returns:
 *  a pointer to an array of cs_join_vertex_t
 *---------------------------------------------------------------------------*/

static cs_join_vertex_t *
_define_vertices(cs_join_param_t        param,
                 cs_join_select_t      *selection,
                 const cs_lnum_t        b_f2v_idx[],
                 const cs_lnum_t        b_f2v_lst[],
                 const cs_lnum_t        i_f2v_idx[],
                 const cs_lnum_t        i_f2v_lst[],
                 const cs_lnum_t        n_vertices,
                 const cs_real_t        vtx_coord[],
                 const cs_gnum_t        vtx_gnum[])
{
  int  i;

  cs_join_vertex_t  *vertices = nullptr;

  assert(selection != nullptr);

  const int  n_ranks = cs_glob_n_ranks;

  /*
     Define a tolerance around each vertex in the selection list.
     Tolerance is the radius of the sphere in which the vertex can be merged
     with another vertex. Radius is the min(fraction * edge_length) on all
     edges connected to a vertex.
     Store all data about a vertex in a cs_join_vertex_t structure.
  */

  if (selection->n_vertices > 0) {

    /* Initialize vertices array */

    CS_MALLOC(vertices, selection->n_vertices, cs_join_vertex_t);

#if defined(DEBUG) && !defined(NDEBUG)
    /* Avoid Valgrind warnings in byte copies due to padding */
    memset(vertices, 0, selection->n_vertices*sizeof(cs_join_vertex_t));
#endif

    for (i = 0; i < selection->n_vertices; i++) {

      cs_lnum_t  vtx_id = selection->vertices[i]-1;

      if (n_ranks > 1)
        vertices[i].gnum = vtx_gnum[vtx_id];
      else
        vertices[i].gnum = vtx_id + 1;

      vertices[i].coord[0] = vtx_coord[3*vtx_id];
      vertices[i].coord[1] = vtx_coord[3*vtx_id+1];
      vertices[i].coord[2] = vtx_coord[3*vtx_id+2];
      vertices[i].tolerance = 0.0;                  /* Default value */
      vertices[i].state = CS_JOIN_STATE_ORIGIN;     /* Default value */

    }


    /* Compute the tolerance for each vertex of the mesh */

    if (param.fraction > 0.0) {

      double  *vtx_tolerance = nullptr;

      CS_MALLOC(vtx_tolerance, n_vertices, double);

      for (i = 0; i < n_vertices; i++)
        vtx_tolerance[i] = DBL_MAX;

      /* Define local tolerance */

      _get_local_tolerance(param,
                           vtx_coord,
                           selection->n_faces,
                           selection->faces,
                           b_f2v_idx,
                           b_f2v_lst,
                           vtx_tolerance);

      if (param.tcm / 10 == 0) {

        /* Update local tolerance with adjacent border faces */

        _get_local_tolerance(param,
                             vtx_coord,
                             selection->n_b_adj_faces,
                             selection->b_adj_faces,
                             b_f2v_idx,
                             b_f2v_lst,
                             vtx_tolerance);

        /* Update local tolerance with adjacent interior faces */

        _get_local_tolerance(param,
                             vtx_coord,
                             selection->n_i_adj_faces,
                             selection->i_adj_faces,
                             i_f2v_idx,
                             i_f2v_lst,
                             vtx_tolerance);

      } /* Include adjacent faces in the computation of the vertex tolerance */

      for (i = 0; i < selection->n_vertices; i++)
        vertices[i].tolerance = vtx_tolerance[selection->vertices[i]-1];

      CS_FREE(vtx_tolerance);

    } /* End if tolerance > 0.0 */

#if 0 && defined(DEBUG) && !defined(NDEBUG)   /* Sanity check */
  for (i = 0; i < selection->n_vertices; i++)
    if (vertices[i].tolerance > (DBL_MAX - 1.))
      bft_error(__FILE__, __LINE__, 0,
                "Incompatible value for the \"vertex tolerance\" item\n"
                "Value must be lower than DBL_MAX and current value is : %f"
                " (global numbering : %u)\n",
                vertices[i].tolerance, vertices[i].gnum);
#endif

  } /* End if selection->n_vertices > 0 */

#if defined(HAVE_MPI)
  if (n_ranks > 1) { /* Parallel treatment : synchro over the ranks */

    /* Global number of selected vertices and associated
       fvm_io_num_t structure */

    fvm_io_num_t  *select_vtx_io_num = fvm_io_num_create(selection->vertices,
                                                         vtx_gnum,
                                                         selection->n_vertices,
                                                         0);

    selection->n_g_vertices = fvm_io_num_get_global_count(select_vtx_io_num);

    _get_global_tolerance(selection->n_vertices,
                          select_vtx_io_num,
                          vertices);

    if (param.verbosity > 1)
      bft_printf(_("  Global number of selected vertices: %11llu\n\n"),
                 (unsigned long long)(selection->n_g_vertices));

    fvm_io_num_destroy(select_vtx_io_num);

  }
#endif /* defined(HAVE_MPI) */

  if (n_ranks == 1) {
    selection->n_g_vertices = selection->n_vertices;

    if (param.verbosity > 1)
      bft_printf(_("  Number of selected vertices: %11llu\n\n"),
                 (unsigned long long)(selection->n_g_vertices));

  }

  return vertices;
}

#if defined(HAVE_MPI)
/*----------------------------------------------------------------------------
 * Find for each face of the list its related rank
 *
 * parameters:
 *   n_elts     <-- number of elements in the glob. list
 *   glob_list  <-- global numbering list (must be ordered)
 *   rank_index <-- index defining a range of global numbers related
 *                  to a rank
 *
 * returns:
 *   an array of size n_elts
 *---------------------------------------------------------------------------*/

static int *
_get_rank_from_index(cs_lnum_t        n_elts,
                     const cs_gnum_t  glob_list[],
                     const cs_gnum_t  rank_index[])
{
  cs_lnum_t  i, rank;

  int  *rank_list = nullptr;

  if (n_elts == 0)
    return nullptr;

  CS_MALLOC(rank_list, n_elts, int);

  for (i = 0, rank = 0; i < n_elts; i++) {

    for (;rank_index[rank+1] < glob_list[i]; rank++);

    assert(rank < cs_glob_n_ranks);
    rank_list[i] = rank;

  } /* End of loop on elements */

  return rank_list;
}

/*----------------------------------------------------------------------------
 * Get the index on ranks and the list of faces to send from a list of global
 * faces to receive.
 *
 * parameters:
 *   gnum_rank_index <-- index on ranks for the global elements
 *   n_elts          <-- number of elements to get
 *   glob_list       <-- global number of faces to get (ordered)
 *   n_send          --> number of face/rank couples to send
 *   send_rank       --> list of ranks for the faces to send
 *   send_faces      --> list of face ids to send
 *---------------------------------------------------------------------------*/

static void
_get_send_faces(const cs_gnum_t   gnum_rank_index[],
                cs_lnum_t         n_elts,
                const cs_gnum_t   glob_list[],
                cs_lnum_t        *n_send,
                int              *send_rank[],
                cs_lnum_t        *send_faces[])
{
  cs_lnum_t  *_send_faces = nullptr;

  MPI_Comm  comm = cs_glob_mpi_comm;

  const int  local_rank = cs::max(cs_glob_rank_id, 0);

  /* Sanity checks */

  assert(gnum_rank_index != nullptr);

  /* Find for each element of the list, the rank which owns the element */

  int *gface_ranks = _get_rank_from_index(n_elts, glob_list, gnum_rank_index);

  cs_gnum_t first_gface_id = gnum_rank_index[local_rank];

  int flags = CS_ALL_TO_ALL_NEED_SRC_RANK;

  cs_all_to_all_t
    *d = cs_all_to_all_create(n_elts,
                              flags,
                              nullptr,  /* dest_id */
                              gface_ranks,
                              comm);

  cs_all_to_all_transfer_dest_rank(d, &gface_ranks);

  /* Exchange list of global num. to exchange */

  cs_gnum_t *gfaces_to_send = cs_all_to_all_copy_array(d,
                                                       1,
                                                       false, /* reverse */
                                                       glob_list);

  cs_lnum_t _n_send = cs_all_to_all_n_elts_dest(d);

  int *_send_rank = cs_all_to_all_get_src_rank(d);

  cs_all_to_all_destroy(&d);

  CS_MALLOC(_send_faces, _n_send, cs_lnum_t);

  /* Define face ids to send */

  for (cs_lnum_t i = 0; i < _n_send; i++)
    _send_faces[i] = gfaces_to_send[i] - 1 - first_gface_id;

  /* Free memory */

  CS_FREE(gfaces_to_send);

  /* Set return pointers */

  *n_send = _n_send;
  *send_rank = _send_rank;
  *send_faces = _send_faces;
}

#endif /* HAVE_MPI */

/*----------------------------------------------------------------------------
 * Clean the given cs_join_mesh_t structure: remove empty edges.
 *
 * parameters:
 *   mesh      <-> pointer to the cs_join_mesh_t structure to clean
 *   verbosity <-- level of display
 *---------------------------------------------------------------------------*/

static void
_remove_empty_edges(cs_join_mesh_t  *mesh,
                    int              verbosity)
{
  cs_lnum_t  i, j, n_face_vertices;

  cs_lnum_t  shift = 0, n_simplified_faces = 0;
  cs_lnum_t  *new_face_vtx_idx = nullptr;

  assert(mesh != nullptr);

  CS_MALLOC(new_face_vtx_idx, mesh->n_faces + 1, cs_lnum_t);

  new_face_vtx_idx[0] = 0;

  for (i = 0; i < mesh->n_faces; i++) {

    cs_lnum_t  s = mesh->face_vtx_idx[i];
    cs_lnum_t  e = mesh->face_vtx_idx[i+1];

    if (mesh->face_vtx_lst[e-1] != mesh->face_vtx_lst[s])
      mesh->face_vtx_lst[shift++] = mesh->face_vtx_lst[s];

    /* Loop on face vertices */

    for (j = s; j < e - 1; j++)
      if (mesh->face_vtx_lst[j] != mesh->face_vtx_lst[j+1])
        mesh->face_vtx_lst[shift++] = mesh->face_vtx_lst[j+1];

    new_face_vtx_idx[i+1] = shift;

    n_face_vertices = new_face_vtx_idx[i+1] - new_face_vtx_idx[i];

    if (n_face_vertices < e - s) {

      n_simplified_faces++;
      if (verbosity > 3)
        bft_printf("  Simplified face %ld (%llu)\n", (long)i+1,
                   (unsigned long long)(mesh->face_gnum[i]));

      if (n_face_vertices < 3)
        bft_error(__FILE__, __LINE__, 0,
                  _("  The simplified face has less than 3 vertices.\n"
                    "  Check your joining parameters.\n"
                    "  Face %ld (%llu)\n"), (long)i+1,
                  (unsigned long long)(mesh->face_gnum[i]));
    }

  } /* End of loop on faces */

  CS_FREE(mesh->face_vtx_idx);
  mesh->face_vtx_idx = new_face_vtx_idx;

  CS_REALLOC(mesh->face_vtx_lst,
              new_face_vtx_idx[mesh->n_faces], cs_lnum_t);

  if (verbosity > 1) {
    cs_gnum_t n_g_simplified_faces = n_simplified_faces;
    cs_parall_counter(&n_g_simplified_faces, 1);
    bft_printf(_("\n  Number of simplified faces: %llu\n"),
               (unsigned long long)n_simplified_faces);
  }
}

/*----------------------------------------------------------------------------
 * Clean the given cs_join_mesh_t structure: remove degenerate edges.
 *
 * parameters:
 *   mesh      <-> pointer to the cs_join_mesh_t structure to clean
 *   verbosity <-- level of display
 *---------------------------------------------------------------------------*/

static void
_remove_degenerate_edges(cs_join_mesh_t  *mesh,
                         int              verbosity)
{
  /*
    - In the definition of faces based on new edges, a same edge may be
      traversed twice, in the opposite direction; this is due to merging
      of edges, as shown below.

       x                                      x
       |\                                     |
       | \                                    |
     a2|  \a3                               A2|
       |   \                                  |
       |    \      a4       Merge of          |
    ---s1----s2------       vertices          x
       |      \             s1 and s2        / \
       |       \                            /   \
     a1|        \a4                      A1/     \A3
       |         \                        /       \
       |          \                      /         \
       x-----------x                    x-----------x
            a5                                A4


    Face: a1 a2 a3 a4 a5            Face: A1 A2 -A2 A3 A4


   Caution:    the final configuration may be
               A2 A1 A3 A4 -A2
               where the references of edges to delete may be at the
               beginning or end of the face definition

   Remark:     several edge pairs may possibly be referenced twice,
               in the form
               ... A1 A2 -A2 -A1 ... (where the removal of A2 will
               make ... A1 -A1 ... appear); We thus run as many passes
               as necessary on a given face.
  */

  assert(mesh != nullptr);

  cs_lnum_t  i, j, k, count, n_face_vertices;

  cs_lnum_t  shift = 0;
  cs_lnum_t  n_faces = mesh->n_faces;
  cs_lnum_t  n_modified_faces = 0;
  cs_gnum_t  n_g_modified_faces = 0;
  cs_join_rset_t  *tmp = nullptr;
  cs_join_rset_t  *kill = nullptr;

  tmp = cs_join_rset_create(8);
  kill = cs_join_rset_create(8);

  for (i = 0; i < n_faces; i++) {

    cs_lnum_t  start_id = mesh->face_vtx_idx[i];
    cs_lnum_t  end_id = mesh->face_vtx_idx[i+1];
    cs_lnum_t  n_init_vertices = end_id - start_id;
    cs_lnum_t  n_elts = n_init_vertices + 2;

    assert(n_init_vertices > 2);

    /* Build a temporary list based on the face connectivity */

    cs_join_rset_resize(&tmp, n_elts);
    cs_join_rset_resize(&kill, n_elts);

    for (j = start_id, k = 0; j < end_id; j++, k++) {
      tmp->array[k] = mesh->face_vtx_lst[j] + 1;
      kill->array[k] = 0;
    }

    tmp->array[k] = mesh->face_vtx_lst[start_id] + 1;
    kill->array[k++] = 0;
    tmp->array[k] = mesh->face_vtx_lst[start_id+1] + 1;
    kill->array[k++] = 0;

    assert(n_elts == k);
    tmp->n_elts = n_elts;
    kill->n_elts = n_elts;

    /* Find degenerate edges */

    count = 1;
    n_face_vertices = n_init_vertices;

    while (count > 0) {

      count = 0;
      for (j = 0; j < n_face_vertices; j++) {
        if (tmp->array[j] == tmp->array[j+2]) {
          count++;
          kill->array[j] = 1;
          kill->array[(j+1)%n_face_vertices] = 1;
        }
      }

      tmp->n_elts = 0;
      for (j = 0; j < n_face_vertices; j++) {
        if (kill->array[j] == 0)
          tmp->array[tmp->n_elts++] = tmp->array[j];
      }

      n_face_vertices = tmp->n_elts;
      tmp->array[tmp->n_elts++] = tmp->array[0];
      tmp->array[tmp->n_elts++] = tmp->array[1];

      kill->n_elts = tmp->n_elts;
      for (j = 0; j < kill->n_elts; j++)
        kill->array[j] = 0;

    } /* End of while */

    if (n_face_vertices != n_init_vertices) {

      n_modified_faces += 1;

      /* Display the degenerate face */

      if (verbosity > 5) {

        bft_printf("\n  Remove edge for face: %ld [%llu]:",
                   (long)i+1, (unsigned long long)(mesh->face_gnum[i]));
        bft_printf("\n    Initial def: ");
        for (j = start_id; j < end_id; j++) {
          cs_lnum_t  v_id = mesh->face_vtx_lst[j];
          bft_printf(" %ld (%llu) ", (long)v_id+1,
                     (unsigned long long)(mesh->vertices[v_id].gnum));
        }
        bft_printf("\n    Final def:   ");
        for (j = 0; j < n_face_vertices; j++) {
          cs_lnum_t  v_id = tmp->array[j] - 1;
          bft_printf(" %ld (%llu) ", (long)v_id+1,
                     (unsigned long long)(mesh->vertices[v_id].gnum));
        }
        bft_printf("\n");
        bft_printf_flush();

      }

      if (n_face_vertices < 3)
        bft_error(__FILE__, __LINE__, 0,
                  _("  The simplified face has less than 3 vertices.\n"
                    "  Check your joining parameters.\n"
                    "  Face %ld (%llu)\n"), (long)i+1,
                  (unsigned long long)(mesh->face_gnum[i]));

    } /* End if n_face_vertices != n_init_vertices */

    for (j = 0; j < n_face_vertices; j++)
      mesh->face_vtx_lst[shift++] = tmp->array[j] - 1;
    mesh->face_vtx_idx[i] = shift;

  } /* End of loop on faces */

  n_g_modified_faces = n_modified_faces;
  cs_parall_counter(&n_g_modified_faces, 1);

  if (verbosity > 0)
    bft_printf("\n  Edge removed for %llu faces (global).\n"
               "  Join mesh cleaning done.\n",
               (unsigned long long)n_g_modified_faces);

  for (i = n_faces; i > 0; i--)
    mesh->face_vtx_idx[i] = mesh->face_vtx_idx[i-1];
  mesh->face_vtx_idx[0] = 0;

  CS_REALLOC(mesh->face_vtx_lst, mesh->face_vtx_idx[n_faces], cs_lnum_t);

  /* Free memory */

  cs_join_rset_destroy(&tmp);
  cs_join_rset_destroy(&kill);
}

/*----------------------------------------------------------------------------
 * Count the number of new vertices to add in the new face definition
 *
 * parameters:
 *   v1_id            <-- first vertex id
 *   v2_id            <-- second vertex id
 *   old2new          <-- indirection array between old and new numbering
 *   edges            <-- cs_join_edges_t structure
 *   edge_index       <-- edge -> new added vertex connectivity index
 *   edge_new_vtx_lst <-- edge -> new added vertex connectivity list
 *
 * returns:
 *   a number of vertices to add
 *---------------------------------------------------------------------------*/

static int
_count_new_added_vtx_to_edge(cs_lnum_t               v1_id,
                             cs_lnum_t               v2_id,
                             const cs_lnum_t         old2new[],
                             const cs_join_edges_t  *edges,
                             const cs_lnum_t         edge_index[],
                             const cs_lnum_t         edge_new_vtx_lst[])
{
  cs_lnum_t  i, edge_id, edge_num;

  cs_lnum_t  new_v1_id = old2new[v1_id];
  cs_lnum_t  new_v2_id = old2new[v2_id];
  cs_lnum_t  n_adds = 0;

  assert(v1_id >= 0);
  assert(v2_id >= 0);
  assert(new_v1_id >= 0);
  assert(new_v2_id >= 0);
  assert(edge_index != nullptr);
  assert(edges != nullptr);

  /* Find the related edge */

  edge_num = cs_join_mesh_get_edge(v1_id+1, v2_id+1, edges);
  edge_id = cs::abs(edge_num) - 1;

  if (v1_id == v2_id)
    bft_error(__FILE__, __LINE__, 0,
              _("\n Problem in mesh connectivity.\n"
                " Detected when updating connectivity.\n"
                " Edge number: %ld (%llu) - (%ld, %ld) in old numbering.\n"),
              (long)edge_num, (unsigned long long)(edges->gnum[edge_id]),
              (long)v1_id, (long)v2_id);

  /* Add the first vertex (new_v1_id) */

  n_adds = 1;

  /* Add another vertices if needed */

  for (i = edge_index[edge_id]; i < edge_index[edge_id+1]; i++) {

    cs_lnum_t  new_vtx_id = edge_new_vtx_lst[i] - 1;

    if (new_vtx_id != new_v1_id && new_vtx_id != new_v2_id)
      n_adds++;

  }

  return n_adds;
}

/*----------------------------------------------------------------------------
 * Add new vertex to the face -> vertex connectivity
 *
 * parameters:
 *   v1_id            <-- first vertex id
 *   v2_id            <-- second vertex id
 *   old2new          <-- indirection array between old and new numbering
 *   edges            <-- cs_join_edges_t structure
 *   edge_index       <-- edge -> new added vertex connectivity index
 *   edge_new_vtx_lst <-- edge -> new added vertex connectivity list
 *   new_face_vtx_lst <-> new face -> vertex connectivity list
 *   p_shift          <-> pointer to the shift in the connectivity list
 *---------------------------------------------------------------------------*/

static void
_add_new_vtx_to_edge(cs_lnum_t               v1_id,
                     cs_lnum_t               v2_id,
                     const cs_lnum_t         old2new[],
                     const cs_join_edges_t  *edges,
                     const cs_lnum_t         edge_index[],
                     const cs_lnum_t         edge_new_vtx_lst[],
                     cs_lnum_t               new_face_vtx_lst[],
                     cs_lnum_t              *p_shift)
{
  cs_lnum_t  new_v1_id = old2new[v1_id];
  cs_lnum_t  shift = *p_shift;

  assert(edges != nullptr);

  /* Add first vertex num to the connectivity list */

  new_face_vtx_lst[shift++] = new_v1_id;

  if (edge_new_vtx_lst != nullptr) {

    cs_lnum_t  i, edge_id, edge_num, e_start, e_end;

    cs_lnum_t  new_v2_id = old2new[v2_id];

    /* Find the related edge */

    edge_num = cs_join_mesh_get_edge(v1_id+1, v2_id+1, edges);
    edge_id = cs::abs(edge_num) - 1;
    e_start = edge_index[edge_id];
    e_end = edge_index[edge_id+1];

    /* Add a vertex if needed */

    if (edge_num > 0) {

      for (i = e_start; i < e_end; i++) {

        cs_lnum_t  new_vtx_id = edge_new_vtx_lst[i] - 1;

        if (new_vtx_id != new_v1_id && new_vtx_id != new_v2_id)
          new_face_vtx_lst[shift++] = new_vtx_id;

      }
    }
    else { /* edge_num < 0 */

      for (i = e_end - 1; i > e_start - 1; i--) {

        cs_lnum_t  new_vtx_id = edge_new_vtx_lst[i] - 1;

        if (new_vtx_id != new_v1_id && new_vtx_id != new_v2_id)
          new_face_vtx_lst[shift++] = new_vtx_id;

      }

    } /* End if edge_num < 0 */

  } /* End if edge_new_vtx_lst != nullptr */

  /* Return pointer */

  *p_shift = shift;
}

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Dump a cs_join_vertex_t structure into a file.
 *
 * parameters:
 *   f      <-- handle to output file
 *   vertex <-- cs_join_vertex_t structure to dump
 *---------------------------------------------------------------------------*/

static void
_log_vertex(cs_join_vertex_t  vertex)
{
  assert(vertex.gnum > 0);
  assert(vertex.tolerance >= 0.0);

  bft_printf(" %10llu | %11.6f | % 12.10e  % 12.10e  % 12.10e | %s\n",
             (unsigned long long)vertex.gnum, vertex.tolerance,
             vertex.coord[0], vertex.coord[1], vertex.coord[2],
             _print_state(vertex.state));
}

/*----------------------------------------------------------------------------
 * Create a MPI_Datatype for the cs_join_vertex_t structure.
 *
 * returns:
 *   a MPI_Datatype associated to the cs_join_vertex_t structure
 *---------------------------------------------------------------------------*/

static MPI_Datatype
_create_vtx_datatype(void)
{
  int  j;
  cs_join_vertex_t  v_data;
  MPI_Datatype  new_type;

  int  blocklengths[4] = {1, 1, 1, 3};
  MPI_Aint  displacements[4] = {0 , 0, 0, 0};
  MPI_Datatype  types[4] = {MPI_INT,  CS_MPI_GNUM, MPI_DOUBLE, CS_MPI_COORD};

  /* Initialize vertex structure */

  v_data.state = CS_JOIN_STATE_UNDEF;
  v_data.gnum = 1;
  v_data.tolerance = 0.0;
  for (j = 0; j < 3; j++)
    v_data.coord[j] = 0.0;

  /* Define array of displacements */

#if defined(MPI_VERSION) && (MPI_VERSION >= 2)
  MPI_Get_address(&v_data, displacements);
  MPI_Get_address(&v_data.gnum, displacements + 1);
  MPI_Get_address(&v_data.tolerance, displacements + 2);
  MPI_Get_address(&v_data.coord, displacements + 3);
#else
  MPI_Address(&v_data, displacements);
  MPI_Address(&v_data.gnum, displacements + 1);
  MPI_Address(&v_data.tolerance, displacements + 2);
  MPI_Address(&v_data.coord, displacements + 3);
#endif

  displacements[3] -= displacements[0];
  displacements[2] -= displacements[0];
  displacements[1] -= displacements[0];
  displacements[0] = 0;

  /* Create new datatype */

#if (MPI_VERSION >= 2)
  MPI_Type_create_struct(4, blocklengths, displacements, types, &new_type);
#else
  MPI_Type_struct(4, blocklengths, displacements, types, &new_type);
#endif

  MPI_Type_commit(&new_type);

  return new_type;
}

/*----------------------------------------------------------------------------
 * Create a function to define an operator for MPI reduction operation
 *
 * parameters:
 *   in        <--  input vertices
 *   inout     <->  in/out vertices (vertex with the min. tolerance)
 *   len       <--  size of input array
 *   datatype  <--  MPI_datatype associated to cs_join_vertex_t
 *---------------------------------------------------------------------------*/

static void
_mpi_vertex_min(cs_join_vertex_t   *in,
                cs_join_vertex_t   *inout,
                int                *len,
                MPI_Datatype       *datatype)
{
  CS_UNUSED(datatype);

  int  i, j;

  assert(in != nullptr && inout != nullptr);

  for (i = 0; i < *len; i++) {

    if (in->tolerance <= inout->tolerance) {

      if (in->tolerance < inout->tolerance) {

        inout->gnum = in->gnum;
        for (j = 0; j < 3; j++)
          inout->coord[j] = in->coord[j];
        inout->tolerance = in->tolerance;
        inout->state = in->state;

      }
      else {

        if (in->gnum < inout->gnum) {
          inout->gnum = in->gnum;
          for (j = 0; j < 3; j++)
            inout->coord[j] = in->coord[j];
          inout->tolerance = in->tolerance;
          inout->state = in->state;
        }

      }

    } /* in.tol <= inout.tol */

  } /* End of loop on array */

}

/*----------------------------------------------------------------------------
 * Create a function to define an operator for MPI reduction operation
 *
 * parameters:
 *   in        <--  input vertices
 *   inout     <->  in/out vertices (vertex with the max. toelrance)
 *   len       <--  size of input array
 *   datatype  <--  MPI_datatype associated to cs_join_vertex_t
 *---------------------------------------------------------------------------*/

static void
_mpi_vertex_max(cs_join_vertex_t   *in,
                cs_join_vertex_t   *inout,
                int                *len,
                MPI_Datatype       *datatype)
{
  CS_UNUSED(datatype);

  int  i, j;

  assert(in != nullptr && inout != nullptr);

  for (i = 0; i < *len; i++) {

    if (in->tolerance >= inout->tolerance) {

      if (in->tolerance > inout->tolerance) {

        inout->gnum = in->gnum;
        for (j = 0; j < 3; j++)
          inout->coord[j] = in->coord[j];
        inout->tolerance = in->tolerance;
        inout->state = in->state;

      }
      else {

        if (in->gnum < inout->gnum) {
          inout->gnum = in->gnum;
          for (j = 0; j < 3; j++)
            inout->coord[j] = in->coord[j];
          inout->tolerance = in->tolerance;
          inout->state = in->state;

        }

      }

    } /* in.tol >= inout.tol */

  } /* End of loop on array */

}

#endif /* HAVE_MPI */

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions
 *===========================================================================*/

/*----------------------------------------------------------------------------
 * Allocate and initialize a new cs_join_mesh_t structure.
 *
 * parameters:
 *   name <-- name of the mesh
 *
 * returns:
 *   a pointer to a cs_join_mesh_t structure.
 *---------------------------------------------------------------------------*/

cs_join_mesh_t *
cs_join_mesh_create(const char  *name)
{
  cs_join_mesh_t  *new_mesh = nullptr;

  CS_MALLOC(new_mesh, 1, cs_join_mesh_t);

  if (name != nullptr) {

    int  len = strlen(name);

    CS_MALLOC(new_mesh->name, len + 1, char);
    strncpy(new_mesh->name, name, len);
    new_mesh->name[len] = '\0';

  }
  else
    new_mesh->name = nullptr;

  new_mesh->n_faces = 0;
  new_mesh->n_g_faces = 0;
  new_mesh->face_gnum = nullptr;
  new_mesh->face_vtx_idx = nullptr;
  new_mesh->face_vtx_lst = nullptr;
  new_mesh->n_vertices = 0;
  new_mesh->n_g_vertices = 0;
  new_mesh->vertices = nullptr;

  return new_mesh;
}

/*----------------------------------------------------------------------------
 * Get a cs_join_mesh_t structure with the given list of global faces inside.
 *
 * Exchange between ranks to get the connectivity associated to each
 * face of the global numbering list.
 *
 * parameters:
 *   mesh_name       <-- name of the created mesh
 *   n_elts          <-- number of elements in the global list
 *   glob_sel        <-- list of global elements (ordered)
 *   gnum_rank_index <-- index on ranks for the global elements
 *   local_mesh      <-- pointer to the local part of the distributed
 *                       cs_join_mesh_t structure on selected elements
 *
 * returns:
 *   a pointer to a new allocated cs_join_mesh_t structure
 *---------------------------------------------------------------------------*/

cs_join_mesh_t *
cs_join_mesh_create_from_glob_sel(const char            *mesh_name,
                                  cs_lnum_t              n_elts,
                                  const cs_gnum_t        glob_sel[],
                                  const cs_gnum_t        gnum_rank_index[],
                                  const cs_join_mesh_t  *local_mesh)
{
  cs_join_mesh_t  *new_mesh = nullptr;

  const int  n_ranks = cs_glob_n_ranks;

  if (n_ranks == 1) {

    cs_lnum_t  i;
    cs_lnum_t *loc_sel = nullptr;

    CS_MALLOC(loc_sel, n_elts, cs_lnum_t);

    for (i = 0; i < n_elts; i++)
      loc_sel[i] = glob_sel[i];

    new_mesh =  cs_join_mesh_create_from_subset(mesh_name,
                                                n_elts,
                                                loc_sel,
                                                local_mesh);

    CS_FREE(loc_sel);
  }

#if defined(HAVE_MPI)

  else { /* Parallel mode */

    int *send_rank = nullptr;
    cs_lnum_t   n_send_faces = 0;
    cs_lnum_t  *send_faces = nullptr;

    new_mesh = cs_join_mesh_create(mesh_name);

    /* Define a send list (face ids to send) from the global list of faces
       to receive. */

    _get_send_faces(gnum_rank_index,
                    n_elts,
                    glob_sel,
                    &n_send_faces,
                    &send_rank,
                    &send_faces);

    /* Get useful connectivity on ranks for computing local intersections */

    cs_join_mesh_exchange(n_send_faces,
                          send_rank,
                          send_faces,
                          local_mesh,
                          new_mesh,
                          cs_glob_mpi_comm);

    CS_FREE(send_faces);
    CS_FREE(send_rank);

    cs_join_mesh_face_order(new_mesh);
  }

#endif

  return new_mesh;
}

/*----------------------------------------------------------------------------
 * Allocate and define a cs_join_mesh_t structure relative to an extraction
 * of selected faces.
 *
 * The selection must be ordered.
 *
 * parameters:
 *   mesh_name   <-- name of the name to create
 *   subset_size <-- number of selected faces in the subset
 *   selection   <-- list of selected faces. Numbering in parent mesh
 *   parent_mesh <-- parent cs_join_mesh_t structure
 *
 * returns:
 *   a pointer to a cs_join_mesh_t structure
 *---------------------------------------------------------------------------*/

cs_join_mesh_t *
cs_join_mesh_create_from_subset(const char            *mesh_name,
                                cs_lnum_t              subset_size,
                                const cs_lnum_t        selection[],
                                const cs_join_mesh_t  *parent_mesh)
{
  cs_lnum_t  i, j, shift, parent_id, start, end;

  cs_lnum_t  n_select_vertices = 0;
  cs_lnum_t  *select_vtx_id = nullptr;

  cs_join_mesh_t  *mesh = nullptr;

  assert(parent_mesh != nullptr);

  /* Get the selected vertices relative to the subset selection */

  CS_MALLOC(select_vtx_id, parent_mesh->n_vertices, cs_lnum_t);

  for (i = 0; i < parent_mesh->n_vertices; i++)
    select_vtx_id[i] = -1;

  for (i = 0; i < subset_size; i++) {

    parent_id = selection[i] - 1;

    for (j = parent_mesh->face_vtx_idx[parent_id];
         j < parent_mesh->face_vtx_idx[parent_id+1];
         j++) {
      select_vtx_id[parent_mesh->face_vtx_lst[j]] = 0;
    }

  }

  n_select_vertices = 0;
  for (i = 0; i < parent_mesh->n_vertices; i++) {
    if (select_vtx_id[i] > -1)
      select_vtx_id[i] = n_select_vertices++;
  }

  /* Create a new cs_join_mesh_t structure */

  mesh = cs_join_mesh_create(mesh_name);

  mesh->n_faces = subset_size;

  /* Build face_vtx_idx, and face global numbering */

  CS_MALLOC(mesh->face_vtx_idx, mesh->n_faces + 1, cs_lnum_t);
  CS_MALLOC(mesh->face_gnum, mesh->n_faces, cs_gnum_t);

  for (i = 0; i < mesh->n_faces; i++) {

    parent_id = selection[i] - 1;

    mesh->face_vtx_idx[i+1] =  parent_mesh->face_vtx_idx[parent_id+1]
                             - parent_mesh->face_vtx_idx[parent_id];
    mesh->face_gnum[i] = parent_mesh->face_gnum[parent_id];

  }

  mesh->face_vtx_idx[0] = 0;
  for (i = 0; i < mesh->n_faces; i++)
    mesh->face_vtx_idx[i+1] += mesh->face_vtx_idx[i];

  CS_MALLOC(mesh->face_vtx_lst,
            mesh->face_vtx_idx[mesh->n_faces], cs_lnum_t);

  /* Build face_vtx_lst */

  for (i = 0; i < mesh->n_faces; i++) {

    parent_id = selection[i] - 1;
    start = parent_mesh->face_vtx_idx[parent_id];
    end = parent_mesh->face_vtx_idx[parent_id+1];
    shift = mesh->face_vtx_idx[i];

    for (j = start; j < end; j++) {

      mesh->face_vtx_lst[shift + j - start]
        = select_vtx_id[parent_mesh->face_vtx_lst[j]];

    }

  } /* End of loop on selected faces */

  /* Define vertices */

  mesh->n_vertices = n_select_vertices;

  CS_MALLOC(mesh->vertices, n_select_vertices, cs_join_vertex_t);

  n_select_vertices = 0;
  for (i = 0; i < parent_mesh->n_vertices; i++) {
    if (select_vtx_id[i] > -1)
      mesh->vertices[n_select_vertices++] = parent_mesh->vertices[i];
  }

  /* Define global numbering */

  if (cs_glob_n_ranks == 1) {

    mesh->n_g_faces = mesh->n_faces;
    mesh->n_g_vertices = mesh->n_vertices;

  }
  else {

    fvm_io_num_t  *io_num = nullptr;
    cs_gnum_t  *vtx_gnum = nullptr;

    const cs_gnum_t  *io_gnum = nullptr;

    /* Get the global number of faces in the subset */

    io_num = fvm_io_num_create(nullptr, mesh->face_gnum, subset_size, 0);

    mesh->n_g_faces = fvm_io_num_get_global_count(io_num);

    io_num = fvm_io_num_destroy(io_num);

    /* Get the global number of vertices in the subset */

    CS_MALLOC(vtx_gnum, mesh->n_vertices, cs_gnum_t);

    for (i = 0; i < mesh->n_vertices; i++)
      vtx_gnum[i] = mesh->vertices[i].gnum;

    io_num = fvm_io_num_create(nullptr, vtx_gnum, mesh->n_vertices, 0);

    mesh->n_g_vertices = fvm_io_num_get_global_count(io_num);

    io_gnum = fvm_io_num_get_global_num(io_num);

    for (i = 0; i < mesh->n_vertices; i++)
      mesh->vertices[i].gnum = io_gnum[i];

    io_num = fvm_io_num_destroy(io_num);

    CS_FREE(vtx_gnum);
  }

  /* Free memory */

  CS_FREE(select_vtx_id);

  /* Order faces by increasing global number */

  cs_join_mesh_face_order(mesh);

  return  mesh;
}

/*----------------------------------------------------------------------------
 * Define a cs_join_mesh_t structure from a selection of faces and its
 * related vertices.
 *
 * parameters:
 *   name       <-- mesh name of the resulting cs_join_mesh_t structure
 *   param      <-- set of user-defined parameters for the joining
 *   selection  <-> selected entities
 *   b_f2v_idx  <-- border "face -> vertex" connectivity index
 *   b_f2v_lst  <-- border "face -> vertex" connectivity
 *   i_f2v_idx  <-- interior "face -> vertex" connectivity index
 *   i_f2v_lst  <-- interior "face -> vertex" connectivity
 *   n_vertices <-- number of vertices in the parent mesh
 *   vtx_coord  <-- coordinates of vertices in parent mesh
 *   vtx_gnum   <-- global numbering of vertices
 *
 * returns:
 *   a pointer to a cs_join_mesh_t structure
 *---------------------------------------------------------------------------*/

cs_join_mesh_t *
cs_join_mesh_create_from_select(const char              *name,
                                const cs_join_param_t    param,
                                cs_join_select_t        *selection,
                                const cs_lnum_t          b_f2v_idx[],
                                const cs_lnum_t          b_f2v_lst[],
                                const cs_lnum_t          i_f2v_idx[],
                                const cs_lnum_t          i_f2v_lst[],
                                const cs_lnum_t          n_vertices,
                                const cs_real_t          vtx_coord[],
                                const cs_gnum_t          vtx_gnum[])
{
  int  i, j, id, face_id, start, end, shift;

  cs_join_vertex_t  *vertices = nullptr;
  cs_join_mesh_t  *mesh = nullptr;

  assert(selection != nullptr);

  mesh = cs_join_mesh_create(name);

  /* Define face connectivity */

  mesh->n_faces = selection->n_faces;
  mesh->n_g_faces = selection->n_g_faces;

  /* Define face_vtx_idx */

  CS_MALLOC(mesh->face_vtx_idx, selection->n_faces + 1, cs_lnum_t);

  for (i = 0; i < selection->n_faces; i++) {
    face_id = selection->faces[i] - 1;
    mesh->face_vtx_idx[i+1] = b_f2v_idx[face_id+1] - b_f2v_idx[face_id];
  }

  mesh->face_vtx_idx[0] = 0;
  for (i = 0; i < selection->n_faces; i++)
    mesh->face_vtx_idx[i+1] += mesh->face_vtx_idx[i];

  CS_MALLOC(mesh->face_vtx_lst,
            mesh->face_vtx_idx[mesh->n_faces], cs_lnum_t);

  /* Define face_vtx_lst */

  for (i = 0; i < selection->n_faces; i++) {

    shift = mesh->face_vtx_idx[i];
    face_id = selection->faces[i] - 1;
    start = b_f2v_idx[face_id];
    end = b_f2v_idx[face_id+1];

    for (j = 0; j < end - start; j++) {

      id = cs_search_binary(selection->n_vertices,
                            b_f2v_lst[start + j] + 1,
                            selection->vertices);

      mesh->face_vtx_lst[shift + j] = id;

    }

  } /* End of loop on selected faces */

  /* Define global face numbering */

  CS_MALLOC(mesh->face_gnum, mesh->n_faces, cs_gnum_t);

  if (selection->compact_face_gnum == nullptr)
    for (i = 0; i < selection->n_faces; i++)
      mesh->face_gnum[i] = selection->faces[i];
  else
    for (i = 0; i < selection->n_faces; i++)
      mesh->face_gnum[i] = selection->compact_face_gnum[i];

  /* Define vertices */

  vertices = _define_vertices(param,
                              selection,
                              b_f2v_idx,
                              b_f2v_lst,
                              i_f2v_idx,
                              i_f2v_lst,
                              n_vertices,
                              vtx_coord,
                              vtx_gnum);

  mesh->n_vertices = selection->n_vertices;
  mesh->n_g_vertices = selection->n_g_vertices;
  mesh->vertices = vertices;

  return  mesh;
}

/*----------------------------------------------------------------------------
 * Destroy a cs_join_mesh_t structure.
 *
 * parameters:
 *  mesh <->  pointer to pointer to cs_join_mesh_t structure to destroy
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_destroy(cs_join_mesh_t  **mesh)
{
  if (*mesh != nullptr) {

    cs_join_mesh_t *m = *mesh;

    CS_FREE(m->name);
    CS_FREE(m->face_vtx_idx);
    CS_FREE(m->face_vtx_lst);
    CS_FREE(m->face_gnum);
    CS_FREE(m->vertices);
    CS_FREE(*mesh);

  }
}

/*----------------------------------------------------------------------------
 * Re-initialize an existing cs_join_mesh_t structure.
 *
 * parameters:
 *   mesh <-> pointer to a cs_join_mesh_t structure
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_reset(cs_join_mesh_t  *mesh)
{
  if (mesh == nullptr)
    return;

  mesh->n_faces = 0;
  mesh->n_g_faces = 0;

  CS_FREE(mesh->face_gnum);
  CS_FREE(mesh->face_vtx_lst);
  CS_FREE(mesh->face_vtx_idx);

  mesh->n_vertices = 0;
  mesh->n_g_vertices = 0;

  CS_FREE(mesh->vertices);
}

/*----------------------------------------------------------------------------
 * Copy a cs_join_mesh_t structure into another.
 *
 * parameters:
 *   mesh     <-> pointer to a cs_join_mesh_t structure to fill
 *   ref_mesh <-- pointer to the reference
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_copy(cs_join_mesh_t        **mesh,
                  const cs_join_mesh_t   *ref_mesh)
{
  cs_lnum_t  i;
  cs_join_mesh_t  *_mesh = *mesh;

  if (ref_mesh == nullptr) {
    cs_join_mesh_destroy(mesh);
    return;
  }

  if (_mesh == nullptr)
    _mesh = cs_join_mesh_create(ref_mesh->name);

  _mesh->n_faces = ref_mesh->n_faces;
  _mesh->n_g_faces = ref_mesh->n_g_faces;

  CS_REALLOC(_mesh->face_gnum, _mesh->n_faces, cs_gnum_t);
  CS_REALLOC(_mesh->face_vtx_idx, _mesh->n_faces + 1, cs_lnum_t);

  _mesh->face_vtx_idx[0] = 0;

  for (i = 0; i < _mesh->n_faces; i++) {
    _mesh->face_gnum[i] = ref_mesh->face_gnum[i];
    _mesh->face_vtx_idx[i+1] = ref_mesh->face_vtx_idx[i+1];
  }

  CS_REALLOC(_mesh->face_vtx_lst,
             _mesh->face_vtx_idx[_mesh->n_faces],
             cs_lnum_t);

  for (i = 0; i < _mesh->face_vtx_idx[_mesh->n_faces]; i++)
    _mesh->face_vtx_lst[i] = ref_mesh->face_vtx_lst[i];

  _mesh->n_vertices = ref_mesh->n_vertices;
  _mesh->n_g_vertices = ref_mesh->n_g_vertices;

  CS_REALLOC(_mesh->vertices, _mesh->n_vertices, cs_join_vertex_t);

  memcpy(_mesh->vertices,
         ref_mesh->vertices,
         _mesh->n_vertices*sizeof(cs_join_vertex_t));

  /* Set return pointer */

  *mesh = _mesh;
}

/*----------------------------------------------------------------------------
 * Compute the global min/max tolerance defined on vertices and display it
 *
 * parameters:
 *   param <-- user-defined parameters for the joining algorithm
 *   mesh  <-- pointer to a cs_join_mesh_t structure
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_minmax_tol(cs_join_param_t    param,
                        cs_join_mesh_t    *mesh)
{
  cs_lnum_t  i;
  cs_join_vertex_t  _min, _max, g_min, g_max;

  assert(mesh != nullptr);

  const int  n_ranks = cs_glob_n_ranks;

  _min.state = CS_JOIN_STATE_UNDEF;
  _max.state = CS_JOIN_STATE_UNDEF;
  _min.gnum = 0;
  _max.gnum = 0;
  _min.tolerance = DBL_MAX;
  _max.tolerance = -DBL_MAX;
  for (i = 0; i < 3; i++) {
    _min.coord[i] = DBL_MAX;
    _max.coord[i] = DBL_MAX;
  }

  g_min = _min;
  g_max = _max;

  /* Compute local min/max */

  if (mesh->n_vertices > 0) {

    for (i = 0; i < mesh->n_vertices; i++) {

      if (_min.tolerance > mesh->vertices[i].tolerance)
        _min = mesh->vertices[i];
      if (_max.tolerance < mesh->vertices[i].tolerance)
        _max = mesh->vertices[i];

    }

    if (param.verbosity > 3) {
      fprintf(cs_glob_join_log,
              "\n  Local min/max. tolerance:\n\n"
              " Glob. Num. |  Tolerance  |              Coordinates\n");
      cs_join_mesh_dump_vertex(cs_glob_join_log, _min);
      cs_join_mesh_dump_vertex(cs_glob_join_log, _max);
    }

  }

#if defined(HAVE_MPI)
#if !defined(_WIN32)
  if (n_ranks > 1) {

    MPI_Datatype  MPI_JOIN_VERTEX = _create_vtx_datatype();
    MPI_Op   MPI_Vertex_min, MPI_Vertex_max;

    MPI_Op_create((MPI_User_function  *)_mpi_vertex_min,
                  true, &MPI_Vertex_min);
    MPI_Op_create((MPI_User_function  *)_mpi_vertex_max,
                  false, &MPI_Vertex_max);

    MPI_Allreduce(&_min, &g_min, 1, MPI_JOIN_VERTEX, MPI_Vertex_min,
                  cs_glob_mpi_comm);
    MPI_Allreduce(&_max, &g_max, 1, MPI_JOIN_VERTEX, MPI_Vertex_max,
                  cs_glob_mpi_comm);

    bft_printf(_("  Global min/max. tolerance:\n\n"
                 " Glob. Num. |  Tolerance  |              Coordinates\n\n"));
    _log_vertex(g_min);
    _log_vertex(g_max);

    MPI_Op_free(&MPI_Vertex_min);
    MPI_Op_free(&MPI_Vertex_max);
    MPI_Type_free(&MPI_JOIN_VERTEX);

  }
#endif
#endif

}


#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Get the connectivity of a list of global elements distributed over the
 * ranks.
 *
 * parameters:
 *   n_send     <-- number of face/rank couples to send
 *   send_rank  <-- index on ranks for the face distribution
 *   send_faces <-- list of face ids to send
 *   send_mesh  <-- pointer to the sending cs_join_mesh_t structure
 *   recv_mesh  <-> pointer to the receiving cs_join_mesh_t structure
 *   comm       <-- mpi communicator on which take places comm.
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_exchange(cs_lnum_t              n_send,
                      const int              send_rank[],
                      const cs_lnum_t        send_faces[],
                      const cs_join_mesh_t  *send_mesh,
                      cs_join_mesh_t        *recv_mesh,
                      MPI_Comm               comm)
{
  assert(send_mesh != nullptr);
  assert(recv_mesh != nullptr);

  /* Try to use datatype larger than char if for join vertex type
     to reduce risk of reaching maximum index size with cs_lnum_t
     (with current size of 48 bytes, it would take more than
     40 million vertices in the connectvity to approach
     the 2 Gb limit, but a safety factor of 4 or 8 is still better).
  */

  size_t vtx_t_size = sizeof(cs_join_vertex_t);
  cs_datatype_t  vtx_type = CS_CHAR;

  if (vtx_t_size % 8 == 0) {
    vtx_t_size /= 8;
    vtx_type = CS_UINT64;
  }
  else if (vtx_t_size % 4 == 0) {
    vtx_t_size /= 4;
    vtx_type = CS_UINT32;
  }

  /* Sanity checks */

  assert(send_mesh != nullptr);
  assert(recv_mesh != nullptr);
  assert(send_rank != nullptr || n_send == 0);

  cs_all_to_all_t *d
    = cs_all_to_all_create(n_send,
                           CS_ALL_TO_ALL_ORDER_BY_SRC_RANK,  /* needed ? */
                           nullptr,                             /* dest_id */
                           send_rank,
                           comm);

  /* The mesh doesn't change from a global point of view.
     It's only a redistribution of the elements according to the send_faces
     list. */

  recv_mesh->n_g_faces = send_mesh->n_g_faces;
  recv_mesh->n_g_vertices = send_mesh->n_g_vertices;

  /* Exchange face connect. count */

  cs_lnum_t *send_index;
  CS_MALLOC(send_index, n_send+1, cs_lnum_t);
  send_index[0] = 0;

  cs_gnum_t *send_gbuf;
  CS_MALLOC(send_gbuf, n_send*2, cs_gnum_t);

  for (cs_lnum_t i = 0; i < n_send; i++) {

    cs_lnum_t face_id = send_faces[i];
    cs_lnum_t n_f_vtx =   send_mesh->face_vtx_idx[face_id+1]
                        - send_mesh->face_vtx_idx[face_id];

    send_gbuf[i*2] = send_mesh->face_gnum[face_id];
    send_gbuf[i*2+1] = n_f_vtx;

    send_index[i+1] = send_index[i] + n_f_vtx;

  } /* End of loop on elements to send */

  cs_gnum_t *recv_gbuf = cs_all_to_all_copy_array(d,
                                                  2,
                                                  false, /* reverse */
                                                  send_gbuf);

  CS_FREE(send_gbuf);

  /* Update cs_join_mesh_t structure */

  cs_lnum_t n_recv = cs_all_to_all_n_elts_dest(d);

  recv_mesh->n_faces = n_recv;

  CS_MALLOC(recv_mesh->face_gnum, n_recv, cs_gnum_t);
  CS_MALLOC(recv_mesh->face_vtx_idx, n_recv + 1, cs_lnum_t);

  /* Scan recv_gbuf to build face->vertex connect. index */

  recv_mesh->face_vtx_idx[0] = 0;

  for (cs_lnum_t i = 0; i < n_recv; i++) {

    recv_mesh->face_gnum[i] = recv_gbuf[i*2];
    recv_mesh->face_vtx_idx[i+1] =   recv_mesh->face_vtx_idx[i]
                                   + recv_gbuf[i*2+1];

  } /* End of loop on received elements */

  CS_FREE(recv_gbuf);

  /* Store vtx_shift data into send_count in order to re-use it */

  cs_join_vertex_t *send_vbuf;
  CS_MALLOC(send_vbuf, send_index[n_send], cs_join_vertex_t);

  /* Exchange vertex buffers */

  for (cs_lnum_t i = 0; i < n_send; i++) {

    cs_lnum_t face_id = send_faces[i];

    cs_lnum_t s_id = send_mesh->face_vtx_idx[face_id];
    cs_lnum_t e_id = send_mesh->face_vtx_idx[face_id+1];

    size_t shift = send_index[i];

    for (cs_lnum_t j = s_id; j < e_id; j++) {
      cs_lnum_t vtx_id = send_mesh->face_vtx_lst[j];
      send_vbuf[shift++] = send_mesh->vertices[vtx_id];
    }

  }

  for (cs_lnum_t i = 0; i < n_send; i++)
    send_index[i+1] *= vtx_t_size;
  for (cs_lnum_t i = 0; i < n_recv; i++)
    recv_mesh->face_vtx_idx[i+1] *= vtx_t_size;

  /* /!\ Use non templated version since "cs_join_vertex_t" is not
   * a base type (its a struct!)
   */
  recv_mesh->vertices = static_cast<cs_join_vertex_t *>(
      cs_all_to_all_copy_indexed(d,
                                 vtx_type,
                                 false, /* reverse */
                                 send_index,
                                 send_vbuf,
                                 recv_mesh->face_vtx_idx,
                                 nullptr)
      );


  for (cs_lnum_t i = 0; i < n_recv; i++)
    recv_mesh->face_vtx_idx[i+1] /= vtx_t_size;

  CS_FREE(send_vbuf);
  CS_FREE(send_index);

  /* Update cs_join_mesh_t structure */

  recv_mesh->n_vertices = recv_mesh->face_vtx_idx[n_recv];

  cs_lnum_t recv_lst_size = recv_mesh->face_vtx_idx[n_recv];
  CS_MALLOC(recv_mesh->face_vtx_lst, recv_lst_size, cs_lnum_t);

  for (cs_lnum_t i = 0; i < recv_lst_size; i++)
    recv_mesh->face_vtx_lst[i] = i;

  /* Delete vertices which appear several times */

  cs_join_mesh_vertex_clean(recv_mesh);

  /* Free memory */

  cs_all_to_all_destroy(&d);
}

#endif /* HAVE_MPI */

/*----------------------------------------------------------------------------
 * Destroy a cs_join_edges_t structure.
 *
 * parameters:
 *   edges <->  pointer to pointer to cs_join_edges_t structure to destroy
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_destroy_edges(cs_join_edges_t  **edges)
{
  if (*edges != nullptr) {

    cs_join_edges_t  *e = *edges;

    if (e->n_edges > 0) {

      CS_FREE(e->gnum);
      CS_FREE(e->def);
    }

    CS_FREE(e->vtx_idx);
    CS_FREE(e->adj_vtx_lst);
    CS_FREE(e->edge_lst);

    CS_FREE(*edges);
  }
}

/*----------------------------------------------------------------------------
 * Order a cs_join_mesh_t structure according to its global face numbering
 * Delete redundancies.
 *
 * parameters:
 *   mesh <-> pointer to a cs_join_mesh_t structure to order
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_face_order(cs_join_mesh_t  *mesh)
{
  int  i, j, o_id;
  cs_lnum_t  shift, start, end, n_new_faces;
  cs_gnum_t  prev, cur;

  cs_lnum_t  n_faces = mesh->n_faces;
  cs_lnum_t  *num_buf = nullptr,  *selection = nullptr;
  cs_lnum_t  *order = nullptr;
  cs_gnum_t  *gnum_buf = nullptr;

  assert(mesh != nullptr);

  if (n_faces == 0)
    return;

  /* Order faces according to their global numbering */

  CS_MALLOC(order, n_faces, cs_lnum_t);

  cs_order_gnum_allocated(nullptr, mesh->face_gnum, order, n_faces);

  /* Order global face numbering */

  CS_MALLOC(gnum_buf, n_faces, cs_gnum_t);
  CS_MALLOC(selection, n_faces, cs_lnum_t);

  for (i = 0; i < n_faces; i++)
    gnum_buf[i] = mesh->face_gnum[i];

  prev = 0;
  n_new_faces = 0;

  for (i = 0; i < n_faces; i++) {

    o_id = order[i];
    cur = gnum_buf[o_id];

    if (prev != cur) {
      prev = cur;
      selection[n_new_faces] = o_id;
      mesh->face_gnum[n_new_faces] = cur;
      n_new_faces++;
    }

  }

  mesh->n_faces = n_new_faces;

  CS_FREE(gnum_buf);
  CS_FREE(order);

  CS_REALLOC(mesh->face_gnum, n_new_faces, cs_gnum_t);
  CS_REALLOC(selection, n_new_faces, cs_lnum_t);

  /* Order face -> vertex connectivity list */

  CS_MALLOC(num_buf, mesh->face_vtx_idx[n_faces], cs_lnum_t);

  for (i = 0; i < mesh->face_vtx_idx[n_faces]; i++)
    num_buf[i] = mesh->face_vtx_lst[i];

  shift = 0;

  for (i = 0; i < n_new_faces; i++) {

    o_id = selection[i];
    start = mesh->face_vtx_idx[o_id];
    end = mesh->face_vtx_idx[o_id+1];

    for (j = start; j < end; j++)
      mesh->face_vtx_lst[shift++] = num_buf[j];

  } /* End of loop on faces */

  CS_REALLOC(num_buf, n_faces, cs_lnum_t);

  for (i = 0; i < n_faces; i++)
    num_buf[i] = mesh->face_vtx_idx[i+1] - mesh->face_vtx_idx[i];

  for (i = 0; i < n_new_faces; i++) {
    o_id = selection[i];
    mesh->face_vtx_idx[i+1] = mesh->face_vtx_idx[i] + num_buf[o_id];
  }

  /* Memory management */

  CS_FREE(selection);
  CS_FREE(num_buf);
  CS_REALLOC(mesh->face_vtx_idx, n_new_faces+1, cs_lnum_t);
  CS_REALLOC(mesh->face_vtx_lst, mesh->face_vtx_idx[n_new_faces], cs_lnum_t);
}

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Synchronize vertices definition over the rank. For a vertex with the same
 * global number but a different tolerance, we keep the minimal tolerance.
 *
 * parameters:
 *  mesh <->  pointer to the cs_join_mesh_t structure to synchronize
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_sync_vertices(cs_join_mesh_t  *mesh)
{
  cs_lnum_t  *order = nullptr;
  cs_gnum_t  *recv_gnum = nullptr;

  MPI_Comm  mpi_comm = cs_glob_mpi_comm;

  const int  n_ranks = cs_glob_n_ranks;
  const int  local_rank = cs::max(cs_glob_rank_id, 0);

  assert(n_ranks > 1);
  assert(mesh != nullptr);

  /* Get the max global number */

  cs_gnum_t l_max_gnum = 0, g_max_gnum = 0;
  for (cs_lnum_t i = 0; i < mesh->n_vertices; i++)
    l_max_gnum = cs::max(l_max_gnum, mesh->vertices[i].gnum);

  MPI_Allreduce(&l_max_gnum, &g_max_gnum, 1, CS_MPI_GNUM, MPI_MAX, mpi_comm);

  cs_block_dist_info_t bi = cs_block_dist_compute_sizes(local_rank,
                                                        n_ranks,
                                                        1,
                                                        0,
                                                        g_max_gnum);

  int  *dest_rank = nullptr;
  CS_MALLOC(dest_rank, mesh->n_vertices, int);

  for (cs_lnum_t i = 0; i < mesh->n_vertices; i++)
    dest_rank[i] = ((mesh->vertices[i].gnum - 1)/bi.block_size) * bi.rank_step;

  cs_all_to_all_t
    *d = cs_all_to_all_create(mesh->n_vertices,
                              0, /* flags */
                              nullptr,
                              dest_rank,
                              mpi_comm);

  cs_all_to_all_transfer_dest_rank(d, &dest_rank);

  /* Send vertices to sync and receive its part of work */

  int stride = sizeof(cs_join_vertex_t);

  auto recv_vertices = static_cast<cs_join_vertex_t *>(
    cs_all_to_all_copy_array(d,
                             CS_CHAR,
                             stride,
                             false, /* reverse */
                             mesh->vertices,
                             nullptr));

  cs_lnum_t n_recv = cs_all_to_all_n_elts_dest(d);

  /* Order vertices by increasing global number */

  CS_MALLOC(recv_gnum, n_recv, cs_gnum_t);
  CS_MALLOC(order, n_recv, cs_lnum_t);

  for (cs_lnum_t i = 0; i < n_recv; i++)
    recv_gnum[i] = recv_vertices[i].gnum;

  cs_order_gnum_allocated(nullptr, recv_gnum, order, n_recv);

  /* Sync. vertices sharing the same global number */

  cs_lnum_t start = 0, end = 0;

  while (start < n_recv) {

    cs_lnum_t i;
    double  min_tol = recv_vertices[order[start]].tolerance;
    cs_gnum_t ref_gnum = recv_vertices[order[start]].gnum;
    for (i = start;
         i < n_recv && ref_gnum == recv_vertices[order[i]].gnum;
         i++);
    end = i;

    /* Get min tolerance */
    for (i = start; i < end; i++)
      min_tol = cs::min(min_tol, recv_vertices[order[i]].tolerance);

    /* Set min tolerance to all vertices sharing the same global number */
    for (i = start; i < end; i++)
      recv_vertices[order[i]].tolerance = min_tol;

    start = end;
  }

  /* Send back vertices after synchronization */

  cs_all_to_all_copy_array(d,
                           CS_CHAR,
                           stride,
                           true, /* reverse */
                           recv_vertices,
                           mesh->vertices);

  /* Free buffers */

  CS_FREE(recv_gnum);
  CS_FREE(order);
  CS_FREE(recv_vertices);

  cs_all_to_all_destroy(&d);
}

#endif /* HAVE_MPI */

/*----------------------------------------------------------------------------
 * Delete vertices which appear several times (same global number) and
 * vertices which are not used in face definition.
 *
 * parameters:
 *   mesh <-> pointer to cs_join_mesh_t structure to clean
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_vertex_clean(cs_join_mesh_t  *mesh)
{
  cs_lnum_t  i, j, shift, n_init_vertices, n_final_vertices;
  cs_gnum_t  prev, cur;

  cs_lnum_t  *order = nullptr;
  cs_lnum_t  *init2final = nullptr, *tag = nullptr;
  cs_gnum_t  *gnum_buf = nullptr;
  cs_join_vertex_t  *final_vertices = nullptr;

  assert(mesh != nullptr);

  n_init_vertices = mesh->n_vertices;

  if (n_init_vertices < 2)
    return;

  /* Count the final number of vertices */

  CS_MALLOC(order, n_init_vertices, cs_lnum_t);
  CS_MALLOC(tag, n_init_vertices, cs_lnum_t);
  CS_MALLOC(gnum_buf, n_init_vertices, cs_gnum_t);

  for (i = 0; i < n_init_vertices; i++) {
    gnum_buf[i] = mesh->vertices[i].gnum;
    tag[i] = 0;
  }

  /* Tag vertices really used in the mesh definition */

  for (i = 0; i < mesh->n_faces; i++)
    for (j = mesh->face_vtx_idx[i]; j < mesh->face_vtx_idx[i+1]; j++)
      tag[mesh->face_vtx_lst[j]] = 1;

  /* Order vertices by increasing global number */

  cs_order_gnum_allocated(nullptr, gnum_buf, order, n_init_vertices);

  n_final_vertices = 0;
  prev = 0;

  for (i = 0; i < n_init_vertices; i++) {

    shift = order[i];
    cur = gnum_buf[shift];

    if (prev != cur && tag[i] > 0) {
      n_final_vertices++;
      prev = cur;
    }

  }

  /* Define the final vertices structure and indirection between
     initial numbering and final numbering */

  CS_MALLOC(final_vertices, n_final_vertices, cs_join_vertex_t);
  CS_MALLOC(init2final, n_init_vertices, cs_lnum_t);

  n_final_vertices = 0;
  prev = 0;

  for (i = 0; i < n_init_vertices; i++) {

    shift = order[i];
    cur = gnum_buf[shift];

    if (prev != cur  && tag[i] > 0) {

      final_vertices[n_final_vertices++] = mesh->vertices[shift];
      prev = cur;

    }

    init2final[shift] = n_final_vertices - 1;

  }

  CS_FREE(mesh->vertices);

  mesh->vertices = final_vertices;
  mesh->n_vertices = n_final_vertices;

  /* Update face->vertex connectivity list */

  for (i = 0; i < mesh->n_faces; i++) {

    for (j = mesh->face_vtx_idx[i]; j < mesh->face_vtx_idx[i+1]; j++)
      mesh->face_vtx_lst[j] = init2final[mesh->face_vtx_lst[j]];

  } /* end of loop on faces */

  CS_FREE(init2final);
  CS_FREE(gnum_buf);
  CS_FREE(tag);
  CS_FREE(order);
}

/*----------------------------------------------------------------------------
 * Clean the given cs_join_mesh_t structure, removing degenerate edges.
 *
 * parameters:
 *   mesh      <-> pointer to the cs_join_mesh_t structure to clean
 *   verbosity <-- level of display
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_clean(cs_join_mesh_t  *mesh,
                   int              verbosity)
{
  assert(mesh != nullptr);

  /* Delete empty edge:
       These edges are generated during the merge step. If two vertices
       sharing the same edge are fused, we have to delete the resulting
       edge.
  */

  _remove_empty_edges(mesh, verbosity);

  /* Delete degenerate edge:
       These edges are generated during the merge step.


       x                                      x
       |\                                     |
       | \                                    |
     a2|  \a3                               A2|
       |   \                Merge            |
       |    \      a4         of              |
    ---s1----s2------       vertices          x
       |      \            s1 and s2         / \
       |       \                            /   \
     a1|        \a4                      A1/     \A3
       |         \                        /       \
       |          \                      /         \
       x-----------x                    x-----------x
            a5                                A4

   */

  _remove_degenerate_edges(mesh, verbosity);
}

/*----------------------------------------------------------------------------
 * Define a list of edges associated to a cs_join_mesh_t structure.
 *
 * parameters:
 *   mesh <-- pointer to a cs_join_mesh_t structure
 *
 * returns:
 *   a pointer to the new defined cs_join_edges_t structure.
 *---------------------------------------------------------------------------*/

cs_join_edges_t *
cs_join_mesh_define_edges(const cs_join_mesh_t  *mesh)
{
  int  i, j;
  cs_lnum_t  v1_num, v2_num, o_id1, o_id2;
  cs_lnum_t  edge_shift, shift, n_init_edges;
  cs_gnum_t  v1_gnum, v2_gnum;

  cs_lnum_t  *order = nullptr;
  cs_lnum_t  *vtx_counter = nullptr, *vtx_lst = nullptr;
  cs_gnum_t  *adjacency = nullptr;
  cs_join_edges_t  *edges = nullptr;

  if (mesh == nullptr)
    return edges;

  /* Initialization and structure allocation */

  CS_MALLOC(edges, 1, cs_join_edges_t);

  edges->n_edges = 0;
  edges->def = nullptr;
  edges->gnum = nullptr;
  edges->n_vertices = mesh->n_vertices;
  edges->vtx_idx = nullptr;
  edges->adj_vtx_lst = nullptr;
  edges->edge_lst = nullptr;

  /* Define edges */

  n_init_edges = mesh->face_vtx_idx[mesh->n_faces];

  CS_MALLOC(edges->def, 2*n_init_edges, cs_lnum_t);
  CS_MALLOC(edges->vtx_idx, mesh->n_vertices + 1, cs_lnum_t);

  for (i = 0; i < mesh->n_vertices + 1; i++)
    edges->vtx_idx[i] = 0;

  /* Loop on faces to initialize edge list */

  CS_MALLOC(vtx_lst, 2*n_init_edges, cs_lnum_t);
  CS_MALLOC(adjacency, 2*n_init_edges, cs_gnum_t);

  for (shift = 0, i = 0; i < mesh->n_faces; i++) {

    cs_lnum_t  start = mesh->face_vtx_idx[i];
    cs_lnum_t  end =  mesh->face_vtx_idx[i+1];

    assert(end-start > 0);

    for (j = start; j < end - 1; j++) {

      v1_num = mesh->face_vtx_lst[j] + 1;
      v1_gnum = (mesh->vertices[v1_num-1]).gnum;
      v2_num = mesh->face_vtx_lst[j+1] + 1;
      v2_gnum = (mesh->vertices[v2_num-1]).gnum;

      if (v1_gnum > v2_gnum) {

        vtx_lst[2*shift] = v2_num;
        adjacency[2*shift] = v2_gnum;
        vtx_lst[2*shift+1] = v1_num;
        adjacency[2*shift+1] = v1_gnum;

      }
      else {

        vtx_lst[2*shift] = v1_num;
        adjacency[2*shift] = v1_gnum;
        vtx_lst[2*shift+1] = v2_num;
        adjacency[2*shift+1] = v2_gnum;

      }

      shift++;

    } /* End of loop on n-1 first vertices */

    v1_num = mesh->face_vtx_lst[end-1] + 1;
    v1_gnum = (mesh->vertices[v1_num-1]).gnum;
    v2_num = mesh->face_vtx_lst[start] + 1;
    v2_gnum = (mesh->vertices[v2_num-1]).gnum;

    if (v1_gnum > v2_gnum) {

      vtx_lst[2*shift] = v2_num;
      adjacency[2*shift] = v2_gnum;
      vtx_lst[2*shift+1] = v1_num;
      adjacency[2*shift+1] = v1_gnum;

    }
    else {

      vtx_lst[2*shift] = v1_num;
      adjacency[2*shift] = v1_gnum;
      vtx_lst[2*shift+1] = v2_num;
      adjacency[2*shift+1] = v2_gnum;

    }

    shift++;

  } /* End of loop on faces */

  assert(shift == n_init_edges);

  CS_MALLOC(order, n_init_edges, cs_lnum_t);

  cs_order_gnum_allocated_s(nullptr, adjacency, 2, order, n_init_edges);

  if (n_init_edges > 0) {

    /* Fill cs_join_edges_t structure */

    o_id1 = order[0];
    edges->def[0] = vtx_lst[2*o_id1];
    edges->def[1] = vtx_lst[2*o_id1+1];
    edges->vtx_idx[vtx_lst[2*o_id1]] += 1;
    edges->vtx_idx[vtx_lst[2*o_id1+1]] += 1;
    edge_shift = 1;

    for (i = 1; i < n_init_edges; i++) {

      o_id1 = order[i-1];
      o_id2 = order[i];

      if (   vtx_lst[2*o_id1]   != vtx_lst[2*o_id2]
          || vtx_lst[2*o_id1+1] != vtx_lst[2*o_id2+1]) {

        edges->vtx_idx[vtx_lst[2*o_id2]] += 1;
        edges->vtx_idx[vtx_lst[2*o_id2+1]] += 1;
        edges->def[2*edge_shift] = vtx_lst[2*o_id2];
        edges->def[2*edge_shift+1] = vtx_lst[2*o_id2+1];
        edge_shift++;

      }

    } /* End of loop on edges */

    edges->n_edges = edge_shift;
    CS_REALLOC(edges->def, 2*edges->n_edges, cs_lnum_t);

  } /* If n_init_edges > 0 */

  /* Build adj_vtx_lst and edge_lst */

  CS_MALLOC(vtx_counter, mesh->n_vertices, cs_lnum_t);

  for (i = 0; i < mesh->n_vertices; i++) {
    edges->vtx_idx[i+1] += edges->vtx_idx[i];
    vtx_counter[i] = 0;
  }

  CS_MALLOC(edges->adj_vtx_lst, edges->vtx_idx[mesh->n_vertices], cs_lnum_t);
  CS_MALLOC(edges->edge_lst, edges->vtx_idx[mesh->n_vertices], cs_lnum_t);

  if (n_init_edges > 0) {

    cs_lnum_t  vtx_id_a, vtx_id_b, shift_a, shift_b;
    cs_gnum_t  vtx_gnum_a, vtx_gnum_b;

    cs_lnum_t  cur_edge_num = 1;

    /* Initiate edge_lst and adj_vtx_lst building */

    o_id1 = order[0];

    vtx_id_a = vtx_lst[2*o_id1]-1;
    vtx_id_b = vtx_lst[2*o_id1+1]-1;

    vtx_gnum_a = (mesh->vertices[vtx_id_a]).gnum;
    vtx_gnum_b = (mesh->vertices[vtx_id_a]).gnum;

    shift_a = edges->vtx_idx[vtx_id_a];
    shift_b = edges->vtx_idx[vtx_id_b];

    vtx_counter[vtx_id_a] += 1;
    vtx_counter[vtx_id_b] += 1;

    edges->adj_vtx_lst[shift_a] = vtx_id_b;
    edges->adj_vtx_lst[shift_b] = vtx_id_a;

    if (vtx_gnum_a > vtx_gnum_b) {
      edges->edge_lst[shift_a] = -cur_edge_num;
      edges->edge_lst[shift_b] = cur_edge_num;
    }
    else {
      edges->edge_lst[shift_a] = cur_edge_num;
      edges->edge_lst[shift_b] = -cur_edge_num;
    }

    cur_edge_num++;

    for (i = 1; i < n_init_edges; i++) {

      o_id1 = order[i-1];
      o_id2 = order[i];

      if (   vtx_lst[2*o_id1]   != vtx_lst[2*o_id2]
          || vtx_lst[2*o_id1+1] != vtx_lst[2*o_id2+1]) {

        vtx_id_a = vtx_lst[2*o_id2]-1;
        vtx_id_b = vtx_lst[2*o_id2+1]-1;

        vtx_gnum_a = (mesh->vertices[vtx_id_a]).gnum;
        vtx_gnum_b = (mesh->vertices[vtx_id_a]).gnum;

        shift_a = edges->vtx_idx[vtx_id_a] + vtx_counter[vtx_id_a];
        shift_b = edges->vtx_idx[vtx_id_b] + vtx_counter[vtx_id_b];

        vtx_counter[vtx_id_a] += 1;
        vtx_counter[vtx_id_b] += 1;

        edges->adj_vtx_lst[shift_a] = vtx_id_b;
        edges->adj_vtx_lst[shift_b] = vtx_id_a;

        if (vtx_gnum_a > vtx_gnum_b) {
          edges->edge_lst[shift_a] = -cur_edge_num;
          edges->edge_lst[shift_b] = cur_edge_num;
        }
        else {
          edges->edge_lst[shift_a] = cur_edge_num;
          edges->edge_lst[shift_b] = -cur_edge_num;
        }

        cur_edge_num++;

      }

    } /* End of loop on edges */

    assert(cur_edge_num - 1 == edges->n_edges);

  } /* End of adj_vtx_lst and edge_lst building if n_init_edges > 0 */

  /* Partial clean-up */

  CS_FREE(vtx_lst);
  CS_FREE(vtx_counter);

  /* Define a global numbering on edges */

  CS_MALLOC(edges->gnum, edges->n_edges, cs_gnum_t);
  CS_REALLOC(adjacency, 2*edges->n_edges, cs_gnum_t);

  for (i = 0; i < edges->n_edges; i++) {

    cs_lnum_t  v1_id = edges->def[2*i] - 1;
    cs_lnum_t  v2_id = edges->def[2*i+1] - 1;

    v1_gnum = (mesh->vertices[v1_id]).gnum;
    v2_gnum = (mesh->vertices[v2_id]).gnum;

    if (v1_gnum > v2_gnum) {
      adjacency[2*i] = v2_gnum;
      adjacency[2*i+1] = v1_gnum;
    }
    else {
      adjacency[2*i] = v1_gnum;
      adjacency[2*i+1] = v2_gnum;
    }

  } /* End of loop on edges */

  /* Order vtx_lst and build an order list into adjacency */

  cs_order_gnum_allocated_s(nullptr, adjacency, 2, order, edges->n_edges);

  if (cs_glob_n_ranks == 1) { /* Serial treatment */

    edges->n_g_edges = edges->n_edges;

    for (i = 0; i < edges->n_edges; i++) {

      cs_lnum_t  o_id = order[i];

      edges->gnum[i] = o_id+1;

    }

  }
  else { /* Parallel treatment */

    cs_gnum_t  *order_couples = nullptr;
    fvm_io_num_t *edge_io_num = nullptr;
    const cs_gnum_t  *edges_gnum = nullptr;

    assert(cs_glob_n_ranks > 1);

    CS_MALLOC(order_couples, 2*edges->n_edges, cs_gnum_t);

    for (i = 0; i < edges->n_edges; i++) {

      cs_lnum_t  o_id = order[i];

      order_couples[2*i] = adjacency[2*o_id];
      order_couples[2*i+1] = adjacency[2*o_id+1];

    } /* End of loop on edges */

    edge_io_num = fvm_io_num_create_from_adj_s(nullptr,
                                               order_couples,
                                               edges->n_edges,
                                               2);

    edges->n_g_edges = fvm_io_num_get_global_count(edge_io_num);
    edges_gnum = fvm_io_num_get_global_num(edge_io_num);

    for (i = 0; i < edges->n_edges; i++)
      edges->gnum[i] = edges_gnum[i];

    /* Partial Clean-up */

    CS_FREE(order_couples);
    fvm_io_num_destroy(edge_io_num);

  }

  /* Memory management */

  CS_FREE(adjacency);
  CS_FREE(order);

  /* Return pointers */

  return edges;
}

/*----------------------------------------------------------------------------
 * Get the edge number relative to a couple of vertex numbers.
 *
 * edge_num > 0 if couple is in the same order as the edge->def
 * edge_num < 0 otherwise
 *
 * parameters:
 *   v1_num <-- vertex number for the first vertex
 *   v2_num <-- vertex number for the second vertex
 *   edges  <-- pointer to a cs_join_edges_t structure
 *
 * returns:
 *   an edge number relative to the couple of vertices
 *---------------------------------------------------------------------------*/

cs_lnum_t
cs_join_mesh_get_edge(cs_lnum_t               v1_num,
                      cs_lnum_t               v2_num,
                      const cs_join_edges_t  *edges)
{
  cs_lnum_t  i;
  cs_lnum_t  edge_num = 0;

  assert(edges != nullptr);
  assert(v1_num > 0);
  assert(v2_num > 0);

  if (edges->vtx_idx[v1_num] - edges->vtx_idx[v1_num-1] == 0)
    bft_error(__FILE__, __LINE__, 0,
              _(" The given vertex number: %ld is not defined"
                " in the edge structure (edges->vtx_idx).\n"), (long)v1_num);

  for (i = edges->vtx_idx[v1_num-1]; i < edges->vtx_idx[v1_num]; i++) {
    if (edges->adj_vtx_lst[i] == v2_num - 1) {
      edge_num = edges->edge_lst[i];
      break;
    }
  }

  if (edge_num == 0)
    bft_error(__FILE__, __LINE__, 0,
              _(" The given couple of vertex numbers :\n"
                "   vertex 1 : %ld\n"
                "   vertex 2 : %ld\n"
                " is not defined in the edge structure.\n"),
              (long)v1_num, (long)v2_num);

  assert(edge_num != 0);

  return edge_num;
}

/*----------------------------------------------------------------------------
 * Re-organize the cs_join_mesh_t structure after a renumbering of
 * the vertices following the merge operation + a new description of each
 * face.
 *
 * parameters:
 *   mesh             <-> pointer to the cs_join_mesh_t structure to update
 *   edges            <-- pointer to a cs_join_edges_t structure
 *   edge_index       <-- index on edges for the new vertices
 *   edge_new_vtx_lst <-- list of new vertices for each edge
 *   n_new_vertices   <-- new local number of vertices after merge
 *   old2new          <-- array storing the relation between old/new vertex id
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_update(cs_join_mesh_t         *mesh,
                    const cs_join_edges_t  *edges,
                    const cs_lnum_t         edge_index[],
                    const cs_lnum_t         edge_new_vtx_lst[],
                    cs_lnum_t               n_new_vertices,
                    const cs_lnum_t         old2new[])
{
  cs_lnum_t  i, j, n_adds;

  cs_join_vertex_t  *new_vertices = nullptr;
  cs_lnum_t  *new_face_vtx_idx = nullptr, *new_face_vtx_lst = nullptr;

  /* Sanity checks */

  assert(mesh != nullptr);
  assert(edges != nullptr);

  /* Update description and numbering for face -> vertex connectivity */

  if (edge_new_vtx_lst != nullptr) {

    CS_MALLOC(new_face_vtx_idx, mesh->n_faces + 1, cs_lnum_t);

    for (i = 0; i < mesh->n_faces + 1; i++)
      new_face_vtx_idx[i] = 0;

    /* Update face -> vertex connectivity.
       add new vertices between the existing one */

    for (i = 0; i < mesh->n_faces; i++) {

      cs_lnum_t  start_id = mesh->face_vtx_idx[i];
      cs_lnum_t  end_id = mesh->face_vtx_idx[i+1];

      for (j = start_id; j < end_id - 1; j++) {

        n_adds = _count_new_added_vtx_to_edge(mesh->face_vtx_lst[j],
                                              mesh->face_vtx_lst[j+1],
                                              old2new,
                                              edges,
                                              edge_index,
                                              edge_new_vtx_lst);

        new_face_vtx_idx[i+1] += n_adds;

      }

      /* Case end - start */

      n_adds = _count_new_added_vtx_to_edge(mesh->face_vtx_lst[end_id-1],
                                            mesh->face_vtx_lst[start_id],
                                            old2new,
                                            edges,
                                            edge_index,
                                            edge_new_vtx_lst);

      new_face_vtx_idx[i+1] += n_adds;

    } /* End of loop on faces */

    /* Build new face_vtx_idx */

    new_face_vtx_idx[0] = 0;
    for (i = 0; i < mesh->n_faces; i++) {

      new_face_vtx_idx[i+1] += new_face_vtx_idx[i];

      if (new_face_vtx_idx[i+1] < 3)
        bft_error(__FILE__, __LINE__, 0,
                  _(" Problem in mesh connectivity."
                    " Face: %llu\n"
                    " Problem detected during connectivity update:\n"
                    " The face is defined by less than 3 points"
                    " (excessive merging has occured).\n\n"
                    " Modify joining parameters to reduce merging"
                    " (fraction & merge).\n"),
                  (unsigned long long)(mesh->face_gnum[i]));

    }

    /* Build new_face_vtx_lst */

    CS_MALLOC(new_face_vtx_lst, new_face_vtx_idx[mesh->n_faces], cs_lnum_t);

  } /* End if edge_new_vtx_lst != nullptr */

  else { /* edge_new_vtx_lst == nullptr */

    new_face_vtx_idx = mesh->face_vtx_idx;
    new_face_vtx_lst = mesh->face_vtx_lst;

  }

  for (i = 0; i < mesh->n_faces; i++) {

    cs_lnum_t  start_id = mesh->face_vtx_idx[i];
    cs_lnum_t  end_id = mesh->face_vtx_idx[i+1];
    cs_lnum_t  shift = new_face_vtx_idx[i];

    for (j = start_id; j < end_id-1; j++)
      _add_new_vtx_to_edge(mesh->face_vtx_lst[j],
                           mesh->face_vtx_lst[j+1],
                           old2new,
                           edges,
                           edge_index,
                           edge_new_vtx_lst,
                           new_face_vtx_lst,
                           &shift);

    /* Case end - start */

    _add_new_vtx_to_edge(mesh->face_vtx_lst[end_id-1],
                         mesh->face_vtx_lst[start_id],
                         old2new,
                         edges,
                         edge_index,
                         edge_new_vtx_lst,
                         new_face_vtx_lst,
                         &shift);

  } /* End of loop on faces */

  if (edge_new_vtx_lst != nullptr) {

    CS_FREE(mesh->face_vtx_idx);
    CS_FREE(mesh->face_vtx_lst);

    mesh->face_vtx_idx = new_face_vtx_idx;
    mesh->face_vtx_lst = new_face_vtx_lst;

  }

  /* Define the new_vertices structure */

  CS_MALLOC(new_vertices, n_new_vertices, cs_join_vertex_t);

  for (i = 0; i < mesh->n_vertices; i++)
    new_vertices[old2new[i]] = mesh->vertices[i];

#if 0 && defined(DEBUG) && !defined(NDEBUG)
  bft_printf("\n\n Dump Old2New array : "
             "n_old_vertices = %d - n_new_vertices = %d\n",
             mesh->n_vertices, n_new_vertices);
  for (i = 0; i < mesh->n_vertices; i++)
    bft_printf("Old num : %7d (%9u) => New num : %7d (%9u)\n",
               i+1         , mesh->vertices[i].gnum,
               old2new[i]+1, new_vertices[old2new[i]].gnum);
  bft_printf_flush();
#endif

  /* Update mesh structure */

  CS_FREE(mesh->vertices);

  mesh->n_vertices = n_new_vertices;
  mesh->n_g_vertices = n_new_vertices;
  mesh->vertices = new_vertices;

#if defined(HAVE_MPI)

  if (cs_glob_n_ranks > 1) {

    cs_gnum_t  *vtx_gnum = nullptr;
    fvm_io_num_t  *io_num = nullptr;

    /* Global number of selected vertices and associated
       fvm_io_num_t structure */

    CS_MALLOC(vtx_gnum, n_new_vertices, cs_gnum_t);

    for (i = 0; i < n_new_vertices; i++)
      vtx_gnum[i] = (mesh->vertices[i]).gnum;

    io_num = fvm_io_num_create(nullptr, vtx_gnum, n_new_vertices, 0);

    mesh->n_g_vertices = fvm_io_num_get_global_count(io_num);

    fvm_io_num_destroy(io_num);

    CS_FREE(vtx_gnum);

  }
#endif
}

/*----------------------------------------------------------------------------
 * Compute for each face of the cs_join_mesh_t structure the face normal.
 * || face_normal || = 1 (divided by the area of the face)
 *
 * The caller is responsible for freeing the returned array.
 *
 * parameters:
 *   mesh <-- pointer to a cs_join_mesh_t structure
 *
 *                          Pi+1
 *              *---------*                   B  : barycenter of the polygon
 *             / .       . \
 *            /   .     .   \                 Pi : vertices of the polygon
 *           /     .   .     \
 *          /       . .  Ti   \               Ti : triangle
 *         *.........B.........* Pi
 *     Pn-1 \       . .       /
 *           \     .   .     /
 *            \   .     .   /
 *             \ .   T0  . /
 *              *---------*
 *            P0
 *
 *
 * returns:
 *   an array with the face normal for each face of the mesh
 *---------------------------------------------------------------------------*/

cs_real_t *
cs_join_mesh_get_face_normal(const cs_join_mesh_t  *mesh)
{
  cs_lnum_t  i, j, k, vid;
  double  inv_norm;

  cs_lnum_t  n_max_vertices = 0;
  cs_real_t  *face_vtx_coord = nullptr;
  cs_real_t  *face_normal = nullptr;

  if (mesh == nullptr)
    return face_normal;

  if (mesh->n_faces == 0)
    return face_normal;

  CS_MALLOC(face_normal, 3*mesh->n_faces, cs_real_t);

  for (i = 0; i < 3*mesh->n_faces; i++)
    face_normal[i] = 0.0;

  /* Compute n_max_vertices */

  for (i = 0; i < mesh->n_faces; i++)
    n_max_vertices = cs::max(n_max_vertices,
                             mesh->face_vtx_idx[i+1] - mesh->face_vtx_idx[i]);

  CS_MALLOC(face_vtx_coord, 3*(n_max_vertices+1), cs_real_t);

  for (i = 0; i < mesh->n_faces; i++) {

    cs_real_t  v1[3], v2[3], tri_normal[3];

    cs_lnum_t  shift = 0;
    cs_lnum_t  s = mesh->face_vtx_idx[i];
    cs_lnum_t  e = mesh->face_vtx_idx[i+1];
    cs_lnum_t  n_face_vertices = e - s;
    double  inv_n_face_vertices = 1/(double)n_face_vertices;

    cs_real_t  bary[3] = { 0.0, 0.0, 0.0};
    cs_real_t  fnorm[3] = { 0.0, 0.0, 0.0};

    /* Fill face_vtx_coord */

    for (j = s; j < e; j++) {
      vid = mesh->face_vtx_lst[j];
      for (k = 0; k < 3; k++)
        face_vtx_coord[shift++] = mesh->vertices[vid].coord[k];
    }

    vid = mesh->face_vtx_lst[s];
    for (k = 0; k < 3; k++)
      face_vtx_coord[shift++] = mesh->vertices[vid].coord[k];

    /* Compute the barycenter of the face */

    for (j = 0; j < n_face_vertices; j++)
      for (k = 0; k < 3; k++)
        bary[k] += face_vtx_coord[3*j+k];

    for (k = 0; k < 3; k++)
      bary[k] *= inv_n_face_vertices;

    /* Loop on the triangles of the face defined by an edge of the face
       and the barycenter */

    for (j = 0; j < n_face_vertices; j++) {

      /*    Computation of the normal of each triangle Ti:
             ->            -->   -->
             N(Ti) = 1/2 ( BPi X BPi+1 )
      */

      for (k = 0; k < 3; k++) {
        v1[k] = face_vtx_coord[3*j    + k] - bary[k];
        v2[k] = face_vtx_coord[3*(j+1)+ k] - bary[k];
      }

      _cross_product(v1, v2, tri_normal);

      /*   Computation of the normal of the polygon
           => vectorial sum of normals of each triangle

           ->      n-1   ->
           N(P) =  Sum ( N(Ti) )
                   i=0
      */

      for (k = 0; k < 3; k++)
        fnorm[k] += 0.5 * tri_normal[k];

    } /* End of loop on vertices of the face */

    inv_norm = 1/sqrt(_dot_product(fnorm, fnorm));

    for (k = 0; k < 3; k++)
      face_normal[3*i+k] = inv_norm * fnorm[k];

#if 0 && defined(DEBUG) && !defined(NDEBUG)
    bft_printf("  Face_num: %5d (%u)- face_normal [%8.4e, %8.4e, %8.4e]\n",
               i+1, mesh->face_gnum[i],
               face_normal[3*i], face_normal[3*i+1], face_normal[3*i+2]);
    bft_printf_flush();
#endif

  } /* End of loop on faces */

  /* Free memory */

  CS_FREE(face_vtx_coord);

  return face_normal;
}

/*----------------------------------------------------------------------------
 * Allocate and define an "edge -> face" connectivity
 *
 * parameters:
 *   mesh          <-- pointer to a cs_join_mesh_t structure
 *   edges         <-- pointer to a cs_join_edges_t structure
 *   edge_face_idx --> pointer to the edge -> face connect. index
 *   edge_face_lst --> pointer to the edge -> face connect. list
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_get_edge_face_adj(const cs_join_mesh_t   *mesh,
                               const cs_join_edges_t  *edges,
                               cs_lnum_t              *edge_face_idx[],
                               cs_lnum_t              *edge_face_lst[])
{
  cs_lnum_t  i, j, k, edge_id, shift;
  cs_lnum_t  n_edges, n_faces;

  cs_lnum_t  n_max_vertices = 0;
  cs_lnum_t  *counter = nullptr, *face_connect = nullptr;
  cs_lnum_t  *_edge_face_idx = nullptr, *_edge_face_lst = nullptr;

  if (mesh == nullptr || edges == nullptr)
    return;

  n_edges = edges->n_edges;
  n_faces = mesh->n_faces;

  /* Compute n_max_vertices */

  for (i = 0; i < n_faces; i++)
    n_max_vertices = cs::max(n_max_vertices,
                             mesh->face_vtx_idx[i+1]-mesh->face_vtx_idx[i]);

  CS_MALLOC(face_connect, n_max_vertices + 1, cs_lnum_t);
  CS_MALLOC(counter, n_edges, cs_lnum_t);

  /* Build an edge -> face connectivity */

  CS_MALLOC(_edge_face_idx, n_edges+1, cs_lnum_t);

  for (i = 0; i < n_edges+1; i++)
    _edge_face_idx[i] = 0;

  for (i = 0; i < n_edges; i++)
    counter[i] = 0;

  /* Build index */

  for (i = 0; i < n_faces; i++) {

    cs_lnum_t  start_id = mesh->face_vtx_idx[i];
    cs_lnum_t  end_id = mesh->face_vtx_idx[i+1];
    cs_lnum_t  n_face_vertices = end_id - start_id;

    for (j = start_id, k = 0; j < end_id; j++, k++)
      face_connect[k] = mesh->face_vtx_lst[j];
    face_connect[n_face_vertices] = mesh->face_vtx_lst[start_id];

    assert(n_face_vertices == k);

    for (j = 0; j < n_face_vertices; j++) {

      cs_lnum_t  vtx_id1 = face_connect[j];

      for (k = edges->vtx_idx[vtx_id1]; k < edges->vtx_idx[vtx_id1+1]; k++)
        if (edges->adj_vtx_lst[k] == face_connect[j+1])
          break;

      assert(k != edges->vtx_idx[vtx_id1+1]);

      _edge_face_idx[cs::abs(edges->edge_lst[k])] += 1;

    } /* End of loop on vertices of the face */

  } /* End of loop on faces */

  for (i = 0; i < n_edges; i++)
    _edge_face_idx[i+1] += _edge_face_idx[i];

  CS_MALLOC(_edge_face_lst, _edge_face_idx[n_edges], cs_lnum_t);

  /* Fill "edge -> face" connectivity list */

  for (i = 0; i < n_faces; i++) {

    cs_lnum_t  start_id = mesh->face_vtx_idx[i];
    cs_lnum_t  end_id = mesh->face_vtx_idx[i+1];
    cs_lnum_t  n_face_vertices = end_id - start_id;

    for (j = start_id, k = 0; j < end_id; j++, k++)
      face_connect[k] = mesh->face_vtx_lst[j];
    face_connect[n_face_vertices] = mesh->face_vtx_lst[start_id];

    for (j = 0; j < n_face_vertices; j++) {

      cs_lnum_t  vtx_id1 = face_connect[j];

      for (k = edges->vtx_idx[vtx_id1];
           k < edges->vtx_idx[vtx_id1+1]; k++)
        if (edges->adj_vtx_lst[k] == face_connect[j+1])
          break;

      edge_id = cs::abs(edges->edge_lst[k]) - 1;
      shift = _edge_face_idx[edge_id] + counter[edge_id];
      _edge_face_lst[shift] = i+1;
      counter[edge_id] += 1;

    } /* End of loop on vertices of the face */

  } /* End of loop on faces */

#if 0 && defined(DEBUG) && !defined(NDEBUG)
  bft_printf("\n DUMP EDGE -> FACE CONNECTIVITY:\n\n");

  for (i = 0; i < n_edges; i++) {

    cs_lnum_t  start = _edge_face_idx[i];
    cs_lnum_t  end = _edge_face_idx[i+1];
    cs_lnum_t  v1_id = edges->def[2*i] - 1;
    cs_lnum_t  v2_id = edges->def[2*i+1] - 1;

    bft_printf(" edge_num: %6d (%9u) [%9u - %9u]: size: %4d, faces: ",
               i+1, edges->gnum[i],
               mesh->vertices[v1_id].gnum, mesh->vertices[v2_id].gnum,
               end-start);
    for (j = start; j < end; j++)
      bft_printf(" %u ", mesh->face_gnum[_edge_face_lst[j]-1]);
    bft_printf("\n");
    bft_printf_flush();
  }
  bft_printf("\n");
#endif

  /* Set return pointers */

  *edge_face_idx = _edge_face_idx;
  *edge_face_lst = _edge_face_lst;

  /* Free memory*/

  CS_FREE(counter);
  CS_FREE(face_connect);
}

/*----------------------------------------------------------------------------
 * Dump a cs_join_vertex_t structure into a file.
 *
 * parameters:
 *   f      <-- handle to output file
 *   vertex <-- cs_join_vertex_t structure to dump
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_dump_vertex(FILE                   *f,
                         const cs_join_vertex_t  vertex)
{
  assert(vertex.gnum > 0);
  assert(vertex.tolerance >= 0.0);

  fprintf(f," %10llu | %11.6f | % 12.10e  % 12.10e  % 12.10e | %s\n",
          (unsigned long long)vertex.gnum, vertex.tolerance,
          vertex.coord[0], vertex.coord[1], vertex.coord[2],
          _print_state( vertex.state));
}

/*----------------------------------------------------------------------------
 * Dump a cs_join_mesh_t structure into a file.
 *
 * parameters:
 *   f    <-- handle to output file
 *   mesh <-- pointer to cs_join_mesh_t structure to dump
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_dump(FILE                  *f,
                  const cs_join_mesh_t  *mesh)
{
  int  i, j;

  if (mesh == nullptr) {
    fprintf(f,
            "\n\n  -- Dump a cs_join_mesh_t structure: (%p) --\n",
            (const void *)mesh);
    return;
  }

  fprintf(f, "\n\n  -- Dump a cs_join_mesh_t structure: %s (%p) --\n",
          mesh->name, (const void *)mesh);
  fprintf(f, "\n mesh->n_faces:     %11ld\n", (long)mesh->n_faces);
  fprintf(f, " mesh->n_g_faces:   %11llu\n\n",
          (unsigned long long)mesh->n_g_faces);

  if (mesh->face_vtx_idx != nullptr) {

    for (i = 0; i < mesh->n_faces; i++) {

      cs_lnum_t  start = mesh->face_vtx_idx[i];
      cs_lnum_t  end = mesh->face_vtx_idx[i+1];

      fprintf(f, "\n face_id: %9ld gnum: %10llu n_vertices : %4ld\n",
              (long)i, (unsigned long long)mesh->face_gnum[i],
              (long)(end-start));

      for (j = start; j < end; j++) {

        cs_lnum_t  vtx_id = mesh->face_vtx_lst[j];
        cs_join_vertex_t  v_data = mesh->vertices[vtx_id];

        fprintf(f," %8ld - %10llu - [ % 7.5e % 7.5e % 7.5e] - %s\n",
                (long)vtx_id+1, (unsigned long long)v_data.gnum,
                v_data.coord[0], v_data.coord[1], v_data.coord[2],
                _print_state(v_data.state));

      }
      fprintf(f,"\n");

      /* Check if there is no incoherency in the mesh definition */

      for (j = start; j < end - 1; j++) {

        cs_lnum_t  vtx_id1 = mesh->face_vtx_lst[j];
        cs_lnum_t  vtx_id2 = mesh->face_vtx_lst[j+1];

        if (vtx_id1 == vtx_id2) {
          fprintf(f,
                  "  Incoherency found in the current mesh definition\n"
                  "  Face number: %ld (global: %llu)\n"
                  "  Vertices: local (%ld, %ld), global (%llu, %llu)"
                  " are defined twice\n",
                  (long)i+1, (unsigned long long)mesh->face_gnum[i],
                  (long)vtx_id1+1, (long)vtx_id2+1,
                  (unsigned long long)(mesh->vertices[vtx_id1]).gnum,
                  (unsigned long long)(mesh->vertices[vtx_id2]).gnum);
          fflush(f);
          assert(0);
        }

      }

      {
        cs_lnum_t  vtx_id1 = mesh->face_vtx_lst[end-1];
        cs_lnum_t  vtx_id2 = mesh->face_vtx_lst[start];

        if (vtx_id1 == vtx_id2) {
          fprintf(f,
                  "  Incoherency found in the current mesh definition\n"
                  "  Face number: %ld (global: %llu)\n"
                  "  Vertices: local (%ld, %ld), global (%llu, %llu)"
                  " are defined twice\n",
                  (long)i+1, (unsigned long long)(mesh->face_gnum[i]),
                  (long)vtx_id1+1, (long)vtx_id2+1,
                  (unsigned long long)((mesh->vertices[vtx_id1]).gnum),
                  (unsigned long long)((mesh->vertices[vtx_id2]).gnum));
          fflush(f);
          assert(0);
        }
      }

    } /* End of loop on faces */

  } /* End if face_vtx_idx != nullptr */

  fprintf(f,
          "\n Dump vertex data\n"
          "   mesh->vertices     :  %p\n"
          "   mesh->n_vertices   : %11ld\n"
          "   mesh->n_g_vertices : %11llu\n\n",
          (const void *)mesh->vertices, (long)mesh->n_vertices,
          (unsigned long long)mesh->n_g_vertices);

  if (mesh->n_vertices > 0) {

    fprintf(f,
            " Local Num | Global Num |  Tolerance  |        Coordinates\n\n");

    for (i = 0; i < mesh->n_vertices; i++) {
      fprintf(f," %9d |", i+1);
      cs_join_mesh_dump_vertex(f, mesh->vertices[i]);
    }

  }
  fprintf(f,"\n");
  fflush(f);
}

/*----------------------------------------------------------------------------
 * Dump a list of cs_join_edge_t structures.
 *
 * parameters:
 *   f     <-- handle to output file
 *   edges <-- cs_join_edges_t structure to dump
 *   mesh  <-- associated cs_join_mesh_t structure
 *---------------------------------------------------------------------------*/

void
cs_join_mesh_dump_edges(FILE                   *f,
                        const cs_join_edges_t  *edges,
                        const cs_join_mesh_t   *mesh)
{
  cs_lnum_t  i, j;

  if (edges == nullptr)
    return;

  fprintf(f, "\n  Edge connectivity used in the joining operation:\n");
  fprintf(f, "  Number of edges:      %8ld\n", (long)edges->n_edges);
  fprintf(f, "  Number of vertices:   %8ld\n", (long)edges->n_vertices);

  for (i = 0; i < edges->n_edges; i++) { /* Dump edge connectivity */

    cs_lnum_t  v1_id = edges->def[2*i] - 1;
    cs_lnum_t  v2_id = edges->def[2*i+1] - 1;
    cs_gnum_t  v1_gnum = (mesh->vertices[v1_id]).gnum;
    cs_gnum_t  v2_gnum = (mesh->vertices[v2_id]).gnum;

    fprintf(f, "  Edge %6ld  (%8llu) <Vertex> [%8llu %8llu]\n",
            (long)i+1, (unsigned long long)edges->gnum[i],
            (unsigned long long)v1_gnum,
            (unsigned long long)v2_gnum);

    /* Check coherency */

    if (v1_id == v2_id) {
      fprintf(f, "  Incoherency found in the current edge definition\n"
                 "  Edge number: %ld\n"
                 "  Vertices: local (%ld, %ld), global (%llu, %llu)"
                 " are defined twice\n",
                 (long)i+1, (long)v1_id+1, (long)v2_id+1,
              (unsigned long long)v1_gnum, (unsigned long long)v2_gnum);
      fflush(f);
      assert(0);
    }

    if (v1_gnum == v2_gnum) {
      fprintf(f, "  Incoherency found in the current edge definition\n"
                 "  Edge number: %ld\n"
                 "  Vertices: local (%ld, %ld), global (%llu, %llu)"
                 " are defined twice\n",
              (long)i+1, (long)v1_id+1, (long)v2_id+1,
              (unsigned long long)v1_gnum, (unsigned long long)v2_gnum);
      fflush(f);
      assert(0);
    }

  } /* End of loop on edges */

  fprintf(f, "\n  Vertex -> Vertex connectivity :\n\n");

  for (i = 0; i < mesh->n_vertices; i++) {

    cs_lnum_t  start = edges->vtx_idx[i];
    cs_lnum_t  end = edges->vtx_idx[i+1];

    fprintf(f, "  Vertex %6ld (%7llu) - %3d - ",
            (long)i+1, (unsigned long long)(mesh->vertices[i]).gnum,
            (int)(end - start));

    for (j = start; j < end; j++) {
      if (edges->edge_lst[j] > 0)
        fprintf
          (f, " [ v: %7llu, e: %7llu] ",
           (unsigned long long)(mesh->vertices[edges->adj_vtx_lst[j]]).gnum,
           (unsigned long long)edges->gnum[edges->edge_lst[j] - 1]);
      else
        fprintf
          (f, " [ v: %7llu, e: %7llu] ",
           (unsigned long long)(mesh->vertices[edges->adj_vtx_lst[j]]).gnum,
           (unsigned long long)edges->gnum[- edges->edge_lst[j] - 1]);
    }
    fprintf(f, "\n");

  }

  fflush(f);
}

/*---------------------------------------------------------------------------*/

END_C_DECLS
