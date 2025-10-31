/*============================================================================
 * Write a nodal representation associated with a mesh and associated
 * variables to Catalyst-2 objects
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

/* On glibc-based systems, define _GNU_SOURCE so as to enable
   modification of floating-point error exceptions handling;
   _GNU_SOURCE must be defined before including any headers, to ensure
   the correct feature macros are defined first. */

#if defined(__linux__) || defined(__linux) || defined(linux)
#define CS_FPE_TRAP
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#endif

#include "base/cs_defs.h"

#if defined(HAVE_CATALYST2)

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * Catalyst and Conduit library headers
 *----------------------------------------------------------------------------*/

#include <catalyst.hpp>
#include <catalyst_version.h>
#include <conduit_config.h>

#include <catalyst_conduit_blueprint.hpp>

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_error.h"
#include "bft/bft_printf.h"

#include "fvm/fvm_defs.h"
#include "fvm/fvm_convert_array.h"
#include "fvm/fvm_io_num.h"
#include "fvm/fvm_nodal.h"
#include "fvm/fvm_nodal_priv.h"
#include "fvm/fvm_writer_priv.h"

#include "base/cs_mem.h"
#include "base/cs_file.h"
#include "base/cs_parall.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "fvm/fvm_to_catalyst2.h"

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

/*============================================================================
 * Local Type Definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Catalyst-2 writer/reader structure
 *----------------------------------------------------------------------------*/

typedef struct {

  char                      *name;           /* Writer name */

  int                        n_meshes;       /* Number of associated meshes */
  fvm_writer_time_dep_t      time_dependency; /* Mesh time dependency */

  bool                       ensight_names;  /* Use EnSight naming scheme */

  conduit_cpp::Node          channel;        /* Conduit channel node */

  int                        n_time_steps;   /* Number of mesh time steps */
  int                        time_step ;     /* Mesh time steps */
  double                     time_value;     /* Mesh time value */

  bool                       modified;        /* Has output been added since
                                                 last coprocessing ? */

} fvm_to_catalyst_t;

/*============================================================================
 * Static global variables
 *============================================================================*/

static bool _debug_print = false;

int _n_writers = 0;
int _n_scripts = 0;

conduit_node *_root_node = nullptr;      /* Catalyst root node */
bool _catalyst_initialized = false;
int _use_mpi = -1;

#if defined(HAVE_MPI)
MPI_Comm  _comm = MPI_COMM_NULL;
#endif

// Type name mappings

static const char  *fvm_to_conduit_type_name[]
  = {"line",
     "tri",
     "quad",
     "polygonal",
     "tet",
     "pyramid",
     "wedge",
     "hex",
     "polyhedral"};


/* The Conduit documentation only mentions the VTK ids as examples, and it
   seems we could use FVM type ids here directly, as long as we are
   consistent with the given type map.
   But it seems that ParaView Catalys does not honor the type map,
   and assumes shapes are VTK shapes, so we also need a mapping to
   VTK type ids, at least until this is fixed. */

static const int fvm_to_conduit_type_id[]
  = {3,
     5,
     9,
     7,
     10,
     14,
     13,
     12,
     42};

// Component value keys

const char *_comp_key_3[] = {"values/x", "values/y", "values/z"};

const char *_comp_key_6[] = {"values/xx", "values/yy", "values/zz",
                             "values/xy", "values/yz", "values/xz"};

const char *_comp_key_9[] = {"values/xx", "values/xy", "values/xz",
                             "values/yx", "values/yy", "values/yz",
                             "values/zx", "values/zy", "values/zz"};

// version info strings

static char _catalyst_info_string_[2][96] = {"", ""};
static char _conduit_info_string_[2][32] = {"", ""};

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Convert a Catalyst status code to a string.
 *
 * parameters:
 *   catalyst_status <-- status
 *
 * returns:
 *   pointer to corresponding status code string.
 *----------------------------------------------------------------------------*/

static const char *
_catalyst_status_to_string(catalyst_status  status)
{
  switch(status) {
  case catalyst_status_ok:
    return "ok";
  case catalyst_status_error_no_implementation:
    return "error_no_implementation";
  case catalyst_status_error_already_loaded:
    return "error_already_loaded";
  case catalyst_status_error_not_found:
    return "error_not_found";
  case catalyst_status_error_not_catalyst:
    return "error_not_catalyst";
  case catalyst_status_error_incomplete:
    return "error_incomplete";
  case catalyst_status_error_unsupported_version:
    return "error_unsupported_version";
  case catalyst_status_error_conduit_mismatch:
    return "error_conduit_mismatch";
  default:
    break;
  };

  return "unknown";
}

/*----------------------------------------------------------------------------
 * Check if a script is a Catalyst script
 *
 * The script only does cursory checks, so may return false positives
 * in some cases, but using "incorrect" scripts for Catalyst only
 * leads to extra warnings, which should be ok pending a more general
 * file checker.
 *
 * parameters:
 *   path <-- scripts path
 *
 * returns:
 *   1 if script is a Catalyst V1, 2 if it seems to be a V2 script,
 *   script, 0 otherwise
 *----------------------------------------------------------------------------*/

static int
_check_script_is_catalyst(const char  *path)
{
  assert(path != nullptr);
  int retval = 0;

  FILE *fp = fopen(path, "r");

  if (fp == nullptr)
    return retval;

  int checks[] = {false, false, false};
  int n_checks = 0;
  bool import_catalyst = false;

  const char *check_strings[]
    = {"CreateCoProcessor(",
       "RequestDataDescription(",
       "DoCoProcessing("};

  /* Note: we could simply check for "from paraview import coprocessing"
     in most cases but in cases of non-autogenerated scripts, this might
     appear in a different form, so we limit ourselves to the main
     required methods */

  while (true) {
    char buffer[1024];
    char *e = buffer+1024;

    char *s = fgets(buffer, 1024, fp);
    if (s == nullptr) break;

    while (s < e && *s == ' ' && *s == '\t')  /* skip initial whitespace */
      s++;

    if (strncmp(s, "def", 3) == 0) {
      s += 3;
      if (*s == ' ' || *s == '\t') {
        while (*s == ' ' && *s == '\t' && *s != '\0')
          s++;
        /* Remove whitespace */
        size_t l = strlen(s);
        size_t i, j;
        for (i = 0, j = 0 ; i < l ; i++) {
          if (s[i] != ' ' && s[i] != '\t')
            s[j++] = s[i];
        }

        for (i = 0; i < 3; i++) {
          if (strncmp(s, check_strings[i], strlen(check_strings[i])) == 0) {
            if (checks[i] == false) {
              checks[i] = true;
              n_checks += 1;
            }
          }
        }

      }
    }
    else if (strncmp(s, "from", 4) == 0) {

      /* cleanup whitespace */
      int n_space = 0;
      int i = 0;
      for (int j = 0; j < 1024 && s+j < e && s[j] != '\0'; j++) {
        if (s[j] == ' ' || s[j] == '\t') {
          if (n_space < 1)
            s[i++] = ' ';
          n_space += 1;
        }
        else {
          s[i++] = s[j];
          n_space = 0;
        }
      }

      if (strncmp(s, "from paraview import catalyst", 29) == 0) {
        import_catalyst = true;
      }

    }

    if (n_checks == 3) {
      retval = 1;
      break;
    }
    else if (import_catalyst) {
      retval = 2;
      break;
    }

    /* Check for end of line; if not present, continue reading from buffer */

    while (s < e && *s != '\0' && *s != '\n')
      s++;
    while (s >= e) {
      s = fgets(buffer, 1024, fp);
      if (s == nullptr) break;
      while (s < e && *s != '\0' && *s != '\n')
        s++;
    }
  }

  fclose(fp);

  return retval;
}

