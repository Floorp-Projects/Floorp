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
# The Original Code is mozilla.org Code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

my %allocs;
my %classes;
my %counter;

LINE: while (<>) {
    next LINE if (! /^</);

    my @fields = split(/ /, $_);
    my $class = shift(@fields);
    my $obj   = shift(@fields);
    my $sno   = shift(@fields);
    my $op    = shift(@fields);
    my $cnt   = shift(@fields);

    # for AddRef/Release $cnt is the refcount, for Ctor/Dtor it's the size

    if ($op eq 'AddRef' && $cnt == 1) {
        # Example: <nsStringBuffer> 0x01AFD3B8 1 AddRef 1

        $allocs{$obj} = ++$counter{$class}; # the order of allocation
        $classes{$obj} = $class;
    }
    elsif ($op eq 'Release' && $cnt == 0) {
        # Example: <nsStringBuffer> 0x01AFD3B8 1 Release 0

        delete($allocs{$obj});
        delete($classes{$obj});
    }
    elsif ($op eq 'Ctor') {
        # Example: <PStreamNotifyParent> 0x08880BD0 8 Ctor (20)

        $allocs{$obj} = ++$counter{$class};
        $classes{$obj} = $class;
    }
    elsif ($op eq 'Dtor') {
        # Example: <PStreamNotifyParent> 0x08880BD0 8 Dtor (20)

        delete($allocs{$obj});
        delete($classes{$obj});
    }
}


sub sort_by_value {
    my %x = @_;
    sub _by_value($) { my %x = @_; $x{$a} cmp $x{$b}; } 
    sort _by_value keys(%x);
} 


foreach my $key (&sort_by_value(%allocs)) {
    # Example: 0x03F1D818 (2078) @ <nsStringBuffer>
    print "$key (", $allocs{$key}, ") @ ", $classes{$key}, "\n";
}
