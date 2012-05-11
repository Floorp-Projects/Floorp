dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

define(GENERATE_SUB_ABS, [
define([AC_OUTPUT_FILES_SUB1], [
patsubst($@, [/\*)], [/* | ?:/*)])
])
])
GENERATE_SUB_ABS(defn([AC_OUTPUT_FILES]))

define(GENERATE_SUB_NOSPLIT, [
define([AC_OUTPUT_FILES], [
patsubst($@, [-e "s%:% \$ac_given_srcdir/%g"], [])
])
])
GENERATE_SUB_NOSPLIT(defn([AC_OUTPUT_FILES_SUB1]))

define(GENERATE_HEADER_NOSPLIT, [
define([AC_OUTPUT_HEADER], [
patsubst($@, [-e "s%:% \$ac_given_srcdir/%g"], [])
])
])
GENERATE_HEADER_NOSPLIT(defn([AC_OUTPUT_HEADER]))

define(GENERATE_SUBDIRS_ABS, [
define([AC_OUTPUT_SUBDIRS], [
patsubst($@, [/\*)], [/* | ?:/*)])
])
])
GENERATE_SUBDIRS_ABS(defn([AC_OUTPUT_SUBDIRS]))
