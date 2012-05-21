#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;

#
# A wrapper for nm that produces output listing symbol size.
#
my($prev_addr) = 0;
my($prev_module) = "";
my($prev_kind) = "";
my($prev_symbol) = "";

open(NM_OUTPUT, "nm -fnol $ARGV[0] | c++filt |") or die "nm failed to run on $ARGV[0]\n";
while (<NM_OUTPUT>)
{
  my($line) = $_;
  chomp($line);
  
  if ($line =~ /^([^:]+):\s*([0-9a-f]{8}) (\w) (.+)$/)
  {
    my($module) = $1;
    my($addr)   = $2;
    my($kind)   = $3;
    my($symbol) = $4;

    #Skip absolute addresses, there should be only a few
    if ('a' eq lc $kind) {
        if ('trampoline_size' ne $symbol) {
            warn "Encountered unknown absolutely addressed symbol '$symbol' in $module";
        }
        next;
    }

    # we expect the input to have been piped through c++filt to
    # demangle symbols. For some reason, it doesn't always demangle
    # all of them, so push still-mangled symbols back through c++filt again.
    if ($symbol =~ /^(_[_Z].+)/)
    {
      # warn "Trying again to unmangle $1\n";
      $symbol = `c++filt '$1'`;
      chomp($symbol);
      # warn "Unmangling again to $symbol\n";
    }

    my($prev_size) = hex($addr) - hex($prev_addr);
    # print "Outputting line $line\n";

    # always print one behind, because only now do we know its size
    if ($prev_module ne "") {
      printf "%s:%08x %s %s\n", $prev_module, $prev_size, $prev_kind, $prev_symbol;
    }
      
    $prev_addr   = $addr;
    $prev_module = $module;
    $prev_kind   = $kind;
    $prev_symbol = $symbol;
  }
  else
  {
    # warn "   Discaring line $line\n";
  }
}

# we don't know how big the last symbol is, so always show 4.
if ($prev_module ne "") {
  printf "%s:%08x %s %s\n", $prev_module, 4, $prev_kind, $prev_symbol;
}
