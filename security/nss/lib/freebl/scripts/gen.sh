#!/bin/sh
#
# Run the scripts in this folder, generating the assembly,
#

perl sha512p8-ppc.pl linux64le sha512-p8.s

# Add the license mention
cat > hdr << "EOF"
# Copyright (c) 2006, CRYPTOGAMS by <appro@openssl.org>
# All rights reserved.
# See the full LICENSE under scripts/.

EOF

cat hdr sha512-p8.s > ../sha512-p8.s

# Cleanup
rm hdr sha512-p8.s