/*----------------------------------------------------------------------------
 * Add a Catalyst script if not already present.
 *
 * parameters:
 *   path <-- scripts path
 *
 * returns:
 *   id of script in list, or -1 if not valid
 *----------------------------------------------------------------------------*/

static int
_add_script(const char  *path)
{
  assert(path != nullptr);

  int is_catalyst = 0;
  int rank = 0, n_ranks = 1;

#if defined(HAVE_MPI)
  if (_comm != MPI_COMM_NULL) {
    MPI_Comm_rank(_comm, &rank);
    MPI_Comm_size(_comm, &n_ranks);
  }
#endif

  if (rank < 1)
    is_catalyst = _check_script_is_catalyst(path);

#if defined(HAVE_MPI)
  if (n_ranks > 1)
    MPI_Bcast(&is_catalyst, 1, MPI_INT, 0, _comm);
#endif

  if (is_catalyst < 1)
    return -1;

  _n_scripts += 1;

  conduit_cpp::Node node = conduit_cpp::cpp_node(_root_node);

  const auto name = "catalyst/scripts/script" + std::to_string(_n_scripts- 1);
  node[name + "/filename"].set_string(path);

  return _n_scripts;
}

/*----------------------------------------------------------------------------
 * Add Catalyst scripts from directoty if not already present.
 *
 * Currently assumes all Python files in the given directory
 * are Catalyst scripts.
 *
 * parameters:
 *   dir_path <-- directory path
 *----------------------------------------------------------------------------*/

static void
_add_dir_scripts(const char  *dir_path)
{
  char **dir_files = cs_file_listdir(dir_path);

  for (int i = 0; dir_files[i] != nullptr; i++) {

    const char *file_name = dir_files[i];
    const char *ext = nullptr;
    int l_ext = 0;

    /* Find extension */
    for (int j = strlen(file_name) - 1; j > -1; j--) {
      l_ext++;
      if (file_name[j] == '.') {
        ext = file_name + j;
        break;
      }
    }
    if (ext != nullptr) {

      /* Filter: Python files only */
      if (l_ext == 3 && strncmp(ext, ".py", 3) == 0) {
        char *tmp_name = nullptr;
        CS_MALLOC(tmp_name,
                  strlen(dir_path) + 1 + strlen(file_name) + 1,
                  char);
        sprintf(tmp_name, "%s/%s", dir_path, file_name);
        _add_script(tmp_name);
        CS_FREE(tmp_name);
      }

    }

    CS_FREE(dir_files[i]);
  }

  CS_FREE(dir_files);
}

/*----------------------------------------------------------------------------
 * Initialize catalyst version info.
 *----------------------------------------------------------------------------*/

static void
_init_version_info(void)
{
  conduit_cpp::Node params;
  catalyst_status err = catalyst_about(conduit_cpp::c_node(&params));
  if (err != catalyst_status_ok) {
    for (int i = 0; i < 2; i++) {
      snprintf(_catalyst_info_string_[0], 95, "Catalyst %s", CATALYST_VERSION);
      snprintf(_conduit_info_string_[0], 31, "Conduit %s", CONDUIT_VERSION);
    }
  }
  else {
    const char u[] = "unknown";
    const  int ui[] = {-1};
    const char *version = (const char *)params["catalyst/version"].data_ptr();
    const char *abi_version = (const char *)params["catalyst/abi_version"].data_ptr();
    const char *implementation = (const char *)params["catalyst/implementation"].data_ptr();
    const int *use_mpi = (const int *)params["catalyst/use_mpi"].data_ptr();
    const char *conduit_version
      = (const char *)params["catalyst/tpl/conduit/version"].data_ptr();
    if (version == nullptr)
      version = u;
    if (abi_version == nullptr)
      abi_version = u;
    if (implementation == nullptr)
      implementation = u;
    if (use_mpi == nullptr)
      use_mpi = ui;
    if (conduit_version == nullptr)
      conduit_version = u;

    snprintf(_catalyst_info_string_[0], 95,
             "Catalyst version: %s, abi_version: %s, "
             "implementation: %s, use_mpi: %d",
             version, abi_version, implementation, *use_mpi);
    _catalyst_info_string_[0][95] = '\0';
    snprintf(_catalyst_info_string_[1], 95,
             "Catalyst version: %s, abi_version: %s",
              CATALYST_VERSION, CATALYST_ABI_VERSION);
    _catalyst_info_string_[1][95] = '\0';

    snprintf(_conduit_info_string_[0], 95,
             "Conduit version: %s", conduit_version);
    _conduit_info_string_[0][95] = '\0';
    snprintf(_conduit_info_string_[1], 95,
             "Conduit version: %s",
              CONDUIT_VERSION);
    _conduit_info_string_[1][95] = '\0';

    _use_mpi = *use_mpi;
  }
}

/*----------------------------------------------------------------------------
 * Initialize catalyst.
 *----------------------------------------------------------------------------*/

