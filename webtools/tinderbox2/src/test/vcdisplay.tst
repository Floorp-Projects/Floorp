#!#perl# -w --

# generate static html pages for use in testing the popup libraries.
# Output is written to standard out to be examined by a programmer.


# $Revision: 1.2 $ 
# $Date: 2003/01/19 17:18:56 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/vcdisplay.tst,v $ 
# $Name:  $ 
#


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 


# Load the standard perl libraries
use File::Basename;



# Load the tinderbox specific libraries
use lib '#tinder_libdir#';

use TinderConfig;
use VCDisplay;
use HTMLPopUp;
use Utils;

sub print_hash {
    my (%args) = @_;

    foreach $arg ( sort keys %args) {
	print "$arg=$args{$arg}\n";
    }

    print "\n";

    return ;
}


sub print_url {
    my ($string) = @_;

    $string =~ s/" *>(.*)</\&linktxt=$1\&/;

    @args = split('[&><"]', $string);
    foreach $arg (@args) {
	print HTMLPopUp::unescapeURL($arg);
	print "\n";
    }

    print "\n";

    return ;
}


sub print_tests {

    # simulation of processmail_build call

    %args = (
	     'tree' => 'Project_A',
	     'file' => 'main.c',
	     'line' => 325,
	     'linktxt' =>  "Compiler error! Some error message here.", 
	     'alt_linktxt' =>  "Compiler error! Some error message here.", 
	     );

    $line = VCDisplay::guess(%args);
    print_hash(%args);
    print_url($line);
    print $line."\n\n";
    print "\n\n\n";
    
    # simulation of Build column call    

    %args = (
	     'tree' => 'Project_A',
	     'linktxt' => 'C',
	     'maxdate' => '1039464800',
	     'mindate' => '1039464205',
	     'windowtitle' => 'Build Info Buildname: Failover_Tests',
	     'windowtxt' => 'endtime: 12/09&nbsp;15:20<br>starttime: 12/09&nbsp;15:13<br>',
	     );
    
    $line = VCDisplay::query(%args);
    print_hash(%args);
    print_url($line);
    print $line."\n\n";
    print "\n\n\n";
    

    # simulation of VC column call    
    
    %args = (
	     'tree' => 'Project_A',
	     'mindate' => 1039467540  - $main::SECONDS_PER_DAY,
	     'maxdate' => 1039467540,
	     'who' => 'fred',
	     );
    
    $line = VCDisplay::query(%args);
    print_hash(%args);
    print_url($line);
    print $line."\n\n";
    print "\n\n\n";
 
    # simulation of time column call

    %args = (
	     'tree' => 'Project_A',
	     'mindate' => 1039467540,
	     'linktxt' => '12/09&nbsp;15:59',
	     );
    
    $line = VCDisplay::query(%args);
    print_hash(%args);
    print_url($line);
    print $line."\n\n";
    print "\n\n\n";
 
    return ;   
}


my @libs = glob "./build/lib/VCDisplay/*";
foreach $lib (@libs) {

    $lib = basename($lib);
    $lib =~ s/\..*//;
    $libname= 'VCDisplay::'.$lib;
    @VCDisplay::ISA = ($libname);
    eval " use $libname ";

    print " ------ $libname -----\n\n";
    print_tests();
}


1;

