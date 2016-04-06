#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# A Bourne shell script for running the NIST SHA Algorithm Validation Suite
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.
                               
sha_ShortMsg_requests="
SHA1ShortMsg.req
SHA256ShortMsg.req
SHA384ShortMsg.req
SHA512ShortMsg.req
"

sha_LongMsg_requests="
SHA1LongMsg.req
SHA256LongMsg.req
SHA384LongMsg.req
SHA512LongMsg.req
"

sha_Monte_requests="
SHA1Monte.req
SHA256Monte.req
SHA384Monte.req
SHA512Monte.req
"
for request in $sha_ShortMsg_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest sha $request > $response
done
for request in $sha_LongMsg_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest sha $request > $response
done
for request in $sha_Monte_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest sha $request > $response
done

