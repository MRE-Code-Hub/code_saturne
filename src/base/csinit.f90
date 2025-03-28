!-------------------------------------------------------------------------------

! This file is part of code_saturne, a general-purpose CFD tool.
!
! Copyright (C) 1998-2024 EDF S.A.
!
! This program is free software; you can redistribute it and/or modify it under
! the terms of the GNU General Public License as published by the Free Software
! Foundation; either version 2 of the License, or (at your option) any later
! version.
!
! This program is distributed in the hope that it will be useful, but WITHOUT
! ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
! FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
! details.
!
! You should have received a copy of the GNU General Public License along with
! this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
! Street, Fifth Floor, Boston, MA 02110-1301, USA.

!-------------------------------------------------------------------------------

subroutine csinit(irgpar, nrgpar)  &
 bind(C, name='cs_f_init')

!===============================================================================
!  Purpose:
!  --------

!  Initialize parallel paramters

!-------------------------------------------------------------------------------
! Arguments
!__________________.____._____.________________________________________________.
! name             !type!mode ! role                                           !
!__________________!____!_____!________________________________________________!
! irgpar           ! e  ! <-- ! rank if parallel; -1 if sequantial             !
! nrgpar           ! e  ! <-- ! number of MPI ranks: 1 if sequantial           !
!__________________!____!_____!________________________________________________!

!     Type: i (integer), r (real), s (string), a (array), l (logical),
!           and composite types (ex: ra real array)
!     mode: <-- input, --> output, <-> modifies data, --- work array
!===============================================================================

!===============================================================================
! Module files
!===============================================================================

use parall

!===============================================================================

use, intrinsic :: iso_c_binding

implicit none

integer(c_int), value :: irgpar, nrgpar

!===============================================================================

irangp = irgpar
nrangp = nrgpar

!----
! End
!----

return
end subroutine csinit
