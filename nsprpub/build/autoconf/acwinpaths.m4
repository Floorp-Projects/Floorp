dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Work around the problem that a DOS-style absolute path is split by
dnl the colon in autoconf 2.59 and earlier. The _AC_OUTPUT_FILES macro
dnl was removed and the problem was fixed in autoconf 2.60.
define(GENERATE_FILES_NOSPLIT, [
define([_AC_OUTPUT_FILES],
       [patsubst($@, [`IFS=:], [`#IFS=:])])
])
m4_ifdef([_AC_OUTPUT_FILES],
         [GENERATE_FILES_NOSPLIT(defn([_AC_OUTPUT_FILES]))])
