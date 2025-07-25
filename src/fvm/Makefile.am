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

# Main part

AM_CPPFLAGS = \
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(CPPFLAGS) $(MPI_CPPFLAGS)
AM_CXXFLAGS = $(CXXFLAGS_SYCL) $(CXXFLAGS_DBG) $(CXXFLAGS_OPT) $(CXXFLAGS_SHARED)
AM_LDFLAGS = $(CCM_LDFLAGS) $(CGNS_LDFLAGS) $(MED_LDFLAGS) \
$(HDF5_LDFLAGS) $(MPI_LDFLAGS)

# Conditionally compiled extensions

libfvm_cgns_a_CPPFLAGS =\
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(CGNS_CPPFLAGS) $(MPI_CPPFLAGS)
libfvm_med_a_CPPFLAGS = \
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(HDF5_CPPFLAGS) $(MED_CPPFLAGS) $(MPI_CPPFLAGS)

# Public header files (to be installed)

csfvm_includedir=$(pkgincludedir)/fvm
csfvm_include_HEADERS = \
fvm_defs.h \
fvm_group.h \
fvm_headers.h \
fvm_interpolate.h \
fvm_io_num.h \
fvm_neighborhood.h \
fvm_nodal.h \
fvm_nodal_append.h \
fvm_nodal_extrude.h \
fvm_nodal_from_desc.h \
fvm_nodal_order.h \
fvm_nodal_project.h \
fvm_nodal_triangulate.h \
fvm_nodal_extract.h \
fvm_periodicity.h \
fvm_point_location.h \
fvm_selector.h \
fvm_trace.h \
fvm_triangulate.h \
fvm_writer.h

noinst_HEADERS = \
fvm_box.h \
fvm_box_priv.h \
fvm_box_tree.h \
fvm_convert_array.h \
fvm_hilbert.h \
fvm_morton.h \
fvm_nodal_priv.h \
fvm_selector_postfix.h \
fvm_tesselation.h \
fvm_to_ccm.h \
fvm_to_cgns.h \
fvm_to_med.h \
fvm_to_catalyst.h \
fvm_to_ensight.h \
fvm_to_ensight_case.h \
fvm_to_histogram.h \
fvm_to_medcoupling.h \
fvm_to_melissa.h \
fvm_to_vtk_histogram.h \
fvm_to_plot.h \
fvm_to_time_plot.h \
fvm_writer_helper.h \
fvm_writer_priv.h

# Library source files

lib_LIBRARIES =
pkglib_LIBRARIES =
noinst_LIBRARIES = libfvm.a libfvm_filters.a
libfvm_a_LIBADD =
libfvm_filters_a_LIBADD =

libfvm_a_SOURCES = \
fvm_box.cpp \
fvm_box_tree.cpp \
fvm_convert_array.cpp \
fvm_defs.cpp \
fvm_group.cpp \
fvm_hilbert.cpp \
fvm_interpolate.cpp \
fvm_io_num.cpp \
fvm_morton.cpp \
fvm_neighborhood.cpp \
fvm_nodal.cpp \
fvm_nodal_append.cpp \
fvm_nodal_extrude.cpp \
fvm_nodal_from_desc.cpp \
fvm_nodal_order.cpp \
fvm_nodal_project.cpp \
fvm_nodal_triangulate.cpp \
fvm_nodal_extract.cpp \
fvm_periodicity.cpp \
fvm_point_location.cpp \
fvm_selector.cpp \
fvm_selector_postfix.cpp \
fvm_tesselation.cpp \
fvm_trace.cpp \
fvm_triangulate.cpp

local_libraries_add =

libfvm_filters_a_SOURCES = \
fvm_to_ensight.cpp \
fvm_to_ensight_case.cpp \
fvm_to_histogram.cpp \
fvm_to_plot.cpp \
fvm_to_time_plot.cpp \
fvm_writer.cpp \
fvm_writer_helper.cpp

