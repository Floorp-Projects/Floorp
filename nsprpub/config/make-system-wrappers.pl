#!/usr/bin/perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

$output_dir = shift;

while (<STDIN>) {
    chomp;
    if (-e "$output_dir/$_") {
	next;
    }

    if (/(.*)\/[^\/*]/) {
	mkdir "$output_dir/$1";
    }

    open OUT, ">$output_dir/$_";
    print OUT "#pragma GCC system_header\n";  # suppress include_next warning
    print OUT "#pragma GCC visibility push(default)\n";
    print OUT "#include_next \<$_\>\n";
    print OUT "#pragma GCC visibility pop\n";
    close OUT;
}

