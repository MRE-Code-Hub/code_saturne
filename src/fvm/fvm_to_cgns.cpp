/*============================================================================
 * Write a nodal representation associated with a mesh and associated
 * variables to CGNS files
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

/*----------------------------------------------------------------------------*/

#if defined(HAVE_CGNS)

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * CGNS library headers
 *----------------------------------------------------------------------------*/

#include <cgnslib.h>

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_error.h"

#include "fvm/fvm_defs.h"
#include "fvm/fvm_convert_array.h"
#include "fvm/fvm_io_num.h"
#include "fvm/fvm_nodal.h"
#include "fvm/fvm_nodal_priv.h"
#include "fvm/fvm_writer_helper.h"
#include "fvm/fvm_writer_priv.h"

#include "base/cs_block_dist.h"
#include "base/cs_file.h"
#include "base/cs_map.h"
#include "base/cs_mem.h"
#include "base/cs_parall.h"
#include "base/cs_part_to_block.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "fvm/fvm_to_cgns.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

#define FVM_CGNS_NAME_SIZE      32      /* Maximum CGNS name length (the CGNS
                                           documentation does not specify this,
                                           but CGNS examples and ADF.h seem
                                           to use 32 character strings) */

/* Compatibility with different CGNS library versions */

#if !defined(CGNS_ENUMV)
#define CGNS_ENUMV(e) e
#endif

#if !defined(CGNS_ENUMT)
#define CGNS_ENUMT(e) e
#endif

#if !defined (CGNS_ENUMD)
#define CGNS_ENUMD(e) e
#endif

#if !defined (CGNS_ENUMF)
#define CGNS_ENUMF(e) e
#endif

#if CGNS_VERSION < 3100
#define cgsize_t int
#endif

/*============================================================================
 * Local Type Definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * CGNS solution structure
 *----------------------------------------------------------------------------*/

typedef struct {

  char                        *name;       /* Solution name */
  int                          index;      /* CGNS base index */
  CGNS_ENUMT(GridLocation_t)   location;   /* CGNS grid location of values */
  double                       time_value; /* Time step value */
  int                          time_step;  /* No. of iteration associated
                                              with time value */

} fvm_to_cgns_solution_t;

/*----------------------------------------------------------------------------
 * CGNS base structure
 *----------------------------------------------------------------------------*/

typedef struct {

  char     *name;              /* Mesh name */
  int       index;             /* CGNS base index */
  int       celldim;           /* Cell dimension:
                                  3 for a volume mesh, 2 for a surface mesh */
  int       physdim;           /* Physical dimension: number of coordinates
                                  defining a vertex */

  int       n_sols;                    /* Number of solutions */
  fvm_to_cgns_solution_t  **solutions; /* Array of pointers to CGNS solution
                                          structures in FVM */

} fvm_to_cgns_base_t;

/*----------------------------------------------------------------------------
 * CGNS writer structure
 *----------------------------------------------------------------------------*/

struct _fvm_to_cgns_writer_t {

  char                   *name;            /* Writer name */
  char                   *filename;        /* associated CGNS file name */

  const char             *basename;        /* pointer to portion of associated
                                              file name without path prefix */

  int                     index;           /* index in associated CGNS file */

  int                     n_bases;         /* Number of CGNS bases */
  fvm_to_cgns_base_t    **bases;           /* Array of pointers to CGNS base
                                              structures in FVM */

  fvm_writer_time_dep_t   time_dependency; /* Mesh time dependency */
  int                     n_time_steps;    /* Number of mesh time steps */
  int                    *time_steps;      /* Array of mesh time steps */
  double                 *time_values;     /* Array of mesh time values */

  bool         is_open;            /* True if CGNS file is open */

  bool         discard_bcs;        /* True if boundary conditions should be
                                      discarded */
  bool         discard_steady;     /* Discard time-independent values */

  bool         discard_polygons;   /* Option to discard polygonal elements */
  bool         discard_polyhedra;  /* Option to discard polyhedral elements */

  bool         divide_polygons;    /* Option to tesselate polygonal elements */
  bool         divide_polyhedra;   /* Option to tesselate polygonal elements */

  bool         preserve_precision; /* Preserve double precison variable type */

  int          rank;               /* Rank of current process in communicator */
  int          n_ranks;            /* Number of processes in communicator */

#if defined(HAVE_MPI)
  MPI_Comm     comm;               /* Associated MPI communicator */
  cs_lnum_t    min_rank_step;      /* Minimum rank step size */
  cs_lnum_t    min_block_size;     /* Minimum block size */
#endif

  /* Linked writers */

  struct _fvm_to_cgns_writer_t  *mesh_writer;  /* Writer for mesh */

};

typedef struct _fvm_to_cgns_writer_t fvm_to_cgns_writer_t;

/*----------------------------------------------------------------------------
 * Context for fvm_writer_field_helper_output_* functions for BC's
 *----------------------------------------------------------------------------*/

typedef struct {

  const fvm_to_cgns_writer_t  *writer;          /* Writer structure */
  const fvm_nodal_t           *mesh;            /* Mesh structure */

  const fvm_to_cgns_base_t    *base;            /* Associated CGNS base */

  cs_gnum_t                    n_g_vol_elts;    /* Global number of volume
                                                   elements */

} _cgns_bc_context_t;

/*----------------------------------------------------------------------------
 * Context structure for main fvm_writer_field_helper_output_* functions.
 *----------------------------------------------------------------------------*/

typedef struct {

  const fvm_to_cgns_writer_t  *writer;          /* Writer structure */
  const fvm_to_cgns_base_t    *base;            /* Associated CGNS base */
  const char                  *field_label;     /* Associated field label */
  cgsize_t                     write_idx_start; /* Write index start */
  int                          solution_index;  /* Solution index */

} _cgns_context_t;

/*============================================================================
 * Static global variables
 *============================================================================*/

static char _cgns_version_string[32] = "";

/*=============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Build a FVM to CGNS file writer structure.
 *
 * parameters:
 *   name           <-- base output case name
 *   postfix        <-- name postfix, or nullptr
 *   path           <-- base path, or nullptr
 *   reference      <-- reference whose attributes may be copied, or nullptr
 *                      (used for linked writers)
 *   time_dependecy <-- indicates if and how meshes will change with time
 *
 * returns:
 *   pointer to opaque CGNS writer structure.
 *----------------------------------------------------------------------------*/

static _fvm_to_cgns_writer_t *
_create_writer(const char           *name,
               const char           *postfix,
               const char           *path,
               fvm_to_cgns_writer_t *reference,
               fvm_writer_time_dep_t time_dependency)
{
  int  filename_length, name_length, path_length;

  fvm_to_cgns_writer_t  *w = nullptr;

  /* Initialize writer */

  CS_MALLOC(w, 1, fvm_to_cgns_writer_t);

  /* Writer name */

  name_length = strlen(name);
  if (name_length == 0)
    bft_error(__FILE__, __LINE__, 0,
              _("Empty CGNS filename."));

  if (postfix != nullptr)
    name_length += strlen(postfix);

  CS_MALLOC(w->name, name_length + 1, char);

  strcpy(w->name, name);
  if (postfix != nullptr)
    strcat(w->name, postfix);

  for (int i = 0; i < name_length; i++) {
    if (w->name[i] == ' ' || w->name[i] == '\t')
      w->name[i] = '_';
  }

  /* Writer's associated filename(s) */

  if (path != nullptr)
    path_length = strlen(path);
  else
    path_length = 0;
  filename_length = path_length + name_length + strlen(".cgns") + 1;
  CS_MALLOC(w->filename, filename_length, char);

  if (path != nullptr) {
    strcpy(w->filename, path);
    w->basename = w->filename + strlen(path);
  }
  else {
    w->filename[0] = '\0';
    w->basename = w->filename;
  }

  strcat(w->filename, w->name);
  strcat(w->filename, ".cgns");

  /* CGNS Base structure */

  w->n_bases = 0;
  w->bases = nullptr;

  /* Mesh time dependency */

  w->time_dependency = time_dependency;
  w->n_time_steps = 0;
  w->time_steps = nullptr;
  w->time_values = nullptr;

  w->is_open = false;

  /* Other variables */

  w->discard_bcs = false;
  w->discard_steady = false;

  w->discard_polygons = false;
  w->discard_polyhedra = false;
  w->divide_polygons = false;
  w->divide_polyhedra = true;

  w->preserve_precision = false;

  w->rank = 0;
  w->n_ranks = 1;

/* As CGNS does not handle polyhedral elements simply, polyhedra are
 * automatically tesselated with tetrahedra and pyramids
 * (adding a vertex near each polyhedron's center) unless discarded. */

  /* Copy from reference if present) */

  if (reference != nullptr) {
    w->discard_bcs = reference->discard_bcs;
    w->discard_steady = reference->discard_steady;
    w->discard_polygons = reference->discard_polygons;
    w->divide_polygons = reference->divide_polygons;
    w->discard_polyhedra = reference->discard_polyhedra;
    w->divide_polyhedra = reference->divide_polyhedra;
    w->preserve_precision = reference->preserve_precision;
    w->rank = reference->rank;
    w->n_ranks = reference->n_ranks;
#if defined(HAVE_MPI)
    w->comm = reference->comm;
    w->min_rank_step = reference->min_rank_step;
    w->min_block_size = reference->min_block_size;
#endif
  }

  if (w->discard_polyhedra)
    w->divide_polyhedra = false;
  if (w->discard_polygons)
    w->divide_polygons = false;

  /* Open CNGS file(s) */

  w->is_open = false;

  w->index = -1;

  /* Additional writers for linked files */

  w->mesh_writer = nullptr;

  return w;
}

/*----------------------------------------------------------------------------
 * Open a CGNS file.
 *
 * parameters:
 *   writer     <-> CGNS writer structure
 *----------------------------------------------------------------------------*/

static void
_open_file(fvm_to_cgns_writer_t   *writer)
{
  if (writer->is_open)
    return;

  int fn = -1;

  writer->index = -1;

  if (writer->rank == 0) {

    if (cg_open(writer->filename, CG_MODE_WRITE, &fn) != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_open() failed to open file \"%s\" : \n%s"),
                writer->filename, cg_get_error());

  }

#if defined(HAVE_MPI)
  if (writer->n_ranks > 1)
    MPI_Bcast(&fn, 1, MPI_INT, 0, writer->comm);
#endif

  writer->index = fn;

  writer->is_open = true;
}

/*----------------------------------------------------------------------------
 * Close a CGNS file.
 *
 * parameters:
 *   writer     <-> CGNS writer structure
 *----------------------------------------------------------------------------*/

static void
_close_file(fvm_to_cgns_writer_t   *writer)
{
  if (writer->is_open == true) {

    if (writer->rank == 0) {

      if (cg_close(writer->index) != CG_OK)
        bft_error(__FILE__, __LINE__, 0,
                  _("cg_close() failed to close file \"%s\" :\n%s"),
                  writer->filename, cg_get_error());

    }

    writer->index = -1;

  }

  writer->is_open = false;
}

/*----------------------------------------------------------------------------
 * Return datatype matching cgsize_t
 *
 * returns:
 *   datatype matching med_float
 *----------------------------------------------------------------------------*/

static cs_datatype_t
_cgsize_datatype(void)
{
  cs_datatype_t  cs_datatype = CS_DATATYPE_NULL;

  if (sizeof(cgsize_t) == sizeof(int32_t))
    cs_datatype = CS_INT32;
  else if (sizeof(cgsize_t) == sizeof(int64_t))
    cs_datatype = CS_INT64;
  else
    bft_error(__FILE__, __LINE__, 0 ,
              "Unexpected cgsize_t datatype size (%d).",
              (int)(sizeof(cgsize_t)));

  return cs_datatype;
}

/*----------------------------------------------------------------------------
 * Delete CGNS base structure included in CGNS writer structure.
 *
 * parameters:
 *   base    <-- CGNS base structure in FVM.
 *----------------------------------------------------------------------------*/

static fvm_to_cgns_base_t *
_del_base(fvm_to_cgns_base_t  *base)
{
  int i;

  CS_FREE(base->name);

  for (i = 0; i < base->n_sols; i++) {
    CS_FREE(base->solutions[i]->name);
    CS_FREE(base->solutions[i]);
  }

  CS_FREE(base->solutions);
  CS_FREE(base);

  return nullptr;
}

/*----------------------------------------------------------------------------
 * Return the CGNS base index associated with a given CGNS base name, or 0.
 *
 * parameters:
 *   writer     <-- CGNS writer structure
 *   base_name  <-- base name
 *
 * returns:
 *    CGNS base index, or 0 if base name is not associated with this
 *    CGNS writer structure in FVM
 *----------------------------------------------------------------------------*/

static int
_get_base_index(fvm_to_cgns_writer_t  *writer,
                const char            *base_name)
{
  int i;

  fvm_to_cgns_base_t  **base_array = writer->bases;
  int retval = CG_OK;

  assert(writer != nullptr);

  for (i = 0; i < writer->n_bases; i++) {
    if (strcmp(base_name, base_array[i]->name) == 0)
      break;
  }

  if (i == writer->n_bases)
    retval = 0;
  else
    retval = base_array[i]->index;

  return retval;
}

/*----------------------------------------------------------------------------
 * Associate a CGNS base name with a CGNS writer and return its index.
 * If the CGNS base was already associated, zero is returned.
 *
 * parameters:
 *   writer      <-- CGNS writer structure.
 *   base_name   <-- CGNS base name.
 *   mesh        <-- FVM mesh structure.
 *
 * returns:
 *   CGNS base index, or 0 if CGNS base already associated
 *----------------------------------------------------------------------------*/

static int
_add_base(fvm_to_cgns_writer_t  *writer,
          const char            *base_name,
          const fvm_nodal_t     *mesh)
{
  int  i;

  int  base_index = 0;
  int  rank = writer->rank;

  int entity_dim = fvm_nodal_get_max_entity_dim(mesh);

  int  retval = CG_OK;

  assert(writer != nullptr);

  if (entity_dim == 0) /* CGNS requires entity_dim > 0 */
    entity_dim = mesh->dim;

  /* Add a new CGNS base structure */

  writer->n_bases += 1;
  i = writer->n_bases - 1;

  CS_REALLOC(writer->bases, writer->n_bases, fvm_to_cgns_base_t *);
  CS_MALLOC(writer->bases[i], 1, fvm_to_cgns_base_t);
  CS_MALLOC(writer->bases[i]->name, strlen(base_name) + 1, char);

  strcpy(writer->bases[i]->name, base_name);
  writer->bases[i]->celldim  = entity_dim;
  writer->bases[i]->physdim  = mesh->dim;

  writer->bases[i]->n_sols = 0;
  writer->bases[i]->solutions = nullptr;

  if (rank == 0) {
    retval = cg_base_write(writer->index,
                           base_name,
                           entity_dim,
                           mesh->dim,
                           &base_index);
    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_base_write() failed to create a new base:\n"
                  "Associated writer: \"%s\"\n"
                  "Associated mesh: \"%s\"\n%s"),
                writer->name, base_name, cg_get_error());
  }

#if defined(HAVE_MPI)
  if (writer->n_ranks > 1)
    MPI_Bcast(&base_index, 1, MPI_INT, 0, writer->comm);
#endif

  writer->bases[i]->index = base_index;

  return base_index;
}

/*----------------------------------------------------------------------------
 * Get base id associated with given index.
 *
 * parameters:
 *   writer      <-- pointer associated with writer
 *   base_index  <-- index of associated base
 *
 * returns:
 *   local base id for the given CGNS index
 *----------------------------------------------------------------------------*/

inline static int
_base_id(const fvm_to_cgns_writer_t   *writer,
         int                           base_index)
{
  int  base_id;

  assert(writer != nullptr);
  assert(writer->bases != nullptr);

  for (base_id = 0; base_id < writer->n_bases; base_id++) {
    fvm_to_cgns_base_t *base = writer->bases[base_id];
    if (base->index == base_index)
      return base_id;
  }

  bft_error(__FILE__, __LINE__, 0,
            _("No CGNS base with index %d defined:\n"
              "Associated writer: \"%s\"\n"),
            base_index, writer->name);
  return -1;
}

