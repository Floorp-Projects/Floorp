#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script expects the following environment variables to be set:
# SYMBOL_SERVER_HOST    : host to upload symbols to
# SYMBOL_SERVER_USER    : username on that host
# SYMBOL_SERVER_PATH    : path on that host to put symbols in
#
# And will use the following optional environment variables if set:
# SYMBOL_SERVER_SSH_KEY : path to a ssh private key to use
# SYMBOL_SERVER_PORT    : port to use for ssh
# POST_SYMBOL_UPLOAD_CMD: a commandline to run on the remote host after
#                         uploading. The full path of the symbol index
#                         file will be appended to the commandline.
#
# The script expects two command-line arguments, in this order:
#   - The symbol index name
#   - The symbol archive
#

set -e

: ${SYMBOL_SERVER_HOST?} ${SYMBOL_SERVER_USER?} ${SYMBOL_SERVER_PATH?} ${1?"You must specify a symbol index name."} ${2?"You must specify a symbol archive to upload"}

SYMBOL_INDEX_NAME="$1"
SYMBOL_ARCHIVE="$2"

hash=`openssl dgst -sha1 "${SYMBOL_ARCHIVE}" | sed 's/^.*)=//' | sed 's/\ //g'`
archive="${hash}-"`basename "${SYMBOL_ARCHIVE}" | sed 's/\ //g'`
echo "Transferring symbols... ${SYMBOL_ARCHIVE}"
scp -oLogLevel=DEBUG -oRekeyLimit=10M ${SYMBOL_SERVER_PORT:+-P $SYMBOL_SERVER_PORT} \
  ${SYMBOL_SERVER_SSH_KEY:+-i "$SYMBOL_SERVER_SSH_KEY"} "${SYMBOL_ARCHIVE}" \
  ${SYMBOL_SERVER_USER}@${SYMBOL_SERVER_HOST}:"${SYMBOL_SERVER_PATH}/${archive}"
echo "Unpacking symbols on remote host..."
ssh -2 ${SYMBOL_SERVER_PORT:+-p $SYMBOL_SERVER_PORT} \
  ${SYMBOL_SERVER_SSH_KEY:+-i "$SYMBOL_SERVER_SSH_KEY"} \
  -l ${SYMBOL_SERVER_USER} ${SYMBOL_SERVER_HOST} \
  "set -e;
   umask 0022;
   cd ${SYMBOL_SERVER_PATH};
   unzip -n '$archive';
   rm -v '$archive';"
if test -n "$POST_SYMBOL_UPLOAD_CMD"; then
  echo "${POST_SYMBOL_UPLOAD_CMD} \"${SYMBOL_SERVER_PATH}/${SYMBOL_INDEX_NAME}\""
  ssh -2 ${SYMBOL_SERVER_PORT:+-p $SYMBOL_SERVER_PORT} \
  ${SYMBOL_SERVER_SSH_KEY:+-i "$SYMBOL_SERVER_SSH_KEY"} \
  -l ${SYMBOL_SERVER_USER} ${SYMBOL_SERVER_HOST} \
  "${POST_SYMBOL_UPLOAD_CMD} \"${SYMBOL_SERVER_PATH}/${SYMBOL_INDEX_NAME}\""
fi
echo "Symbol transfer completed"
