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

SUBDIRS = . \
bft fvm gui \
base cdo pprt alge mesh turb \
atmo cfbl cogz comb ctwr elec gwf lagr rayt \
user user_examples meg apps

# General flags

AM_FCFLAGS = \
$(FCMODINCLUDE)$(top_builddir)/src \
$(FCFLAGS_DBG) $(FCFLAGS_OPT) $(FCFLAGS_SHARED)

AM_LDFLAGS = -no-undefined

# Library source files

noinst_LIBRARIES = libcsmodules.a

# Public header files (to be installed)

pkginclude_HEADERS = \
cs_headers.h

libcsmodules_a_SOURCES = \
base/paramx.f90 \
base/mesh.f90 \
base/field.f90 \
base/cstphy.f90 \
base/cstnum.f90 \
base/dimens.f90 \
base/entsor.f90 \
base/numvar.f90 \
base/optcal.f90 \
base/parall.f90 \
base/pointe.f90 \
base/cs_c_bindings.f90 \
atmo/atincl.f90 \
atmo/atchem.f90 \
atmo/atimbr.f90 \
atmo/sshaerosol.f90 \
pprt/ppincl.f90

# Module dependencies (handled manually, as Automake does
# not yet handle Fortran module dependencies)

base/cs_c_bindings.$(OBJEXT): base/paramx.$(OBJEXT) base/entsor.$(OBJEXT) \
 base/mesh.$(OBJEXT)  base/field.$(OBJEXT)

base/numvar.$(OBJEXT): base/paramx.$(OBJEXT)

base/optcal.$(OBJEXT): base/cs_c_bindings.$(OBJEXT)

atmo/atincl.$(OBJEXT): pprt/ppincl.$(OBJEXT) \
 base/mesh.$(OBJEXT) base/cs_c_bindings.$(OBJEXT) \
 atmo/atchem.$(OBJEXT) atmo/sshaerosol.$(OBJEXT)

atmo/atchem.$(OBJEXT): base/mesh.$(OBJEXT) \
 base/entsor.$(OBJEXT) base/numvar.$(OBJEXT) \
 base/cs_c_bindings.$(OBJEXT)

atmo/atimbr.$(OBJEXT): base/cstphy.$(OBJEXT) base/entsor.$(OBJEXT) \
 atmo/atincl.$(OBJEXT)

atmo/sshaerosol.$(OBJEXT): base/cs_c_bindings.$(OBJEXT) \
 atmo/atchem.$(OBJEXT) base/mesh.$(OBJEXT)

# Additional local targets

clean-local:
	-rm -f *__genmod.f90 *.mod

install-exec-local: $(libcsmodules_a_SOURCES)
	@$(NORMAL_INSTALL)
	test -z "$(pkgincludedir)" || $(MKDIR_P) "$(DESTDIR)$(pkgincludedir)"
	test -h $(DESTDIR)$(pkgincludedir)/cs_defs.h || ln -s base/cs_defs.h $(DESTDIR)$(pkgincludedir)/cs_defs.h;
	@list='$(libcsmodules_a_SOURCES)'; test -n "$(pkgincludedir)" || list=; \
	modlist=`for p in $$list; do echo $$p; done | $(SED) -e 's|.f90|.$(FCMODEXT)|g'`; \
	files=`for p in $$modlist; do echo $$p; done | $(SED) -e 's|^.*/||'`; \
	test -n "$$files" || exit 0; \
	for p in $$files; do \
	  if test ! -f $$p; then \
	    p=`echo $$p | sed -e 's/.$(FCMODEXT)//' -e 's/\(^.*\)/\U\1/'`.$(FCMODEXT); \
	  fi; \
	  echo " $(INSTALL_HEADER) $$p '$(DESTDIR)$(pkgincludedir)'"; \
	  $(INSTALL_HEADER) $$p "$(DESTDIR)$(pkgincludedir)" || exit $$?; \
	done

uninstall-local:
	@$(NORMAL_UNINSTALL)
	rm -f $(DESTDIR)$(pkgincludedir)/cs_defs.h;
	@list='$(libcsmodules_a_SOURCES)'; test -n "$(pkgincludedir)" || list=; \
	modlist=`for p in $$list; do echo $$p; done | $(SED) -e 's|.f90|.$(FCMODEXT)|g'`; \
	lcfiles=`for p in $$modlist; do echo $$p; done | $(SED) -e 's|^.*/||'`; \
	test -d "$(DESTDIR)$(pkgincludedir)" || exit 0; \
	test -n "$$lcfiles" || exit 0; \
	echo " ( cd '$(DESTDIR)$(pkgincludedir)' )"; \
	files=""; \
	for p in $$lcfiles; do \
	  if test ! -f $$p; then \
	    p=`echo $$p | sed -e 's/.$(FCMODEXT)//' -e 's/\(^.*\)/\U\1/'`.$(FCMODEXT); \
	  fi; \
	  files="$$files $$p"; \
	done; \
	echo " ( cd '$(DESTDIR)$(pkgincludedir)' && rm -f" $$files ")"; \
	cd "$(DESTDIR)$(pkgincludedir)" && rm -f $$files
	rmdir $(pkgincludedir) || exit 0