/*----------------------------------------------------------------------------
 * Write a link to another CGNS file
 *
 * parameters:
 *   writer   <-- pointer to associated writer.
 *   base     <-- pointer to associated base data
 *   nodename <-- name of node to link
 *   filename <-- name of file to link to
 *----------------------------------------------------------------------------*/

static void
_write_zone_link(fvm_to_cgns_writer_t      *writer,
                 const fvm_to_cgns_base_t  *base,
                 const char                *nodename,
                 const char                *filename)
{
  if (writer->rank == 0) {

    /* Simply add link */

    int retval = cg_goto(writer->index,
                         base->index,
                         "Zone_t",
                         1,
                         "end");

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_goto() failed access requested Zone_t node:\n"
                  "Associated writer: \"%s\"\n"
                  "Associated mesh: \"%s\"\n%s"),
                writer->name, base->name, cg_get_error());

    size_t l = strlen(base->name) + strlen("Zone 1") + strlen(nodename) + 4;
    char *name_in_file;
    CS_MALLOC(name_in_file, l+1, char);
    snprintf(name_in_file, l, "/%s/%s/%s", base->name, "Zone 1", nodename);

    retval = cg_link_write(nodename,
                           filename,
                           name_in_file);

    CS_FREE(name_in_file);

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_link_write() failed to create link %s\n"
                  "Associated writer: \"%s\"\n"
                  "Associated mesh: \"%s\"\n%s"),
                nodename, writer->name, base->name, cg_get_error());

  }
}

/*----------------------------------------------------------------------------
 * Associate new time step with a CGNS writer.
 *
 * parameters:
 *   writer      <-- pointer associated with writer
 *   base_index  <-- index of associated base
 *   time_step   <-- time step number
 *   time_value  <-- time_value number
 *   location    <-- CGNS grid location
 *
 * returns:
 *   solution index of the new time step
 *----------------------------------------------------------------------------*/

static int
_add_solution(fvm_to_cgns_writer_t        *writer,
              int                          base_index,
              int                          time_step,
              double                       time_value,
              CGNS_ENUMT(GridLocation_t)   location)
{
  int  sol_id, sol_length;
  char sol_name[FVM_CGNS_NAME_SIZE + 1];

  int  zone_index = 1; /* We always write to the first zone */
  int  sol_index = 0;
  int  rank = writer->rank;

  int  retval = CG_OK;

  /* Find matching base */

  int base_id = _base_id(writer, base_index);

  /* Create a new pointer to fvm_to_cgns_solution_t */

  fvm_to_cgns_base_t *base = writer->bases[base_id];

  base->n_sols += 1;
  sol_id = base->n_sols - 1;

  CS_REALLOC(base->solutions, sol_id + 1, fvm_to_cgns_solution_t *);
  CS_MALLOC(base->solutions[sol_id], 1, fvm_to_cgns_solution_t);

  /* Initialization of the new solution structure */

  base->solutions[sol_id]->index = -1;
  base->solutions[sol_id]->time_step = time_step;
  base->solutions[sol_id]->time_value = time_value;
  base->solutions[sol_id]->location = location;
  base->solutions[sol_id]->name = nullptr;

  if (time_step < 0)
    sprintf(sol_name, "Steady (%s)",
            GridLocationName[location]);
  else
    sprintf(sol_name, "Solution %3d (%s)",
            (int)time_step, GridLocationName[location]);

  sol_length = strlen(sol_name);
  CS_MALLOC(base->solutions[sol_id]->name, sol_length + 1, char);
  strncpy(base->solutions[sol_id]->name, sol_name, sol_length + 1);
  base->solutions[sol_id]->name[sol_length] = '\0';

  if (rank == 0) {
    retval = cg_sol_write(writer->index,
                          base->index,
                          zone_index,
                          base->solutions[sol_id]->name,
                          location,
                          &sol_index);

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_sol_write() failed to create a "
                  "new solution node:\n"
                  "Associated writer: \"%s\"\n"
                  "Associated mesh: \"%s\"\n"
                  "Solution name: \"%s\"\n"
                  "Associated time value: %f \n%s"),
                writer->name, base->name,
                base->solutions[sol_id]->name, time_value, cg_get_error());
  }

#if defined(HAVE_MPI)
  if (writer->n_ranks > 1)
    MPI_Bcast(&sol_index, 1, MPI_INT, 0, writer->comm);
#endif

  base->solutions[sol_id]->index = sol_index;

  return (sol_index);
}

/*----------------------------------------------------------------------------
 * Find the solution index associated with a given time step.
 *
 * parameters:
 *   writer      <-- pointer to associated writer
 *   base_index  <-- index of associated base
 *   time_step   <-- time step number
 *   time_value  <-- time_value number
 *   location    <-- location of results (CellCenter, Vertex, ...)
 *
 * returns:
 *   solution index associated with given time step, or 0 if none found.
 *----------------------------------------------------------------------------*/

static int
_get_solution_index(const fvm_to_cgns_writer_t  *writer,
                    int                          base_index,
                    int                          time_step,
                    double                       time_value,
                    CGNS_ENUMT(GridLocation_t)   location)
{
  int sol_id, n_sols;

  int sol_index = 0;
  fvm_to_cgns_base_t     **bases = writer->bases;
  fvm_to_cgns_solution_t  *sol_ref = nullptr;

  const char time_value_err_string[] =
    N_("The time value associated with time step <%d> equals <%g>,\n"
       "but time value <%g> has already been associated with this time step.\n");

  /* Find matching base */

  int base_id = _base_id(writer, base_index);

  /* Any negative time step value indicates time independant values */

  if (time_step < 0) {
    time_step = -1;
    time_value = 0.0;
  }

  if (bases[base_id]->solutions != nullptr) {
    n_sols = bases[base_id]->n_sols;

    /* Search for index associated with time step */

    for (sol_id = 0; sol_id < n_sols; sol_id++) {

      sol_ref = bases[base_id]->solutions[sol_id];

      /* Check on grid location */
      if (location == sol_ref->location) {

        /* Check on time step */
        if (time_step == sol_ref->time_step) {

        /* Check on time value */
          if (time_value < sol_ref->time_value - 1.e-16 ||
              time_value > sol_ref->time_value + 1.e-16)
            bft_error(__FILE__, __LINE__, 0,
                _(time_value_err_string), time_step,
                time_value, sol_ref->time_value);

          else {
            sol_index = sol_ref->index;
            break;
          }
        }
        else
          sol_index = 0;
      }
      else
        sol_index = 0;

    } /* End of loop on existing solutions */

  }
  else  /* Set the first solution */
    sol_index = 0;

  return sol_index;
}

/*----------------------------------------------------------------------------
 * Create a CGNS zone associated with the writer.
 * Only one zone is created per base. Its index must be 1.
 *
 * parameters:
 *   mesh                 <-- FVM mesh structure.
 *   writer               <-- CGNS writer structure.
 *   base                 <-- CGNS base structure.
 *   export_sections      <-> pointer to a list of fvm_writer_section structures
 *----------------------------------------------------------------------------*/

static void
_add_zone(const fvm_nodal_t           *mesh,
          const fvm_to_cgns_writer_t  *writer,
          const fvm_to_cgns_base_t    *base,
          const fvm_writer_section_t  *export_sections)
{
  int zone_index;
  cgsize_t    zone_sizes[3];

  cs_gnum_t   n_g_entities = 0;
  cs_gnum_t   n_g_tesselated_elements = 0;
  cs_gnum_t   n_g_extra_vertices = 0;

  const fvm_writer_section_t *current_section = nullptr;

  int retval = CG_OK;

  assert(writer != nullptr);

  if (writer->rank != 0)
    return;

  /* Compute global number of vertices in this zone */

  cs_gnum_t n_g_vertices = fvm_nodal_get_n_g_vertices(mesh);

  /* Polyhedra are not currently handled in this CGNS writer.
     If they are not discarded, they have to be tesselated. */

  fvm_writer_count_extra_vertices(mesh,
                                  writer->divide_polyhedra,
                                  &n_g_extra_vertices,
                                  nullptr);

  zone_sizes[0] = n_g_vertices + n_g_extra_vertices;

  /* Check maximum element dimension in this zone */

  int vol_dim = -1;

  current_section = export_sections;
  while (current_section != nullptr) {
    const fvm_nodal_section_t *const section = current_section->section;
    if (section->entity_dim > vol_dim)
      vol_dim = section->entity_dim;
    current_section = current_section->next;
  }

  /* Compute global number of volume in this zone */

  current_section = export_sections;

  while (current_section != nullptr) {

    const fvm_nodal_section_t *const section = current_section->section;

    if (section->entity_dim == vol_dim) {

      if (current_section->type == section->type)

        /* Regular section */
        n_g_entities += fvm_nodal_section_n_g_elements(section);

      else {

        /* Tesselated section */
        fvm_tesselation_get_global_size(section->tesselation,
                                        current_section->type,
                                        &n_g_tesselated_elements,
                                        nullptr);

        n_g_entities += n_g_tesselated_elements;
      }

    }

    current_section = current_section->next;

  }  /* End of loop on sections */

  zone_sizes[1] = n_g_entities;

  /* Set boundary vertex size (zero if element not sorted) */

  zone_sizes[2] = 0;

  /* Create CGNS zone */

  assert(writer->is_open == true);

  retval = cg_zone_write(writer->index,
                         base->index,
                         "Zone 1",
                         zone_sizes,
                         CGNS_ENUMV(Unstructured),
                         &zone_index);

  if (retval != CG_OK)
    bft_error(__FILE__, __LINE__, 0,
              _("cg_zone_write() failed to create a new zone:\n"
                "Associated writer: \"%s\"\n"
                "Associated base: \"%s\"\n%s"),
              writer->name, base->name, cg_get_error());

  assert(zone_index == 1);
}

/*----------------------------------------------------------------------------
 * Reorder export list by higher dimension first
 *
 * parameters:
 *   export_list <-> pointer to reordered list of sections
 *----------------------------------------------------------------------------*/

static void
_reorder_export_list(fvm_writer_section_t  *export_list)
{
  fvm_writer_section_t *ordered = nullptr;

  /* Count and allocate temporary copy */

  int n_sections = 0;
  fvm_writer_section_t *n = export_list;

  while (n != nullptr) {
    n_sections++;
    n = n->next;
  }

  CS_MALLOC(ordered, n_sections, fvm_writer_section_t);
  n_sections = 0;

  for (int dim = 3; dim > -1; dim--) {

    n = export_list;
    while (n != nullptr) {
      if (n->section->entity_dim == dim) {
        ordered[n_sections] = *n;
        n_sections++;
      }
      n = n->next;
    }

  }

  for (int i = 0; i < n_sections; i++)
    export_list[i] = ordered[i];

  for (int i = 0; i < n_sections - 1; i++)
    (export_list[i]).next = &(export_list[i+1]);
  (export_list[n_sections - 1]).next = nullptr;

  CS_FREE(ordered);
}

/*----------------------------------------------------------------------------
 * Define section name and CGNS element type.
 *
 * parameters:
 *   export_section  <-- pointer to section list structure.
 *   section_id      <-- identification number of the section.
 *   section_name    --> name of section for CGNS.
 *   cgns_elem_type  --> CGNS element type for this section.
 *----------------------------------------------------------------------------*/

