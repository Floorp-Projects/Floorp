#!/bin/sh
#
# A Bourne shell script for running the NIST RNG Validation Suite
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.

drbg_requests="
SHA256_DRBG.req
"

for request in $drbg_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest drbg $request > $response
done
