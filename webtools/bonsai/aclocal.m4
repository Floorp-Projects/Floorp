dnl autoconf tests for bonsai
dnl Pontus Lidman 99-05-04
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
  AC_MSG_CHECKING(for perl Date::Parse module)
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