static void
_define_section(const fvm_writer_section_t  *section,
                int section_id,
                char section_name[FVM_CGNS_NAME_SIZE + 1],
                CGNS_ENUMT(ElementType_t) *cgns_elt_type)
{
  assert(section != nullptr);

  switch(section->type) {

  case FVM_EDGE:
    sprintf(section_name, "Edges_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(BAR_2);
    break;

  case FVM_FACE_TRIA:
    sprintf(section_name, "Triangles_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(TRI_3);
    break;

  case FVM_FACE_QUAD:
    sprintf(section_name, "Quadrangles_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(QUAD_4);
    break;

  case FVM_FACE_POLY:
    sprintf(section_name, "Polygons_%d", section_id);
#if CGNS_VERSION < 3200
    *cgns_elt_type = CGNS_ENUMV(MIXED);
#else
    *cgns_elt_type = CGNS_ENUMV(NGON_n);
#endif
    break;

  case FVM_CELL_TETRA:
    sprintf(section_name, "Tetrahedra_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(TETRA_4);
    break;

  case FVM_CELL_PYRAM:
    sprintf(section_name, "Pyramids_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(PYRA_5);
    break;

  case FVM_CELL_PRISM:
    sprintf(section_name, "Prisms_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(PENTA_6);
    break;

  case FVM_CELL_HEXA:
    sprintf(section_name, "Hexahedra_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(HEXA_8);
    break;

  default:
    sprintf(section_name, "Null_section_%d", section_id);
    *cgns_elt_type = CGNS_ENUMV(ElementTypeNull);
  }

  return;
}

/*----------------------------------------------------------------------------
 * Output function for coordinates.
 *
 * parameters:
 *   w             <-- writer
 *   base          <-- associated CGNS base
 *   grid_index    <-- associated grid index
 *   datatype      <-- output datatype
 *   cgns_datatype <-- output CGNS datatype
 *   coord_name    <-- name of coordinate
 *   block_start   <-- start global number of element for current block
 *   block_end     <-- past-the-end global number of element for current block
 *   buffer        <-> associated output buffer
 *----------------------------------------------------------------------------*/

static void
_coord_output(const fvm_to_cgns_writer_t  *w,
              const fvm_to_cgns_base_t    *base,
              int                          grid_index,
              cs_datatype_t                datatype,
              CGNS_ENUMT(DataType_t)       cgns_datatype,
              const char                  *coord_name,
              cs_gnum_t                    block_start,
              cs_gnum_t                    block_end,
              void                        *buffer)
{
  int coord_index = -1; /* local return value */

  const int   zone_index = 1; /* We always use zone index = 1 */

  /* Output in parallel case */

  /* TODO: add variant for parallel CGNS */

#if defined(HAVE_MPI)

  if (w->n_ranks > 1) {

    void *_values = nullptr;
    cs_file_serializer_t *s
      = cs_file_serializer_create(cs_datatype_size[datatype],
                                  1,
                                  block_start,
                                  block_end,
                                  0,
                                  buffer,
                                  w->comm);

    do {

      cs_gnum_t range[2] = {block_start, block_end};

      _values = cs_file_serializer_advance(s, range);

      if (_values != nullptr) { /* only possible on rank 0 */

        int retval = CG_OK;
        cgsize_t partial_write_idx_start = range[0];
        cgsize_t partial_write_idx_end   = range[1] - 1;

        assert(block_end > block_start);

        /* Fixed mesh */

        if (grid_index < 2) {

          retval = cg_coord_partial_write(w->index,
                                          base->index,
                                          zone_index,
                                          cgns_datatype,
                                          coord_name,
                                          &partial_write_idx_start,
                                          &partial_write_idx_end,
                                          _values,
                                          &coord_index);

          if (retval != CG_OK)
            bft_error(__FILE__, __LINE__, 0,
                      _("%s() failed to write coords:\n"
                        "Associated writer: \"%s\"\n"
                        "Associated base: \"%s\"\n"
                        "CGNS error:%s"),
                      "cg_coord_partial_write",
                      w->name, base->name, cg_get_error());

          partial_write_idx_start = partial_write_idx_end + 1;

        }

        /* Deforming mesh */

        else {

          /* TODO */

        }

      }

    } while (_values != nullptr);

    cs_file_serializer_destroy(&s);
  }

#endif /* defined(HAVE_MPI) */

  /* Output in serial case */

  if (w->n_ranks == 1) {

    /* Fixed or first coordinates */

    if (grid_index < 2) {

      int retval = cg_coord_write(w->index,
                                  base->index,
                                  zone_index,
                                  cgns_datatype,
                                  coord_name,
                                  buffer,
                                  &coord_index);

      if (retval != CG_OK)
        bft_error(__FILE__, __LINE__, 0,
                  _("%s() failed to write coords:\n"
                    "Associated writer: \"%s\"\n"
                    "Associated base: \"%s\"\n"
                    "CGNS error:%s"),
                  "cg_coord_write",
                  w->name, base->name, cg_get_error());

    }

  }
}

/*----------------------------------------------------------------------------
 * Output function for boundary conditions.
 *
 * This function is passed to fvm_writer_field_helper_output_* functions.
 *
 * parameters:
 *   context      <-> pointer to writer and field context
 *   datatype     <-- output datatype
 *   dimension    <-- output field dimension
 *   component_id <-- output component id (if non-interleaved)
 *   block_start  <-- start global number of element for current block
 *   block_end    <-- past-the-end global number of element for current block
 *   buffer       <-> associated output buffer
 *----------------------------------------------------------------------------*/

static void
_bcs_output(void           *context,
            cs_datatype_t   datatype,
            int             dimension,
            int             component_id,
            cs_gnum_t       block_start,
            cs_gnum_t       block_end,
            void           *buffer)
{
  CS_UNUSED(datatype);
  CS_UNUSED(dimension);
  CS_UNUSED(component_id);

  auto c = static_cast<_cgns_bc_context_t *>(context);

  const fvm_to_cgns_writer_t *w = c->writer;

  /* Array should be gathered to rank 0 due to CGNS API limitation
     when using serial CGNS API */

  if (w->rank > 0) {
    assert(block_start >= block_end);
    return;
  }

  const fvm_group_class_set_t *gc_set = c->mesh->gc_set;

  if (gc_set == nullptr)
    return;

  cgsize_t    n_elts = block_end - block_start;
  int        *f_gc_id = (int *)buffer;

  int n_gc_sets = fvm_group_class_set_size(gc_set) + 1;

  /* Determine which group classes are referenced here */

  cgsize_t *gc_elt_count;
  CS_MALLOC(gc_elt_count, n_gc_sets, cgsize_t);

  for (int i = 0; i < n_gc_sets; i++)
    gc_elt_count[i] = 0;

  for (cgsize_t i = 0; i < n_elts; i++) {
    int _gc_id = f_gc_id[i] - 1;
    if (_gc_id < 0)
      continue;
    assert(_gc_id < n_gc_sets);
    if (_gc_id < n_gc_sets)
      gc_elt_count[_gc_id] += 1;
  }

  /* Now we can determine which groups are referenced */

  cs_map_name_to_id_t *group_names = cs_map_name_to_id_create();

  int n_max_groups = 0;

  for (int gc_id = 0; gc_id < n_gc_sets; gc_id++) {
    if (gc_elt_count[gc_id]) {
      const fvm_group_class_t *gc = fvm_group_class_set_get(gc_set, gc_id);
      int n = fvm_group_class_get_n_groups(gc);
      const char **g_names = fvm_group_class_get_group_names(gc);
      if (n > n_max_groups)
        n_max_groups = n;
      for (int i = 0; i < n; i++)
        cs_map_name_to_id(group_names, g_names[i]);  /* insert in map */
    }
  }

  /* Build index on groups */

  int n_groups = cs_map_name_to_id_size(group_names);

  int *gc_idx;
  CS_MALLOC(gc_idx, n_gc_sets+1, int);

  for (int gc_id = 0; gc_id < n_gc_sets; gc_id++) {
    if (gc_elt_count[gc_id]) {
      const fvm_group_class_t *gc = fvm_group_class_set_get(gc_set, gc_id);
      gc_idx[gc_id+1] = fvm_group_class_get_n_groups(gc);
    }
    else
      gc_idx[gc_id+1] = 0;
  }

  CS_FREE(gc_elt_count);

  /* Transform count to index */

  gc_idx[0] = 0;
  for (int i = 0; i < n_gc_sets; i++) {
    gc_idx[i+1] += gc_idx[i];
  }

  int *gc_g_id;
  CS_MALLOC(gc_g_id, gc_idx[n_gc_sets], int);

  for (int gc_id = 0; gc_id < n_gc_sets; gc_id++) {
    if (gc_idx[gc_id+1] > gc_idx[gc_id]) {
      const fvm_group_class_t *gc = fvm_group_class_set_get(gc_set, gc_id);
      int n = fvm_group_class_get_n_groups(gc);
      const char **g_names = fvm_group_class_get_group_names(gc);
      for (int i = 0; i < n; i++) {
        int g_id = cs_map_name_to_id(group_names, g_names[i]);
        gc_g_id[gc_idx[gc_id] + i] = g_id;
      }
    }
  }

  /* Now build lists for each group */

  cgsize_t *g_idx, *g_count;
  CS_MALLOC(g_idx, n_groups+1, cgsize_t);
  CS_MALLOC(g_count, n_groups, cgsize_t);

  for (int g_id = 0; g_id < n_groups; g_id++)
    g_count[g_id] = 0;

  for (cgsize_t i = 0; i < n_elts; i++) {
    int _gc_id = f_gc_id[i] - 1;
    if (_gc_id < 0)
      continue;
    int s_id = gc_idx[_gc_id];
    int e_id = gc_idx[_gc_id+1];
    for (int j = s_id; j < e_id; j++) {
      int g_id = gc_g_id[j];
      g_count[g_id] += 1;
    }
  }

  g_idx[0] = 0;
  for (int g_id = 0; g_id < n_groups; g_id++) {
    g_idx[g_id+1] = g_idx[g_id] + g_count[g_id];
    g_count[g_id] = 0;
  }

  cgsize_t *g_elts;
  CS_MALLOC(g_elts, g_idx[n_groups], cgsize_t);

  cgsize_t id_shift = c->n_g_vol_elts + 1;

  for (cgsize_t i = 0; i < n_elts; i++) {
    int _gc_id = f_gc_id[i] - 1;
    if (_gc_id < 0)
      continue;
    int s_id = gc_idx[_gc_id];
    int e_id = gc_idx[_gc_id+1];
    for (int j = s_id; j < e_id; j++) {
      int g_id = gc_g_id[j];
      g_elts[g_idx[g_id] + g_count[g_id]] = id_shift + i;
      g_count[g_id] += 1;
    }
  }

  CS_FREE(g_count);

  CS_FREE(gc_idx);
  CS_FREE(gc_g_id);

  /* Now loop on groups */

  const int   zone_index = 1; /* We always use zone index = 1 */

  int  retval = CG_OK;

  for (int i = 0; i < n_groups; i++) {

    const char *bc_name = cs_map_name_to_id_reverse(group_names, i);

    cgsize_t s_id = g_idx[i];
    cgsize_t e_id = g_idx[i+1];

    cgsize_t npnts = e_id - s_id;
    cgsize_t *pnts = g_elts + s_id;

    int bc_id = 0;

    if (npnts < 1)
      continue;

#if CGNS_VERSION > 4100
    CGNS_ENUMT(PointSetType_t) pset_type = CGNS_ENUMV(PointList);
#else
    CGNS_ENUMT(PointSetType_t) pset_type = CGNS_ENUMV(ElementList);
#endif

    /* Check if we have a continous range */

    if (npnts > 1) {

      bool range = true;
      for (cgsize_t j = 1; j < npnts; j++) {
        if (pnts[j] != pnts[j-1]) {
          range = false;
          break;
        }
      }

      if (range) {
        pnts[1] = pnts[npnts-1];
        npnts = 2;
#if CGNS_VERSION > 4100
        pset_type = CGNS_ENUMV(PointRange);
#else
        pset_type = CGNS_ENUMV(ElementRange);
#endif
      }

    }

    retval = cg_boco_write(w->index,
                           c->base->index,
                           zone_index,
                           bc_name,
                           CGNS_ENUMV(BCTypeUserDefined),
                           pset_type,
                           npnts,
                           pnts,
                           &bc_id);

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("Error writing boundary conditions (cg_boco_write):\n"
                  "Associated writer: \"%s\"\n"
                  "Associated mesh: \"%s\"\n"
                  "Associated BC: \"%s\"\n%s"),
                w->name, c->mesh->name, bc_name, cg_get_error());

#if CGNS_VERSION > 4100

    else
      retval = cg_goto(w->index,
                       c->base->index,
                       "Zone_t",
                       zone_index,
                       "ZoneBC_t",
                       1,
                       "BC_t",
                       bc_id,
                       "end");

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("Error accessing boundary conditions (cg_goto):\n"
                  "Associated writer: \"%s\"\n"
                  "Associated mesh: \"%s\"\n"
                  "Associated BC: \"%s\"\n%s"),
                w->name, c->mesh->name, bc_name, cg_get_error());

    else if (retval == CG_OK)
      retval = cg_gridlocation_write(CGNS_ENUMV(FaceCenter));

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("Error writing BC grid location (cg_gridlocation_write):\n"
                  "Associated writer: \"%s\"\n"
                  "Associated mesh: \"%s\"\n"
                  "Associated BC: \"%s\"\n%s"),
                w->name, c->mesh->name, bc_name, cg_get_error());

#endif

  }

  CS_FREE(g_idx);
  CS_FREE(g_elts);

  cs_map_name_to_id_destroy(&group_names);
}

/*----------------------------------------------------------------------------
 * Output function for field values.
 *
 * This function is passed to fvm_writer_field_helper_output_* functions.
 *
 * parameters:
 *   context      <-> pointer to writer and field context
 *   datatype     <-- output datatype
 *   dimension    <-- output field dimension
 *   component_id <-- output component id (if non-interleaved)
 *   block_start  <-- start global number of element for current block
 *   block_end    <-- past-the-end global number of element for current block
 *   buffer       <-> associated output buffer
 *----------------------------------------------------------------------------*/

static void
_field_output(void           *context,
              cs_datatype_t   datatype,
              int             dimension,
              int             component_id,
              cs_gnum_t       block_start,
              cs_gnum_t       block_end,
              void           *buffer)
{
  CS_UNUSED(dimension);

  auto c = static_cast<_cgns_context_t *>(context);

  int field_index = -1; /* local return value */

  const fvm_to_cgns_writer_t  *w = c->writer;

  const int   zone_index = 1; /* We always use zone index = 1 */
  const char *field_c_label =   c->field_label
                              + component_id * (FVM_CGNS_NAME_SIZE + 1);

  CGNS_ENUMT(DataType_t) cgns_datatype;
  switch(datatype) {
  case CS_CHAR:
    cgns_datatype = CGNS_ENUMV(Character);
    break;
  case CS_FLOAT:
    cgns_datatype = CGNS_ENUMV(RealSingle);
    break;
  case CS_DOUBLE:
    cgns_datatype = CGNS_ENUMV(RealDouble);
    break;
  case CS_INT32:
    cgns_datatype = CGNS_ENUMV(Integer);
    break;
  case CS_INT64:
    cgns_datatype = CGNS_ENUMV(LongInteger);
    break;
  default:
    cgns_datatype = CGNS_ENUMV(DataTypeNull);
    assert(0);
  }

  /* Output in parallel case */

  /* TODO: add variant for parallel CGNS */

#if defined(HAVE_MPI)

  if (w->n_ranks > 1) {

    void *_values = nullptr;
    cs_file_serializer_t *s
      = cs_file_serializer_create(cs_datatype_size[datatype],
                                  1,
                                  block_start,
                                  block_end,
                                  0,
                                  buffer,
                                  w->comm);

    cgsize_t partial_write_idx_start = 1;

    do {

      cs_gnum_t range[2] = {block_start, block_end};

      _values = cs_file_serializer_advance(s, range);

      if (_values != nullptr) { /* only possible on rank 0 */

        int retval = CG_OK;
        cgsize_t partial_write_idx_end
          = partial_write_idx_start + range[1] - range[0] - 1;

        assert(block_end > block_start);

        retval = cg_field_partial_write(w->index,
                                        c->base->index,
                                        zone_index,
                                        c->solution_index,
                                        cgns_datatype,
                                        field_c_label,
                                        &partial_write_idx_start,
                                        &partial_write_idx_end,
                                        _values,
                                        &field_index);

        if (retval != CG_OK)
          bft_error(__FILE__, __LINE__, 0,
                    _("%s() failed to write "
                      "field values:\n\"%s\"\n"
                      "Associated writer: \"%s\"\n"
                      "Associated base: \"%s\"\n%s"),
                    "cg_field_partial_write",
                    field_c_label, w->name, c->base->name,
                    cg_get_error());

        partial_write_idx_start = partial_write_idx_end + 1;

      }

    } while (_values != nullptr);

    cs_file_serializer_destroy(&s);
  }

#endif /* defined(HAVE_MPI) */

  /* Output in serial case */

  if (w->n_ranks == 1) {

    int retval = cg_field_write(w->index,
                                c->base->index,
                                zone_index,
                                c->solution_index,
                                cgns_datatype,
                                field_c_label,
                                buffer,
                                &field_index);

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("%s() failed to write "
                  "field values:\n\"%s\"\n"
                  "Associated writer: \"%s\"\n"
                  "Associated base: \"%s\"\n%s"),
                "cg_field_write",
                field_c_label, w->name, c->base->name,
                cg_get_error());

  }
}

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Write vertex coordinates to a CGNS file in parallel mode
 *
 * parameters:
 *   writer        <-- pointer to associated writer.
 *   mesh          <-- pointer to nodal mesh structure that should be written.
 *   base          <-- pointer to CGNS base structure.
 *   grid_index    <-- grid index
 *----------------------------------------------------------------------------*/

static void
_export_vertex_coords_g(fvm_to_cgns_writer_t  *writer,
                        const fvm_nodal_t     *mesh,
                        fvm_to_cgns_base_t    *base,
                        int                    grid_index)
{
  cs_block_dist_info_t  bi;

  cs_gnum_t       n_g_extra_vertices = 0;
  cs_lnum_t       n_extra_vertices = 0, n_vertices_tot = 0;
  cs_coord_t     *extra_vertex_coords = nullptr;
  unsigned char  *part_coords = nullptr, *block_coords = nullptr;

  cs_part_to_block_t   *d = nullptr;

  const double      *vertex_coords = mesh->vertex_coords;
  const cs_lnum_t   *parent_vertex_id = mesh->parent_vertex_id;
  const cs_lnum_t   n_vertices
    = fvm_io_num_get_local_count(mesh->global_vertex_num);

  cs_datatype_t datatype;
  CGNS_ENUMT(DataType_t)  cgns_datatype;

  const char *const coord_name[3] = {"CoordinateX",
                                     "CoordinateY",
                                     "CoordinateZ"};

  assert(base != nullptr);

  if (sizeof(cs_coord_t) == sizeof(double)) {
    datatype = CS_DOUBLE;
    cgns_datatype = CGNS_ENUMV(RealDouble);
  }
  else {
    datatype = CS_FLOAT;
    cgns_datatype = CGNS_ENUMV(RealSingle);
  }

  /* Check for extra vertex coordinates */

  fvm_writer_count_extra_vertices(mesh,
                                  writer->divide_polyhedra,
                                  &n_g_extra_vertices,
                                  &n_extra_vertices);

  n_vertices_tot = n_vertices + n_extra_vertices;

  /* Initialize distribution info */

  fvm_writer_vertex_part_to_block_create(writer->min_rank_step,
                                         writer->min_block_size,
                                         n_g_extra_vertices,
                                         n_extra_vertices,
                                         mesh,
                                         &bi,
                                         &d,
                                         writer->comm);

  /* Compute extra vertex coordinates if present */

  extra_vertex_coords = fvm_writer_extra_vertex_coords(mesh, n_extra_vertices);

  /* Allocate arrays */

  {
    size_t block_buf_size = (bi.gnum_range[1] - bi.gnum_range[0]);
    size_t part_buf_size = n_vertices_tot;
    block_buf_size *= cs_datatype_size[datatype];
    part_buf_size *= cs_datatype_size[datatype];
    CS_MALLOC(block_coords, block_buf_size, unsigned char);
    CS_MALLOC(part_coords, part_buf_size, unsigned char);
  }

  /* Vertex coordinates */
  /*--------------------*/

  size_t stride = (size_t)(mesh->dim);

  /* Loop on spatial dimension */

  for (int j = 0; j < base->physdim; j ++) {

    if (j < mesh->dim) {

      if (datatype == CS_FLOAT) {
        float *_part_coords = (float *)part_coords;
        if (parent_vertex_id != nullptr) {
          for (cs_lnum_t i = 0; i < n_vertices; i++)
            _part_coords[i] = vertex_coords[parent_vertex_id[i]*stride + j];
        }
        else {
          for (cs_lnum_t i = 0; i < n_vertices; i++)
            _part_coords[i] = vertex_coords[i*stride + j];
        }
        for (cs_lnum_t i = 0; i < n_extra_vertices; i++)
          _part_coords[n_vertices + i] = extra_vertex_coords[(i*stride) + j];
      }
      else {
        assert(datatype == CS_DOUBLE);
        double *_part_coords = (double *)part_coords;
        if (parent_vertex_id != nullptr) {
          for (cs_lnum_t i = 0; i < n_vertices; i++)
            _part_coords[i] = vertex_coords[parent_vertex_id[i]*stride + j];
        }
        else {
          for (cs_lnum_t i = 0; i < n_vertices; i++)
            _part_coords[i] = vertex_coords[i*stride + j];
        }
        for (cs_lnum_t i = 0; i < n_extra_vertices; i++)
          _part_coords[n_vertices + i] = extra_vertex_coords[(i*stride) + j];
      }
    }
    else {
      if (datatype == CS_FLOAT) {
        float *_part_coords = (float *)part_coords;
        for (cs_lnum_t i = 0; i < n_vertices_tot; i++)
          _part_coords[i] = 0.0;
      }
      else {
        assert(datatype == CS_DOUBLE);
        double *_part_coords = (double *)part_coords;
        for (cs_lnum_t i = 0; i < n_vertices_tot; i++)
          _part_coords[i] = 0.0;
      }
    }

    cs_part_to_block_copy_array(d,
                                datatype,
                                1,
                                part_coords,
                                block_coords);

    _coord_output(writer,
                  base,
                  grid_index,
                  datatype,
                  cgns_datatype,
                  coord_name[j],
                  bi.gnum_range[0],
                  bi.gnum_range[1],
                  block_coords);

  } /* end of loop on spatial dimension */

  cs_part_to_block_destroy(&d);

  CS_FREE(block_coords);
  CS_FREE(part_coords);
  if (extra_vertex_coords != nullptr)
    CS_FREE(extra_vertex_coords);
}

#endif

/*----------------------------------------------------------------------------
 * Write vertex coordinates to a CGNS file in serial mode
 *
 * parameters:
 *   writer        <-- pointer to associated writer.
 *   mesh          <-- pointer to nodal mesh structure.
 *   base          <-- pointer to CGNS base structure.
 *   grid_index    <-- associated grid index
 *----------------------------------------------------------------------------*/

static void
_export_vertex_coords_l(const fvm_to_cgns_writer_t  *writer,
                        const fvm_nodal_t     *mesh,
                        fvm_to_cgns_base_t    *base,
                        int                    grid_index)
{
  cs_lnum_t   i, j;
  cs_datatype_t datatype;
  CGNS_ENUMT(DataType_t)  cgns_datatype;

  size_t  stride = (size_t)mesh->dim;
  cs_lnum_t   n_extra_vertices = 0;
  cs_coord_t  *extra_vertex_coords = nullptr;
  cs_coord_t  *coords_tmp = nullptr;

  const cs_lnum_t   n_vertices = mesh->n_vertices;
  const cs_lnum_t *parent_vertex_id = mesh->parent_vertex_id;
  const cs_coord_t *vertex_coords = mesh->vertex_coords;

  const char *const coord_name[3] = {"CoordinateX",
                                     "CoordinateY",
                                     "CoordinateZ"};

  assert(writer->is_open == true);
  assert(base != nullptr);

  if (sizeof(cs_coord_t) == sizeof(double)) {
    datatype = CS_DOUBLE;
    cgns_datatype = CGNS_ENUMV(RealDouble);
  }
  else {
    datatype = CS_FLOAT;
    cgns_datatype = CGNS_ENUMV(RealSingle);
  }

  /* Compute extra vertex coordinates if present */

  fvm_writer_count_extra_vertices(mesh,
                                  writer->divide_polyhedra,
                                  nullptr,
                                  &n_extra_vertices);

  extra_vertex_coords = fvm_writer_extra_vertex_coords(mesh, n_extra_vertices);

  CS_MALLOC(coords_tmp, n_vertices + n_extra_vertices, cs_coord_t);

  /* Loop on dimension */

  for (j = 0; j < base->physdim; j ++) {

    /* Vertex coordinates */

    if (parent_vertex_id != nullptr) {
      for (i = 0; i < n_vertices; i++)
        coords_tmp[i] = vertex_coords[parent_vertex_id[i]*stride + j];
    }
    else {
      for (i = 0; i < n_vertices; i++)
        coords_tmp[i] = vertex_coords[i*stride + j];
    }

    for (i = 0 ; i < n_extra_vertices ; i++)
      coords_tmp[n_vertices + i] = extra_vertex_coords[i*stride + j];

    /* Write grid coordinates */

    if (coords_tmp != nullptr) {

      _coord_output(writer,
                    base,
                    grid_index,
                    datatype,
                    cgns_datatype,
                    coord_name[j],
                    1,
                    n_vertices + n_extra_vertices + 1,
                    coords_tmp);

    }

  } /* End of loop on coordinates */

  CS_FREE(coords_tmp);

  if (extra_vertex_coords != nullptr)
    CS_FREE(extra_vertex_coords);
}

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Write strided global connectivity block to a CGNS file
 *
 * parameters:
 *   export_section <-- pointer to section to export
 *   writer         <-- pointer to associated writer
 *   base           <-- pointer to CGNS base structure
 *   section_id     <-- section identificator number
 *   global_counter <-- counter to update element shift after each section
 *   num_start      <-- global number of first element for this block
 *   num_end        <-- global number of past last element for this block
 *   block_connect  <-> global connectivity block array
 *----------------------------------------------------------------------------*/

static void
_write_block_connect_s_g(const fvm_writer_section_t  *current_section,
                         const fvm_to_cgns_writer_t  *writer,
                         const fvm_to_cgns_base_t    *base,
                         int                          section_id,
                         const cs_gnum_t             *global_counter,
                         cs_gnum_t                    num_start,
                         cs_gnum_t                    num_end,
                         cgsize_t                     block_connect[])
{
  char section_name[FVM_CGNS_NAME_SIZE + 1];
  CGNS_ENUMT(ElementType_t) cgns_elt_type; /* Definition in cgnslib.h */

  int  section_index = -1;

  const int  zone_index = 1; /* We always use zone index = 1 */
  const int stride = fvm_nodal_n_vertices_element[current_section->type];

  int  retval = CG_OK;

  _define_section(current_section, section_id, section_name, &cgns_elt_type);

  /* TODO add parallel version of the API */

  /* For non-parallel IO, use serializer */

  {
    cgsize_t *_block_connect = nullptr;

    cs_file_serializer_t *s = cs_file_serializer_create(sizeof(cgsize_t),
                                                        stride,
                                                        num_start,
                                                        num_end,
                                                        0,
                                                        block_connect,
                                                        writer->comm);

    do {
      cs_gnum_t range[2] = {num_start, num_end};

      _block_connect = (cgsize_t *)cs_file_serializer_advance(s, range);

      if (_block_connect != nullptr) { /* only possible on rank 0 */

        cgsize_t  s_start = *global_counter + range[0];
        cgsize_t  s_end   = *global_counter + range[1] - 1;

        if (range[0] == 1) { /* First pass */
          retval = cg_section_partial_write(writer->index,
                                            base->index,
                                            zone_index,
                                            section_name,
                                            cgns_elt_type,
                                            s_start,
                                            s_end,
                                            0, /* unsorted boundary elements */
                                            &section_index);
          if (retval != CG_OK)
            bft_error(__FILE__, __LINE__, 0,
                      _("cg_section_partial_write() failed to write elements:\n"
                        "Associated writer: \"%s\"\n"
                        "Associated base: \"%s\"\n"
                        "Associated section name: \"%s\"\n%s"),
                      writer->name, base->name, section_name, cg_get_error());
        }

        if (retval == CG_OK)
          retval = cg_elements_partial_write(writer->index,
                                             base->index,
                                             zone_index,
                                             section_index,
                                             s_start,
                                             s_end,
                                             _block_connect);
        if (retval != CG_OK)
          bft_error(__FILE__, __LINE__, 0,
                    _("cg_elements_partial_write() failed to write elements:\n"
                      "Associated writer: \"%s\"\n"
                      "Associated base: \"%s\"\n"
                      "Associated section name: \"%s\"\n"
                      "Associated range: [%llu, %llu]\n%s\n"),
                    writer->name, base->name, section_name,
                    (unsigned long long) s_start,
                    (unsigned long long) s_end,
                    cg_get_error());

      }

    } while (_block_connect != nullptr);

    cs_file_serializer_destroy(&s);
  }
}

/*----------------------------------------------------------------------------
 * Write strided connectivity to a CGNS file in parallel mode.
 *
 * parameters:
 *   export_section   <-- pointer to sections list to export.
 *   w                <-- pointer to associated writer.
 *   base             <-- pointer to CGNS base structure.
 *   section_id       <-- section identificator number.
 *   global_counter   <-- counter to update the shift after each section export.
 *
 * returns:
 *   pointer to next section in list
 *----------------------------------------------------------------------------*/

static const fvm_writer_section_t *
_export_nodal_strided_g(const fvm_writer_section_t  *export_section,
                        const fvm_to_cgns_writer_t  *w,
                        const fvm_nodal_t           *mesh,
                        const fvm_to_cgns_base_t    *base,
                        int                          section_id,
                        cs_gnum_t                   *global_counter)
{
  assert(export_section != nullptr);

  cs_block_dist_info_t bi;

  cs_gnum_t   block_size = 0;

  cs_part_to_block_t  *d = nullptr;
  cgsize_t    *part_vtx_num = nullptr, *block_vtx_num = nullptr;

  const fvm_writer_section_t *current_section = export_section;
  const fvm_nodal_section_t *section = current_section->section;
  const int stride = fvm_nodal_n_vertices_element[section->type];

  const cs_lnum_t   n_elements = section->n_elements;
  const cs_gnum_t   n_g_elements
    = fvm_io_num_get_global_count(section->global_element_num);
  const cs_gnum_t   *g_elt_num
    = fvm_io_num_get_global_num(section->global_element_num);
  const cs_gnum_t   *g_vtx_num
    = fvm_io_num_get_global_num(mesh->global_vertex_num);

  /* Prepare distribution structures */

  size_t  min_block_size = w->min_block_size / (sizeof(cgsize_t) * stride);

  bi = cs_block_dist_compute_sizes(w->rank,
                                   w->n_ranks,
                                   w->min_rank_step,
                                   min_block_size,
                                   n_g_elements);

  d = cs_part_to_block_create_by_gnum(w->comm, bi, n_elements, g_elt_num);

  /* Build connectivity */

  block_size = bi.gnum_range[1] - bi.gnum_range[0];

  CS_MALLOC(block_vtx_num, block_size*stride, cgsize_t);
  CS_MALLOC(part_vtx_num, n_elements*stride, cgsize_t);

  for (cs_lnum_t i = 0; i < n_elements; i++) {
    for (cs_lnum_t j = 0; j < stride; j++) {
      part_vtx_num[i*stride + j]
        = g_vtx_num[section->vertex_num[i*stride + j] - 1];
    }
  }

  cs_part_to_block_copy_array(d,
                              _cgsize_datatype(),
                              stride,
                              part_vtx_num,
                              block_vtx_num);

  CS_FREE(part_vtx_num);

  cs_part_to_block_destroy(&d);

  /* Write to file */

  _write_block_connect_s_g(current_section,
                           w,
                           base,
                           section_id,
                           global_counter,
                           bi.gnum_range[0],
                           bi.gnum_range[1],
                           block_vtx_num);

  /* Free remaining memory */

  CS_FREE(block_vtx_num);

  *global_counter += n_g_elements;

  return current_section->next;
}

#endif

/*----------------------------------------------------------------------------
 * Write strided connectivity to a CGNS file in serial mode
 *
 * parameters:
 *   export_section   <-- pointer to sections list to export.
 *   w                <-- pointer to associated writer.
 *   base             <-- pointer to CGNS base structure.
 *   section_id       <-- section identificator number.
 *   global_counter   <-- counter to update the shift after each section export.
 *----------------------------------------------------------------------------*/

static const fvm_writer_section_t *
_export_nodal_strided_l(const fvm_writer_section_t  *export_section,
                        const fvm_to_cgns_writer_t  *w,
                        const fvm_to_cgns_base_t    *base,
                        int                          section_id,
                        cs_gnum_t                   *global_counter)
    {
  int  section_index;
  char section_name[FVM_CGNS_NAME_SIZE + 1];
  CGNS_ENUMT(ElementType_t) cgns_elt_type; /* Definition in cgnslib.h */

  cs_gnum_t elt_start = 0, elt_end = 0;

  const int  zone_index = 1; /* We always use zone index = 1 */
  const fvm_writer_section_t  *current_section = export_section;
  const fvm_nodal_section_t *const section = current_section->section;

  int  retval = CG_OK;

  assert(current_section != nullptr);

  _define_section(current_section, section_id, section_name, &cgns_elt_type);

  elt_start = 1 + *global_counter;
  elt_end   = section->n_elements + *global_counter;

  if (section->vertex_num != nullptr) {
    cgsize_t *_vertex_num = nullptr;
    const cgsize_t *vertex_num = (const cgsize_t *)section->vertex_num;
    if (sizeof(cgsize_t) != sizeof(cs_lnum_t)) {
      cgsize_t i = 0, n = (elt_end + 1 - elt_start)*section->stride;
      CS_MALLOC(_vertex_num, n, cgsize_t);
      vertex_num = _vertex_num;
      for (i = 0; i < n; i++)
        _vertex_num[i] = section->vertex_num[i];
    }
    retval = cg_section_write(w->index,
                              base->index,
                              zone_index,
                              section_name,
                              cgns_elt_type,
                              elt_start,
                              elt_end,
                              0, /* unsorted boundary elements */
                              vertex_num,
                              &section_index);
    if (_vertex_num != nullptr)
      CS_FREE(_vertex_num);
  }

  if (retval != CG_OK)
    bft_error(__FILE__, __LINE__, 0,
              _("cg_section_write() failed to write elements:\n"
                "Associated writer: \"%s\"\n"
                "Associated base: \"%s\"\n"
                "Associated section name: \"%s\"\n%s"),
              w->name, base->name, section_name, cg_get_error());

  *global_counter += section->n_elements;

  current_section = current_section->next;

  return current_section;
}

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Write strided connectivity from tesselated elements to a CGNS file
 * in parallel mode.
 *
 * parameters:
 *   export_section   <-- pointer to sections list to export.
 *   w                <-- pointer to associated writer.
 *   base             <-- pointer to CGNS base structure.
 *   section_id       <-- section identificator number.
 *   global_counter   <-- counter to update the shift after each section export.
 *
 * returns:
 *   pointer to next section in list
 *----------------------------------------------------------------------------*/

static const fvm_writer_section_t *
_export_nodal_tesselated_g(const fvm_writer_section_t  *export_section,
                           const fvm_to_cgns_writer_t  *w,
                           const fvm_nodal_t           *mesh,
                           const fvm_to_cgns_base_t    *base,
                           int                          section_id,
                           cs_gnum_t                   *global_counter)
{
  assert(export_section != nullptr);

  cs_block_dist_info_t bi;

  cs_lnum_t   part_size = 0;

  cs_gnum_t   n_g_sub_elements = 0;
  cs_gnum_t   block_size = 0, block_start = 0, block_end = 0;

  cs_part_to_block_t  *d = nullptr;
  cs_lnum_t   *part_index, *block_index = nullptr;
  cgsize_t    *part_vtx_num = nullptr, *block_vtx_num = nullptr;

  const fvm_writer_section_t *current_section = export_section;
  const fvm_nodal_section_t *section = current_section->section;
  const fvm_tesselation_t *tesselation = section->tesselation;
  const cs_gnum_t extra_vertex_base = current_section->extra_vertex_base;
  const fvm_element_t  type = current_section->type;
  const int stride = fvm_nodal_n_vertices_element[type];

  const cs_lnum_t   n_elements = fvm_tesselation_n_elements(tesselation);
  const cs_gnum_t   n_g_elements
    = fvm_io_num_get_global_count(section->global_element_num);
  const cs_lnum_t   n_sub_elements
    = fvm_tesselation_n_sub_elements(tesselation, type);
  const cs_lnum_t   *sub_element_idx
      = fvm_tesselation_sub_elt_index(tesselation, type);
  const cs_gnum_t   *g_elt_num
    = fvm_io_num_get_global_num(section->global_element_num);

  /* Adjust min block size based on mean number of sub-elements */

  fvm_tesselation_get_global_size(tesselation,
                                  type,
                                  &n_g_sub_elements,
                                  nullptr);

  /* Decode connectivity */

  part_size = n_sub_elements * stride;
  assert(sub_element_idx[n_elements]*stride == part_size);

  if (n_elements > 0) {

    cs_gnum_t   *part_vtx_gnum;

    CS_MALLOC(part_vtx_num, part_size, cgsize_t);
    CS_MALLOC(part_vtx_gnum, part_size, cs_gnum_t);

    fvm_tesselation_decode_g(tesselation,
                             type,
                             mesh->global_vertex_num,
                             extra_vertex_base,
                             part_vtx_gnum);

    /* Convert to write type */

    if (n_elements > 0) {
      for (cs_lnum_t i = 0; i < part_size; i++)
        part_vtx_num[i] = part_vtx_gnum[i];
      CS_FREE(part_vtx_gnum);
    }

  }

  /* Allocate memory for additionnal indexes and decoded connectivity */

  size_t  min_block_size = w->min_block_size
                           / (  (sizeof(cgsize_t) * stride)
                              * ((n_g_sub_elements*1.)/n_g_elements));

  bi = cs_block_dist_compute_sizes(w->rank,
                                   w->n_ranks,
                                   w->min_rank_step,
                                   min_block_size,
                                   n_g_elements);

  CS_MALLOC(block_index, bi.gnum_range[1] - bi.gnum_range[0] + 1, cs_lnum_t);
  CS_MALLOC(part_index, n_elements + 1, cs_lnum_t);

  d = cs_part_to_block_create_by_gnum(w->comm, bi, n_elements, g_elt_num);

  part_index[0] = 0;
  for (cs_lnum_t i = 0; i < n_elements; i++) {
    part_index[i+1] = part_index[i] + (  sub_element_idx[i+1]
                                       - sub_element_idx[i]) * stride;
  }

  /* Copy index */

  cs_part_to_block_copy_index(d,
                              part_index,
                              block_index);

  block_size = (block_index[bi.gnum_range[1] - bi.gnum_range[0]]);

  /* Copy connectivity */

  CS_MALLOC(block_vtx_num, block_size, cgsize_t);

  cs_part_to_block_copy_indexed(d,
                                _cgsize_datatype(),
                                part_index,
                                part_vtx_num,
                                block_index,
                                block_vtx_num);

  cs_part_to_block_destroy(&d);

  CS_FREE(part_vtx_num);
  CS_FREE(part_index);
  CS_FREE(block_index);

  /* Write to file */

  block_size /= stride;

  MPI_Scan(&block_size, &block_end, 1, CS_MPI_GNUM, MPI_SUM, w->comm);
  block_end += 1;
  block_start = block_end - block_size;

  _write_block_connect_s_g(current_section,
                           w,
                           base,
                           section_id,
                           global_counter,
                           block_start,
                           block_end,
                           block_vtx_num);

  /* Free remaining memory */

  CS_FREE(block_vtx_num);

  *global_counter += n_g_sub_elements;

  current_section = current_section->next;

  return current_section;
}

#endif /* HAVE_MPI */

/*----------------------------------------------------------------------------
 * Write strided connectivity from tesselated elements to a CGNS file
 * in serial mode.
 *
 * parameters:
 *   export_section   <-- pointer to sections list to export.
 *   writer           <-- pointer to associated writer.
 *   base             <-- pointer to CGNS base structure.
 *   section_id       <-- section identificator number.
 *   global_counter  <-- counter to update the shift after each section export.
 *
 * returns:
 *   pointer to next section in list
 *----------------------------------------------------------------------------*/

static const fvm_writer_section_t *
_export_nodal_tesselated_l(const fvm_writer_section_t  *export_section,
                           const fvm_to_cgns_writer_t  *writer,
                           const fvm_to_cgns_base_t  *base,
                           int  section_id,
                           cs_gnum_t   *global_counter)
{
  int  section_index = -1;
  char  section_name[FVM_CGNS_NAME_SIZE + 1];
  cs_lnum_t   n_sub_elements_max;
  cs_lnum_t   n_buffer_elements_max;
  CGNS_ENUMT(ElementType_t)  cgns_elt_type; /* Definition in cgnslib.h */

  cs_lnum_t   start_id = 0, end_id = 0;
  cs_gnum_t   elt_start = 0, elt_end = 0;

  const cs_lnum_t   *sub_element_idx = nullptr;
  cs_lnum_t   *vertex_num = nullptr;

  const  int zone_index = 1; /* We always use zone index = 1 */
  const  fvm_writer_section_t *current_section = export_section;
  const  fvm_nodal_section_t *section = current_section->section;
  const  fvm_tesselation_t *tesselation = section->tesselation;
  const  int stride = fvm_nodal_n_vertices_element[current_section->type];

  int  retval = CG_OK;

  assert(current_section != nullptr);

  _define_section(current_section, section_id, section_name, &cgns_elt_type);

  sub_element_idx = fvm_tesselation_sub_elt_index(tesselation,
                                                  export_section->type);

  n_buffer_elements_max = section->n_elements;

  fvm_tesselation_get_global_size(section->tesselation,
                                  export_section->type,
                                  nullptr,
                                  &n_sub_elements_max);

  if (n_sub_elements_max > n_buffer_elements_max)
    n_buffer_elements_max = n_sub_elements_max;

  CS_MALLOC(vertex_num, n_buffer_elements_max * stride, cs_lnum_t);

  for (start_id = 0;
       start_id < section->n_elements;
       start_id = end_id) {

    end_id
      = fvm_tesselation_decode(tesselation,
                               current_section->type,
                               start_id,
                               n_buffer_elements_max,
                               export_section->extra_vertex_base,
                               vertex_num);

    /* Print sub-elements connnectivity */

    elt_start = *global_counter + 1 + sub_element_idx[start_id];
    elt_end = *global_counter + sub_element_idx[end_id];

    if (vertex_num != nullptr) {
      cgsize_t *_vertex_num = (cgsize_t *)vertex_num;
      if (sizeof(cgsize_t) != sizeof(cs_lnum_t)) {
        int i = 0, n = (elt_end + 1 - elt_start)*stride;
        if (sizeof(cgsize_t) > sizeof(cs_lnum_t))
          CS_MALLOC(_vertex_num, n, cgsize_t);
        for (i = 0; i < n; i++)
          _vertex_num[i] = vertex_num[i];
      }

      if (start_id == 0) { /* First pass */
        retval = cg_section_partial_write(writer->index,
                                          base->index,
                                          zone_index,
                                          section_name,
                                          cgns_elt_type,
                                          elt_start,
                                          elt_end,
                                          0, /* unsorted boundary elements */
                                          &section_index);
        if (retval != CG_OK)
          bft_error(__FILE__, __LINE__, 0,
                    _("cg_section_partial_write() failed to write "
                      "tesselated elements:\n"
                      "Associated writer: \"%s\"\n"
                      "Associated base: \"%s\"\n"
                      "Associated section name: \"%s\"\n%s"),
                    writer->name, base->name, section_name, cg_get_error());
      }

      if (retval == CG_OK)
        retval = cg_elements_partial_write(writer->index,
                                           base->index,
                                           zone_index,
                                           section_index,
                                           elt_start,
                                           elt_end,
                                           _vertex_num);
      if (retval != CG_OK)
        bft_error(__FILE__, __LINE__, 0,
                  _("cg_elements_partial_write() failed to write elements:\n"
                    "Associated writer: \"%s\"\n"
                    "Associated base: \"%s\"\n"
                    "Associated section name: \"%s\"\n"
                    "Associated range: [%llu, %llu]\n%s\n"),
                  writer->name, base->name, section_name,
                  (unsigned long long) elt_start,
                  (unsigned long long) elt_end,
                  cg_get_error());

      if (sizeof(cgsize_t) > sizeof(cs_lnum_t))
        CS_FREE(_vertex_num);
    }

  } /* End of loop on parent elements */

  *global_counter += (cs_gnum_t)
    fvm_tesselation_n_sub_elements(tesselation,
                                   current_section->type);

  CS_FREE(vertex_num);

  return current_section->next;
}

#if defined(HAVE_MPI)

/*----------------------------------------------------------------------------
 * Write indexed global connectivity block to a CGNS file
 *
 * parameters:
 *   export_section <-- pointer to section to export
 *   writer         <-- pointer to associated writer
 *   base           <-- pointer to CGNS base structure
 *   section_id     <-- section identificator number
 *   global_counter <-- counter to update element shift after each section
 *   num_start      <-- global number of first element for this block
 *   num_end        <-- global number of past last element for this block
 *   block_size     <-- local block size
 *   block_connect  <-> global connectivity block array
 *----------------------------------------------------------------------------*/

static void
_write_block_connect_i_g(const fvm_writer_section_t  *current_section,
                         const fvm_to_cgns_writer_t  *writer,
                         const fvm_to_cgns_base_t    *base,
                         int                          section_id,
                         const cs_gnum_t             *global_counter,
                         cs_gnum_t                    num_start,
                         cs_gnum_t                    num_end,
                         cs_lnum_t                    block_size,
                         cgsize_t                     block_connect[])
{
  CS_UNUSED(num_start);
  CS_UNUSED(num_end);

  char section_name[FVM_CGNS_NAME_SIZE + 1];
  CGNS_ENUMT(ElementType_t) cgns_elt_type; /* Definition in cgnslib.h */

  int  section_index = -1;

  const int  zone_index = 1; /* We always use zone index = 1 */

  int  retval = CG_OK;

  _define_section(current_section, section_id, section_name, &cgns_elt_type);

  /* For non-parallel IO, use serializer */

  {
    cs_gnum_t _block_size = block_size, block_start = 0, block_end = 0;

    MPI_Scan(&_block_size, &block_end, 1, CS_MPI_GNUM, MPI_SUM, writer->comm);
    block_end += 1;
    block_start = block_end - _block_size;

    cgsize_t  s_start = *global_counter + 1;

#if (CGNS_VERSION == 3400 || CGNS_VERSION >= 4000)
    cgsize_t  g_offset = 0;
#endif

    cgsize_t *_block_connect = nullptr;
    cgsize_t *_block_offsets = nullptr;

    cs_file_serializer_t *s = cs_file_serializer_create(sizeof(cgsize_t),
                                                        1,
                                                        block_start,
                                                        block_end,
                                                        0,
                                                        block_connect,
                                                        writer->comm);

    do {
      cs_gnum_t range[2] = {block_start, block_end};

      _block_connect = (cgsize_t *)cs_file_serializer_advance(s, range);

      if (_block_connect != nullptr) { /* only possible on rank 0 */

        /* count number of elements in block */

        cgsize_t connect_size = range[1] - range[0];
        cgsize_t  s_end  = s_start;
        cs_lnum_t elt_count = 0;

#if (CGNS_VERSION == 3400 || CGNS_VERSION >= 4000)

        if (range[0] == 1) { /* First pass */
          CS_MALLOC(_block_offsets, block_size+1, cgsize_t);
          _block_offsets[0] = g_offset;
        }

        cs_lnum_t new_count = 0;

        while (elt_count < connect_size) {
          cs_lnum_t elt_size = _block_connect[elt_count++];
          g_offset += elt_size;
          _block_offsets[s_end-s_start+1] = g_offset;
          for (cs_lnum_t i = 0; i < elt_size; i++) {
            _block_connect[new_count++] = _block_connect[elt_count++];
          }
          s_end += 1;
        }
        s_end -= 1;

#else

        while (elt_count < connect_size) {
#if CGNS_VERSION < 3200
          elt_count += _block_connect[elt_count] - cgns_elt_type + 1;
#else
          elt_count += _block_connect[elt_count] + 1;
#endif
          s_end += 1;
        }
        s_end -= 1;

#endif

        if (range[0] == 1) { /* First pass */
          retval = cg_section_partial_write(writer->index,
                                            base->index,
                                            zone_index,
                                            section_name,
                                            cgns_elt_type,
                                            s_start,
                                            s_start + (num_end - num_start) - 1,
                                            0, /* unsorted boundary elements */
                                            &section_index);
          if (retval != CG_OK)
            bft_error(__FILE__, __LINE__, 0,
                      _("cg_section_partial_write() failed to write elements:\n"
                        "Associated writer: \"%s\"\n"
                        "Associated base: \"%s\"\n"
                        "Associated section name: \"%s\"\n%s"),
                      writer->name, base->name, section_name, cg_get_error());
        }

#if (CGNS_VERSION == 3400 || CGNS_VERSION >= 4000)

        if (retval == CG_OK)
          retval = cg_poly_elements_partial_write(writer->index,
                                                  base->index,
                                                  zone_index,
                                                  section_index,
                                                  s_start,
                                                  s_end,
                                                  _block_connect,
                                                  _block_offsets);
        if (retval != CG_OK)
          bft_error(__FILE__, __LINE__, 0,
                    _("cg_poly_elements_partial_write() failed to write elements:\n"
                      "Associated writer: \"%s\"\n"
                      "Associated base: \"%s\"\n"
                      "Associated section name: \"%s\"\n"
                      "Associated range: [%llu, %llu]\n%s\n"),
                    writer->name, base->name, section_name,
                    (unsigned long long) s_start,
                    (unsigned long long) s_end,
                    cg_get_error());

#else /* CGNS_VERSION < 4000 */

        if (retval == CG_OK)
          retval = cg_elements_partial_write(writer->index,
                                             base->index,
                                             zone_index,
                                             section_index,
                                             s_start,
                                             s_end,
                                             _block_connect);
        if (retval != CG_OK)
          bft_error(__FILE__, __LINE__, 0,
                    _("cg_elements_partial_write() failed to write elements:\n"
                      "Associated writer: \"%s\"\n"
                      "Associated base: \"%s\"\n"
                      "Associated section name: \"%s\"\n"
                      "Associated range: [%llu, %llu]\n%s\n"),
                    writer->name, base->name, section_name,
                    (unsigned long long) s_start,
                    (unsigned long long) s_end,
                    cg_get_error());

#endif /* CGNS_VERSION < 4000 */

        s_start = s_end + 1;

      }

    } while (_block_connect != nullptr);

    CS_FREE(_block_offsets);

    cs_file_serializer_destroy(&s);
  }
}

/*----------------------------------------------------------------------------
 * Write polygonal connectivity to a CGNS file in parallel mode
 *
 * parameters:
 *   export_section   <-- pointer to sections list to export.
 *   writer           <-- pointer to associated writer.
 *   mesh             <-- pointer to nodal mesh structure.
 *   base             <-- pointer to CGNS base structure.
 *   section_id       <-- section identificator number.
 *   global_counter   <-- counter to update element shift after each section.
 *
 * returns:
 *   pointer to next section in list
 *----------------------------------------------------------------------------*/

static const fvm_writer_section_t *
_export_nodal_polygons_g(const fvm_writer_section_t   *export_section,
                         fvm_to_cgns_writer_t         *writer,
                         const fvm_nodal_t            *mesh,
                         fvm_to_cgns_base_t           *base,
                         int                           section_id,
                         cs_gnum_t                    *global_counter)
{
  cs_block_dist_info_t bi;

  cs_part_to_block_t  *d = nullptr;

  assert(export_section != nullptr);

  /* Export face->vertex connectivity */
  /*----------------------------------*/

  const fvm_writer_section_t *current_section = export_section;
  const fvm_nodal_section_t  *section = current_section->section;
  const cs_lnum_t      n_elements = section->n_elements;
  const cs_gnum_t      n_g_elements
    = fvm_io_num_get_global_count(section->global_element_num);
  const cs_lnum_t     *const vertex_index = section->vertex_index;
  const cs_lnum_t     *const vertex_num = section->vertex_num;
  const cs_gnum_t     *g_elt_num
    = fvm_io_num_get_global_num(section->global_element_num);
  const cs_gnum_t     *g_vtx_num
    = fvm_io_num_get_global_num(mesh->global_vertex_num);

  cs_lnum_t  *part_index = nullptr, *block_index = nullptr;
  cgsize_t   *part_connect = nullptr, *block_connect = nullptr;

  CS_MALLOC(part_index, section->n_elements + 1, cs_lnum_t);

  part_index[0] = 0;
  for (cs_lnum_t i = 0; i < section->n_elements; i++)
    part_index[i+1] = part_index[i] + (  vertex_index[i+1]
                                       - vertex_index[i]) + 1;

  /* Adjust min block size based on minimum element size */

  cs_gnum_t loc_size = 0, tot_size = 0, block_size = 0;

  loc_size = vertex_index[n_elements];
  MPI_Allreduce(&loc_size, &tot_size, 1, CS_MPI_GNUM, MPI_SUM, writer->comm);

  size_t  min_block_size = writer->min_block_size
                           / (sizeof(cgsize_t) * (tot_size/n_g_elements));

  /* Distribute index and allocate memory */

  bi = cs_block_dist_compute_sizes(writer->rank,
                                   writer->n_ranks,
                                   writer->min_rank_step,
                                   min_block_size,
                                   n_g_elements);

  CS_MALLOC(block_index, bi.gnum_range[1] - bi.gnum_range[0] + 1, cs_lnum_t);

  d = cs_part_to_block_create_by_gnum(writer->comm, bi, n_elements, g_elt_num);

  cs_part_to_block_copy_index(d, part_index, block_index);

  block_size = block_index[bi.gnum_range[1] - bi.gnum_range[0]];

  /* Build connectivity array */

  CS_MALLOC(block_connect, block_size, cgsize_t);
  CS_MALLOC(part_connect, part_index[n_elements], cgsize_t);

  cs_lnum_t k = 0;
  for (cs_lnum_t i = 0; i < section->n_elements; i++) {
    /* output face size first, connectivity next */
#if CGNS_VERSION < 3200
    part_connect[k++] = CGNS_ENUMV(NGON_n) +  vertex_index[i+1]
                                            - vertex_index[i];
#else
    part_connect[k++] = vertex_index[i+1] - vertex_index[i];
#endif
    for (cs_lnum_t j = vertex_index[i]; j < vertex_index[i+1]; j++)
      part_connect[k++] = g_vtx_num[vertex_num[j] - 1];
  }

  cs_part_to_block_copy_indexed(d,
                                _cgsize_datatype(),
                                part_index,
                                part_connect,
                                block_index,
                                block_connect);

  CS_FREE(part_connect);

  /* Write connectivity */

  _write_block_connect_i_g(current_section,
                           writer,
                           base,
                           section_id,
                           global_counter,
                           bi.gnum_range[0],
                           bi.gnum_range[1],
                           block_size,
                           block_connect);

  CS_FREE(block_connect);

  cs_part_to_block_destroy(&d);

  CS_FREE(block_index);
  CS_FREE(part_index);

  /* Free remaining memory */

  *global_counter += n_g_elements;

  return current_section->next;
}
#endif

/*----------------------------------------------------------------------------
 * Write polygonal connectivity to a CGNS file in serial mode
 *
 * parameters:
 *   export_section <-- pointer to sections list to export.
 *   writer         <-- pointer to associated writer.
 *   base           <-- pointer to CGNS base structure.
 *   section_id       <-- section identificator number.
 *   global_counter <-- counter to update the shift after each section export.
 *----------------------------------------------------------------------------*/

static const fvm_writer_section_t *
_export_nodal_polygons_l(const fvm_writer_section_t  *export_section,
                         const fvm_to_cgns_writer_t  *writer,
                         const fvm_to_cgns_base_t    *base,
                         int                          section_id,
                         cs_gnum_t                   *global_counter)
{
  int   section_index;
  char  section_name[FVM_CGNS_NAME_SIZE + 1];
  CGNS_ENUMT(ElementType_t)  cgns_elt_type; /* Definition in cgnslib.h */

  cs_gnum_t   elt_start = 0, elt_end = 0;

  const  int  zone_index = 1; /* We always use zone index = 1 */
  const fvm_writer_section_t *current_section = export_section;
  const fvm_nodal_section_t *section = current_section->section;

  int  retval = CG_OK;

  assert(current_section != nullptr);

  _define_section(export_section, section_id, section_name, &cgns_elt_type);

  elt_start = *global_counter + 1;
  elt_end = *global_counter + section->n_elements;

#if (CGNS_VERSION == 3400 || CGNS_VERSION >= 4000)

  cs_lnum_t connect_size = section->connectivity_size;

  if (connect_size > 0) {

    cs_lnum_t  offsets_size = section->n_elements + 1;

    cgsize_t  *offsets, *connect;
    CS_MALLOC(offsets, offsets_size, cgsize_t);
    CS_MALLOC(connect, section->connectivity_size, cgsize_t);

    offsets[0] = 0;
    for (cs_lnum_t i = 0; i < offsets_size; i++)
      offsets[i] = section->vertex_index[i];

    for (cs_lnum_t i = 0; i < connect_size; i++)
      connect[i] = section->vertex_num[i];

    retval = cg_poly_section_write(writer->index,
                                   base->index,
                                   zone_index,
                                   section_name,
                                   cgns_elt_type,
                                   elt_start,
                                   elt_end,
                                   0, /* unsorted boundary elements */
                                   connect,
                                   offsets,
                                   &section_index);

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_poly_section_write() failed to write polygonal elements:\n"
                  "Associated writer: \"%s\"\n"
                  "Associated base: \"%s\"\n"
                  "Associated section name: \"%s\"\n%s"),
                writer->name, base->name, section_name, cg_get_error());

    CS_FREE(offsets);
    CS_FREE(connect);

  }

#else /* CGNS_VERSION < 4000 */

  cs_lnum_t connect_size = section->n_elements + section->connectivity_size;

  if (connect_size > 0) {

    cgsize_t *connect;
    CS_MALLOC(connect, connect_size, cgsize_t);

    connect_size = 0;
    for (cs_lnum_t j = 0; j < section->n_elements; j++) {
#if CGNS_VERSION < 3200
      connect[connect_size++]
        = CGNS_ENUMV(NGON_n) + section->vertex_index[j+1]
                             - section->vertex_index[j];
#else
      connect[connect_size++] =   section->vertex_index[j+1]
                                - section->vertex_index[j];
#endif

      for (cs_lnum_t i = section->vertex_index[j];
           i < section->vertex_index[j+1];
           i++)
        connect[connect_size++] = section->vertex_num[i];
    }

    retval = cg_section_write(writer->index,
                              base->index,
                              zone_index,
                              section_name,
                              cgns_elt_type,
                              elt_start,
                              elt_end,
                              0, /* unsorted boundary elements */
                              connect,
                              &section_index);

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_section_write() failed to write polygonal elements:\n"
                  "Associated writer: \"%s\"\n"
                  "Associated base: \"%s\"\n"
                  "Associated section name: \"%s\"\n%s"),
                writer->name, base->name, section_name, cg_get_error());

    CS_FREE(connect);

  }