static void
_init_catalyst(void)
{
  if (_catalyst_initialized)
    return;

  conduit_cpp::Node node = conduit_cpp::cpp_node(_root_node);

  // node["catalyst_load/implementation"] = "paraview";
  // node["catalyst_load/search_paths/paraview"] = PARAVIEW_IMPL_DIR;

  // Add MPI communicator handle

#if defined(HAVE_MPI)
  {
    int mpi_flag = 0;
    MPI_Initialized(&mpi_flag);
    if (mpi_flag && _comm != MPI_COMM_NULL && _comm != MPI_COMM_WORLD) {
      node["catalyst/mpi_comm"] = MPI_Comm_c2f(_comm);
    }
  }
#endif

  catalyst_status err = catalyst_initialize(conduit_cpp::c_node(&node));
  if (err == catalyst_status_ok) {
    _catalyst_initialized = true;
    conduit_cpp::Node params;
    err = catalyst_about(conduit_cpp::c_node(&params));
    if (err != catalyst_status_ok)
      bft_error(__FILE__, __LINE__, 0,
                _("catalyst_about error: %d (%s)"),
                (int)err, _catalyst_status_to_string(err));
    else {
      // conduit_node_print(conduit_cpp::c_node(&params));
      _init_version_info();
      bft_printf(_("\n"
                   "Catalyst initialized\n"
                   "  %s\n"
                   "  %s\n\n"),
                 _catalyst_info_string_[0],
                 _conduit_info_string_[0]);
    }
  }
  else
    bft_error(__FILE__, __LINE__, 0,
              _("catalyst_initialize error: %d (%s)"),
              (int)err, _catalyst_status_to_string(err));

  // Activate debug printing if needed

  const char *s = getenv("CS_CATALYST_DEBUG_PRINT");
  if (s != nullptr) {
    if (atoi(s) > 0)
      _debug_print = true;
  }
}

/*----------------------------------------------------------------------------
 * Finalize catalyst.
 *
 * parameters:
 *   private_comm <-- if true, use dedicated communicator
 *   comm         <-- associated MPI communicator.
 *----------------------------------------------------------------------------*/

static void
_finalize_catalyst(void)
{
  if (_catalyst_initialized && _n_writers < 1) {

    catalyst_status err = catalyst_finalize(_root_node);
    if (err != catalyst_status_ok) {
      bft_error(__FILE__, __LINE__, 0,
                _("Catalyst finalization error: %d (%s)"),
                (int)err, _catalyst_status_to_string(err));
    }

    _catalyst_initialized = false;

    conduit_node_destroy(_root_node);
    _root_node = nullptr;
  }
}

/*----------------------------------------------------------------------------
 * Get vertex order to describe Conduit element type.
 *
 * parameters:
 *   norm_elt_type <-- Catalyst element type.
 *   vertex_order  --> Pointer to vertex order array (0 to n-1), if different
 *
 * return:
 *   -1 if not relevant (polyhedra), 0 if identical, 1 if different
 *----------------------------------------------------------------------------*/

static int
_get_vertex_order(fvm_element_t  fvm_elt_type,
                  int           *vertex_order)
{
  int retval = 0;

  switch(fvm_elt_type) {

  case FVM_EDGE:
    vertex_order[0] = 0;
    vertex_order[1] = 1;
    break;

  case FVM_FACE_TRIA:
    vertex_order[0] = 0;
    vertex_order[1] = 1;
    vertex_order[2] = 2;
    break;

  case FVM_FACE_QUAD:
    vertex_order[0] = 0;
    vertex_order[1] = 1;
    vertex_order[2] = 2;
    vertex_order[3] = 3;
    break;

  case FVM_CELL_TETRA:
    vertex_order[0] = 0;
    vertex_order[1] = 1;
    vertex_order[2] = 2;
    vertex_order[3] = 3;
    break;

  case FVM_CELL_PYRAM:
    vertex_order[0] = 0;
    vertex_order[1] = 1;
    vertex_order[2] = 2;
    vertex_order[3] = 3;
    vertex_order[4] = 4;
    break;

  case FVM_CELL_PRISM:
    vertex_order[0] = 0;
    vertex_order[1] = 2;
    vertex_order[2] = 1;
    vertex_order[3] = 3;
    vertex_order[4] = 5;
    vertex_order[5] = 4;
    retval = 1;
    break;

  case FVM_CELL_HEXA:
    vertex_order[0] = 0;
    vertex_order[1] = 1;
    vertex_order[2] = 2;
    vertex_order[3] = 3;
    vertex_order[4] = 4;
    vertex_order[5] = 5;
    vertex_order[6] = 6;
    vertex_order[7] = 7;
    break;

  case FVM_FACE_POLY:
    vertex_order[0] = -1;
    retval = 0;
    break;

  case FVM_CELL_POLY:
    vertex_order[0] = -1;
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              "_get_vertex_order(): No associated Conduit element type known\n"
              "FVM element type: \"%s\"\n",
              fvm_element_type_name[fvm_elt_type]);
  }

  return retval;
}

/*----------------------------------------------------------------------------
 * Write vertex coordinates to VTK.
 *
 * parameters:
 *   mesh      <-- pointer to nodal mesh structure
 *   mesh_node <-- pointer to Conduit mesh node
 *----------------------------------------------------------------------------*/

static void
_export_vertex_coords(const fvm_nodal_t   *mesh,
                      conduit_cpp::Node   &mesh_node)
{
  mesh_node["coordsets/coords/type"].set_string("explicit");

  const double  *vertex_coords = mesh->vertex_coords;
  const cs_lnum_t  n_vertices = mesh->n_vertices;

  /* Vertex coordinates */
  /*--------------------*/

  cs_lnum_t stride = mesh->dim;

  if (mesh->parent_vertex_id != nullptr) {

    // Use the conduit_cpp::Node::set(std::vector<..>) API, which deep-copies

    double *coords;
    CS_MALLOC(coords, n_vertices*3, double);

    const cs_lnum_t  *parent_vertex_id = mesh->parent_vertex_id;
    for (cs_lnum_t i = 0; i < n_vertices; i++) {
      for (cs_lnum_t j = 0; j < stride; j++)
        coords[i*3 + j] = vertex_coords[parent_vertex_id[i]*stride + j];
      for (cs_lnum_t j = stride; j < 3; j++)
        coords[i*stride + j] = vertex_coords[parent_vertex_id[i]*stride + j];
    }
    mesh_node["coordsets/coords/values/x"].set(coords,
                                               n_vertices,
                                               0, // offset
                                               stride * sizeof(double));
    mesh_node["coordsets/coords/values/y"].set(coords,
                                               n_vertices,
                                               sizeof(double),
                                               stride * sizeof(double));
    mesh_node["coordsets/coords/values/z"].set(coords,
                                               n_vertices,
                                               2*sizeof(double),
                                               stride * sizeof(double));

    CS_FREE(coords);
  }
  else { // zero-copy
    mesh_node["coordsets/coords/values/x"].set_external
      (vertex_coords,
       n_vertices,
       0, // offset
       stride * sizeof(double));

    mesh_node["coordsets/coords/values/y"].set_external
      (vertex_coords,
       n_vertices,
       sizeof(double),
       stride * sizeof(double));

    if (stride > 2)
      mesh_node["coordsets/coords/values/z"].set_external
        (vertex_coords,
         n_vertices,
         2 * sizeof(double),
         stride * sizeof(double));
    else {
      cs_coord_t coord_z[0];
      mesh_node["coordsets/coords/values/z"].set
        (coord_z,
         n_vertices,
         0,  // offsets
         0); // stride (0, repeating)
    }
  }

  // TODO: map adjacency set in case of parallel run.
  // Requires building or sharing a cs_interface_set_t object
  // for vertices, as Conduit pairwise adjacency sets can be
  // build directly from cs_interface_t object arrays.

  mesh_node["state/domain"].set(mesh->num_dom - 1);

}

