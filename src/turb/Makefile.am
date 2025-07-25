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

AM_CFLAGS = $(CFLAGS_DBG) $(CFLAGS_OPT) $(CFLAGS_SHARED)
AM_CXXFLAGS = $(CXXFLAGS_SYCL) $(CXXFLAGS_DBG) $(CXXFLAGS_OPT) $(CXXFLAGS_SHARED)

AM_LDFLAGS =

# Public header files (to be installed)

csturb_includedir=$(pkgincludedir)/turb
csturb_include_HEADERS = \
cs_turbulence_rotation.h \
cs_les_balance.h \
cs_les_filter.h \
cs_les_inflow.h \
cs_les_mu_t.h \
cs_turbulence_bc.h \
cs_turbulence_inflow.h \
cs_turbulence_init.h \
cs_turbulence_ke.h \
cs_turbulence_kw.h \
cs_turbulence_ml.h \
cs_turbulence_model.h \
cs_turbulence_rij.h \
cs_turbulence_rit.h \
cs_turbulence_sa.h \
cs_turbulence_v2f.h \
cs_turbulence_htles.h \
cs_turbulence_headers.h

# Library source files

noinst_LIBRARIES = libcsturb.a

libcsturb_a_SOURCES = \
cs_les_balance.cpp \
cs_les_filter.cpp \
cs_les_inflow.cpp \
cs_les_mu_t.cpp \
cs_turbulence_bc.cpp \
cs_turbulence_inflow.cpp \
cs_turbulence_init.cpp \
cs_turbulence_ke.cpp \
cs_turbulence_kw.cpp \
cs_turbulence_ml.cpp \
cs_turbulence_model.cpp \
cs_turbulence_rij.cpp \
cs_turbulence_rit.cpp \
cs_turbulence_rotation.cpp \
cs_turbulence_sa.cpp \
cs_turbulence_v2f.cpp \
cs_turbulence_htles.cpp

# Rules for CUDA (not known by Automake)

if HAVE_CUDA_CPP
.cpp.$(OBJEXT):
	$(NVCC) $(DEFS) $(AM_CPPFLAGS) $(NVCCFLAGS) $(NVCCFLAGS_DBG) $(NVCCFLAGS_OPT) $(NVCCFLAGS_CPP) -c -o $@ $<
endif

# Additional rules

clean-local:
	-rm -rf .deps
