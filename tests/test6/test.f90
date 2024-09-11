program hello_world
  use iso_c_binding, only: c_ptr, c_null_ptr, c_char, c_loc, c_f_pointer, c_associated, c_size_t
  implicit none

  interface
    function malloc(size) bind(c, name="malloc")
      use iso_c_binding, only: c_ptr, c_size_t
      implicit none
      type(c_ptr) :: malloc
      integer(c_size_t), value :: size
    end function malloc

    subroutine free(ptr) bind(c, name="free")
      use iso_c_binding, only: c_ptr
      implicit none
      type(c_ptr), value :: ptr
    end subroutine free

    subroutine sleep(seconds) bind(c, name="sleep")
      use iso_c_binding, only: c_int
      implicit none
      integer(c_int), value :: seconds
    end subroutine sleep
  end interface

  type(c_ptr) :: mem_ptr
  character(len=11), pointer :: msg_ptr
  integer(c_size_t) :: size
  integer :: i, num_iterations

  ! Define the size for allocation
  size = 12
  num_iterations = 50

  do i = 1, num_iterations
    ! Allocate memory for the string "Hello world!"
    mem_ptr = malloc(size)

    ! Check if the memory allocation was successful
    if (.not. c_associated(mem_ptr)) then
      print *, "Memory allocation failed"
      stop
    endif

    ! Associate the C pointer with a Fortran pointer
    call c_f_pointer(mem_ptr, msg_ptr)

    ! Write "Hello world" to the allocated memory
    msg_ptr = "Hello world"

    ! Print the string
    print *, trim(msg_ptr)

    ! Free the allocated memory
    call free(mem_ptr)

    ! Sleep for 1 second
    call sleep(1)
  end do

end program hello_world