/*----------------------------------------------------------------------------
 * Map strided connectivity block to Conduit
 *
 * The connectivity on input may use 1 to n numbering, so it is shifted
 * by -1 here.
 *
 * TODO: in most cases, when it uses the same element winding as
 * The reference Conduit blueprint (or VTK), we could use zero-copy
 * if the FVM element numbering were switched to 0-based instead of 1-based.
 *
 * parameters:
 *   type        <-- FVM element type
 *   n_elts      <-- number of elements in block
 *   offset_base <-- values offsets base
 *   connect     <-- section connectivity array
 *   values      --> exported connectivity array
 *   offsets     --> element offsets, or nullptr
 *   elt_types   --> element connectivity offsets, or nullptr
 *   elt_sizes   --> element sizes, or nullptr
 *----------------------------------------------------------------------------*/

static void
_map_section_strided(fvm_element_t        type,
                     cs_lnum_t            n_elts,
                     cs_lnum_t            offsets_base,
                     const cs_lnum_t      connect[],
                     cs_lnum_t            values[],
                     cs_lnum_t           *offsets,
                     unsigned int        *elt_types,
                     unsigned int        *elt_sizes)
{
  int vertex_order[8];

  const cs_lnum_t  stride = fvm_nodal_n_vertices_element[type];

  int order_type = _get_vertex_order(type, vertex_order);

  if (order_type >= 0) {
    if (order_type == 0) {
      cs_lnum_t n = n_elts * stride;
      for (cs_lnum_t i = 0; i < n; i++) {
        values[i] = connect[i] - 1;
      }
    }
    else {
      for (cs_lnum_t i = 0; i < n_elts; i++) {
        for (cs_lnum_t j = 0; j < stride; j++)
          values[i*stride + j] = connect[i*stride + vertex_order[j]] - 1;
      }
    }
  }

  if (elt_types != nullptr) {
    assert(elt_sizes != nullptr);
    const unsigned int type_id = fvm_to_conduit_type_id[type];
    const unsigned int elt_size = stride;
    for (cs_lnum_t i = 0; i < n_elts; i++) {
      offsets[i] = offsets_base + stride*i;
      elt_types[i] = type_id;
      elt_sizes[i] = elt_size;
    }
  }
}

/*----------------------------------------------------------------------------
 * Map polygons to Conduit
 *
 * parameters:
 *   section     <-- pointer to FVM nodal mesh section
 *   offset_base <-- offsets base
 *   values      --> exported connectivity array
 *   offsets     --> element offsets, or nullptr
 *   elt_types   --> element connectivity offsets, or nullptr
 *   elt_sizes   --> element sizes, or nullptr
 *----------------------------------------------------------------------------*/

static void
_map_section_polygons(const fvm_nodal_section_t  *section,
                      cs_lnum_t                   offsets_base,
                      cs_lnum_t                   values[],
                      cs_lnum_t                  *offsets,
                      unsigned int               *elt_types,
                      unsigned int               *elt_sizes)
{
  const cs_lnum_t n_elts = (section->type == FVM_CELL_POLY) ?
    section->n_faces : section->n_elements;

  const cs_lnum_t *vertex_index = section->vertex_index;
  const cs_lnum_t *vertex_num = section->vertex_num;

  const unsigned int type_id = fvm_to_conduit_type_id[FVM_FACE_POLY];

  /* Loop on all polygonal faces
     --------------------------- */

  for (cs_lnum_t i = 0; i < n_elts; i++) {

    offsets[i] = offsets_base + vertex_index[i];
    elt_sizes[i] = vertex_index[i+1] - vertex_index[i];
    if (elt_types != nullptr)
      elt_types[i] = type_id;

    for (cs_lnum_t j = vertex_index[i]; j < vertex_index[i+1]; j++)
      values[j] = vertex_num[j] - 1;

  } /* End of loop on polygonal faces */
}

/*----------------------------------------------------------------------------
 * Partially map and write polyhedra to Conduit
 *
 * parameters:
 *   section     <-- pointer to FVM nodal mesh section
 *   values      <-- values (face ids) base
 *   offset_base <-- offsets base
 *   values      --> exported connectivity array
 *   offsets     --> element offsets, or nullptr
 *   elt_types   --> element connectivity offsets, or nullptr
 *   elt_sizes   --> element sizes, or nullptr
 *----------------------------------------------------------------------------*/

static void
_map_section_polyhedra(const fvm_nodal_section_t  *section,
                       cs_lnum_t                   values_base,
                       cs_lnum_t                   offsets_base,
                       cs_lnum_t                   values[],
                       cs_lnum_t                  *offsets,
                       unsigned int               *elt_types,
                       unsigned int               *elt_sizes)
{
  const cs_lnum_t n_elts = section->n_elements;

  const cs_lnum_t *face_index = section->face_index;
  const cs_lnum_t *face_num = section->face_num;

  const unsigned int type_id = fvm_to_conduit_type_id[section->type];

  /* Loop on all polyhedral faces
     ---------------------------- */

  for (cs_lnum_t i = 0; i < n_elts; i++) {

    offsets[i] = offsets_base + face_index[i],
    elt_sizes[i] = face_index[i+1] - face_index[i];
    if (elt_types != nullptr)
      elt_types[i] = type_id;

    for (cs_lnum_t j = face_index[i]; j < face_index[i+1]; j++)
      values[j] = cs::abs(face_num[j]) - 1 + values_base;

  } /* End of loop on polygonal faces */
}

/*----------------------------------------------------------------------------
 * Export global ids ton Conduit.
 *
 * Output fields are non interlaced. Input arrays may be interlaced or not.
 *
 * parameters:
 *   mesh          <-- pointer to nodal mesh structure
 *   c_mesh        <-- Conduit mesh node
 *----------------------------------------------------------------------------*/