if HAVE_CCM
noinst_LIBRARIES += libfvm_ccm.a
libfvm_ccm_a_CPPFLAGS =\
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(CFLAGS_DBG) $(CFLAGS_OPT) \
$(CCM_CPPFLAGS) $(MPI_CPPFLAGS)
libfvm_ccm_a_SOURCES = fvm_to_ccm.cpp
libfvm_filters_a_LIBADD += libfvm_ccm_a-fvm_to_ccm.$(OBJEXT)
endif

if HAVE_CGNS
noinst_LIBRARIES += libfvm_cgns.a
libfvm_cgns_a_SOURCES = fvm_to_cgns.cpp
libfvm_filters_a_LIBADD += libfvm_cgns_a-fvm_to_cgns.$(OBJEXT)
endif

if HAVE_MED
noinst_LIBRARIES += libfvm_med.a
libfvm_med_a_SOURCES = fvm_to_med.cpp
libfvm_filters_a_LIBADD += libfvm_med_a-fvm_to_med.$(OBJEXT)
endif

if HAVE_MEDCOUPLING

libfvm_medcoupling_a_CPPFLAGS = \
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(MEDCOUPLING_CPPFLAGS) $(MPI_CPPFLAGS)
libfvm_medcoupling_a_CXXFLAGS = \
$(CXXFLAGS_SYCL) $(CXXFLAGS_DBG) $(CXXFLAGS_OPT) \
$(MEDCOUPLING_CXXFLAGS) $(CXXFLAGS_SHARED)
libfvm_medcoupling_a_SOURCES = fvm_to_medcoupling.cxx
noinst_LIBRARIES += libfvm_medcoupling.a

if HAVE_PLUGIN_MEDCOUPLING

local_libraries_add += fvm_medcoupling.${SHARED_LIB_EXT}
LDADD_MEDCOUPLING = \
$(MEDCOUPLING_LDFLAGS) $(MEDCOUPLING_LIBS) \
$(MEDCOUPLINGRUNPATH)

fvm_medcoupling.${SHARED_LIB_EXT}: libfvm_medcoupling.a
	@echo "Building fvm_medcoupling plugin..."
	rm -f $@ ; \
	$(PYTHON) -B $(top_srcdir)/build-aux/cs_link_library.py --echo --linker ${CS_LD} -o $@ --whole-archive-start libfvm_medcoupling.a --whole-archive-end ${LDFLAGS_SHARED} ${CXXFLAGS_SHARED} ${LDFLAGS} ${LDADD_MEDCOUPLING} $(AM_LIBS)

local_libraries_add += fvm_medcoupling.${SHARED_LIB_EXT}

else

libfvm_filters_a_LIBADD += libfvm_medcoupling_a-fvm_to_medcoupling.$(OBJEXT)

endif

endif

# Catalyst (plugin or linked)

if HAVE_CATALYST

libfvm_catalyst_a_CPPFLAGS = \
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(CATALYST_CPPFLAGS) $(MPI_CPPFLAGS)
libfvm_catalyst_a_CXXFLAGS = \
$(CXXFLAGS_DBG) $(CXXFLAGS_OPT) \
$(CATALYST_CXXFLAGS) $(CXXFLAGS_SHARED)
libfvm_catalyst_a_SOURCES = fvm_to_catalyst.cxx \
                            fvm_to_vtk_histogram.cxx
noinst_LIBRARIES += libfvm_catalyst.a

if HAVE_PLUGIN_CATALYST

LDADD_CATALYST = \
$(CATALYST_LDFLAGS) $(CATALYST_LIBS)

fvm_catalyst.${SHARED_LIB_EXT}: libfvm_catalyst.a
	@echo "Building fvm_catalyst plugin..."
	rm -f $@ ; \
	$(PYTHON) -B $(top_srcdir)/build-aux/cs_link_library.py --echo --linker ${CXX} -o $@ --whole-archive-start libfvm_catalyst.a --whole-archive-end ${LDFLAGS_SHARED} ${CXXFLAGS_SHARED} ${LDFLAGS} ${LDADD_CATALYST} $(AM_LIBS)

local_libraries_add += fvm_catalyst.${SHARED_LIB_EXT}

else

libfvm_filters_a_LIBADD += \
libfvm_catalyst_a-fvm_to_catalyst.$(OBJEXT) \
libfvm_catalyst_a-fvm_to_vtk_histogram.$(OBJEXT)

