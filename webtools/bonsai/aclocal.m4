dnl autoconf tests for bonsai
dnl Pontus Lidman 99-04-27
dnl
dnl Check if mysqltclsh is compiled with tclX support
dnl
dnl AC_PROG_MYSQLTCL_TCLX([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for mysqltclsh compiled with tclX and define MYSQLTCL
dnl
AC_DEFUN(AC_PROG_MYSQLTCL_TCLX,
[dnl 
dnl Get the cflags and libraries from the gtk-config script
dnl
  AC_PATH_PROGS(MYSQLTCL, mysqltclsh mysqltcl, no)
  AC_MSG_CHECKING(for tclX flock in mysqltclsh)
  no_mysqltclsh=""

  if test "$MYSQLTCL" = "no" ; then
    no_mysqltclsh=yes
  else
dnl
dnl Perform test
dnl
    have_flock=`cat tclx_test.tcl | $MYSQLTCL`
    if test "x$have_flock" = "x0" ; then
      no_mysqltclsh=yes
    fi
  fi
  if test "x$no_mysqltclsh" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])
  else
     AC_MSG_RESULT(no)
     if test "$MYSQLTCL" = "no" ; then
       echo "*** mysqltclsh could not be found"
       echo "*** make sure it is installed and in your PATH, then try again"
     else
	echo "*** mysqltclsh is not compiled with tclX support"
	echo "*** see the file INSTALL for additional information"
     fi
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(MYSQLTCL)
])
dnl
dnl check if Perl::DB is installed
dnl
AC_DEFUN(AC_PERL_DB,
[
  AC_MSG_CHECKING(for perl DBD::mysql module)
  $PERL -w -c -e 'use DBD::mysql' 2>/dev/null; has_dbd=$?
  if test "x$has_dbd" = "x0" ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])
  else
     AC_MSG_RESULT(no)
     echo "*** the perl MySQL module (DBD::mysql) could not be found"
     ifelse([$2], , :, [$2])
  fi
])
dnl
dnl check if Date::Parse is installed
dnl
AC_DEFUN(AC_PERL_DATEPARSE,
[
  AC_MSG_CHECKING(for perl Date::parse module)
  $PERL -w -c -e 'use Date::Parse' 2>/dev/null; has_dateparse=$?
  if test "x$has_dateparse" = "x0" ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])
  else
     AC_MSG_RESULT(no)
     echo "*** the perl Date::Parse module could not be found"
     ifelse([$2], , :, [$2])
  fi
])
