dnl The contents of this file are subject to the Netscape Public License
dnl Version 1.0 (the "NPL"); you may not use this file except in
dnl compliance with the NPL.  You may obtain a copy of the NPL at
dnl http://www.mozilla.org/NPL/
dnl
dnl Software distributed under the NPL is distributed on an "AS IS" basis,
dnl WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
dnl for the specific language governing rights and limitations under the
dnl NPL.
dnl
dnl The Original Code is this file as it was released upon February 18, 1999.
dnl
dnl The Initial Developer of this code under the NPL is Netscape
dnl Communications Corporation.  Portions created by Netscape are
dnl Copyright (C) 1999 Netscape Communications Corporation.  All Rights
dnl Reserved.
dnl
divert(-1)dnl Throw away output until we want it
changequote([, ])

dnl webify-configure.m4 - Read in configure.in and print options to stdout.
dnl
dnl Usage:
dnl    m4 webify-configure.in configure.in
dnl 
dnl Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).


dnl MOZ_ARG_ENABLE_BOOL(           NAME, HELP, IF-YES [, IF-NO [, ELSE]])
dnl MOZ_ARG_DISABLE_BOOL(          NAME, HELP, IF-NO [, IF-YES [, ELSE]])
dnl MOZ_ARG_ENABLE_STRING(         NAME, HELP, IF-SET [, ELSE])
dnl MOZ_ARG_ENABLE_BOOL_OR_STRING( NAME, HELP, IF-YES, IF-NO, IF-SET[, ELSE]]])
dnl MOZ_ARG_WITH_BOOL(             NAME, HELP, IF-YES [, IF-NO [, ELSE])
dnl MOZ_ARG_WITHOUT_BOOL(          NAME, HELP, IF-NO [, IF-YES [, ELSE])
dnl MOZ_ARG_WITH_STRING(           NAME, HELP, IF-SET [, ELSE])
dnl MOZ_READ_MYCONFIG() - Read in 'myconfig.sh' file

define(field_separator,<<fs>>)

dnl MOZ_ARG_PRINT(TYPE, PRENAME, NAME, HELP)
define(MOZ_ARG_PRINT,
[divert(0)dnl
[$1]field_separator[$2]field_separator[$3]field_separator[$4]
divert(-1)])

dnl MOZ_ARG_ENABLE_BOOL(NAME, HELP, IF-YES [, IF-NO [, ELSE]])
define(MOZ_ARG_ENABLE_BOOL, 
[MOZ_ARG_PRINT(bool,enable,[$1],[$2])])

dnl MOZ_ARG_DISABLE_BOOL(NAME, HELP, IF-NO [, IF-YES [, ELSE]])
define(MOZ_ARG_DISABLE_BOOL,
[MOZ_ARG_PRINT(bool,disable,[$1],[$2])])

dnl MOZ_ARG_ENABLE_STRING(NAME, HELP, IF-SET [, ELSE])
define(MOZ_ARG_ENABLE_STRING,
[MOZ_ARG_PRINT(string,enable,[$1],[$2])])

dnl MOZ_ARG_ENABLE_BOOL_OR_STRING(NAME, HELP, IF-YES, IF-NO, IF-SET[, ELSE]]])
define(MOZ_ARG_ENABLE_BOOL_OR_STRING,
[MOZ_ARG_PRINT(bool_or_string,enable,[$1],[$2])])

dnl MOZ_ARG_WITH_BOOL(NAME, HELP, IF-YES [, IF-NO [, ELSE])
define(MOZ_ARG_WITH_BOOL,
[MOZ_ARG_PRINT(bool,with,[$1],[$2])])

dnl MOZ_ARG_WITHOUT_BOOL(NAME, HELP, IF-NO [, IF-YES [, ELSE])
define(MOZ_ARG_WITHOUT_BOOL,
[MOZ_ARG_PRINT(bool,without,[$1],[$2])])

dnl MOZ_ARG_WITH_STRING(NAME, HELP, IF-SET [, ELSE])
define(MOZ_ARG_WITH_STRING,
[MOZ_ARG_PRINT(string,with,[$1],[$2])])

dnl AC_ARG_ENABLE(NAME, HELP, IF-SET [, ELSE])
define(AC_ARG_ENABLE,
[MOZ_ARG_PRINT(unhandled,enable,[$1],[$2])])

dnl AC_ARG_WITH(NAME, HELP, IF-SET [, ELSE])
define(AC_ARG_WITH,
[MOZ_ARG_PRINT(unhandled,with,[$1],[$2])])

dnl MOZ_ARG_HEADER(Comment)
dnl This is used by webconfig to group options
define(MOZ_ARG_HEADER,
[MOZ_ARG_PRINT(header,,,[$1])])
