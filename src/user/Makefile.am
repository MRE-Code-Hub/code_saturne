## Process this file with automake to produce Makefile.in

#-------------------------------------------------------------------------------

# This file is part of code_saturne, a general-purpose CFD tool.
#
# Copyright (C) 1998-2025 EDF S.A.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.

#-------------------------------------------------------------------------------

# PLE dependency

if HAVE_INTERNAL_PLE
  CPPFLAGS_PLE = -I$(top_srcdir)/libple/src -I$(top_builddir)/libple
else
  CPPFLAGS_PLE = $(PLE_CPPFLAGS)
endif

# Main part

AM_CPPFLAGS = \
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(CPPFLAGS) $(CPPFLAGS_PLE) \
$(MPI_CPPFLAGS)

AM_CXXFLAGS = $(CXXFLAGS_SYCL) $(CXXFLAGS_DBG) $(CXXFLAGS_OPT) $(CXXFLAGS_SHARED)

AM_LDFLAGS =

# Library source files

userfiles = \
cs_user_1d_wall_thermal.cpp \
cs_user_atmo.cpp \
cs_user_boundary_conditions.cpp \
cs_user_coupling.cpp \
cs_user_electric_scaling.cpp \
cs_user_extra_operations.cpp \
cs_user_fluid_structure_interaction.cpp \
cs_user_head_losses.cpp \
cs_user_hgn.cpp \
cs_user_ibm.cpp \
cs_user_ibm_modify.cpp \
cs_user_initialization.cpp \
cs_user_lagr_model.cpp \
cs_user_lagr_particle.cpp \
cs_user_lagr_boundary_conditions.cpp \
cs_user_lagr_volume_conditions.cpp \
cs_user_les_inflow.cpp \
cs_user_mesh.cpp \
cs_user_paramedmem_coupling.cpp \
cs_user_parameters.cpp \
cs_user_performance_tuning.cpp \
cs_user_physical_properties.cpp \
cs_user_postprocess.cpp \
cs_user_porosity.cpp \
cs_user_radiative_transfer.cpp \
cs_user_radiative_transfer_bcs.cpp \
cs_user_solver.cpp \
cs_user_source_terms.cpp \
cs_user_time_table.cpp \
cs_user_turbomachinery.cpp \
cs_user_wall_condensation.cpp \
cs_user_zones.cpp

noinst_LIBRARIES = libcsuser.a
libcsuser_a_SOURCES = $(userfiles)

userdir = $(pkgdatadir)/user_sources/REFERENCE
user_DATA = $(userfiles)

clean-local:
	-rm -rf .deps

if HAVE_CUDA_CPP
.cpp.$(OBJEXT):
	$(NVCC) $(DEFS) $(AM_CPPFLAGS) $(NVCCFLAGS) $(NVCCFLAGS_DBG) $(NVCCFLAGS_OPT) $(NVCCFLAGS_CPP) -c -o $@ $<
endif

# Finish user files installation
# - tag with the package version number
# - remove the test preventing from using a source file
# Done with two sed passes (does not work otherwise)

install-data-hook:
	c_user_files=`ls ${top_srcdir}/src/user/*.c* 2>/dev/null`; \
	for f in $${c_user_files}; do \
	  file=`basename "$${f}"` && \
	  $(PYTHON) "${top_srcdir}/build-aux/cs_user_src_filter.py" "$(cs_version_short)" < "${top_srcdir}/src/user/$${file}" > "$(DESTDIR)$(userdir)/$${file}"; \
	done

