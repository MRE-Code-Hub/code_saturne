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

!> \file parall.f90
!> \brief Module for basic MPI and OpenMP parallelism-related values

module parall

  !=============================================================================

  implicit none

  !=============================================================================

  !> \defgroup parall Module for basic MPI and OpenMP parallelism-related values

  !> \addtogroup parall
  !> \{

  !> process rank
  !> - -1 in sequential mode
  !> - r (0 < r < n_processes) in distributed parallel run
  integer, save ::  irangp = -1

  !> number of processes (=1 if sequental)
  integer, save ::  nrangp = 1

  !> \}

  !=============================================================================

  interface

    !---------------------------------------------------------------------------

    !> \brief Compute the global sum of a real number in case of parellism.

    !> \param[in, out]   sum  local sum in, global sum out

    subroutine parsom(sum)  &
      bind(C, name='cs_f_parall_sum_r')
      use, intrinsic :: iso_c_binding
      implicit none
      real(c_double), intent(inout) :: sum
    end subroutine parsom

    !---------------------------------------------------------------------------

  end interface

end module parall


