#!/usr/bin/perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# $Id: ptab.pl,v 1.4 2012/04/25 14:49:54 gerv%gerv.net Exp $
#

while(<>) {
    chomp;
    push(@primes, $_);
}

printf("mp_size   prime_tab_size = %d;\n", ($#primes + 1));
print "mp_digit  prime_tab[] = {\n";

print "\t";
$last = pop(@primes);
foreach $prime (sort {$a<=>$b} @primes) {
    printf("0x%04X, ", $prime);
    $brk = ($brk + 1) % 8;
    print "\n\t" if(!$brk);
}
printf("0x%04X", $last);
print "\n" if($brk);
print "};\n\n";

exit 0;
