## Process this file with automake to produce Makefile.in

#-------------------------------------------------------------------------------

# This file is part of code_saturne, a general-purpose CFD tool.
#
# Copyright (C) 1998-2024 EDF S.A.
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

EXTRA_DIST = Doxyfile-pre Doxyfile-src Doxyfile-ui \
DoxygenLayout.xml cs_dox_header.html cs_dox_style.css cs_math_macros.tex \
cs_mathjax_macros.js doxypypy.py field_names.h function_defaults.h keywords.h \
mainpage.dox cs_external_links.dox\
images examples \
user_guide developer_guide

# Overload Doxygen executable

if HAVE_DOXYGEN

DOXYGENCMD = export TEXINPUTS="$(abs_top_srcdir)/docs/style/$(cs_tex_path_end)"; \
  srcdir="$(top_srcdir)" builddir="$(top_builddir)" ple_docdir="$(ple_docdir)" \
  PLE_DOX_SECTION="$(ple_dox_tag)" NEPTUNE_CFD="$(NEPTUNE_CFD)" \
  package_name="$(PACKAGE_NAME)" package_version="$(cs_version_short)" \
  have_dot="$(cs_have_dot)" use_mathjax="$(cs_enable_mathjax)" \
  "$(DOXYGEN)"

doxygen-pre:
	$(DOXYGENCMD) $(top_srcdir)/docs/doxygen/Doxyfile-pre

doxygen-src:
	if test "$(ple_type)" = "internal" ; then \
	  cp $(top_builddir)/libple/doc/doxygen/html/ple.tag . ;\
	else \
	  cp $(ple_docdir)/doxygen/ple.tag . ;\
	fi ;\
	if test "$(NEPTUNE_CFD)" = "neptune_cfd" ; then \
	  cp $(top_builddir)/neptune_cfd/docs/doxygen/neptune.tag . ;\
	fi ;\
	$(DOXYGENCMD) $(top_srcdir)/docs/doxygen/Doxyfile-src

doxygen-ui:
	$(DOXYGENCMD) $(top_srcdir)/docs/doxygen/Doxyfile-ui

doxygen: doxygen-src

html-local: doxygen

clean-local:
	for d in pre src ui ;\
	do \
	  if test -d $$d ;\
	  then \
	    (cd $$d && rm -rf html latex man index.bt) ;\
          fi ;\
	done ;\
	rm -f *.tag

endif

distclean-local:
	-rm -rf Makefile

# Generate the documentation at dist stage so that the user do not need
# to have Doxygen (and mandatory tools).

if HAVE_DOXYGEN

dist-hook: html
	\cp -R src/html $(distdir)/src

endif

install-data-local:
	if test -d $(srcdir)/src; then \
	  $(MKDIR_P) "$(DESTDIR)$(htmldir)/doxygen/src"; \
	  \cp -R $(srcdir)/src $(DESTDIR)$(htmldir)/doxygen/ ;\
	fi

install-html-local:
	$(MKDIR_P) "$(DESTDIR)$(htmldir)/doxygen"
	if test -d pre/html; \
	then \
		$(MKDIR_P) "$(DESTDIR)$(htmldir)/doxygen/pre"; \
	  \cp -R pre/html/* $(DESTDIR)$(htmldir)/doxygen/pre/ ;\
	fi ;\
	if test -d ui/html; \
	then \
		$(MKDIR_P) "$(DESTDIR)$(htmldir)/doxygen/ui"; \
	  \cp -R ui/html/*  $(DESTDIR)$(htmldir)/doxygen/ui/ ;\
	fi ;\
	$(MKDIR_P) "$(DESTDIR)$(htmldir)/doxygen/src"; \
	\cp -R src/html/* $(DESTDIR)$(htmldir)/doxygen/src/

uninstall-local:
	-rm -rf $(htmldir)/doxygen