static void
_export_global_vtx_ids(const fvm_nodal_t     *mesh,
                       conduit_cpp::Node     &mesh_node)
{
  const cs_lnum_t n_vtx = fvm_nodal_get_n_entities(mesh, 0);

  int64_t *global_ids = nullptr;

  CS_MALLOC(global_ids, n_vtx, int64_t);

  if (_debug_print)
    printf("Export global vertex ids %s to Conduit\n", mesh->name);

  const cs_gnum_t *g_vtx_num
    = fvm_io_num_get_global_num(mesh->global_vertex_num);

  for (cs_lnum_t i = 0; i < n_vtx; i++)
    global_ids[i] = g_vtx_num[i] - 1;

  /* Now pass to Conduit */

  auto field = mesh_node["fields/GlobalNodeIds"];

  field["association"].set_string("vertex");
  field["topology"].set_string("mesh");
  field["volume_dependent"].set_string("false");  // true if extensive
  field["values"].set(global_ids, n_vtx);

  // Set associated VTK metadata
  auto field_metadata = mesh_node["state/metadata/vtk_fields/GlobalNodeIds"];
  field_metadata["attribute_type"] = "GlobalIds";

  if (_debug_print)
    field.print();  // dump local field to stdout (debug)

  CS_FREE(global_ids);
}

/*----------------------------------------------------------------------------
 * Increment element and connectivity size counts for a given mesh section.
 *----------------------------------------------------------------------------*/

static void
_section_counts_increment
(
  const fvm_nodal_section_t  *section,      // pointer to section
  cs_lnum_t                  &n_elts,       // number of elements
  cs_lnum_t                  &n_sub_elts,   // number polyhedra faces
  cs_lnum_t                  &n_values,     // connectivity size
  cs_lnum_t                  &n_sub_values  // polyhedral faces connect. size
)
{
  n_elts += section->n_elements;
  if (section->stride > 0)
    n_values += (cs_lnum_t)(section->stride)*section->n_elements;
  else if (section->type == FVM_FACE_POLY) {
    n_values += section->vertex_index[section->n_elements];
  }
  else if (section->type == FVM_CELL_POLY) {
    n_values += section->face_index[section->n_elements];
    n_sub_elts += section->n_faces;
    n_sub_values += section->vertex_index[section->n_faces];
  }
}

/*----------------------------------------------------------------------------
 * Create a Conduit mesh structure.
 *
 * parameters:
 *   w     <-- Writer
 *   mesh  <-- FVM mesh  structure.
 *----------------------------------------------------------------------------*/

static void
_export_conduit_mesh(fvm_to_catalyst_t  *w,
                     const fvm_nodal_t  *mesh)
{
  std::string cpp_mesh_name(mesh->name);

  const int  elt_dim = fvm_nodal_get_max_entity_dim(mesh);

  // Create (or access) associated mesh node
  auto mesh_node = w->channel["data/" + cpp_mesh_name];

  mesh_node["state/cycle"].set(w->time_step);
  mesh_node["state/time"].set(w->time_value);

  _export_vertex_coords(mesh, mesh_node);

  if (cs_glob_n_ranks > 1 && _use_mpi)
    _export_global_vtx_ids(mesh, mesh_node);

  /* Count used element types and compute associated sizes
     ----------------------------------------------------- */

  bool type_is_present[FVM_N_ELEMENT_TYPES];
  for (int i = 0; i < FVM_N_ELEMENT_TYPES; i++)
    type_is_present[i] = false;

  int n_active_sections = 0;
  cs_lnum_t n_elts = 0, n_sub_elts = 0;
  cs_lnum_t n_values = 0, n_sub_values = 0;
  bool need_offsets = false;

  for (int section_id = 0; section_id < mesh->n_sections; section_id++) {
    const fvm_nodal_section_t  *section = mesh->sections[section_id];
    if (section->entity_dim < elt_dim)
      continue;

    type_is_present[section->type] = true;
    _section_counts_increment(section,
                              n_elts, n_sub_elts,
                              n_values, n_sub_values);

    if (section->stride <= 0)
      need_offsets = true;

    n_active_sections++;
  }

  std::string tpath = "topologies/mesh";

  mesh_node[tpath + "/coordset"].set_string("coords");
  mesh_node[tpath + "/type"].set_string("unstructured");

  auto topology = mesh_node[tpath + "/elements"];

  if (n_active_sections == 1) {
    for (int i = 0; i < FVM_N_ELEMENT_TYPES; i++) {
      if (type_is_present[i])
        topology["shape"].set_string(fvm_to_conduit_type_name[i]);
    }
  }
  else {
    need_offsets = true;
    topology["shape"].set_string("mixed");
    auto shape_map = topology["/shape_map"];
    for (int i = 0; i < FVM_N_ELEMENT_TYPES; i++) {
      if (type_is_present[i])
        shape_map[fvm_to_conduit_type_name[i]].set(fvm_to_conduit_type_id[i]);
    }
  }

  cs_lnum_t *values = nullptr, *offsets = nullptr;
  unsigned int *shapes = nullptr, *elt_sizes = nullptr;

  CS_MALLOC(values, n_values, cs_lnum_t);
  if (need_offsets) {
    CS_MALLOC(offsets, n_elts, cs_lnum_t);
    CS_MALLOC(shapes, n_elts, unsigned int);
    CS_MALLOC(elt_sizes, n_elts, unsigned int);
  }

  /* Element connectivity */
  /*----------------------*/

  bool have_polyhedra = false;
  cs_lnum_t elt_shift = 0, sub_elt_shift = 0;
  cs_lnum_t values_shift = 0, sub_values_shift = 0;

  for (int section_id = 0; section_id < mesh->n_sections; section_id++) {

    const fvm_nodal_section_t  *section = mesh->sections[section_id];

    if (section->entity_dim < elt_dim)
      continue;

    cs_lnum_t *_values = values + values_shift;
    cs_lnum_t *_offsets = (offsets != nullptr) ?
      offsets + elt_shift : nullptr;
    unsigned int *_shapes = (shapes != nullptr) ?
      shapes + elt_shift : nullptr;
    unsigned int *_elt_sizes = (elt_sizes != nullptr) ?
      elt_sizes + elt_shift : nullptr;

    if (section->stride > 0)
      _map_section_strided(section->type,
                           section->n_elements,
                           values_shift,
                           section->vertex_num,
                           _values,
                           _offsets,
                           _shapes,
                           _elt_sizes);

    else if (section->type == FVM_FACE_POLY)
      _map_section_polygons(section,
                            values_shift,
                            _values,
                            _offsets,
                            _shapes,
                            _elt_sizes);

    else if (section->type == FVM_CELL_POLY) {
      _map_section_polyhedra(section,
                             sub_elt_shift,
                             values_shift,
                             _values,
                             _offsets,
                             _shapes,
                             _elt_sizes);
      /* Faces -> vertices connectivity output in separate loop
         to reduce number simultaneous extra arrray copies/buffers. */
      have_polyhedra = true;
    }

    _section_counts_increment(section,
                              elt_shift, sub_elt_shift,
                              values_shift, sub_values_shift);


  } /* End of loop on sections */

  topology["connectivity"].set(values, n_values);

  if (shapes != nullptr)
    topology["shapes"].set(shapes, n_elts);
  if (elt_sizes != nullptr)
    topology["sizes"].set(elt_sizes, n_elts);
  if (offsets != nullptr)
    topology["offsets"].set(offsets, n_elts);

  CS_FREE(values);
  CS_FREE(offsets);
  CS_FREE(shapes);
  CS_FREE(elt_sizes);

  /* Face connectivity for polyhedra
     ------------------------------- */

  if (have_polyhedra) {

    auto subelements = mesh_node[tpath + "/subelements"];

    cs_lnum_t *sub_values, *sub_offsets;
    unsigned int *sub_shapes, *sub_elt_sizes;
    CS_MALLOC(sub_values, n_sub_values, cs_lnum_t);
    CS_MALLOC(sub_offsets, n_sub_elts, cs_lnum_t);
    CS_MALLOC(sub_shapes, n_sub_elts, unsigned int);
    CS_MALLOC(sub_elt_sizes, n_sub_elts, unsigned int);

    elt_shift = 0; sub_elt_shift = 0;
    values_shift = 0; sub_values_shift = 0;

    for (int section_id = 0; section_id < mesh->n_sections; section_id++) {

      const fvm_nodal_section_t  *section = mesh->sections[section_id];

      if (section->entity_dim < elt_dim)
        continue;

      cs_lnum_t *_sub_values = sub_values + sub_values_shift;
      cs_lnum_t *_sub_offsets = (sub_offsets != nullptr) ?
        sub_offsets + sub_elt_shift : nullptr;
      unsigned int *_sub_shapes = (sub_shapes != nullptr) ?
        sub_shapes + sub_elt_shift : nullptr;
      unsigned int *_sub_elt_sizes = (sub_elt_sizes != nullptr) ?
        sub_elt_sizes + sub_elt_shift : nullptr;

      if (section->type == FVM_CELL_POLY)
        _map_section_polygons(section,
                              sub_values_shift,
                              _sub_values,
                              _sub_offsets,
                              _sub_shapes,
                              _sub_elt_sizes);

      _section_counts_increment(section,
                                elt_shift, sub_elt_shift,
                                values_shift, sub_values_shift);

    } /* End of loop on sections */

    subelements["shape"].set_string("mixed");
    auto shape_map = subelements["/shape_map"];
    {
      int i = FVM_FACE_POLY;
      shape_map[fvm_to_conduit_type_name[i]].set(fvm_to_conduit_type_id[i]);
    }
    subelements["connectivity"].set(sub_values, n_sub_values);
    subelements["shapes"].set(sub_shapes, n_sub_elts);
    subelements["sizes"].set(sub_elt_sizes, n_sub_elts);
    subelements["offsets"].set(sub_offsets, n_sub_elts);

    CS_FREE(sub_values);
    CS_FREE(sub_offsets);
    CS_FREE(sub_shapes);
    CS_FREE(sub_elt_sizes);

  }

  w->modified = true;

  if (_debug_print) {
    printf("Export %s (%dD) to Conduit\n", mesh->name, elt_dim);
    mesh_node.print();  // dump local mesh to stdout (debug)

    conduit_cpp::Node info;

    conduit_cpp::Blueprint::verify("mesh", mesh_node, info);
    info.print();
  }
}