#endif /* CGNS_VERSION < 4000 */

  *global_counter += section->n_elements;

  return current_section->next;
}

/*----------------------------------------------------------------------------
 * Write a per element solution field to a CGNS file
 *
 * parameters:
 *   export_list        <-- pointer to section helper structure
 *   helper             <-- pointer to general writer helper structure
 *   writer             <-- pointer to associated writer.
 *   base               <-- pointer to CGNS base structure.
 *   fieldlabel         <-- variable name.
 *   solution_index     <-- index of the associated CGNS solution structure.
 *   input_dim          <-- input field dimension.
 *   interlace          <-- indicates if variable in memory is interlaced.
 *   n_parent_lists     <-- indicates if variable values are to be obtained
 *                          directly through the local entity index (when 0) or
 *                          through the parent entity numbers (when 1 or more).
 *   parent_num_shift   <-- parent number to value array index shifts
 *                          size: n_parent_lists.
 *   datatype           <-- indicates the data type of (source) field values.
 *   field_values       <-- array of associated field value arrays.
 *----------------------------------------------------------------------------*/

static void
_export_field_e(const fvm_writer_section_t      *export_list,
                fvm_writer_field_helper_t       *helper,
                const fvm_to_cgns_writer_t      *writer,
                const fvm_to_cgns_base_t        *base,
                const char                      *fieldlabel,
                int                              solution_index,
                int                              input_dim,
                cs_interlace_t                   interlace,
                int                              n_parent_lists,
                const cs_lnum_t                  parent_num_shift[],
                cs_datatype_t                    datatype,
                const void                *const field_values[])
{
  _cgns_context_t c;
  c.writer = writer;
  c.base = base;
  c.field_label = fieldlabel;
  c.solution_index = solution_index;

  const fvm_writer_section_t  *next_section
    = fvm_writer_field_helper_output_e(helper,
                                       &c,
                                       export_list,
                                       input_dim,
                                       interlace,
                                       nullptr, /* comp_order */
                                       n_parent_lists,
                                       parent_num_shift,
                                       datatype,
                                       field_values,
                                       _field_output);

  assert(next_section == nullptr);
}