endif

endif

# Melissa (plugin or linked)

if HAVE_MELISSA

libfvm_melissa_a_CPPFLAGS = \
-I$(top_srcdir)/src -I$(top_builddir)/src \
$(MELISSA_CPPFLAGS) $(MPI_CPPFLAGS)
libfvm_melissa_a_CXXFLAGS = \
$(CXXFLAGS_DBG) $(CXXFLAGS_OPT) \
$(MELISSA_CXXFLAGS) $(CXXFLAGS_SHARED)
libfvm_melissa_a_SOURCES = fvm_to_melissa.cpp
noinst_LIBRARIES += libfvm_melissa.a

if HAVE_PLUGIN_MELISSA

LDADD_MELISSA = \
$(MELISSA_LDFLAGS) $(MELISSA_LIBS) \
$(MELISSARUNPATH)

fvm_melissa.${SHARED_LIB_EXT}: libfvm_melissa.a
	@echo "Building fvm_melissa plugin..."
	rm -f $@ ; \
	$(PYTHON) -B $(top_srcdir)/build-aux/cs_link_library.py --echo --linker ${CS_LD} --std-search-paths='no' -o $@ --whole-archive-start libfvm_melissa.a --whole-archive-end ${LDFLAGS_SHARED} ${LDFLAGS} ${LDADD_MELISSA} $(AM_LIBS)

local_libraries_add += fvm_melissa.${SHARED_LIB_EXT}

else

libfvm_filters_a_LIBADD += libfvm_melissa_a-fvm_to_melissa.$(OBJEXT)

endif

endif

# Rules for CUDA (not known by Automake)

if HAVE_CUDA_CPP
.cpp.$(OBJEXT):
	$(NVCC) $(DEFS) $(AM_CPPFLAGS) $(NVCCFLAGS) $(NVCCFLAGS_DBG) $(NVCCFLAGS_OPT) $(NVCCFLAGS_CPP) -c -o $@ $<
endif

# Additional rules

all-local: ${local_libraries_add}

clean-local:
	@list=; for p in ${local_libraries_add}; do \
	  if test -f $$p; then \
	    echo "rm -f $$p"; \
	    rm -f $$p; \
	  else :; fi; \
	done
	-rm -rf .deps

# Remark:
# When installing shared libraries, remove library then copy it, rather than
# simply overwriting it, so that if a running instance is using that library,
# it will still access the old copy (not really removed until closed based
# on POSIX file semantics), instead of a mofified copy that can lead to
# SIGBUS errors due to an incomplete (in progress) copy or possibly other
# address changes.

install-exec-local: ${local_libraries_add}
	@list=; for p in ${local_libraries_add}; do \
	  if test -f $$p; then \
	    list="$$list $$p"; \
	  else :; fi; \
	done; \
	test -z "$$list" -a -n "$(pkglibdir)" || { \
	  echo " $(MKDIR_P) '$(DESTDIR)$(pkglibdir)'"; \
	  $(MKDIR_P) "$(DESTDIR)$(pkglibdir)" || exit 1; \
	  for p in $$list; do \
	    echo " rm -f '$(DESTDIR)$(pkglibdir)/$$p'"; \
	    rm -f "$(DESTDIR)$(pkglibdir)/$$p"; \
	    echo " cp -fp $$p '$(DESTDIR)$(pkglibdir)'"; \
	    cp -fpP $$p "$(DESTDIR)$(pkglibdir)"; \
	  done; \
	  cp -fpP $$list "$(DESTDIR)$(pkglibdir)"; \
	}

uninstall-local:
	@list=; for p in ${local_libraries_add}; do \
	  if test -f $$p; then \
	    list="$$list $$p"; \
	  else :; fi; \
	done; \
	for p in $$list; do \
	  echo " rm -f '$(DESTDIR)$(pkglibdir)/$$p'"; \
	  rm -f "$(DESTDIR)$(pkglibdir)/$$p"; \
	done; \
	rmdir "$(DESTDIR)$(pkglibdir)" || exit 0