/*----------------------------------------------------------------------------
 * Build component name for field
 *
 * parameters:
 *   dim       <-- field dimension
 *   comp_id   <-- component id
 *
 * Return component name string
 *----------------------------------------------------------------------------*/

static std::string
_field_c_name(int    dim,
              int    comp_id)
{
  char   buffer[8];

  if (dim == 6) {
    const char *cname[] = {"_XX", "_YY", "_ZZ", "_XY", "_YZ", "_XZ"};
    (void)strncpy(buffer, cname[comp_id], 7);
  }
  else if (dim == 9) {
    const char *cname[] = {"_XX", "_XY", "_XZ",
                           "_YX", "_YY", "_YZ",
                           "_ZX", "_ZY", "_ZZ"};
    (void)strncpy(buffer, cname[comp_id], 7);
  }
  else
    snprintf(buffer, 7, "_%d", comp_id);

  buffer[7] = '\0';

  return std::string(buffer);
}

/*----------------------------------------------------------------------------
 * Write field values associated with element values of a nodal mesh to VTK.
 *
 * Output fields are non interlaced. Input arrays may be interlaced or not.
 *
 * parameters:
 *   w                <-- pointer to writer
 *   mesh             <-- pointer to nodal mesh structure
 *   mesh_name        <-- mesh name
 *   field_name       <-- field name
 *   elt_dim          <-- 0 for vertices, > 0 for edges, faces, cells
 *   dim              <-- field dimension
 *   interlace        <-- indicates if field in memory is interlaced
 *   n_parent_lists   <-- indicates if field values are to be obtained
 *                        directly through the local entity index (when 0) or
 *                        through the parent entity numbers (when 1 or more)
 *   parent_num_shift <-- parent list to common number index shifts;
 *                        size: n_parent_lists
 *   datatype         <-- indicates the data type of (source) field values
 *   field_values     <-- array of associated field value arrays
 *----------------------------------------------------------------------------*/

