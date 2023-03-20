#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sed \
 -e s/@HAVE_ARM_ASM_EDSP@/1/g \
 -e s/@HAVE_ARM_ASM_MEDIA@/1/g \
 -e s/@HAVE_ARM_ASM_NEON@/1/g \
 lib/arm/armopts.s.in > lib/arm/armopts.s
