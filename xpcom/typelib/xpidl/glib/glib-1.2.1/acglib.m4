dnl GLIB_IF_VAR_EQ (ENV_VAR, VALUE [, EQUALS_ACTION] [, ELSE_ACTION])
AC_DEFUN(GLIB_IF_VAR_EQ,[
        case "$[$1]" in
        "[$2]"[)]
                [$3]
                ;;
        *[)]
                [$4]
                ;;
        esac
])
dnl GLIB_STR_CONTAINS (SRC_STRING, SUB_STRING [, CONTAINS_ACTION] [, ELSE_ACTION])
AC_DEFUN(GLIB_STR_CONTAINS,[
        case "[$1]" in
        *"[$2]"*[)]
                [$3]
                ;;
        *[)]
                [$4]
                ;;
        esac
])
dnl GLIB_ADD_TO_VAR (ENV_VARIABLE, CHECK_STRING, ADD_STRING)
AC_DEFUN(GLIB_ADD_TO_VAR,[
        GLIB_STR_CONTAINS($[$1], [$2], [$1]="$[$1]", [$1]="$[$1] [$3]")
])

dnl GLIB_SIZEOF (INCLUDES, TYPE, ALIAS [, CROSS-SIZE])
AC_DEFUN(GLIB_SIZEOF,
[changequote(<<, >>)dnl
dnl The name to #define.
define(<<AC_TYPE_NAME>>, translit(glib_sizeof_$3, [a-z *], [A-Z_P]))dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, translit(glib_cv_sizeof_$3, [ *], [_p]))dnl
changequote([, ])dnl
AC_MSG_CHECKING(size of $2)
AC_CACHE_VAL(AC_CV_NAME,
[AC_TRY_RUN([#include <stdio.h>
$1
main()
{
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  fprintf(f, "%d\n", sizeof($2));
  exit(0);
}], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0, ifelse([$4], , , AC_CV_NAME=$4))])dnl
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME)
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

dnl GLIB_BYTE_CONTENTS (INCLUDES, TYPE, ALIAS, N_BYTES, INITIALIZER)
AC_DEFUN(GLIB_BYTE_CONTENTS,
[changequote(<<, >>)dnl
dnl The name to #define.
define(<<AC_TYPE_NAME>>, translit(glib_byte_contents_$3, [a-z *], [A-Z_P]))dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, translit(glib_cv_byte_contents_$3, [ *], [_p]))dnl
changequote([, ])dnl
AC_MSG_CHECKING(byte contents of $2)
AC_CACHE_VAL(AC_CV_NAME,
[AC_TRY_RUN([#include <stdio.h>
$1
main()
{
  static $2 tv = $5;
  char *p = (char*) &tv;
  int i;
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  for (i = 0; i < $4; i++)
    fprintf(f, "%s%d", i?",":"", *(p++));
  fprintf(f, "\n");
  exit(0);
}], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0, AC_CV_NAME=0)])dnl
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME)
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

dnl GLIB_SYSDEFS (INCLUDES, DEFS_LIST, OFILE [, PREFIX])
AC_DEFUN(GLIB_SYSDEFS,
[glib_sysdefso="translit($3, [-_a-zA-Z0-9 *], [-_a-zA-Z0-9])"
glib_sysdef_msg=`echo $2 | sed 's/:[[^ 	]]*//g'`
if test "x`(echo '\n') 2>/dev/null`" != 'x\n'; then
  glib_nl='\\n'
else
  glib_nl='\n'
fi
AC_MSG_CHECKING(system definitions for $glib_sysdef_msg)
cat >confrun.c <<_______EOF
#include <stdio.h>
$1
int main (int c, char **v) {
  FILE *f = fopen ("$glib_sysdefso", "a");
  if (!f) return 1;
_______EOF
for glib_sysdef_input in $2 ; do
	glib_sysdef=`echo $glib_sysdef_input | sed 's/^\([[^:]]*\):.*$/\1/'`
	glib_default=`echo $glib_sysdef_input | sed 's/^[[^:]]*:\(.*\)$/\1/'`
	echo "#ifdef $glib_sysdef" >>confrun.c
	echo "  fprintf (f, \"#define GLIB_SYSDEF_%s %s%d${glib_nl}\", \"$glib_sysdef\", \"$4\", $glib_sysdef);" >>confrun.c
	echo "#else" >>confrun.c
	if test $glib_sysdef != $glib_default; then
		echo "  fprintf (f, \"#define GLIB_SYSDEF_%s %s%d${glib_nl}\", \"$glib_sysdef\", \"$4\", $glib_default);" >>confrun.c
	else
		echo "  fprintf (f, \"#define GLIB_SYSDEF_%s${glib_nl}\", \"$glib_sysdef\");" >>confrun.c
	fi
	echo "#endif" >>confrun.c
done
echo "return 0; }" >>confrun.c
AC_TRY_RUN(`cat confrun.c`, AC_MSG_RESULT(done),
[	for glib_sysdef_input in $2 ; do
		glib_sysdef=`echo $glib_sysdef_input | sed 's/^\([[^:]]*\):.*$/\1/'`
		glib_default=`echo $glib_sysdef_input | sed 's/^[[^:]]*:\(.*\)$/\1/'`
		if test $glib_sysdef != $glib_default; then
			glib_default=" $4$glib_default"
		else
			glib_default=
		fi
		echo "#define GLIB_SYSDEF_$glib_sysdef$glib_default" >>$glib_sysdefso
	done
	AC_MSG_RESULT(failed)])
rm -f confrun.c
])
