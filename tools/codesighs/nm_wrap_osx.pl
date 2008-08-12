#!/usr/bin/perl -w
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is autosummary.linx.bash code, released
# Oct 10, 2002.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Simon Fraser <sfraser@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
