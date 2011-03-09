MODULE deck_dist_fn_block

  USE strings_advanced
  USE dist_fn

  IMPLICIT NONE

  SAVE

  TYPE(distribution_function_block), POINTER :: working_block

CONTAINS

  FUNCTION handle_dist_fn_deck(element, value)

    CHARACTER(*), INTENT(IN) :: element, value
    INTEGER :: handle_dist_fn_deck

    CHARACTER(LEN=string_length) :: part1
    INTEGER :: part2
    INTEGER :: work, io
    REAL(num) :: work1, work2

    handle_dist_fn_deck = c_err_none
    IF (element .EQ. blank .OR. value .EQ. blank) RETURN

    IF (str_cmp(element, "name")) THEN
      working_block%name = value
      RETURN
    ENDIF

    IF (str_cmp(element, "ndims")) THEN
      work = as_integer(value, handle_dist_fn_deck)
      IF (work .GE. 1 .AND. work .LE. 3) THEN
        working_block%ndims = work
      ELSE
        IF (rank .EQ. 0) THEN
          DO io = stdout, du, du - stdout ! Print to stdout and to file
            WRITE(io,*) '*** ERROR ***'
            WRITE(io,*) 'Distribution functions can only be 1D, 2D or 3D'
          ENDDO
        ENDIF
        handle_dist_fn_deck = c_err_bad_value
      ENDIF
      RETURN
    ENDIF

    IF (working_block%ndims .EQ. -1) THEN
      IF (rank .EQ. 0) THEN
        DO io = stdout, du, du - stdout ! Print to stdout and to file
          WRITE(io,*) '*** ERROR ***'
          WRITE(io,*) 'Must set number of dimensions before setting other', &
              ' distribution'
          WRITE(io,*) 'function properties.'
        ENDDO
      ENDIF
      extended_error_string = "ndims"
      handle_dist_fn_deck = c_err_required_element_not_set
      RETURN
    ENDIF

    IF (str_cmp(element, "dumpmask")) THEN
      working_block%dumpmask = as_integer(value, handle_dist_fn_deck)
      RETURN
    ENDIF

    IF (str_cmp(element, "restrict_x")) THEN
      CALL split_range(value, work1, work2, handle_dist_fn_deck)
      IF (handle_dist_fn_deck .NE. c_err_none) RETURN
      working_block%use_restrictions(1) = .TRUE.
      working_block%restrictions(:,1) = (/work1, work2/)
    ENDIF

    IF (str_cmp(element, "restrict_y")) THEN
      CALL split_range(value, work1, work2, handle_dist_fn_deck)
      IF (handle_dist_fn_deck .NE. c_err_none) RETURN
      working_block%use_restrictions(2) = .TRUE.
      working_block%restrictions(:,2) = (/work1, work2/)
    ENDIF

    IF (str_cmp(element, "restrict_px")) THEN
      CALL split_range(value, work1, work2, handle_dist_fn_deck)
      IF (handle_dist_fn_deck .NE. c_err_none) RETURN
      working_block%use_restrictions(3) = .TRUE.
      working_block%restrictions(:,3) = (/work1, work2/)
    ENDIF

    IF (str_cmp(element, "restrict_py")) THEN
      CALL split_range(value, work1, work2, handle_dist_fn_deck)
      IF (handle_dist_fn_deck .NE. c_err_none) RETURN
      working_block%use_restrictions(4) = .TRUE.
      working_block%restrictions(:,4) = (/work1, work2/)
    ENDIF

    IF (str_cmp(element, "restrict_pz")) THEN
      CALL split_range(value, work1, work2, handle_dist_fn_deck)
      IF (handle_dist_fn_deck .NE. c_err_none) RETURN
      working_block%use_restrictions(5) = .TRUE.
      working_block%restrictions(:,5) = (/work1, work2/)
    ENDIF

    IF (str_cmp(element, "include_species")) THEN
      part2 = as_integer(value, handle_dist_fn_deck)
      working_block%use_species(part2) = .TRUE.
      RETURN
    ENDIF

    CALL split_off_int(element, part1, part2, handle_dist_fn_deck)

    IF (handle_dist_fn_deck .NE. c_err_none) THEN
      handle_dist_fn_deck = c_err_unknown_element
      RETURN
    ENDIF

    IF (str_cmp(part1, "direction")) THEN
      working_block%directions(part2) = &
          as_integer(value, handle_dist_fn_deck)
      RETURN
    ENDIF

    IF (str_cmp(part1, "range")) THEN
      CALL split_range(TRIM(value), work1, work2, handle_dist_fn_deck)
      IF (IAND(handle_dist_fn_deck, c_err_bad_value) .NE. 0) THEN
        handle_dist_fn_deck = &
            IAND(handle_dist_fn_deck, NOT(c_err_bad_value))
        handle_dist_fn_deck = &
            IOR(handle_dist_fn_deck, c_err_warn_bad_value)
        RETURN
      ENDIF
      working_block%ranges(1,part2) = work1
      working_block%ranges(2,part2) = work2
      RETURN
    ENDIF

    IF (str_cmp(part1, "resolution")) THEN
      working_block%resolution(part2) = &
          as_integer(value, handle_dist_fn_deck)
      RETURN
    ENDIF

    handle_dist_fn_deck = c_err_unknown_element

  END FUNCTION handle_dist_fn_deck



  FUNCTION check_dist_fn_block()

    INTEGER :: check_dist_fn_block

    ! Should do error checking but can't be bothered at the moment
    check_dist_fn_block = c_err_none

  END FUNCTION check_dist_fn_block



  SUBROUTINE dist_fn_start

    ! Every new laser uses the internal time function
    ALLOCATE(working_block)
    CALL init_dist_fn(working_block)

  END SUBROUTINE dist_fn_start



  SUBROUTINE dist_fn_end

    CALL attach_dist_fn(working_block)

  END SUBROUTINE dist_fn_end

END MODULE deck_dist_fn_block