static void
_export_field_values(fvm_to_catalyst_t         *w,
                     const fvm_nodal_t         *mesh,
                     const std::string          mesh_name,
                     const std::string          field_name,
                     int                        elt_dim,
                     int                        dim,
                     cs_interlace_t             interlace,
                     int                        n_parent_lists,
                     const cs_lnum_t            parent_num_shift[],
                     cs_datatype_t              datatype,
                     const void          *const field_values[])
{
  const cs_lnum_t dest_dim = dim;

  const cs_lnum_t n_elts = fvm_nodal_get_n_entities(mesh, elt_dim);

  float *values;
  CS_MALLOC(values, n_elts*dest_dim, float);

  if (_debug_print)
    printf("Export %s:%s to Conduit\n", mesh->name, field_name.c_str());

  /* loop on sections which should be appended */

  auto mesh_node = w->channel["data/" + mesh_name];

  if (elt_dim > 0) {

    cs_lnum_t src_shift = 0, start_id = 0;

    for (int section_id = 0; section_id < mesh->n_sections; section_id++) {

      const fvm_nodal_section_t  *section = mesh->sections[section_id];

      if (section->entity_dim < elt_dim)
        continue;

      assert(values != nullptr || section->n_elements == 0);

      fvm_convert_array(dim,
                        0,
                        dest_dim,
                        src_shift,
                        section->n_elements + src_shift,
                        interlace,
                        datatype,
                        CS_FLOAT,
                        n_parent_lists,
                        parent_num_shift,
                        section->parent_element_id,
                        field_values,
                        values + start_id);

      start_id += section->n_elements*dest_dim;
      if (n_parent_lists == 0)
        src_shift += section->n_elements;

    }

  }

  const char *s_association = (elt_dim == 0) ? "vertex" : "element";

  /* Now pass to Conduit */

  if (dim == 1 || dim == 3 || dim == 6 || dim == 9) {

    std::string sname = std::string("fields/") + field_name;

    auto field = mesh_node[sname];

    field["association"].set_string(s_association);
    field["topology"].set_string("mesh");
    field["volume_dependent"].set_string("false");  // true if extensive

    if (dim == 1) {
      field["values"].set(values, n_elts);
    }

    else {
      cs_lnum_t stride = sizeof(float)*dim;
      if (dim == 3) {
        for (int i = 0; i < dim; i++)
          field[_comp_key_3[i]].set(values, n_elts, i*sizeof(float), stride);
      }
      else if (dim == 6) {
        for (int i = 0; i < dim; i++)
          field[_comp_key_6[i]].set(values, n_elts, i*sizeof(float), stride);
      }
      else if (dim == 9) {
        for (int i = 0; i < dim; i++)
          field[_comp_key_9[i]].set(values, n_elts, i*sizeof(float), stride);
      }
    }

    if (_debug_print)
      field.print();  // dump local field to stdout (debug)

  }

  else {

    for (int comp_id = 0; comp_id < dim; comp_id++) {

      std::string sname = "fields/" + field_name + _field_c_name(dim, comp_id);

      auto field = mesh_node[sname];

      field["association"].set_string("element");
      field["topology"].set_string("mesh");
      field["volume_dependent"].set_string("false");  // true if extensive

      cs_lnum_t stride = sizeof(float)*dim;
      field["values"].set(values, n_elts, comp_id*sizeof(float), stride);

      if (_debug_print)
        field.print();  // dump local field to stdout (debug)

    }
  }

  CS_FREE(values);
}

/*============================================================================
 * Public function definitions
 *============================================================================*/

BEGIN_C_DECLS

/*----------------------------------------------------------------------------
 * Returns number of library version strings associated with Catalyst-2.
 *
 * The first associated version string should corresponds to Catalyst,
 * The second to the Conduit library.
 *
 * returns:
 *   number of library version strings associated with Catalyst output.
 *----------------------------------------------------------------------------*/

int
fvm_to_catalyst2_n_version_strings(void)
{
  return 2;
}

/*----------------------------------------------------------------------------
 * Returns a library version string associated with the Catalyst-2 output.
 *
 * The first associated version string should correspond to Catalyst info,
 * The second to conduit info.
 *
 * In certain cases, when using dynamic libraries, fvm may be compiled
 * with one library version, and linked with another. If both run-time
 * and compile-time version information is available, this function
 * will return the run-time version string by default.
 *
 * Setting the compile_time flag to 1, the compile-time version string
 * will be returned if this is different from the run-time version.
 * If the version is the same, or only one of the 2 version strings are
 * available, a null character string will be returned with this flag set.
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
fvm_to_catalyst2_version_string(int string_index,
                                int compile_time_version)
{
  const char *retval = nullptr;

  if (strlen(_catalyst_info_string_[0])  < 1)
    _init_version_info();

  if (compile_time_version) {
    if (string_index == 0)
      retval = _catalyst_info_string_[1];
    else if (string_index == 1)
      retval = _conduit_info_string_[1];
  }

  else {
    if (string_index == 0)
      retval = _catalyst_info_string_[0];
    else if (string_index == 1)
      retval = _conduit_info_string_[0];
  }

  return retval;
}

/*----------------------------------------------------------------------------
 * Initialize FVM to Catalyst-2 object writer.
 *
 * Options are:
 *   private_comm        use private MPI communicator (default: false)
 *   names=<fmt>         use same naming rules as <fmt> format
 *                       (default: ensight)
 *
 * parameters:
 *   name           <-- base output case name.
 *   options        <-- whitespace separated, lowercase options list
 *   time_dependecy <-- indicates if and how meshes will change with time
 *   comm           <-- associated MPI communicator.
 *
 * returns:
 *   pointer to opaque writer structure.
 *----------------------------------------------------------------------------*/

#if defined(HAVE_MPI)
void *
fvm_to_catalyst2_init_writer(const char             *name,
                             const char             *path,
                             const char             *options,
                             fvm_writer_time_dep_t   time_dependency,
                             MPI_Comm                comm)
#else
void *
fvm_to_catalyst2_init_writer(const char             *name,
                             const char             *path,
                             const char             *options,
                             fvm_writer_time_dep_t   time_dependency)
#endif
{
  CS_UNUSED(path);

  fvm_to_catalyst_t  *w = nullptr;

  // Initialize writer

  CS_MALLOC(w, 1, fvm_to_catalyst_t);

  w->time_dependency = time_dependency;

  w->time_step  = -1;
  w->time_value = 0.0;

  w->ensight_names = true;

  // Writer name

  if (name != nullptr) {
    CS_MALLOC(w->name, strlen(name) + 1, char);
    strcpy(w->name, name);
  }
  else {
    const char _name[] = "Catalyst2";
    CS_MALLOC(w->name, strlen(_name) + 1, char);
    strcpy(w->name, _name);
  }

  // Parse options

  if (options != nullptr) {

    int i1, i2, l_opt;
    int l_tot = strlen(options);

    i1 = 0; i2 = 0;
    while (i1 < l_tot) {

      for (i2 = i1; i2 < l_tot && options[i2] != ' '; i2++);
      l_opt = i2 - i1;

      if ((l_opt > 6) && (strncmp(options + i1, "names=", 6) == 0)) {
        if ((l_opt == 6+7) && (strncmp(options + i1 + 6, "ensight", 7) == 0))
          w->ensight_names = true;
        else
          w->ensight_names = false;
      }

      for (i1 = i2 + 1; i1 < l_tot && options[i1] == ' '; i1++);

    }

  }

  // Parallel parameters

#if defined(HAVE_MPI)

  // Check we always use the same communicator

  if (comm != _comm) {
    if (comm != MPI_COMM_NULL && _comm != MPI_COMM_NULL)
      bft_error(__FILE__, __LINE__, 0,
                _("All Catalyst writers must use the same MPI communicator"));
    else
      _comm = comm;
  }

#endif

  // Prepare initialization of Catalyst

  if (_root_node == nullptr)
    _root_node = conduit_node_create();

  if (_n_scripts < 1)
    _add_dir_scripts(".");

  if (_n_scripts > 0)
    _init_catalyst();

  // Initialize state for this writer

  conduit_cpp::Node exec_params = conduit_cpp::cpp_node(_root_node);

  auto state = exec_params["catalyst/state"];
  state["cycle"].set(0);
  state["time"].set(0);
  state["multiblock"].set(1);

  // Add channel for this writer

  std::string cpp_name(name);
  w->channel = exec_params["catalyst/channels/" + cpp_name];

  w->channel["type"].set("multimesh");

  w->modified = true;

  _n_writers += 1;

  return w;
}

