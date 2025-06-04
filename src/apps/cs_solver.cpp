/*============================================================================
 * Main program
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

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "bft/bft_mem.h"
#include "bft/bft_printf.h"

#include "base/cs_ale.h"
#include "base/cs_all_to_all.h"
#include "base/cs_ast_coupling.h"
#include "atmo/cs_atmo.h"
#include "alge/cs_balance.h"
#include "base/cs_base.h"
#include "base/cs_base_fortran.h"
#include "alge/cs_benchmark.h"
#include "base/cs_boundary_conditions.h"
#include "base/cs_boundary_zone.h"
#include "base/cs_calcium.h"
#include "cdo/cs_cdo_main.h"
#include "alge/cs_cell_to_vertex.h"
#include "base/cs_control.h"
#include "base/cs_coupling.h"
#include "ctwr/cs_ctwr.h"
#include "cdo/cs_domain_setup.h"
#include "base/cs_ext_library_info.h"
#include "base/cs_fan.h"
#include "base/cs_field.h"
#include "base/cs_field_pointer.h"
#include "base/cs_file.h"
#include "base/cs_fp_exception.h"
#include "base/cs_function.h"
#include "alge/cs_gradient.h"
#include "gui/cs_gui.h"
#include "gui/cs_gui_boundary_conditions.h"
#include "gui/cs_gui_conjugate_heat_transfer.h"
#include "gui/cs_gui_mesh.h"
#include "gui/cs_gui_mobile_mesh.h"
#include "gui/cs_gui_output.h"
#include "gui/cs_gui_particles.h"
#include "gui/cs_gui_radiative_transfer.h"
#include "gui/cs_gui_util.h"
#include "base/cs_ibm.h"
#include "base/cs_io.h"
#include "mesh/cs_join.h"
#include "turb/cs_les_inflow.h"
#include "base/cs_log.h"
#include "base/cs_log_iteration.h"
#include "base/cs_log_setup.h"
#include "alge/cs_matrix_default.h"
#include "mesh/cs_mesh.h"
#include "mesh/cs_mesh_adjacencies.h"
#include "mesh/cs_mesh_bad_cells.h"
#include "mesh/cs_mesh_coherency.h"
#include "mesh/cs_mesh_location.h"
#include "mesh/cs_mesh_quality.h"
#include "mesh/cs_mesh_quantities.h"
#include "mesh/cs_mesh_smoother.h"
#include "base/cs_notebook.h"
#include "base/cs_opts.h"
#include "base/cs_parall.h"
#include "cdo/cs_param_cdo.h"
#include "base/cs_paramedmem_coupling.h"
#include "base/cs_parameters.h"
#include "base/cs_physical_properties.h"
#include "base/cs_post.h"
#include "base/cs_post_default.h"
#include "base/cs_preprocess.h"
#include "base/cs_preprocessor_data.h"
#include "base/cs_probe.h"
#include "cdo/cs_property.h"
#include "base/cs_prototypes.h"
#include "base/cs_random.h"
#include "base/cs_restart.h"
#include "base/cs_restart_map.h"
#include "base/cs_runaway_check.h"
#include "base/cs_sat_coupling.h"
#include "base/cs_setup.h"
#include "alge/cs_sles.h"
#include "alge/cs_sles_default.h"
#include "base/cs_syr_coupling.h"
#include "base/cs_sys_coupling.h"
#include "base/cs_system_info.h"
#include "cdo/cs_thermal_system.h"
#include "base/cs_time_moment.h"
#include "base/cs_time_stepping.h"
#include "base/cs_time_table.h"
#include "base/cs_timer.h"
#include "base/cs_timer_stats.h"
#include "base/cs_tree.h"
#include "base/cs_turbomachinery.h"
#include "base/cs_utilities.h"
#include "alge/cs_vertex_to_cell.h"
#include "base/cs_volume_mass_injection.h"
#include "base/cs_volume_zone.h"

#if defined(HAVE_CUDA)
#include "base/cs_base_cuda.h"
#include "alge/cs_blas_cuda.h"
#endif

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro definitions
 *============================================================================*/

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/* Initialize Fortran base common block values */

