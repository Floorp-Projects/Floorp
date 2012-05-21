#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Run as:
# opensslverify.sh <ca certificate file> <server certificate file>
#
# `openssl verify` doesn't return an error code if the cert fails
# to verify, so we have to grep the output, and we can't do that via
# nsIProcess, so we use a shell script.

if openssl verify -CAfile $1 -purpose sslserver $2 2>&1 | grep -q "^error"; then
  exit 1;
else
  exit 0;
fi