/*----------------------------------------------------------------------------
 * Finalize FVM to Catalyst-2 object writer.
 *
 * parameters:
 *   this_writer_p <-- pointer to opaque writer structure.
 *
 * returns:
 *   null pointer
 *----------------------------------------------------------------------------*/

void *
fvm_to_catalyst2_finalize_writer(void  *this_writer_p)
{
  fvm_to_catalyst_t  *w = (fvm_to_catalyst_t *)this_writer_p;

  assert(w != nullptr);

  /* Write output if not done already */

  fvm_to_catalyst2_flush(this_writer_p);

  /* Free structures */

  CS_FREE(w->name);

  /* Free grid and field structures
     (reference counters should go to 0) */

  _n_writers -= 1;

  _finalize_catalyst();

  /* Free fvm_to_catalyst_t structure */

  CS_FREE(w);

  return nullptr;
}

/*----------------------------------------------------------------------------
 * Associate new time step with an Catalyst-2 geometry.
 *
 * parameters:
 *   this_writer_p <-- pointer to associated writer
 *   time_step     <-- time step number
 *   time_value    <-- time_value number
 *----------------------------------------------------------------------------*/

void
fvm_to_catalyst2_set_mesh_time(void    *this_writer_p,
                               int      time_step,
                               double   time_value)
{
  fvm_to_catalyst_t  *w = (fvm_to_catalyst_t *)this_writer_p;

  int _time_step = (time_step > -1) ? time_step : 0;
  double _time_value = (time_value > 0.0) ? time_value : 0.0;

  if (_time_step > w->time_step) {
    w->time_step = _time_step;
    assert(time_value >= w->time_value);
    w->time_value = _time_value;
  }
}

/*----------------------------------------------------------------------------
 * Write nodal mesh to a Catalyst-2 object
 *
 * parameters:
 *   this_writer_p <-- pointer to associated writer
 *   mesh          <-- pointer to nodal mesh structure that should be written
 *----------------------------------------------------------------------------*/

void
fvm_to_catalyst2_export_nodal(void               *this_writer_p,
                              const fvm_nodal_t  *mesh)
{
  fvm_to_catalyst_t  *w = (fvm_to_catalyst_t *)this_writer_p;

  _export_conduit_mesh(w, mesh);
}

/*----------------------------------------------------------------------------
 * Write field associated with a nodal mesh to a Catalyst-2 object.
 *
 * Assigning a negative value to the time step indicates a time-independent
 * field (in which case the time_value argument is unused).
 *
 * parameters:
 *   this_writer_p    <-- pointer to associated writer
 *   mesh             <-- pointer to associated nodal mesh structure
 *   name             <-- variable name
 *   location         <-- variable definition location (nodes or elements)
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
fvm_to_catalyst2_export_field(void                  *this_writer_p,
                              const fvm_nodal_t     *mesh,
                              const char            *name,
                              fvm_writer_var_loc_t   location,
                              int                    dimension,
                              cs_interlace_t         interlace,
                              int                    n_parent_lists,
                              const cs_lnum_t        parent_num_shift[],
                              cs_datatype_t          datatype,
                              int                    time_step,
                              double                 time_value,
                              const void      *const field_values[])
{
  std::string cpp_mesh_name(mesh->name);

  fvm_to_catalyst_t *w = (fvm_to_catalyst_t *)this_writer_p;

  /* Initialization */
  /*----------------*/

  // Create (or access) associated mesh node
  auto mesh_node = w->channel["data/" + cpp_mesh_name];

  if (mesh_node.number_of_children() == 0)
    fvm_to_catalyst2_export_nodal(w, mesh);

  char _name[128];

  strncpy(_name, name, 127);
  _name[127] = '\0';
  if (w->ensight_names) {
    for (int i = 0; i < 127 && _name[i] != '\0'; i++) {
      switch (_name[i]) {
      case '(':
      case ')':
      case ']':
      case '[':
      case '+':
      case '-':
      case '@':
      case ' ':
      case '\t':
      case '!':
      case '#':
      case '*':
      case '^':
      case '$':
      case '/':
        _name[i] = '_';
        break;
      default:
        break;
      }
      if (_name[i] == ' ')
        _name[i] = '_';
    }
  }

  std::string cpp_field_name(_name);

  /* Per node variable */
  /*-------------------*/

  if (location == FVM_WRITER_PER_NODE) {
    _export_field_values(w,
                         mesh,
                         cpp_mesh_name,
                         cpp_field_name,
                         0, // elt_dim
                         dimension,
                         interlace,
                         n_parent_lists,
                         parent_num_shift,
                         datatype,
                         field_values);
  }

  /* Per element variable */
  /*----------------------*/

  else if (location == FVM_WRITER_PER_ELEMENT) {
    _export_field_values(w,
                         mesh,
                         cpp_mesh_name,
                         cpp_field_name,
                         fvm_nodal_get_max_entity_dim(mesh), // elt_dim
                         dimension,
                         interlace,
                         n_parent_lists,
                         parent_num_shift,
                         datatype,
                         field_values);
  }

  /* Update field status */
  /*---------------------*/

  fvm_to_catalyst2_set_mesh_time(w, time_step, time_value);

  mesh_node["state/cycle"].set(w->time_step);
  mesh_node["state/time"].set(w->time_value);

  w->modified = true;
}

/*----------------------------------------------------------------------------
 * Flush files associated with a given writer.
 *
 * In this case, the effective call to coprocessing is done.
 *
 * parameters:
 *   this_writer_p    <-- pointer to associated writer
 *----------------------------------------------------------------------------*/

void
fvm_to_catalyst2_flush(void  *this_writer_p)
{
  fvm_to_catalyst_t  *w = (fvm_to_catalyst_t *)this_writer_p;

  if (_catalyst_initialized == false) {
    _init_catalyst();
  }

  const conduit_node *params = _root_node;

  conduit_cpp::Node exec_params = conduit_cpp::cpp_node(_root_node);

  auto state = exec_params["catalyst/state"];
  state["cycle"].set(w->time_step);
  state["time"].set(w->time_value);

  if (_debug_print) {
    w->channel["data"].print();
  }

  catalyst_status err = catalyst_execute(params);
  if (err != catalyst_status_ok) {
    bft_error(__FILE__, __LINE__, 0,
              _("catalyst_execute error: %d (%s)"),
              (int)err, _catalyst_status_to_string(err));
  }

  w->modified = false;
}

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* HAVE_CATALYST */
