#!#perl# -w --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# generate static html pages for use in testing the popup libraries.
# Output is written to the file
# $TinderConfig::TINDERBOX_HTML_DIR/vcdisplay.htmlo to be examined by
# a programmer. Be sure to test the None.pm module as well as the
# other VC Modules.

# $Revision: 1.5 $ 
# $Date: 2003/08/17 00:57:38 $ 
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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


# Load the standard perl libraries
use File::Basename;



# Load the tinderbox specific libraries
use lib '#tinder_libdir#';

use TinderConfig;
use VCDisplay;
use HTMLPopUp;
use Utils;
use TreeData;

sub print_hash {
    my (%args) = @_;
    my $out;

    foreach $arg ( sort keys %args) {
	$out .= "$arg=$args{$arg}\n";
    }

    $out .= "\n";

    return $out;
}


sub print_url {
    my ($string) = @_;
    my $out;

    $string =~ s/\" *>(.*)</\&linktxt=$1\&/;

   my @args = split('[&><"]', $string);
    foreach $arg (@args) {
	$out .= HTMLPopUp::unescapeURL($arg);
	$out .= "\n";
    }

    $out .= "\n";

    return $out;
}


sub print_test {
    my ($url, %args) = @_;
    my $out;

    $out .= print_hash(%args);
    $out .= print_url($url);
    $out .= $url."\n\n";
    $out .= "\n\n\n";

    return $out;
}


sub print_tests {

    my %args;
    my $url;
    my $out;

    my $seperator = ("-" x 30)."\n\n\n";


    $out .= "Simulation of processmail_build call\n";
    $out .=  $seperator;

    %args = (
	     'tree' => 'Project_A',
	     'file' => 'main.c',
	     'line' => 325,
	     'linktxt' =>  "Compiler error! Some error message here.", 
	     );

    $url = VCDisplay::guess(%args);
    $out .= print_test($url, %args);
    
    $out .= "Simulation of Build column call";

    %args = (
	     'tree' => 'Project_A',
	     'linktxt' => 'C',
	     'maxdate' => '1039464800',
	     'mindate' => '1039464205',
	     'windowtitle' => 'Build Info Buildname: Failover_Tests',
	     'windowtxt' => 'endtime: 12/09&nbsp;15:20<br>starttime: 12/09&nbsp;15:13<br>',
	     );
    
    $url = VCDisplay::query(%args);
    $out .= print_test($url, %args);


    $out .= "Simulation of VC column call\n";
    $out .=  $seperator;
    
    %args = (
	     'tree' => 'Project_A',
	     'mindate' => 1039467540 - $main::SECONDS_PER_DAY,
	     'maxdate' => 1039467540,
	     'who' => 'fred',
	     );
    
    $url = VCDisplay::query(%args);
    $out .= print_test($url, %args);
 
    $out .= "Simulation of time column call\n";
    $out .=  $seperator;

    %args = (
	     'tree' => 'Project_A',
	     'mindate' => 1039467540,
	     'linktxt' => '12/09&nbsp;15:59',
	     );
    
    $url = VCDisplay::query(%args);
    $out .= print_test($url, %args);
 
    return $out;
}

sub main {

    my $outfile ="$TinderConfig::TINDERBOX_HTML_DIR/vcdisplay.html";
    my @libs = glob "./build/lib/VCDisplay/*";

    set_static_vars();
    get_env();

    my $out = "<pre>\n";
    foreach $lib (@libs) {
	$lib = basename($lib);
	$lib =~ s/\..*//;
	my $libname= 'VCDisplay::'.$lib;
	@VCDisplay::ISA = ($libname);
	eval " use $libname ";
	
	$out .= " ------ $libname -----\n\n";
	$out .= print_tests();
	
    }
    overwrite_file($outfile, $out);

    return ;
}

main();
exit 0;

1;