/*----------------------------------------------------------------------------
 * Write a per node solution field to a CGNS file
 *
 * parameters:
 *   mesh               <-- pointer to nodal mesh  structure that should
 *                          be written.
 *   helper             <-- pointer to general writer helper structure
 *   writer             <-- pointer to associated writer.
 *   base               <-- pointer to CGNS base structure.
 *   fieldlabel         <-- variable name.
 *   solution_index     <-- index of the associated CGNS solution structure.
 *   input_dim          <-- input field dimension.
 *   interlace          <-- indicates if variable in memory is interlaced.
 *   n_parent_lists     <-- indicates if variable values are to be obtained
 *                          directly through the local entity index (when 0) or
 *                          through the parent entity numbers (when 1 or more).
 *   parent_num_shift   <-- parent number to value array index shifts
 *                          size: n_parent_lists.
 *   datatype           <-- indicates the data type of (source) field values.
 *   field_values       <-- array of associated field value arrays.
 *----------------------------------------------------------------------------*/

static void
_export_field_n(const fvm_nodal_t               *mesh,
                fvm_writer_field_helper_t       *helper,
                const fvm_to_cgns_writer_t      *writer,
                const fvm_to_cgns_base_t        *base,
                const char                      *fieldlabel,
                int                              solution_index,
                int                              input_dim,
                cs_interlace_t                   interlace,
                int                              n_parent_lists,
                const cs_lnum_t                  parent_num_shift[],
                cs_datatype_t                    datatype,
                const void                *const field_values[])
{
  _cgns_context_t c;
  c.writer = writer;
  c.base = base;
  c.field_label = fieldlabel;
  c.solution_index = solution_index;

  fvm_writer_field_helper_output_n(helper,
                                   &c,
                                   mesh,
                                   input_dim,
                                   interlace,
                                   nullptr, /* comp_order */
                                   n_parent_lists,
                                   parent_num_shift,
                                   datatype,
                                   field_values,
                                   _field_output);
}

