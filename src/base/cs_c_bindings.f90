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

!> \file cs_c_bindings.f90
!> Definition of C function and subroutine bindings.

module cs_c_bindings

  !=============================================================================

  use, intrinsic :: iso_c_binding

  use field

  implicit none

  !=============================================================================

  interface

    !---------------------------------------------------------------------------

    ! Interface to C exit routine function.

    subroutine csexit(status) &
      bind(C, name='cs_exit')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int), value :: status
    end subroutine csexit

    !---------------------------------------------------------------------------

    ! Interface to C function activating default log.

    function cs_log_default_is_active() result(is_active) &
      bind(C, name='cs_log_default_is_active')
      use, intrinsic :: iso_c_binding
      implicit none
      logical(kind=c_bool) :: is_active
    end function cs_log_default_is_active

    !---------------------------------------------------------------------------

    ! Scalar clipping

    subroutine cs_f_scalar_clipping(f_id)  &
      bind(C, name='cs_f_scalar_clipping')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int), intent(in), value :: f_id
    end subroutine cs_f_scalar_clipping

    !---------------------------------------------------------------------------

    ! Temporal and z-axis interpolation for meteorological profiles

    function cs_intprf(nprofz, nproft, profz, proft,              &
                       profv, xz, t) result (var)                 &
         bind(C, name='cs_intprf')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int), intent(in), value :: nprofz, nproft
      real(kind=c_double), dimension(nprofz), intent(in) :: profz
      real(kind=c_double), dimension(nproft), intent(in) :: proft
      real(kind=c_double), dimension(nprofz,nproft), intent(in) :: profv
      real(kind=c_double), intent(in), value :: xz, t
      real(kind=c_double) :: var
    end function cs_intprf

    !---------------------------------------------------------------------------

    ! Z-axis interpolation for meteorological profiles

    subroutine cs_intprz(nprofz, profz, profv, xz, z_lv, var)  &
      bind(C, name='cs_intprz')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int), intent(in), value :: nprofz
      real(kind=c_double), dimension(nprofz), intent(in) :: profz, profv
      real(kind=c_double), intent(in), value :: xz
      integer(c_int), dimension(2), intent(out) :: z_lv
      real(kind=c_double), intent(out) :: var
    end subroutine cs_intprz

    !---------------------------------------------------------------------------

    !> \cond DOXYGEN_SHOULD_SKIP_THIS

    !---------------------------------------------------------------------------

    ! Interface to C function to get the bc type array pointer

    subroutine cs_f_boundary_conditions_get_pointers(itypfb) &
      bind(C, name='cs_f_boundary_conditions_get_pointers')
      use, intrinsic :: iso_c_binding
      implicit none
      type(c_ptr), intent(out) :: itypfb
    end subroutine cs_f_boundary_conditions_get_pointers

    !---------------------------------------------------------------------------

    ! Interface to C function indicating if radiation model is active
    ! at a given time step

    function cs_rad_time_is_active() result(is_active)  &
      bind(C, name='cs_rad_time_is_active')
      use, intrinsic :: iso_c_binding
      implicit none
      logical(kind=c_bool) :: is_active
    end function cs_rad_time_is_active

    !---------------------------------------------------------------------------

    ! Interface to C function which returns if the restart present
    function cs_restart_present() result(flag) &
      bind(C, name='cs_restart_present')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int) :: flag
    end function cs_restart_present

    !---------------------------------------------------------------------------

    !> \brief Get the gas concentrations from aerosol code

    subroutine cs_atmo_aerosol_get_gas(array)   &
      bind(C, name='cs_atmo_aerosol_get_gas')
      use, intrinsic :: iso_c_binding
      implicit none
      real(kind=c_double), dimension(*), intent(out) :: array
    end subroutine cs_atmo_aerosol_get_gas

    !---------------------------------------------------------------------------

    !> \brief Return pointers to atmo chemistry arrays

    subroutine cs_f_atmo_chem_arrays_get_pointers(isca_chem, dmmk,  &
                                                  chempoint)        &
      bind(C, name='cs_f_atmo_chem_arrays_get_pointers')
      use, intrinsic :: iso_c_binding
      implicit none
      type(c_ptr), intent(out) :: isca_chem, dmmk, chempoint
    end subroutine cs_f_atmo_chem_arrays_get_pointers

    !---------------------------------------------------------------------------

    ! Interface to C function for atmo

    subroutine raysze(xlat, xlong, jour, heurtu, imer, albe, za, muzero, &
                      omega, fo) &
      bind(C, name='cs_atmo_compute_solar_angles')
      use, intrinsic :: iso_c_binding
      implicit none
      real(kind=c_double), value :: xlat, xlong, jour, heurtu
      integer(kind=c_int), value :: imer
      real(kind=c_double), intent(inout) :: albe, za, muzero, omega, fo
    end subroutine raysze

    !---------------------------------------------------------------------------

    !> \brief Initialize C chemistry structure from Fortran

    subroutine cs_f_atmo_chem_initialize_species_to_fid(species_fid) &
      bind(C, name='cs_f_atmo_chem_initialize_species_to_fid')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int), dimension(*), intent(in) :: species_fid
    end subroutine cs_f_atmo_chem_initialize_species_to_fid

    !---------------------------------------------------------------------------

    ! Computes the explicit chemical source term for atmospheric chemistry
    ! in case of a semi-coupled resolution

     subroutine chem_source_terms(iscal, st_exp, st_imp) &
       bind(C, name='cs_atmo_chem_source_terms')
       use, intrinsic :: iso_c_binding
       implicit none
       integer(c_int), value :: iscal
       real(kind=c_double), dimension(*), intent(inout) :: st_exp, st_imp
     end subroutine chem_source_terms

    !---------------------------------------------------------------------------

    ! Interface to C function for data assimilation (atmospheric module)

    subroutine cs_at_data_assim_initialize()                        &
      bind(C, name='cs_at_data_assim_initialize')
      use, intrinsic :: iso_c_binding
      implicit none
    end subroutine cs_at_data_assim_initialize

    !---------------------------------------------------------------------------

    ! Interface to C function for data assimilation (atmospheric module)

    function cs_at_opt_interp_is_p1_proj_needed() result (ineeded)   &
      bind(C, name='cs_at_opt_interp_is_p1_proj_needed')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int) :: ineeded
    end function cs_at_opt_interp_is_p1_proj_needed

    !---------------------------------------------------------------------------

    ! Interface to C function for data assimilation (atmospheric module).

    subroutine cs_at_data_assim_source_term(f_id, exp_st, imp_st)   &
      bind(C, name='cs_at_data_assim_source_term')
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int), value :: f_id
      real(kind=c_double), dimension(*), intent(inout) :: exp_st
      real(kind=c_double), dimension(*), intent(inout) :: imp_st
    end subroutine cs_at_data_assim_source_term

    !---------------------------------------------------------------------------

    ! Interface to C function computing standard atmospheric profile

    subroutine atmstd(z_ref, p_ref, t_ref, z, p, t, r) &
      bind(C, name='cs_atmo_profile_std')
      use, intrinsic :: iso_c_binding
      implicit none
      real(kind=c_double), intent(in), value :: z_ref, p_ref, t_ref, z
      real(kind=c_double), intent(out) :: p, t, r
    end subroutine atmstd

    !---------------------------------------------------------------------------

    ! Interface to C function computing etheta and eq variable
    ! knowing the saturation.

    subroutine atprke(tinstk, smbrk, smbre)  &
      bind(C, name='cs_atprke')
      use, intrinsic :: iso_c_binding
      implicit none
      real(kind=c_double), dimension(*), intent(inout) :: tinstk, smbrk, smbre
    end subroutine atprke

    !---------------------------------------------------------------------------
    ! Interface to C function to compute the number of aerosols

    subroutine cs_atmo_aerosol_ssh_set_t_p_h(t, p, h) &
       bind(C, name='cs_atmo_aerosol_ssh_set_t_p_h')
       use, intrinsic :: iso_c_binding
       implicit none
       real(kind=c_double), intent(inout) :: t, p, h
    end subroutine cs_atmo_aerosol_ssh_set_t_p_h

    !---------------------------------------------------------------------------

    ! Interface to C function updating scalar array ghost values.

    subroutine synsca(var)  &
      bind(C, name='cs_mesh_sync_var_scal')
      use, intrinsic :: iso_c_binding
      implicit none
      real(c_double), dimension(*) :: var
    end subroutine synsca

    !---------------------------------------------------------------------------

    subroutine atlecc (imode) &
      bind(C, name="cs_f_read_chemistry_profile")
      use, intrinsic :: iso_c_binding
      implicit none
      integer(c_int), value :: imode
    end subroutine atlecc

    !> (DOXYGEN_SHOULD_SKIP_THIS) \endcond

    !---------------------------------------------------------------------------

  end interface

  !=============================================================================

