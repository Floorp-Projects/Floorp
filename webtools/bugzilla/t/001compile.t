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
#Bugzilla Test 1#
###Compilation###

use strict;

use lib 't';

use Support::Files;

use Test::More tests => scalar(@Support::Files::testitems);

# Bugzilla requires Perl 5.6 now.  Checksetup will tell you this if you run it, but
# it tests it in a polite/passive way that won't make it fail at compile time.  We'll
# slip in a compile-time failure if it's missing here so a tinderbox on 5.00503 won't
# pass and mistakenly let people think Bugzilla works on 5.00503
require 5.006;

# Capture the TESTOUT from Test::More or Test::Builder for printing errors.
# This will handle verbosity for us automatically.
my $fh;
{
    local $^W = 0;  # Don't complain about non-existent filehandles
    if (-e \*Test::More::TESTOUT) {
        $fh = \*Test::More::TESTOUT;
    } elsif (-e \*Test::Builder::TESTOUT) {
        $fh = \*Test::Builder::TESTOUT;
    } else {
        $fh = \*STDOUT;
    }
}

my @testitems = @Support::Files::testitems;
my $perlapp = $^X;

# Test the scripts by compiling them

foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    open (FILE,$file);
    my $bang = <FILE>;
    close (FILE);
    my $T = "";
    if ($bang =~ m/#!\S*perl\s+-.*T/) {
        $T = "T";
    }
    my $command = "$perlapp -c$T $file 2>&1";
    my $loginfo=`$command`;
    #print '@@'.$loginfo.'##';
    if ($loginfo =~ /syntax ok$/im) {
        if ($loginfo ne "$file syntax OK\n") {
            ok(0,$file." --WARNING");
            print $fh $loginfo;
        } else {
            ok(1,$file);
        }
    } else {
        ok(0,$file." --ERROR");
        print $fh $loginfo;
    }
}      

exit 0;