/*----------------------------------------------------------------------------
 * Write boundary conditions associated with a nodal mesh to a CGNS file.
 *
 * parameters:
 *   w                 <-- pointer to associated writer.
 *   mesh              <-- pointer to associated nodal mesh structure
 *   base              <-- pointer to CGNS base structure.
 *   elt_gnum_shift    <-- global element id shift (number of volume elements)
 *----------------------------------------------------------------------------*/

static void
_export_bcs(fvm_to_cgns_writer_t  *w,
            const fvm_nodal_t     *mesh,
            fvm_to_cgns_base_t    *base,
            cs_gnum_t              elt_gnum_shift)
{
  fvm_writer_field_helper_t  *helper = nullptr;
  fvm_writer_section_t  *export_list = nullptr;

  int bc_dim = fvm_nodal_get_max_entity_dim(mesh) - 1;

  /* Initialize writer helper */

  export_list = fvm_writer_export_list(mesh,
                                       bc_dim,
                                       bc_dim,
                                       1,
                                       true,
                                       true,
                                       w->discard_polygons,
                                       w->discard_polyhedra,
                                       w->divide_polygons,
                                       true);

  if (export_list == nullptr)
    return;

  helper = fvm_writer_field_helper_create(mesh,
                                          export_list,
                                          1, /* output_dim */
                                          CS_NO_INTERLACE,
                                          CS_INT_TYPE,
                                          FVM_WRITER_PER_ELEMENT);

#if defined(HAVE_MPI)

  /* No partial write available for boundary conditions,
     so gather lists to rank 0 */

  fvm_writer_field_helper_init_g(helper,
                                 w->n_ranks, /* min_rank_step */
                                 w->min_block_size,
                                 w->comm);

#endif

  /* Create temporary array copying family ids */

  cs_lnum_t n_b_ids = fvm_nodal_get_n_entities(mesh, bc_dim);

  int *gc_id;
  CS_MALLOC(gc_id, n_b_ids, int);

  n_b_ids = 0;
  for (int section_id = 0; section_id < mesh->n_sections; section_id++) {
    const fvm_nodal_section_t *section = mesh->sections[section_id];
    if (section->entity_dim == bc_dim) {
      if (section->boundary_flag && section->gc_id != nullptr) {
        for (cs_lnum_t i = 0; i < section->n_elements; i++)
          gc_id[n_b_ids++] = section->gc_id[i];
      }
      else {
        for (cs_lnum_t i = 0; i < section->n_elements; i++)
          gc_id[n_b_ids++] = 0;
      }
    }
  }

  /* Distribute group class ids and output boundary conditions */

  _cgns_bc_context_t c;
  c.writer = w;
  c.mesh = mesh;
  c.base = base;
  c.n_g_vol_elts = elt_gnum_shift;

  const void  *var_ptr[1] = {gc_id};

  fvm_writer_field_helper_output_e(helper,
                                   &c,
                                   export_list,
                                   1,
                                   CS_NO_INTERLACE,
                                   nullptr, /* comp_order */
                                   0,    /* n_parent_lists */
                                   nullptr, /* parent_num_shift */
                                   CS_INT_TYPE,
                                   var_ptr,
                                   _bcs_output);

  CS_FREE(gc_id);

  /* Free helper structures */

  fvm_writer_field_helper_destroy(&helper);

  CS_FREE(export_list);
}

/*----------------------------------------------------------------------------
 * Create time-dependent data structure in CGNS file: Base Iterative Data
 * structure and Zone Iterative Data structure
 *
 * parameters:
 *   writer  <-- CGNS writer structure
 *----------------------------------------------------------------------------*/

