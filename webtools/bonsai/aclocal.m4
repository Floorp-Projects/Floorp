dnl -*- Mode: m4; tab-width: 4; indent-tabs-mode: nil; -*-
dnl autoconf tests for bonsai

AC_DEFUN(AC_CHECK_PERL_MODULE,
[   AC_MSG_CHECKING("for perl $1...")
    ac_mod_name=`echo $1 | tr ':' '_'`
    AC_CACHE_VAL(ac_cv_perl_$ac_mod_name,
    [   $PERL -w -c -e "use $1;" 2>/dev/null
        ac_has_mod=$?
        if test "$ac_has_mod" = "0"; then
            eval "ac_cv_perl_$ac_mod_name=yes"
        else
            eval "ac_cv_perl_$ac_mod_name=no"
        fi
    ])
    if eval "test \"`echo '$ac_cv_perl_'$ac_mod_name`\" = yes"; then
        AC_MSG_RESULT(yes)
        ifelse([$2], , :, [$2])
    else
        AC_MSG_RESULT(no)
        ifelse([$3], , , [$3
])dnl
    fi
])



AC_DEFUN(AC_CHECK_PERL_MODULES,
[for ac_mod in $1; do
    AC_CHECK_PERL_MODULE($ac_mod,
    [   changequote(, )dnl
        ac_tr_func=HAVE_`echo $ac_func | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' | tr ':' '_'` 
        changequote([, ])dnl
        AC_DEFINE_UNQUOTED($ac_tr_func) $2], $3)dnl
done
])
