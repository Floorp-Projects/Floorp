# -*- Mode: perl; indent-tabs-mode: nil -*-
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code are the Bugzilla Tests.
# 
# The Initial Developer of the Original Code is Zach Lipton
# Portions created by Zach Lipton are 
# Copyright (C) 2001 Zach Lipton.  All
# Rights Reserved.
# 
# Contributor(s): Zach Lipton <zach@zachlipton.com>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 David D. Kilzer <ddkilzer@theracingworld.com>
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

#################
#Bugzilla Test 2#
####GoodPerl#####

use strict;

use lib 't';

use Support::Files;

use Test::More tests => (scalar(@Support::Files::testitems) * 2);

my @testitems = @Support::Files::testitems; # get the files to test.

foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    if (! open (FILE, $file)) {
        ok(0,"could not open $file --WARNING");
    }
    my $file_line1 = <FILE>;
    close (FILE);

    $file =~ m/.*\.(.*)/;
    my $ext = $1;

    if ($file_line1 !~ /\/usr\/bonsaitools\/bin\/perl/) {
        ok(1,"$file does not have a shebang");	
    } else {
        my $flags;
        if ($file eq "processmail") {
            # special case processmail, which is tainted checked
            $flags = "wT";
        } elsif (!defined $ext || $ext eq "pl") {
            # standalone programs (eg syncshadowdb) aren't taint checked yet
            $flags = "w";
        } elsif ($ext eq "pm") {
            ok(0, "$file is a module, but has a shebang");
            next;
        } elsif ($ext eq "cgi") {
            # cgi files must be taint checked, but only the user-accessible
            # ones have been checked so far
            if ($file =~ m/^edit/) {
                $flags = "w";
            } else {
                $flags = "wT";
            }
        } else {
            ok(0, "$file has shebang but unknown extension");
            next;
        }

        if ($file_line1 =~ m#/usr/bonsaitools/bin/perl -$flags#) {
            ok(1,"$file uses -$flags");
        } else {
            ok(0,"$file is MISSING -$flags --WARNING");
        }
    }
}

foreach my $file (@testitems) {
    my $found_use_strict = 0;
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    if (! open (FILE, $file)) {
        ok(0,"could not open $file --WARNING");
        next;
    }
    while (my $file_line = <FILE>) {
        if ($file_line =~ m/^\s*use strict/) {
            $found_use_strict = 1;
            last;
        }
    }
    close (FILE);
    if ($found_use_strict) {
        ok(1,"$file uses strict");
    } else {
        ok(0,"$file DOES NOT use strict --WARNING");
    }
}

exit 0;