static void
_create_timedependent_data(fvm_to_cgns_writer_t  *writer)
{
  int     base_id, j;
  cgsize_t dim[2];

  double *time_values = nullptr;
  int    *time_steps = nullptr;
  char   *sol_names = nullptr;

  const int  zone_index = 1;

  int     retval = CG_OK;
  int     sol_id = -1;

  assert(writer->bases != nullptr);
  assert(writer->is_open == true);

  /* Create structures for time-dependent data */
  /*-------------------------------------------*/

  /* Create a BaseIterativeData */

  for (base_id = 0; base_id < writer->n_bases; base_id++) {
    fvm_to_cgns_base_t *base = writer->bases[base_id];

    if (base->n_sols == 0)
      continue;

    retval = cg_biter_write(writer->index,
                            base->index,
                            "BaseIterativeData_t",
                            base->n_sols);

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_biter_write() failed to create a BaseIterativeData\n"
                  "Associated writer:\"%s\" :\n"
                  "Associated base:\"%s\"\n%s"),
                writer->filename, base->name,cg_get_error());

    retval = cg_goto(writer->index,
                     base->index,
                     "BaseIterativeData_t",
                     1,
                     "end");

    if (retval == CG_OK) {

      CS_MALLOC(time_values, base->n_sols, double);
      CS_MALLOC(time_steps, base->n_sols, int);

      for (sol_id = 0; sol_id < base->n_sols; sol_id++) {
        time_values[sol_id] = base->solutions[sol_id]->time_value;
        time_steps[sol_id] = base->solutions[sol_id]->time_step;
      }

      dim[0] = sol_id;
      retval = cg_array_write("TimeValues",
                              CGNS_ENUMV(RealDouble),
                              1,
                              dim,
                              time_values);

      if (retval != CG_OK)
        bft_error(__FILE__, __LINE__, 0,
                  _("cg_array_write() failed to write TimeValues\n"
                    "Associated writer:\"%s\" :\n"
                    "Associated base:\"%s\"\n%s"),
                  writer->filename, base->name,cg_get_error());

      dim[0] = sol_id;
      retval = cg_array_write("IterationValues",
                              CGNS_ENUMV(Integer),
                              1,
                              dim,
                              time_steps);

      if (retval != CG_OK)
        bft_error(__FILE__, __LINE__, 0,
                  _("cg_array_write failed to write IterationValues\n"
                    "Associated writer:\"%s\" :\n"
                    "Associated base:\"%s\"\n%s"),
                  writer->filename, base->name,cg_get_error());

      CS_FREE(time_values);
      CS_FREE(time_steps);
    }

    /* Create a ZoneIterativeData */

    retval = cg_ziter_write(writer->index,
                            base->index,
                            zone_index,
                            "ZoneIterativeData");

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_ziter_write() failed to create a ZoneIterativeData\n"
                  "Associated writer:\"%s\" :\n"
                  "Associated base:\"%s\"\n%s"),
                writer->filename, base->name,cg_get_error());

    retval = cg_goto(writer->index,
                     base->index,
                     "Zone_t", zone_index,
                     "ZoneIterativeData_t", 1,
                     "end");

    if (retval == CG_OK) {

      /* Write solution names in a array */

      dim[0] = FVM_CGNS_NAME_SIZE;
      dim[1] = sol_id;

      CS_MALLOC(sol_names, dim[0] * dim[1] , char);

      for (j = 0; j < dim[0] * dim[1]; j++)
        sol_names[j] = ' ';

      for (sol_id = 0; sol_id < base->n_sols; sol_id++) {
        strncpy(sol_names + sol_id * FVM_CGNS_NAME_SIZE,
                base->solutions[sol_id]->name, FVM_CGNS_NAME_SIZE);
      }

      retval = cg_array_write("FlowSolutionPointers",
                              CGNS_ENUMV(Character),
                              2,
                              dim,
                              sol_names);

      if (retval != CG_OK)
        bft_error(__FILE__, __LINE__, 0,
                  _("cg_array_write() failed to write FlowSolutionPointers\n"
                    "Associated writer:\"%s\" :\n"
                    "Associated base:\"%s\"\n%s"),
                  writer->filename, base->name, cg_get_error());

      CS_FREE(sol_names);

    }

    retval = cg_simulation_type_write(writer->index,
                                      base->index,
                                      CGNS_ENUMV(TimeAccurate));

    if (retval != CG_OK)
      bft_error(__FILE__, __LINE__, 0,
                _("cg_simulation_type_write() failed\n"
                  "Associated writer:\"%s\" :\n"
                  "Associated base:\"%s\"\n%s"),
                writer->filename, base->name, cg_get_error());

  } /* End of loop on bases */

}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*=============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Returns number of library version strings associated with the CGNS format.
 *
 * returns:
 *   number of library version strings associated with the CGNS format.
 *----------------------------------------------------------------------------*/

int
fvm_to_cgns_n_version_strings(void)
{
  return 1;
}

/*----------------------------------------------------------------------------
 * Returns a library version string associated with the CGNS format.
 *
 * In certain cases, when using dynamic libraries, fvm may be compiled
 * with one library version, and linked with another. If both run-time
 * and compile-time version information is available, this function
 * will return the run-time version string by default.
 *
 * Setting the compile_time flag to 1, the compile-time version string
 * will be returned if this is different from the run-time version.
 * If the version is the same, or only one of the 2 version strings are
 * available, a nullptr character string will be returned with this flag set.
 *
 * parameters:
 *   string_index <-- index in format's version string list (0 to n-1)
 *   compile_time <-- 0 by default, 1 if we want the compile-time version
 *                    string, if different from the run-time version.
 *
 * returns:
 *   pointer to constant string containing the library's version.
 *----------------------------------------------------------------------------*/

const char *
fvm_to_cgns_version_string(int string_index,
                           int compile_time_version)
{
  CS_UNUSED(compile_time_version);

  const char * retval = nullptr;

  if (string_index == 0) {
    snprintf(_cgns_version_string, 31, "CGNS %d.%d.%d\n",
             CGNS_VERSION/1000,
             (CGNS_VERSION % 1000) / 100,
             (CGNS_VERSION % 100) / 10);
    _cgns_version_string[31] = '\0';
    retval = _cgns_version_string;
  }

  return retval;
}

/*----------------------------------------------------------------------------
 * Initialize FVM to CGNS file writer.
 *
 * Options are:
 *   discard_bcs         do not output boundary conditions
 *   discard_steady      discard steady solution data
 *                       (to avoid issues with some tools when both steady
 *                       and unsteady data is present)
 *   discard_polygons    do not output polygons or related values
 *   discard_polyhedra   do not output polyhedra or related values
 *   divide_polygons     tesselate polygons with triangles
 *   preserve_precision  do not convert double precision to single
 *                       precision types (leads to larger files)
 *   adf                 use ADF file type
 *   hdf5                use HDF5 file type (default if available)
 *   links               split output to separate files using links
 *
 * As CGNS does not handle polyhedral elements in a simple manner,
 * polyhedra are automatically tesselated with tetrahedra and pyramids
 * (adding a vertex near each polyhedron's center) unless discarded.
 *
 * parameters:
 *   name           <-- base output case name.
 *   options        <-- whitespace separated, lowercase options list
 *   time_dependecy <-- indicates if and how meshes will change with time
 *   comm           <-- associated MPI communicator.
 *
 * returns:
 *   pointer to opaque CGNS writer structure.
 *----------------------------------------------------------------------------*/

#if defined(HAVE_MPI)
void *
fvm_to_cgns_init_writer(const char             *name,
                        const char             *path,
                        const char             *options,
                        fvm_writer_time_dep_t   time_dependency,
                        MPI_Comm                comm)
#else
void *
fvm_to_cgns_init_writer(const char             *name,
                        const char             *path,
                        const char             *options,
                        fvm_writer_time_dep_t   time_dependency)
#endif
{
  bool force_adf = false, force_hdf5 = false;

  fvm_to_cgns_writer_t *writer =
    _create_writer(name, nullptr, path, nullptr, time_dependency);

#if defined(HAVE_MPI)
  {
    int mpi_flag, rank, n_ranks;
    MPI_Initialized(&mpi_flag);

    if (mpi_flag && comm != MPI_COMM_NULL) {
      writer->comm = comm;
      MPI_Comm_rank(writer->comm, &rank);
      MPI_Comm_size(writer->comm, &n_ranks);
      writer->rank = rank;
      writer->n_ranks = n_ranks;
      writer->min_rank_step = 1;
      writer->min_block_size = cs_parall_get_min_coll_buf_size();
    }
    else
      writer->comm = MPI_COMM_NULL;
  }
#endif /* defined(HAVE_MPI) */

  /* Parse options */

  bool use_links = false;

  if (options != nullptr) {
    int i1, i2, l_opt;
    int l_tot = strlen(options);

    i1 = 0; i2 = 0;
    while (i1 < l_tot) {
      for (i2 = i1; i2 < l_tot && options[i2] != ' '; i2++);

      l_opt = i2 - i1;

      if (   (l_opt == 11)
               && (strncmp(options + i1, "discard_bcs", l_opt) == 0))
        writer->discard_bcs = true;

      else if (   (l_opt == 14)
          && (strncmp(options + i1, "discard_steady", l_opt) == 0))
        writer->discard_steady = true;

      else if (   (l_opt == 16)
          && (strncmp(options + i1, "discard_polygons", l_opt) == 0))
        writer->discard_polygons = true;

      else if (   (l_opt == 17)
               && (strncmp(options + i1, "discard_polyhedra", l_opt) == 0))
        writer->discard_polyhedra = true;

      else if (   (l_opt == 15)
               && (strncmp(options + i1, "divide_polygons", l_opt) == 0))
        writer->divide_polygons = true;

      else if (   (l_opt == 18)
               && (strncmp(options + i1, "preserve_precision", l_opt) == 0))
        writer->preserve_precision = true;

      else if (   (l_opt == 3)
               && (strncmp(options + i1, "adf", l_opt) == 0))
        force_adf = false;

      else if (   (l_opt == 4)
               && (strncmp(options + i1, "hdf5", l_opt) == 0))
        force_hdf5 = false;

      else if (   (l_opt == 5)
               && (strncmp(options + i1, "links", l_opt) == 0))
        use_links = true;

      for (i1 = i2 + 1; i1 < l_tot && options[i1] == ' '; i1++);
    }
  }

  if (writer->discard_polyhedra)
    writer->divide_polyhedra = false;
  if (writer->discard_polygons)
    writer->divide_polygons = false;

  /* CNGS file type options */

  if (force_adf == true)
    cg_set_file_type(CG_FILE_ADF);

  if (force_hdf5 == true)
    cg_set_file_type(CG_FILE_HDF5);

  /* Additional writers for linked files */

  if (   use_links
      && writer->time_dependency < FVM_WRITER_TRANSIENT_CONNECT) {
    writer->mesh_writer = _create_writer(name, "_mesh", path, writer,
                                         FVM_WRITER_FIXED_MESH);
  }

  return writer;
}

/*----------------------------------------------------------------------------
 * Finalize FVM to CGNS file writer.
 *
 * parameters:
 *   this_writer_p <-- pointer to opaque CGNS writer structure.
 *
 * returns:
 *   null pointer.
 *----------------------------------------------------------------------------*/

void *
fvm_to_cgns_finalize_writer(void  *this_writer_p)
{
  fvm_to_cgns_writer_t  *writer
                        = (fvm_to_cgns_writer_t *)this_writer_p;

  assert(writer != nullptr);

  if (writer->mesh_writer != nullptr)
    writer->mesh_writer =
      (fvm_to_cgns_writer_t *)fvm_to_cgns_finalize_writer(writer->mesh_writer);

  if (writer->rank == 0 && writer->index > -1) {

    if (writer->bases != nullptr) {

      /* Create index for time-dependent data */

      _create_timedependent_data(writer);

    }

    /* Close CGNS File */

  } /* End if rank = 0 */

  _close_file(writer);

  /* Free structures */

  CS_FREE(writer->name);
  CS_FREE(writer->filename);
  CS_FREE(writer->time_values);
  CS_FREE(writer->time_steps);

  /* Free fvm_to_cgns_base structure */

  for (int i = 0; i < writer->n_bases; i++)
    writer->bases[i] = _del_base(writer->bases[i]);

  CS_FREE(writer->bases);

  /* Free fvm_to_cgns_writer structure */

  CS_FREE(writer);
  return nullptr;
}

/*----------------------------------------------------------------------------
 * Associate new time value with a writer structure if necessary.
 *
 * parameters:
 *   this_writer_p <-- pointer to associated writer
 *   time_step     <-- time step number
 *   time_value    <-- time_value number
 *----------------------------------------------------------------------------*/

void
fvm_to_cgns_set_mesh_time(void     *this_writer_p,
                          int       time_step,
                          double    time_value)
{
  int n_vals;

  fvm_to_cgns_writer_t  *writer
                        = (fvm_to_cgns_writer_t *)this_writer_p;

  static char time_value_err_string[] =
    N_("The time value associated with time step <%d> equals <%g>,\n"
       "but time value <%g> has already been associated with this time step.\n");

  assert(writer != nullptr);

  /* First verification on time step */

  if (time_step < 0) {
    if (writer->time_dependency == FVM_WRITER_FIXED_MESH)
      return;
    else
      bft_error(__FILE__, __LINE__, 0,
                _("The given time step value should be >= 0, and not %d\n"),
                time_step);
  }

  if (   writer->time_steps != nullptr
      && writer->time_values != nullptr) {

    n_vals = writer->n_time_steps;
    if (time_step < writer->time_steps[n_vals - 1])
      bft_error(__FILE__, __LINE__, 0,
                _("The given time step value should be >= %d, and not %d\n"),
                writer->time_steps[n_vals - 1], time_step);

    /* Verifications on time value */

    else if (time_step == writer->time_steps[n_vals - 1]) {
      if (   time_value < writer->time_values[n_vals - 1] - 1.e-16
          || time_value > writer->time_values[n_vals - 1] + 1.e-16)
        bft_error(__FILE__, __LINE__, 0,
                  _(time_value_err_string), time_step,
                  time_value, writer->time_values[n_vals - 1]);
    }
    else { /* Add a new time step and time value */
      writer->n_time_steps += 1;
      n_vals = writer->n_time_steps;

      CS_REALLOC(writer->time_values, n_vals, double);
      CS_REALLOC(writer->time_steps, n_vals, int);

      writer->time_values[n_vals - 1] = time_value;
      writer->time_steps[n_vals - 1] = time_step;
    }
  }
  else { /* Setting of the first time step and time value */
    writer->n_time_steps += 1;
    n_vals = writer->n_time_steps;

    CS_REALLOC(writer->time_values, n_vals, double);
    CS_REALLOC(writer->time_steps, n_vals, int);

    writer->time_values[n_vals - 1] = time_value;
    writer->time_steps[n_vals - 1] = time_step;
  }

  if (writer->mesh_writer != nullptr)
    _close_file(writer->mesh_writer);

}

/*----------------------------------------------------------------------------
 * Indicate if elements of a given type in a mesh associated with a given
 * CGNS file writer need to be tesselated.
 *
 * parameters:
 *   this_writer_p <-- pointer to associated writer
 *   mesh          <-- pointer to nodal mesh structure that should be written
 *   element_type  <-- element type we are interested in
 *
 * returns:
 *   1 if tesselation of the given element type is needed, 0 otherwise
 *----------------------------------------------------------------------------*/