extern void
cs_f_init(int  irgpar,   /* MPI Rank in parallel, -1 otherwise */
          int  nrgpar);  /* Number of MPI processes, or 1 */

/*============================================================================
 * Static global variables
 *============================================================================*/

static cs_opts_t  opts;

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle first SIGINT by setting the max number of iterations to
 * the current value.
 *
 * \param[in]  signum  signal number.
 */
/*----------------------------------------------------------------------------*/

static void
_sigint_handler(int signum)
{
  cs_time_step_define_nt_max(cs_glob_time_step->nt_cur);

  cs_log_printf(CS_LOG_DEFAULT,
                _("Signal %d received.\n"
                  "--> computation interrupted by environment.\n"
                  "\n"
                  "    maximum time step number set to: %d\n"),
                signum,
                cs_glob_time_step->nt_max);
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Main function for code_saturne run.
 *
 * This function is called by main().
 *----------------------------------------------------------------------------*/

static void
_run(void)
{
  int  check_mask = 0;
  cs_halo_type_t halo_type = CS_HALO_STANDARD;

  cs_base_sigint_handler_set(_sigint_handler);

  /* System information */

#if defined(HAVE_MPI)
  cs_system_info(cs_glob_mpi_comm);
#else
  cs_system_info();
#endif
  cs_ext_library_info();

#if defined(HAVE_CUDA)
  cs_base_cuda_select_default_device();
#endif
#if defined(HAVE_SYCL)
  cs_sycl_select_default_device();
#endif
#if defined(HAVE_OPENMP_TARGET)
  cs_omp_target_select_default_device();
#endif

  if (cs_get_device_id() < 0)
    cs_halo_set_buffer_alloc_mode(CS_ALLOC_HOST);

  cs_timer_stats_initialize();
  cs_timer_stats_define_defaults();

  if (cs_glob_tree == nullptr)
    cs_glob_tree = cs_tree_node_create(nullptr);

  cs_gui_parallel_io();
  if (cs_glob_n_ranks > 1)
    cs_user_parallel_io();
  cs_file_defaults_info();

  cs_gui_mpi_algorithms();

  /* Load user time tables */
  cs_gui_time_tables();
  cs_user_time_table();

  bft_printf("\n");

  cs_base_update_status("initializing\n");

  /* Initialize random number generator
     (used in only some cases, but safe to do, and inexpensive) */

  cs_random_seed(cs_glob_rank_id + 1);

  /* Initialize global structures for main mesh */

  cs_mesh_location_initialize();
  cs_glob_mesh = cs_mesh_create();
  cs_glob_mesh_builder = cs_mesh_builder_create();
  cs_glob_mesh_quantities_g = cs_mesh_quantities_create();
  cs_glob_mesh_quantities = cs_glob_mesh_quantities_g;
  cs_boundary_zone_initialize();
  cs_volume_zone_initialize();

  cs_preprocess_mesh_define();

  cs_turbomachinery_define();

  /* Call main calculation initialization function or help */

  cs_io_log_initialize();

  cs_field_define_keys_base();
  cs_parameters_define_field_keys();

  cs_sles_initialize();
  cs_sles_set_default_verbosity(cs_sles_default_get_verbosity);

  cs_preprocessor_data_read_headers(cs_glob_mesh,
                                    cs_glob_mesh_builder,
                                    false);

  cs_gui_zones();
  cs_user_zones();

  cs_ctwr_define_zones();

  /* Create a new structure for the computational domain */

  cs_glob_domain = cs_domain_create();

  /* Define MPI-based Couplings if applicable */

  cs_gui_syrthes_coupling();
  cs_user_syrthes_coupling();
  cs_user_saturne_coupling();

  /* Initialize Fortran API and calculation setup */

  if ((opts.preprocess | opts.verif) == false && opts.benchmark <= 0) {

    const char default_restart_mesh[] = "restart_mesh_input";
    if (cs_file_isreg(default_restart_mesh))
      cs_restart_map_set_mesh_input(default_restart_mesh);

    cs_f_init(cs_glob_rank_id, cs_glob_n_ranks);

    cs_setup();

    if (cs_parameters_need_extended_neighborhood())
      halo_type = CS_HALO_EXTENDED;

    if (cs_glob_ale != CS_ALE_NONE) {
      cs_restart_map_set_locations(true, true);
      cs_gui_mobile_mesh_get_boundaries(cs_glob_domain);
      if (cs_glob_mesh->time_dep < CS_MESH_TRANSIENT_COORDS)
        cs_glob_mesh->time_dep = CS_MESH_TRANSIENT_COORDS;
    }

    /* Initialization step for the setup of CDO schemes
     * - Perform the initialization for each activated module
     * - Create predefined properties and advection fields
     * - Create fields associated to equations or modules
     * - Create fields associated to user-defined equations
     */

    cs_cdo_initialize_setup(cs_glob_domain);

    /* Setup linear solvers */

    cs_gui_linear_solvers();
    cs_user_linear_solvers();

    cs_timer_stats_set_start_time(cs_glob_time_step->nt_cur);

  }
  else if (opts.verif)
    halo_type = CS_HALO_EXTENDED;

  /* Discover applications visible through MPI (requires communication);
     this is done after main calculation initialization so that the user
     may have the option of assigning a name to this instance. */

#if defined(HAVE_MPI)
  if (cs_param_cdo_has_cdo_only() && cs_thermal_system_is_activated()) {
    const char app_type[] = "Code_Saturne::CDO_THERMAL " CS_APP_VERSION;
    cs_coupling_discover_mpi_apps(opts.app_name, app_type);
  }
  else
    cs_coupling_discover_mpi_apps(opts.app_name, nullptr);
#endif

  if (opts.app_name != nullptr)
    BFT_FREE(opts.app_name);

  /* Initialize couplings and communication if necessary */

  cs_syr_coupling_all_init();
  cs_sat_coupling_all_init();
  cs_sys_coupling_all_init();

  cs_paramedmem_coupling_all_init();

  /* Initialize main post-processing */

  cs_gui_postprocess_writers();
  cs_user_postprocess_writers();
  cs_post_init_writers();

  cs_gui_postprocess_meshes();
  cs_user_postprocess_meshes();
  cs_user_postprocess_probes();

  /* Print info on fields and associated keys and other setup options */

  if (opts.verif == false && opts.preprocess == false && opts.benchmark <= 0)
    cs_log_setup();

  /* Preprocess mesh */

  cs_preprocess_mesh(halo_type);
  cs_mesh_adjacencies_initialize();

  /* Initialization for turbomachinery computations */

  cs_turbomachinery_initialize();

  /* Initialization of internal coupling */

  cs_internal_coupling_initialize();

  cs_internal_coupling_dump();

  /* Initialize meshes for the main post-processing */

  check_mask = ((opts.preprocess | opts.verif) == true) ? 2 + 1 : 0;

  cs_post_init_meshes(check_mask);

  /* Compute iterations or quality criteria depending on verification options */

  if (opts.verif == true) {
    bft_printf(_("\n Computing quality criteria\n"));
    cs_mesh_quality(cs_glob_mesh, cs_glob_mesh_quantities);
    cs_mesh_coherency_check();
    cs_mesh_bad_cells_postprocess(cs_glob_mesh, cs_glob_mesh_quantities);
  }
  else if (opts.preprocess == true)
    cs_mesh_coherency_check();

  if (check_mask && cs_syr_coupling_n_couplings())
    bft_error(__FILE__, __LINE__, 0,
              _("Coupling with SYRTHES is not possible in mesh preprocessing\n"
                "or verification mode."));

  if (opts.preprocess == false) {

    cs_mesh_adjacencies_update_mesh();

#if defined(HAVE_ACCEL)
    cs_preprocess_mesh_update_device();
#endif

  }

  if (opts.benchmark > 0) {
    int mpi_trace_mode = (opts.benchmark == 2) ? 1 : 0;
    cs_benchmark(mpi_trace_mode);
  }

  if (opts.preprocess == false && opts.benchmark <= 0) {

    /* Check that mesh seems valid */

    cs_mesh_quantities_check_vol(cs_glob_mesh,
                                 cs_glob_mesh_quantities,
                                 (opts.verif ? 1 : 0));

    /* Initialization related to CDO/HHO schemes */

    cs_cdo_initialize_structures(cs_glob_domain,
                                 cs_glob_mesh,
                                 cs_glob_mesh_quantities);

    /* Initialize balance and gradient timers and computation */

    cs_gradient_initialize();
    cs_balance_initialize();

    if (opts.verif == false) {

      /* Initialize sparse linear systems resolution */

      cs_user_matrix_tuning();

      cs_matrix_initialize();

      /* Update Fortran mesh sizes and quantities */

      cs_preprocess_mesh_update_fortran();

      /* Choose between standard and user solver */

      if (cs_user_solver_set() == 0) {
        if (cs_param_cdo_has_cdo_only()) {
          /* CHT coupling */
          cs_syr_coupling_init_meshes();

          /*----------------------------------------------
           * Call main calculation function (CDO Kernel)
           *----------------------------------------------*/

          cs_cdo_main(cs_glob_domain);
        }
        else {

          /* Additional initializations required by some models */

          cs_fan_build_all(cs_glob_mesh, cs_glob_mesh_quantities);

          cs_ctwr_build_all();

          cs_volume_mass_injection_flag_zones();

          /* Setup couplings and fixed-mesh postprocessing */

          cs_syr_coupling_init_meshes();

          cs_paramedmem_coupling_define_mesh_fields();

          cs_post_default_write_meshes();

          cs_turbomachinery_restart_mesh();

          /*----------------------------------------------
           * Call main calculation function (code Kernel)
           *----------------------------------------------*/

          /* Maybe should be allocate in cs_time_stepping.c */
          cs_ale_allocate();

          cs_time_stepping();

        }

      }
      else {

          /*--------------------------------
           * Call user calculation function
           *--------------------------------*/

          cs_user_solver(cs_glob_mesh,
                         cs_glob_mesh_quantities);

      }

      /* Finalize user extra operations */

      cs_user_extra_operations_finalize(cs_glob_domain);

    }

    /* Finalize gradient computation */

    cs_gradient_finalize();

    /* Finalize synthetic inlet condition generation */

    cs_les_inflow_finalize();

  }

  /* Finalize linear system resolution */

  cs_sles_default_finalize();

  /* Finalize matrix API */

  cs_matrix_finalize();

#if defined(HAVE_CUDA)
  cs_blas_cuda_finalize();
#endif

  bft_printf(_("\n Destroying structures and ending computation\n"));
  bft_printf_flush();

  /* Free the boundary conditions face type and face zone arrays */

  cs_boundary_conditions_free();

  /* Final stage for CDO/HHO/MAC schemes */

  cs_cdo_finalize(cs_glob_domain);

  /* Free cs_domain_structure */

  cs_domain_free(&cs_glob_domain);

  /* Free coupling-related data */

#if defined(HAVE_MPI)
  cs_ast_coupling_finalize();
  cs_syr_coupling_all_finalize();
  cs_sat_coupling_all_finalize();
  cs_sys_coupling_all_finalize();
  cs_paramedmem_coupling_all_finalize();
  cs_coupling_finalize();
#endif

  cs_control_finalize();

  /* Free remapping/intersector related structures (stl or medcoupling) */

  cs_utilities_destroy_all_remapping();

  /* Free the checkpoint multiwriter structure */

  cs_restart_multiwriters_destroy_all();

  /* Print some mesh statistics */

  cs_gui_usage_log();
  cs_mesh_selector_stats(cs_glob_mesh);

  /* Finalizations related to some models */

  cs_atmo_finalize();
  cs_ctwr_all_destroy();
  cs_fan_destroy_all();

  /* Free internal coupling */

  cs_internal_coupling_finalize();

  /* Free memory related to properties */

  cs_property_destroy_all();
  cs_thermal_table_finalize();

  /* Free immersed boundaries related structures */

  cs_ibm_finalize();

  /* Free turbomachinery related structures */

  cs_turbomachinery_finalize();
  cs_join_finalize();

  /* Free post processing or logging related structures */

  cs_probe_finalize();
  cs_post_finalize();
  cs_log_iteration_destroy_all();

  cs_function_destroy_all();

  /* Free moments info */

  cs_time_moment_destroy_all();

  /* Free field info */

  cs_gui_radiative_transfers_finalize();
  cs_gui_finalize();

  cs_notebook_destroy_all();
  cs_time_table_destroy_all();

  cs_field_pointer_destroy_all();
  cs_field_destroy_all();
  cs_field_destroy_all_keys();

  /* Free Physical model related structures
     TODO: extend cs_base_atexit_set mechanism to allow registering
     model-specific cleanup functions at their activation point,
     avoiding the need for modification of this function, which
     should be more "generic". */

  cs_base_finalize_sequence();

  /* Free main mesh after printing some statistics */

  cs_cell_to_vertex_free();
  cs_vertex_to_cell_free();
  cs_mesh_adjacencies_finalize();

  cs_boundary_zone_finalize();
  cs_volume_zone_finalize();
  cs_mesh_location_finalize();
  cs_mesh_quantities_destroy(cs_glob_mesh_quantities_g);

  cs_mesh_destroy(cs_glob_mesh);

  /* Free parameters tree info */

  cs_tree_node_free(&cs_glob_tree);

  /* CPU times and memory management finalization */

  cs_all_to_all_log_finalize();
  cs_io_log_finalize();

  cs_timer_stats_finalize();

  cs_file_free_defaults();

  cs_base_time_summary();

  cs_base_mem_finalize();

  cs_log_printf_flush(CS_LOG_N_TYPES);

  cs_runaway_check_finalize();
}

/*============================================================================
 * Main program
 *============================================================================*/

int
main(int    argc,
     char  *argv[])
{
  /* Initialize wall clock timer */

  (void)cs_timer_wtime();

  /* First analysis of the command line to determine if MPI is required,
     and MPI initialization if it is. */

#if defined(HAVE_MPI)
  cs_base_mpi_init(&argc, &argv);
#endif

#if defined(HAVE_OPENMP) /* Determine default number of OpenMP threads */
  {
    int t_id;
#pragma omp parallel private(t_id)
    {
      t_id = omp_get_thread_num();
      if (t_id == 0)
        cs_glob_n_threads = omp_get_max_threads();
    }
  }

  if (cs_glob_n_threads > 1)
    cs_glob_e2n_sum_type = CS_E2N_SUM_SCATTER;
#endif

  /* Default initialization */

#if defined(_CS_ARCH_Linux)

  if (getenv("LANG") != nullptr)
    setlocale(LC_ALL,"");
  else
    setlocale(LC_ALL, "C");
  setlocale(LC_NUMERIC, "C");

#endif

  /* Trap floating-point exceptions on most systems */

#if defined(DEBUG)
  cs_fp_exception_enable_trap();
#endif

  /* Initialize memory management */

  cs_base_mem_init();

  /* Initialize internationalization */

#if defined(ENABLE_NLS)
  bindtextdomain(PACKAGE, cs_base_get_localedir());
  textdomain(PACKAGE);
#endif

  /* Parse command line */

  cs_opts_define(argc, argv, &opts);

  /* Initialize error handling */

  cs_base_error_init(opts.sig_defaults);

  /* Open 'run_solver.log' (log) files */

  cs_base_trace_set(opts.trace);
  cs_base_fortran_bft_printf_set("run_solver", opts.logrp);

  /* Log-file header and command line arguments recap */

  cs_base_logfile_head(argc, argv);

  /* Load setup parameters if present */

  const char s_param[] = "setup.xml";
  if (cs_file_isreg(s_param)) {
    cs_gui_load_file(s_param);
    cs_notebook_load_from_file();
  }

  /* Call main run() method */

  _run();

  /* Return */

  cs_exit(EXIT_SUCCESS);

  /* Never called, but avoids compiler warning */
  return 0;
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
