#!/usr/bin/env perl
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##

use FindBin;
use lib $FindBin::Bin;
use thumb;

print "; This file was created from a .asm file\n";
print ";  using the ads2armasm_ms.pl script.\n";

while (<STDIN>)
{
    undef $comment;
    undef $line;

    s/REQUIRE8//;
    s/PRESERVE8//;
    s/^\s*ARM\s*$//;
    s/AREA\s+\|\|(.*)\|\|/AREA |$1|/;
    s/qsubaddx/qsax/i;
    s/qaddsubx/qasx/i;

    thumb::FixThumbInstructions($_, 1);

    s/ldrneb/ldrbne/i;
    s/ldrneh/ldrhne/i;
    s/^(\s*)ENDP.*/$&\n$1ALIGN 4/;

    print;
}