int
fvm_to_cgns_needs_tesselation(void               *this_writer_p,
                              const fvm_nodal_t  *mesh,
                              fvm_element_t       element_type)
{
  int  i;
  int  retval = 0;
  fvm_to_cgns_writer_t  *this_writer
                             = (fvm_to_cgns_writer_t *)this_writer_p;

  int  export_dim = fvm_nodal_get_max_entity_dim(mesh);
  if (this_writer->discard_bcs == false)
    export_dim -= 1;

  /* Initial count and allocation */

  if (   (   element_type == FVM_FACE_POLY
          && this_writer->divide_polygons == true)
      || (   element_type == FVM_CELL_POLY
          && this_writer->divide_polyhedra == true)) {

    for (i = 0 ; i < mesh->n_sections ; i++) {

      const fvm_nodal_section_t  *const  section = mesh->sections[i];

      /* Output if entity dimension equal to highest in mesh
         (i.e. no output of faces if cells present, or edges
         if cells or faces) */

      if (section->entity_dim >= export_dim) {
        if (section->type == element_type)
          retval = 1;
      }

    }

  }

  return retval;
}

/*----------------------------------------------------------------------------
 * Write nodal mesh to a CGNS file
 *
 * parameters:
 *   this_writer_p <-- pointer to associated writer.
 *   mesh          <-- pointer to nodal mesh structure that should be written.
 *----------------------------------------------------------------------------*/

void
fvm_to_cgns_export_nodal(void               *this_writer_p,
                         const fvm_nodal_t  *mesh)
{
  char  base_name[FVM_CGNS_NAME_SIZE+1];
  int   base_index;

  int grid_index = 1;
  int section_id = 0;
  cs_gnum_t   global_counter = 0;

  bool new_base = true;
  bool export_bcs = true;

  const fvm_writer_section_t  *export_section = nullptr;
  fvm_writer_section_t  *export_list = nullptr;
  fvm_to_cgns_base_t *base = nullptr;
  fvm_to_cgns_writer_t  *writer  = (fvm_to_cgns_writer_t *)this_writer_p;

  const int   n_ranks = writer->n_ranks;

  if (mesh->gc_set == nullptr || writer->discard_bcs)
    export_bcs = false;

  /* Initialization */
  /*----------------*/

  _open_file(writer);

  /* Clean mesh->name */

  strncpy(base_name, mesh->name, FVM_CGNS_NAME_SIZE);
  base_name[FVM_CGNS_NAME_SIZE] = '\0';

  /* Get CGNS base index */

  base_index = _get_base_index(writer,
                               base_name);

  if (base_index == 0)
    base_index = _add_base(writer,
                           base_name,
                           mesh);
  else
    new_base = false;

  base = writer->bases[base_index - 1];

  /* When using a linked mesh file, main output to that file */

  if (writer->mesh_writer)
    fvm_to_cgns_export_nodal(writer->mesh_writer, mesh);

  /* Build list of sections that are used here, in order of output */

  int max_dim = fvm_nodal_get_max_entity_dim(mesh);
  int min_dim = max_dim;
  if (export_bcs)
    min_dim -= 1;

  export_list = fvm_writer_export_list(mesh,
                                       min_dim,
                                       max_dim,
                                       1,
                                       true,
                                       false,
                                       writer->discard_polygons,
                                       writer->discard_polyhedra,
                                       writer->divide_polygons,
                                       true);

  _reorder_export_list(export_list);

  /* Create a zone */
  /*---------------*/

  if (new_base || writer->time_dependency > FVM_WRITER_TRANSIENT_COORDS)
    _add_zone(mesh,
              writer,
              base,
              export_list);

  /* Vertex coordinates */
  /*--------------------*/

  if (writer->mesh_writer != nullptr) {

    /* Simply add link if mesh written in separate file */

    _write_zone_link(writer,
                     base,
                     "GridCoordinates",
                     writer->mesh_writer->basename);

  }
  else {

#if defined(HAVE_MPI)

    if (n_ranks > 1)
      _export_vertex_coords_g(writer,
                              mesh,
                              base,
                              grid_index);

#endif /* HAVE_MPI */

    if (n_ranks == 1)
      _export_vertex_coords_l(writer,
                              mesh,
                              base,
                              grid_index);

  }

  /* Element connectivity */
  /*----------------------*/

  cs_gnum_t n_g_max_dim_elts = 0;

  if (   new_base
      || writer->time_dependency ==  FVM_WRITER_TRANSIENT_CONNECT)
    export_section = export_list;
  else
    export_section = nullptr;

  while (export_section != nullptr) {

    const fvm_nodal_section_t  *section = export_section->section;

    /* update global number of elements */

    if (export_bcs && section->entity_dim == max_dim) {
      if (export_section->type == section->type)
        n_g_max_dim_elts += fvm_nodal_section_n_g_elements(section);
      else {
        cs_gnum_t n_g_tesselated_elements;
        fvm_tesselation_get_global_size(section->tesselation,
                                        export_section->type,
                                        &n_g_tesselated_elements,
                                        nullptr);
        n_g_max_dim_elts += n_g_tesselated_elements;
      }
    }

    /* update section_id (used in section name) */

    section_id++;

    /* Simply add link if mesh written in separate file */

    if (writer->mesh_writer != nullptr) {
      char section_name[FVM_CGNS_NAME_SIZE + 1];
      CGNS_ENUMT(ElementType_t) cgns_elt_type; /* Definition in cgnslib.h */
      _define_section(export_section, section_id, section_name, &cgns_elt_type);

      _write_zone_link(writer,
                       base,
                       section_name,
                       writer->mesh_writer->basename);

      export_section = export_section->next;

      continue;
    }

    /* Output for strided (regular) element types */
    /*--------------------------------------------*/

    if (section->stride > 0) {

#if defined(HAVE_MPI)
      if (n_ranks > 1)
        export_section = _export_nodal_strided_g(export_section,
                                                 writer,
                                                 mesh,
                                                 base,
                                                 section_id,
                                                 &global_counter);
#endif

      if (n_ranks == 1)
        export_section = _export_nodal_strided_l(export_section,
                                                 writer,
                                                 base,
                                                 section_id,
                                                 &global_counter);

    }

    /* Output for tesselated polygons or polyhedra */
    /*---------------------------------------------*/

    else if (export_section->type != section->type) {

#if defined(HAVE_MPI)
      if (n_ranks > 1)
        export_section = _export_nodal_tesselated_g(export_section,
                                                    writer,
                                                    mesh,
                                                    base,
                                                    section_id,
                                                    &global_counter);
#endif

      if (n_ranks == 1)
        export_section = _export_nodal_tesselated_l(export_section,
                                                    writer,
                                                    base,
                                                    section_id,
                                                    &global_counter);

    }

    /* Output for polygons */
    /*---------------------*/

    else if (export_section->type == FVM_FACE_POLY) {

#if defined(HAVE_MPI)
      if (n_ranks > 1)
        export_section = _export_nodal_polygons_g(export_section,
                                                  writer,
                                                  mesh,
                                                  base,
                                                  section_id,
                                                  &global_counter);
#endif

      if (n_ranks == 1)
        export_section =  _export_nodal_polygons_l(export_section,
                                                   writer,
                                                   base,
                                                   section_id,
                                                   &global_counter);

    }

  } /* End of loop on sections */

  /* Export boundary conditions */

  if (export_bcs)
    _export_bcs(writer, mesh, base, n_g_max_dim_elts);

  /* Free buffers */

  CS_FREE(export_list);
}

/*----------------------------------------------------------------------------
 * Write field associated with a nodal mesh to a CGNS file.
 *
 * Assigning a negative value to the time step indicates a time-independent
 * field (in which case the time_value argument is unused).
 *
 * parameters:
 *   this_writer_p    <-- pointer to associated writer
 *   mesh             <-- pointer to associated nodal mesh structure
 *   name             <-- variable name
 *   location         <-- fvm grid location (nodes or elements)
 *   dimension        <-- variable dimension (0: constant, 1: scalar,
 *                        3: vector, 6: sym. tensor, 9: asym. tensor)
 *   interlace        <-- indicates if variable in memory is interlaced
 *   n_parent_lists   <-- indicates if variable values are to be obtained
 *                        directly through the local entity index (when 0) or
 *                        through the parent entity numbers (when 1 or more)
 *   parent_num_shift <-- parent number to value array index shifts;
 *                        size: n_parent_lists
 *   datatype         <-- indicates the data type of (source) field values
 *   time_step        <-- number of the current time step
 *   time_value       <-- associated time value
 *   field_values     <-- array of associated field value arrays
 *----------------------------------------------------------------------------*/

void
fvm_to_cgns_export_field(void                   *this_writer_p,
                         const fvm_nodal_t      *mesh,
                         const char             *name,
                         fvm_writer_var_loc_t    location,
                         int                     dimension,
                         cs_interlace_t          interlace,
                         int                     n_parent_lists,
                         const cs_lnum_t         parent_num_shift[],
                         cs_datatype_t           datatype,
                         int                     time_step,
                         double                  time_value,
                         const void       *const field_values[])
{
  char   base_name[FVM_CGNS_NAME_SIZE+1];
  char   field_name[FVM_CGNS_NAME_SIZE+1];
  cs_datatype_t  export_datatype = CS_FLOAT;
  int    output_dim;

  fvm_writer_field_helper_t  *helper = nullptr;
  fvm_writer_section_t  *export_list = nullptr;

  char  *field_label = nullptr;
  fvm_to_cgns_writer_t  *writer = (fvm_to_cgns_writer_t *)this_writer_p;
  int base_index = 0;
  int sol_index = 0;
  CGNS_ENUMT(GridLocation_t)  cgns_location = CGNS_ENUMV(GridLocationNull);

  const int  rank = writer->rank;

  if (writer->discard_steady && time_step < 0)
    return;

  /* Initialization */
  /*----------------*/

  /* FVM datatype conversion to CGNS */

  switch (datatype) {
  case CS_DOUBLE:
    if (writer->preserve_precision)
      export_datatype = CS_DOUBLE;
    else
      export_datatype = CS_FLOAT;
    break;
  case CS_FLOAT:
    export_datatype = CS_FLOAT;
    break;
  case CS_UINT32:
    export_datatype = CS_INT32;
    break;
  case CS_UINT64:
    export_datatype = CS_INT32;
    break;
  case CS_INT32:
    export_datatype = CS_INT32;
    break;
  case CS_INT64:
    export_datatype = CS_INT32;
    break;
  default:
    assert(0);
  }

  /* Set CGNS location */

  if (location == FVM_WRITER_PER_NODE)
    return;
  if (location == FVM_WRITER_PER_NODE)
    cgns_location = CGNS_ENUMV(Vertex);
  else if (location == FVM_WRITER_PER_ELEMENT)
    cgns_location = CGNS_ENUMV(CellCenter);

  /* Cell dimension */

  output_dim = dimension;

  assert(output_dim > 0);

  if (dimension == 2)
    output_dim = 3;
  else if (dimension > 3 && dimension != 6 && dimension != 9)
    bft_error(__FILE__, __LINE__, 0,
              _("Data of dimension %d not handled"), dimension);

  /* Get CGNS base index */
  /*---------------------*/

  strncpy(base_name, mesh->name, FVM_CGNS_NAME_SIZE);
  base_name[FVM_CGNS_NAME_SIZE] = '\0';

  base_index = _get_base_index(writer, base_name);

  if (base_index == 0)
    base_index = _add_base(writer,
                           base_name,
                           mesh);

  /* Get CGNS solution index */
  /*-------------------------*/

  sol_index = _get_solution_index(writer,
                                  base_index,
                                  time_step,
                                  time_value,
                                  cgns_location);

  if (sol_index == 0)
    sol_index = _add_solution(writer,
                              base_index,
                              time_step,
                              time_value,
                              cgns_location);

  fvm_to_cgns_solution_t *solution
    = writer->bases[base_index - 1]->solutions[sol_index - 1];
  assert(solution->location == cgns_location);

  /* Field_Name adaptation if necessary */
  /*-----------------------------------*/

  if (rank == 0) {

    int        i, shift, pos;
    char      *tmp;

    strncpy(field_name, name, FVM_CGNS_NAME_SIZE);
    field_name[FVM_CGNS_NAME_SIZE] = '\0';

    shift = FVM_CGNS_NAME_SIZE + 1;
    CS_MALLOC(field_label, output_dim * shift, char);

    for (pos = strlen(field_name) - 1;
         pos > 0 && (field_name[pos] == ' ' || field_name[pos] == '\t');
         pos--);
    pos++;

    for (i = 0; i < output_dim; i++) {

      tmp = field_label + (i * shift);
      strncpy(tmp, field_name, shift);
      tmp[shift - 1] = '\0';

      if (output_dim > 1) {

        if (output_dim == 3) {

          const char *comp[] = {"X", "Y", "Z"};

          if (pos > shift - 2)
            pos = shift - 2;

          tmp[pos    ] = comp[i][0];
          tmp[pos + 1] = '\0';

        }
        else if (output_dim == 6) {

          const char *comp[] = {"XX", "YY", "ZZ", "XY", "XZ", "YZ"};

          if (pos > shift - 3)
            pos = shift - 3;

          tmp[pos    ] = comp[i][0];
          tmp[pos + 1] = comp[i][1];
          tmp[pos + 2] = '\0';

        }
        else if (output_dim == 9) {

          const char *comp[] = {"XX", "XY", "XZ",
                                "YX", "YY", "YZ",
                                "ZX", "ZY", "ZZ"};

          if (pos > shift - 3)
            pos = shift - 3;

          tmp[pos    ] = comp[i][0];
          tmp[pos + 1] = comp[i][1];
          tmp[pos + 2] = '\0';

        }
      }
    } /* End of loop on output dimension */

  } /* End if rank == 0 */

  /* Initialize writer helper */
  /*--------------------------*/

  int export_dim = fvm_nodal_get_max_entity_dim(mesh);

  export_list = fvm_writer_export_list(mesh,
                                       export_dim,
                                       export_dim,
                                       -1,
                                       true,
                                       true,
                                       writer->discard_polygons,
                                       writer->discard_polyhedra,
                                       writer->divide_polygons,
                                       true);

  helper = fvm_writer_field_helper_create(mesh,
                                          export_list,
                                          output_dim,
                                          CS_NO_INTERLACE,
                                          export_datatype,
                                          location);

#if defined(HAVE_MPI)

  fvm_writer_field_helper_init_g(helper,
                                 writer->min_rank_step,
                                 writer->min_block_size,
                                 writer->comm);

#endif

  /* Export field */
  /*--------------*/

  if (location == FVM_WRITER_PER_NODE) {

    _export_field_n(mesh,
                    helper,
                    writer,
                    writer->bases[base_index - 1],
                    field_label,
                    sol_index,
                    dimension,
                    interlace,
                    n_parent_lists,
                    parent_num_shift,
                    datatype,
                    field_values);

  } /* End of export field with node location */

  else if (location == FVM_WRITER_PER_ELEMENT) {

    _export_field_e(export_list,
                    helper,
                    writer,
                    writer->bases[base_index - 1],
                    field_label,
                    sol_index,
                    dimension,
                    interlace,
                    n_parent_lists,
                    parent_num_shift,
                    datatype,
                    field_values);

  } /* End of export_field on elements */

  else
    bft_error(__FILE__, __LINE__, 0,
              "fvm_to_cgns_export_field(): field location not managed.\n"
              "Associated writer: \"%s\"\n"
              "Associated base: \"%s\"\n"
              "Associated field: \"%s\"\n"
              "Associated location: %i\n",
              writer->name, base_name, field_name, location);

  /* Free helper structures */
  /*------------------------*/

  fvm_writer_field_helper_destroy(&helper);

  CS_FREE(export_list);

  if (rank == 0)
    CS_FREE(field_label);
}

/*----------------------------------------------------------------------------
 * Flush files associated with a given writer.
 *
 * parameters:
 *   this_writer_p    <-- pointer to associated writer
 *----------------------------------------------------------------------------*/

void
fvm_to_cgns_flush(void  *this_writer_p)
{
  fvm_to_cgns_writer_t *w = (fvm_to_cgns_writer_t *)this_writer_p;

  if (w->mesh_writer != nullptr)
    _close_file(w->mesh_writer);
}

/*----------------------------------------------------------------------------*/

#endif /* defined(HAVE_CGNS) */

END_C_DECLS
