#!/bin/sh
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Ted Mielczarek <ted.mielczarek@gmail.com>
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
#
# This script expects the following environment variables to be set:
# AIRBAG_SYMBOL_SERVER  : host to upload symbols to
# AIRBAG_SYMBOL_USER    : username on that host
# AIRBAG_SYMBOL_PATH    : path on that host to put symbols in
#
set -e

: ${AIRBAG_SYMBOL_SERVER?} ${AIRBAG_SYMBOL_USER?} ${AIRBAG_SYMBOL_PATH?} ${1?"You must specify a symbol archive to upload"}
archive=`basename $1`
echo "Transferring symbols... $1"
scp -v $1 ${AIRBAG_SYMBOL_USER}@${AIRBAG_SYMBOL_SERVER}:${AIRBAG_SYMBOL_PATH}/
echo "Unpacking symbols on remote host..."
ssh -2 -l ${AIRBAG_SYMBOL_USER} ${AIRBAG_SYMBOL_SERVER} \
  "set -e;
   cd ${AIRBAG_SYMBOL_PATH};
   unzip $archive;
   rm -v $archive;"
echo "Symbol transfer completed"