contains

  !=============================================================================

  !> \brief Temporal and z-axis interpolation for meteorological profiles

  !> An optimized linear interpolation is used.

  subroutine intprf(nprofz, nproft, profz, proft,              &
                    profv, xz, temps, var)
    use, intrinsic :: iso_c_binding
    implicit none

    integer(c_int), intent(in), value :: nprofz, nproft
    real(kind=c_double), dimension(nprofz), intent(in) :: profz
    real(kind=c_double), dimension(nproft), intent(in) :: proft
    real(kind=c_double), dimension(nprofz, nproft), intent(in) :: profv
    real(kind=c_double), intent(in), value :: xz, temps
    real(kind=c_double), intent(out) :: var

    var = cs_intprf(nprofz, nproft, profz, proft, profv, xz, temps)

  end subroutine intprf

  !=============================================================================

  !> \brief z-axis interpolation for meteorological profiles

  !> An optimized linear interpolation is used.

  subroutine intprz(nprofz, profz, profv, xz, iz1, iz2, var)
    use, intrinsic :: iso_c_binding
    implicit none

    integer(c_int), intent(in), value :: nprofz
    real(kind=c_double), dimension(nprofz), intent(in) :: profz, profv
    real(kind=c_double), intent(in), value :: xz
    integer(c_int), intent(out) :: iz1, iz2
    real(kind=c_double), intent(out) :: var

    integer(c_int), dimension(2) :: z_lv

    call cs_intprz(nprofz, profz, profv, xz, z_lv, var)
    iz1 = z_lv(1) + 1
    iz2 = z_lv(2) + 1

  end subroutine intprz

  !=============================================================================

  !> brief Clipping scalar field.
  ! \param[in]   iscal

  subroutine clpsca(iscal)

    use, intrinsic :: iso_c_binding
    use numvar
    implicit none

    ! Arguments
    integer, intent(in) :: iscal

    ! Local variables

    integer(c_int) :: f_id

    f_id = ivarfl(isca(iscal))

    call cs_f_scalar_clipping(f_id)

  end subroutine clpsca

  !=============================================================================

end module cs_c_bindings
