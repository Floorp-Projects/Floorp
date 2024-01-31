#!/bin/bash -eu
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
################################################################################

set -e -x -v

cd $HOME/ecckiila
cp $HOME/nss/.clang-format ./
for c in secp384r1 secp521r1; do (cd ecp/$c && cmake . && make && unifdef ecp_$c.c -DRIG_NULL -URIG_NSS -URIG_GOST -UOPENSSL_BUILDING_OPENSSL -UKIILA_OPENSSL_EMIT_CURVEDEF -UKIILA_UNUSED -UOPENSSL_NO_ASM -ULIB_TEST -x2 > tmp_ecp_$c.c && clang-format-10 -i tmp_ecp_$c.c && diff $HOME/nss/lib/freebl/ecl/ecp_$c.c tmp_ecp_$c.c); done;

