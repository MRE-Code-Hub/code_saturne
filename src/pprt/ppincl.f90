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

!> \file ppincl.f90
!> General module for specific physics

module ppincl

  !===========================================================================

  use, intrinsic :: iso_c_binding

  !=============================================================================

  implicit none

  !===========================================================================

  !> \defgroup ppincl General module for specific physics

  !> \addtogroup ppincl
  !> \{

  !> \defgroup choice Indicator table for specific physics

  !> \addtogroup choice
  !> \{

  !----------------------------------------------------------------------------
  !--> TABLEAU INDICATEURS DU CHOIX DE LA PHYSIQUE PARTICULIERE CHOISIE

  !> number of specific physics
  integer   nmodmx
  parameter(nmodmx = 15)

  !> global indicator for speciphic physics
  !> By default, all the indicators ippmod(i.....) are initialized to -1,
  !> which means that no specific physics is activated.
  !>   - Diffusion flame in the framework of “3 points” rapid complete chemistry:
  !>  indicator ippmod(icod3p)
  !>      - ippmod(icod3p) = 0 adiabatic conditions
  !>      - ippmod(icod3p) = 1 permeatic conditions (enthalpy transport)
  !>      - ippmod(icod3p) =-1 module not activated
  !>
  !>   - Diffusion flames in the framework of steady laminar flamelet approach
  !>  indicator ippmod(islfm)
  !>      - ippmod(islfm) = 0 classic steady laminar flamelet model:
  !>                          adiabatic conditions
  !>      - ippmod(islfm) = 1 classic steady laminar flamelet model:
  !>                          non-adiabatic conditions (enthalpy transport with heat loss)
  !>      - ippmod(islfm) = 2 flamelet/progress variable model:
  !>                          adiabatic conditions
  !>      - ippmod(islfm) = 3 flamelet/progress variable model:
  !>                          non-adiabatic conditions (enthalpy transport with heat loss)
  !>      - ippmod(islfm) =-1 module not activated

  !>   - Eddy Break Up pre-mixed flame: indicator ippmod(icoebu)
  !>      - ippmod(icoebu) = 0 adiabatic conditions at constant richness
  !>      - ippmod(icoebu) = 1 permeatic conditions at constant richness
  !>      - ippmod(icoebu) = 2 adiabatic conditions at variable richness
  !>      - ippmod(icoebu) = 3 permeatic conditions at variable richness
  !>      - ippmod(icoebu) =-1 module not activated
  !>   - Libby-Williams pre-mixed flame: indicator ippmod(icolwc)
  !>      - ippmod(icolwc)=0 two peak model with adiabiatic conditions.
  !>      - ippmod(icolwc)=1 two peak model with permeatic conditions.
  !>      - ippmod(icolwc)=2 three peak model with adiabiatic conditions.
  !>      - ippmod(icolwc)=3 three peak model with permeatic conditions.
  !>      - ippmod(icolwc)=4 four peak model with adiabiatic conditions.
  !>      - ippmod(icolwc)=5 four peak model with permeatic conditions.
  !>      - ippmod(icolwc)=-1 module not activated.
  !>   - Multi-coals and multi-classes pulverised coal combustion: indicator ippmod(iccoal)
  !>
  !>  The number of different coals must be inferior or equal to ncharm = 5.
  !>  The number of particle size classes nclpch(icha) for the coal icha, must be
  !>  inferior or equal to ncpcmx = 20.
  !>      - ippmod(iccoal) = 0 imbalance between the temperature of the continuous and the
  !>        solid phases
  !>      - ippmod(iccoal) = 1 otherwise
  !>      - ippmod(iccoal) =-1 module not activated
  !>   - Electric arcs module (Joule effect and Laplace forces): indicator ippmod(ielarc)
  !>      - ippmod(ielarc) = 1 determination of the magnetic field by means of the Ampere’
  !>   theorem  (not available)
  !>      - ippmod(ielarc) = 2 determination of the magnetic field by means of the vector potential
  !>      - ippmod(ielarc) =-1 module not activated
  !>   - Joule effect module (Laplace forces not taken into account): indicator ippmod(ieljou)
  !>      - ippmod(ieljou) = 1 use of a real potential
  !>      - ippmod(ieljou) = 2 use of a complex potential
  !>      - ippmod(ieljou) = 3 use of real potential and specific boundary conditions
  !>  for transformers.
  !>      - ippmod(ieljou) = 4 use of complex potential and specific boundary conditions
  !>  for transformers.
  !>      - ippmod(ieljou) =-1 module not activated
  !>   - compressible flow module: indicator ippmod(icompf)
  !>      - ippmod(icompf) = 2 module activated: homogeneous two phase model
  !>      - ippmod(icompf) = 1 module activated: single phase model
  !>      - ippmod(icompf) = 0 module activated: single phase barotropic model
  !>      - ippmod(icompf) =-1 module not activated
  !>   - atmospheric flow module: indicator ippmod(iatmos)
  !>      - ippmod(iatmos) =-1 module not activated
  !>      - ippmod(iatmos) = 0 standard modelling
  !>      - ippmod(iatmos) = 1 dry atmosphere
  !>      - ippmod(iatmos) = 2 humid atmosphere

  integer(c_int), pointer, save :: ippmod(:)

  !> ippmod(iphpar) is a global indicator for the specific physics:
  !>  - 0: no specific physics
  !>  - 1: switch on the specific physics
  !>  - 2: switch on the specific physics plus radiative transfer
  !>       with a parametric file
  integer :: iphpar

  !> pointer for specific physics
  !> - ippmod(icod3p) = 0 adiabatic conditions
  !> - ippmod(icod3p) = 1 permeatic conditions (enthalpy transport)
  !> - ippmod(icod3p) =-1 module not activated
  integer ::  icod3p

  !> pointer to specify steady laminar flamelet approach
  !> - ippmod(islfm) = 0 classic steady laminar flamelet model:
  !>                     adiabatic conditions
  !> - ippmod(islfm) = 1 classic steady laminar flamelet model:
  !>                     non-adiabatic conditions (enthalpy transport with heat loss)
  !> - ippmod(islfm) = 2 flamelet/progress variable model:
  !>                     adiabatic conditions
  !> - ippmod(islfm) = 3 flamelet/progress variable model:
  !>                     non-adiabatic conditions (enthalpy transport with heat loss)
  !> - ippmod(islfm) =-1 module not activated
  integer :: islfm

  !> pointer to specify Eddy Break Up pre-mixed flame with indicator ippmod(icoebu)
  !> - ippmod(icoebu) = 0 adiabatic conditions at constant richness
  !> - ippmod(icoebu) = 1 permeatic conditions at constant richness
  !> - ippmod(icoebu) = 2 adiabatic conditions at variable richness
  !> - ippmod(icoebu) = 3 permeatic conditions at variable richness
  !> - ippmod(icoebu) =-1 module not activated
  integer ::  icoebu

  !> pointer to specify Libby-Williams pre-mixed flame withy indicator ippmod(icolwc)
  !> - ippmod(icolwc)=0 two peak model with adiabiatic conditions.
  !> - ippmod(icolwc)=1 two peak model with permeatic conditions.
  !> - ippmod(icolwc)=2 three peak model with adiabiatic conditions.
  !> - ippmod(icolwc)=3 three peak model with permeatic conditions.
  !> - ippmod(icolwc)=4 four peak model with adiabiatic conditions.
  !> - ippmod(icolwc)=5 four peak model with permeatic conditions.
  !> - ippmod(icolwc)=-1 module not activated.
  integer ::  icolwc

  !> pointer to specify Joule effect module (Laplace forces not taken into account)
  !> with indicator ippmod(ieljou):
  !> - ippmod(ieljou) = 1 use of a real potential
  !> - ippmod(ieljou) = 2 use of a complex potential
  !> - ippmod(ieljou) = 3 use of real potential and specific boundary conditions
  !>  for transformers.
  !> - ippmod(ieljou) = 4 use of complex potential and specific boundary conditions
  !>  for transformers.
  !> - ippmod(ieljou) =-1 module not activated
  integer ::  ieljou

  !> pointer to specify Electric arcs module (Joule effect and Laplace forces)
  !> with indicator ippmod(ielarc):
  !> - ippmod(ielarc) = 1 determination of the magnetic field by means of the Ampere’
  !> theorem  (not available)
  !> - ippmod(ielarc) = 2 determination of the magnetic field by means of the vector potential
  !> - ippmod(ielarc) =-1 module not activated
  integer ::  ielarc

  !> pointer to specify multi-coals and multi-classes pulverised coal combustion
  !> with indicator ippmod(iccoal).
  !> The number of different coals must be inferior or equal to ncharm = 3.
  !> The number of particle size classes nclpch(icha) for the coal icha, must be
  !> inferior or equal to ncpcmx = 10.
  !> - ippmod(iccoal) = 0 imbalance between the temperature of the continuous and the
  !> solid phases
  !> - ippmod(iccoal) = 1 otherwise
  !> - ippmod(iccoal) =-1 module not activated
  integer ::  iccoal

  !> pointer to specify compressible module with indicator ippmod(icompf)
  !>      - ippmod(icompf) = 2 module activated: homogeneous two phase model
  !>      - ippmod(icompf) = 1 module activated: single phase model
  !>      - ippmod(icompf) = 0 module activated: single phase barotropic model
  !>      - ippmod(icompf) =-1 module not activated
  integer ::  icompf

  !> pointer to specify atmospheric flow module with indicator ippmod(iatmos)
  !> - ippmod(iatmos) =-1 module not activated
  !> - ippmod(iatmos) = 0 standard modelling
  !> - ippmod(iatmos) = 1 dry atmosphere
  !> - ippmod(iatmos) = 2 humid atmosphere
  integer ::  iatmos

  !> pointer to specify cooling towers module with indicator ippmod(iaeros)
  !> - ippmod(iaeros) =-1 module not activated
  !> - ippmod(iaeros) >= 0  activated
  integer ::  iaeros

  !> pointer to specify gas mixture module with indicator ippmod(igmix)
  !> - ippmod(igmix) =-1 module not activated
  !> - ippmod(igmix) = 0  Air/Helium   gas mixtures
  !> - ippmod(igmix) = 1  Air/Hydrogen gas mixtures
  !> - ippmod(igmix) = 2  Air/Steam    gas mixtures
  !> - ippmod(igmix) = 3  Air/Helium/Steam gas mixtures
  !> - ippmod(igmix) = 4  Air/Hydrogen/Steam gas mixtures
  !> - ippmod(igmix) = 5  Air/Helium with O2 from the air deduced

  integer ::  igmix

  parameter       (iphpar = 1,  icod3p = 2,  islfm = 3,             &
                   icoebu = 4,  icolwc = 5,  iccoal = 6,            &
                   ieljou = 7,  ielarc = 8,  icompf = 9,            &
                   iatmos = 10, iaeros = 11,                        &
                   igmix  = 12)

  !> \}

  !> \defgroup enthalpy Enthalpic variables pointers

  !> \addtogroup enthalpy
  !> \{

  !> enthalpy, if transported or if deduced
  integer, save :: ihm = -1

  !> \}

  !=============================================================================

  !> \}

  !=============================================================================

  interface

    !> \cond DOXYGEN_SHOULD_SKIP_THIS

    !---------------------------------------------------------------------------

    ! Interface to C function retrieving pointers to members of the
    ! global physical model flags

    subroutine cs_f_physical_model_get_pointers(p_ippmod)    &
      bind(C, name='cs_f_physical_model_get_pointers')
      use, intrinsic :: iso_c_binding
      implicit none
      type(c_ptr), intent(out) :: p_ippmod
    end subroutine cs_f_physical_model_get_pointers

    !---------------------------------------------------------------------------

    !> (DOXYGEN_SHOULD_SKIP_THIS) \endcond

    !---------------------------------------------------------------------------

  end interface

  !=============================================================================

contains

  !=============================================================================

  !> \brief Initialize Fortran physical models properties API.
  !> This maps Fortran pointers to global C variables.

  subroutine pp_models_init

    use, intrinsic :: iso_c_binding
    implicit none

    ! Local variables

    type(c_ptr) :: p_ippmod

    call cs_f_physical_model_get_pointers(p_ippmod)

    call c_f_pointer(p_ippmod, ippmod, [nmodmx])

  end subroutine pp_models_init

  !=============================================================================

end module ppincl
