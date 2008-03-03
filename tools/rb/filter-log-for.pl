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
# The Original Code is make-tree.pl
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Authors of make-tree.pl.
#   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
#     (made filter-log-for.pl from make-tree.pl)
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

# Filter a refcount log to show only the entries for a single object.
# Useful when manually examining refcount logs containing multiple
# objects.

use 5.004;
use strict;
use Getopt::Long;

GetOptions("object=s");

$::opt_object ||
     die qq{
usage: filter-log-for.pl < logfile
  --object <obj>         The address of the object to examine (required)
};

warn "object $::opt_object\n";

LINE: while (<>) {
    next LINE if (! /^</);
    my $line = $_;
    my @fields = split(/ /, $_);

    my $class = shift(@fields);
    my $obj   = shift(@fields);
    next LINE unless ($obj eq $::opt_object);
    my $sno   = shift(@fields);
    my $op  = shift(@fields);
    my $cnt = shift(@fields);

    print $line;

    # The lines in the stack trace
    CALLSITE: while (<>) {
        print;
        last CALLSITE if (/^$/);
    }
}
