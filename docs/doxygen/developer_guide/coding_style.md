<!--
  This file is part of code_saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2024 EDF S.A.

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
-->

\page cs_dg_coding_style Coding style guidelines

[TOC]

Master rule {#sec_prg_master_rule}
===========

**Keep the style consistent !**

This rule should be observed above all others. The coding style in code_saturne
has evolved over the years, but unless you are ready to update a whole
file to a more current style (in which case the other guidelines should be
followed), try to remain consistent with the style in the current file.

For new files, use recently updated or reference examples, such as
`src/base/cs_field.cpp` and  `src/base/cs_field.h`, `src/base/cs_array.cpp`,
and `src/base/cs_array.h` for C++.

If you consider your preferred coding style is better than the one used is
superior in some manner to the one used, suggestions are welcome, and
may lead to evolutions in the current style if the arguments are convincing,
but remember that an inconsistent style is more time-consuming and difficult
to read and understand.

Developers trying to follow a consistent style should expect help and
counseling when they have questions, but those whose time is too precious
to observe the existing code before providing their own or requesting assistance
should understand that this is also true of those whom they expect to read
their code.

General rules
=============

The following general rules are strongly recommended:

- Except for files in which they have a special meaning (such as
  Makefiles), use spaces, not tabs. *Absolutely* avoid tabs in
  Python code. Most importantly, use a decent text editor that does not
  randomly mix spaces and tabs.
  The `sbin/rmb` script can be used to remove trailing white-space and
  replace tabs with spaces, but this may appear to damage indentation when
  it is defined with an odd mix of spaces and tabs, so using a good editor
  in the first place is always preferred.
- 80 characters maximum line length; split lines longer than this to ensure
  readability on small screens, or (more commonly) when reading or comparing
  code side-by-side on wider screens. This rule is less important for LaTeX
  documentation, and may be difficult to follow for some Doxygen or Markdown
  constructs (one could argue that using one line per paragraph and relying
  on line wrapping would actually make revision merging simpler).

For new developments, prefer C++ to C, as the code base will be progressively
migrated to C++ (though C++ features will only be introduced by small
increments)..

Punctuation
-----------

Except when adding additional white space to align similar definitions
or arguments on several lines, standard English punctuation rules should be
followed:

- No white space before a punctuation mark (, ; . ), one white space
  after a punctuation mark.

- Use white space before an opening parenthesis, no white space after an opening
  parenthesis.

- No white space before a closing parenthesis, white-space after a closing
  parenthesis.

C and C++ coding style
======================

The code_saturne coding style inherits from common conventions, with
a few specific additions.

General rules
-------------

The following presentation rules should be followed:

- Indentation step: 2 characters.

- Do not use tabulation characters, do not leave whitespace
  at the end of lines.

- Always use lowercase characters for instructions and identifiers,
  except for enumerations and macros which should be in uppercase.
  - A mix of lowercase and uppercase characters (for example CamelCase,
    often encountered in C++ libraries) is allowed in sections
    dealing specifically with external libraries using such coding styles.

- Header `.h` files should have a mechanism to prevent
  multiple inclusions.

The following coding rules are strongly recommended:

- All macro parameters must be enclosed inside parentheses.

- A function's return type must always be defined.

- Variables should be initialized before use
  (pointers are set to NULL). A good compiler should issue warnings when
  this is not the case, and those warnings must be acted upon;

- when a structure definition is only needed in a single file,
  it is preferred to define it directly in the C source file,
  so as to make as little visible as possible in the matching header file.
  structures only used through pointers may be made opaque in this
  manner, which ensures that their possible future modification should
  not require changes in other parts of the code or have unexpected side-effects.

- When a public function is defined in a C source file, a matching
  header file containing its prototype must be included.

- Usage of global variables must be kept to a minimum, though such
  variables may be useful to maintain state or references to mesh or
  variable structures.
  If a global variable is only needed inside a single file, it should
  be declared `static`. It it is needed in other files, then it must
  instead  be declared `extern`' in the matching header file.

- A `const` type must not be cast into a non-`const` type.

- Every `switch` construct should have a `default`
  clause (which may reduce to `assert(0)` to check code paths in
  debug mode, but at least this much must be ensured).

- A `const` attribute should be used when an array or structure
  is not  modified. Recall that for example `const cs_mesh_t *m`
  means that the contents of mesh structure `m` are not modified
  by the function, while `cs_mesh_t *const m` only means that
  the pointer to `m` is not modified; `const cs_mesh_t *const m`
  means both, but its usage in a function prototype gives no additional
  useful information on the function's side effects than the first form
  `const cs_mesh_t *m`, so that form is preferred, as it
  does not clutter the code.

- When an array is passed to a function, describing it as
  `array[]` is preferred to `*array`, as the array
  nature of the argument is made more visible.

- Where both a macro or an enumerated constant could be used,
  an enumeration is preferred, as values will appear with the
  enumerated value's name under a debugger, while only a macro's
  expanded value will appear. An additional advantage of enumerated
  values is that a compiler may issue a warning when a `switch`
  construct has no `case` for a given enumeration value.

Line spacing
------------

Try to be consistent regarding line spacing. Use 1 separation line to
highlight code blocks, but no more (i.e. avoid 2 successive blank lines).
Make the code symmetric also: when a blank line follows an opening brace
`{`, a similar line should precede the matching closing brace `}`.
When no such line is used after an opening brace, none should be used
either before the closing brace.

Automatic formatting
--------------------

A `.clang-format` configuration file is available in the top-level
source directory, so the [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
tool may be used to obtain an acceptable presentation.
Automated formatting does not exactly follow the above recommendations,
but is as close as possible using the available parameters, so can provide
a good starting point, though it is not activated automatically to allow for
manual fine-tuning.

Language {#sec_prg_lang_c}
--------

ANSI C11 or above is required, so C11-specific constructs are allowed.
 C99 variable-length arrays should be avoided, as it is not
always clear whether they are allocated on the stack or heap, and are
an optional feature only (though we could expect that support for those
constructs will remain available on general-purpose architectures, and
removed only in the embedded space).

For C++, at least C++14 is assumed.

Assertions
----------

Assertions are conditions which must always be verified. Several
expanded macro libraries may be available, but a standard C language
assertion has the following properties:

- It is only compiled in debug mode (and so incur no run-time
  performance penalty in production code, where the `NDEBUG`
  macro is defined).

- When its predicate is not verified, it causes a core dump;
  when running under a debugger, the code is stopped inside the
  assertion, but does not exit, which simplifies debugging.

Assertions are thus very useful to ensure that conditions
which are always expected (and not dependent on program input)
are met. They also make code more readable, in the sense that
it is made clear that conditions checked by an assertion
are always expected, and that not handling other cases is not an
programming error or omission.

If a condition may not be met for some program inputs,
and not just in case of programmer error, a more complete
test and call to an error handler (such as \ref bft_error)
is preferred.

Naming conventions
------------------

The following rules should be followed:

- Identifier names are in lowercase, except for macro or enumeration
  definitions, which are in uppercase; words in an identifier are
  separated by an underscore character (for example,
  `n_elt_groups_`.

- Global identifier names are prefixed by the matching library prefix,
  such as `cs_=` or `CS_`.

- Local (static) variable and function should be prefixed by an underscore
  character.

- Index arrays used with *0* to *n-1* (zero-based) numbering should
  be named using a `idx_` or `index_` prefix or suffix, while
  similar arrays using a *1* to *n* numbering (usually those that were
  also used in prior Fortran code) are named using a `pos_`
  prefix or suffix.

- In a similar manner, element identifiers should in general use
  a *0* to *n-1* (zero-based) numbering and be named using a `id_` prefix
  or suffix, while identifiers using a *1* to *n* numbering (usually those
  that were be also used in Fortran code) are named using a `num_`
  prefix or suffix.

Naming of enumerations
----------------------

The following form is preferred for enumerations:

```{.c}
typedef myclass { CS_MYCLASS_ENUM1,
                  CS_MYCLASS_ENUM2,
                  /* etc. */
                } cs_myclass_t;
```

Naming of structures and associated functions
---------------------------------------------

Macros and enumerations related to `myclass` structures
are prefixed by `CS_MYCLASS_`.

Public functions implementing methods are named
`cs_`*class_method*, while private functions are simply named:
*class_method* and are declared static.

Files containing these functions are named *class*`.c`.

Integer types {#sec_prg_lang_integer_types}
-------------

Several integer types are found in code_saturne:

- `cs_lnum_t` should be used for local entity (i.e. vertex, face,
   cell) numbers or connectivity. It is a signed integer, normally identical
   to `int`, but a larger size could be used in the future for very
   large meshes on shared memory machines.

- `cs_gnum_t` should be used for global entity numbers, usually
   necessary only for I/O aspects. It is an unsigned 32 or 64-bit integer,
   depending on whether the code was configured with the
   `--enable-long-gnum` option. Global numbers should always use
   this type, as for very large meshes, they may exceed the maximum size
   of a 32-bit integer (2 147 483 648). The choice of unsigned integers
   is two-fold: it doubles the range of available values, and good compilers
   will issue warnings when this type is mixed without precaution with
   the usual integer types. These warnings should be heeded, as they may
   avoid many hours of debugging.

- In all other cases, the standard C types `int` and `size_t`
  should be preferred (for example for loops over variables, probes, or
  any entity independent of mesh size.

Base functions and types
------------------------

In the code_saturne kernel, it is preferable to use base functions provided by the
BFT subsystem to the usual C functions, as those logging, exit and error-handling
functions will work correctly when running in parallel, and the memory
management macros ensure return value checking and allow additional logging.

The array below summarizes the replacements for usual functions:

Standard C function         | code_saturne macro or function | Header
----------------------------|--------------------------------|------------------
`exit`                      | \ref cs_exit                   | \ref cs_base.h
`exit` on error             | \ref bft_error                 | \ref bft_error.h
`printf`                    | \ref bft_printf                | \ref bft_printf.h
`printf` (to standard logs) | \ref cs_log_printf             | \ref cs_log.h
`malloc`                    | \ref CS_MALLOC                 | \ref cs_mem.h
` `                         | \ref CS_MALLOC_HD              | \ref cs_base_accel.h
`realloc`                   | \ref CS_REALLOC                | \ref cs_mem.h
` `                         | \ref CS_REALLOC_HD             | \ref cs_base_accel.h
`free`                      | \ref CS_FREE                   | \ref cs_mem.h
` `                         | \ref CS_FREE_HD                | \ref cs_base_accel.h

Internationalization
--------------------

Messages for the user should always be defined in US English in the source
code (which avoids using extended characters and the accompanying
text encoding issues in source code).

To make future internationalization using a mechanism such as
`gettext()` possible, translatable strings should be
encased in a `_( )` macro (actually an abbreviation for a call
to `gettext()` if available, which reverts to an empty (identity)
macro is internationalization is unavailable or disabled).
Strings assigned to variables must be encased in a `N_( )`
macro (which is an ``empty'' macro, used by the `gettext` toolchain
to determine that those strings should appear in the translation dictionary),
and the variable to which such a string is assigned should be
encased in the `_( )` macro where used.

Note that with UTF-8 type character strings, accented or otherwise
extended characters are represented on multiple bytes. The `strlen`
C function will return the string's real size, which may be greater
than the number of output columns it uses. The \ref cs_log_strlen
function may be used to compute the printable width of a character
string, while and \ref cs_log_strpad and \ref cs_log_strpadl may be use
to pad a string.

C++ coding style
================

Most rules applicable to C apply here also. Note that standard C++ stream output
(using `std::cout` ) may be practical for temporary debugging code, but
should not be used instead of `bft_printf` or `cs_log` for production code.

Python coding style
===================

The core python scripts use a coding style similar to PEP-8
[PEP-8](https://www.python.org/dev/peps/pep-0008/).
PEP-257 (docstring conventions) is also recommended.

Other parts of the code tend to use a camelCase naming, but should otherwise
adhere to the same standards. Moving the to PEP-8 style would be ideal,
though to avoid confusion, this should be done in an *atomic* step
for each module.

Documentation
=============

Documentation of the main code is based on the
[Doxygen](https://www.doxygen.nl/index.html/) tool, whose documentation
may be found on its web site.

Additional pages for the documentation may be found in the source tree,
under `docs/doxygen`. Files containing mostly examples may use the `.h` or
`.dox` extension (with `.dox` preferred for easier identification), and
pages which describe general aspects instead of code are preferably
written in Doxygen Markdown (`.md` extension), as this allows better
readability and interoperation with some editors (such as preview,
syntax highlighting, ...).

When building the code, remember to often use `make html` and check the
`docs/doxygen/doxygen_warn.log` file in the build directory for errors
and warnings.

When modifying arguments to a function or modifying structures, make sure
the special Doxygen comments are kept up to date. Using Doxygen
comments in the C code and simplified comments in the headers
(see \ref cs_medcoupling_postprocess.cxx and \ref cs_medcoupling_postprocess.h for example)
is recommended, but as this
adds to the coding effort, duplicating the headers from the reference
C code to the headers is not recommended since recent versions of Doxygen complain
about this, so avoiding duplicates is the preferred way.
For inlined functions or structures the documentation should be available in the
.h files since that is where the function or structure are defined.

For functions the following structure is advised (example extracted from
\ref cs_medcoupling_postprocess.cxx):
```
cs_real_t
cs_medcoupling_slice_scalar_mean_weighted
(
  const char        *name,     /*!<[in] name of the slice */
  const cs_real_t   *scalar,   /*!<[in] array of scalar values (size n_cells) */
  const cs_real_t   *weight_s, /*!<[in] scalar weight array (n_cells) */
  const cs_real_3_t *weight_v  /*!<[in] vectorial weight array (n_cells) */
)
{
  <Function code>
}
```

Private functions or structures should not appear in the documentation
(though their arguments should be documented in the source code),
so in most source files,
```
/*! \cond DOXYGEN_SHOULD_SKIP_THIS */
```
is used to mark the beginning of a section which should be ignored
by Doxygen, and
```
/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */
```
used to mark the end of that section. In most cases, this includes private
structures and functions in C code, but could be extended to public
function definitions if the `.h` file header already contains the same
Doxygen-formated comments.
