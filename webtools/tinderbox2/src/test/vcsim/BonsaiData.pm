# -*- Mode: perl; indent-tabs-mode: nil -*-

# BonsaiData.pm - A simulation of bonsai data for when there is not
# bonsai database availible.


# $Revision: 1.2 $ 
# $Date: 2003/08/17 00:57:43 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/vcsim/BonsaiData.pm,v $ 
# $Name:  $ 


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




package BonsaiData;


# Standard perl libraries

use File::Basename;


# Tinderbox Specific Libraries
use lib '#tinder_libdir#';



@RECTYPES = qw( M A );

@AUTHORS = qw( bob steve joe alice john jane sue
	       dbaron%fas.harvard.edu timeless%mozdev.org
	       roc+%cs.cmu.edu) ;

@BASENAMES = (
	      'download.html',
	      'nsLocalFileOS2.cpp',
	      'nsLocalFileOS2.h',
	      'Makefile.in',
	      '.cvsignore',
	      'configure',
	      'rel_notes.txt',
	      'configure.in',
	      'file with spaces',
	      );

@DIRNAMES = qw(
	       /mozilla/webtools
	       mozilla/webtools/tinderbox2
	       mozilla/webtools/tinderbox2/src
	       );

@LOGS = (
	 'Initialize service at Standard constructor',
	 'Adding check for url recognition',
	 'bugid=186056 browser crashed on second call to applet using javascript
r=brendan@mozilla.org sr=beard@netscape.com',
	 'don\'t return a value from a |void| function. fixing ports bustage.',
	 'Leak cleaning.',
	 );

sub pickone {
    my (@list) = @_;

    my ($random_num) = rand scalar(@list);
    $random_num =~ s/\..*//;
    my $element = $list[$random_num];

    return $element;
}

sub time2cvsformat {
  # convert time() format to cvs input format
  my ($time) = @_;

  my ($sec,$min,$hour,$mday,$mon,
      $year,$wday,$yday,$isdst) =
        localttime($time);

  $mon++;
  $year += 1900;

  # some versions of CVS give data in the format:
  #        2001-01-04
  # others use:
  #        01/04
  # we will simulate this by picking format for each line.
  # though in real life any cvs output will have only one format.

  my ($cvs_date_str);

  my $num = rand 10;
  if ($num >= 5) {
      $cvs_date_str = sprintf("%02u-%02u-%04u %02u:%02u",
			       $year, $mon, $mday, $hour, $min,);
  } else {
      $cvs_date_str = sprintf("%02u/%02u %02u:%02u",
			      $mon, $mday, $hour, $min, );
  }

  return $cvs_date_str;
}


sub simulate_cvs_version {

    # Most revisions have only one number after the first . 
    # Though some are quite long

    my $num = rand 10;
    $num =~ s/\..*//;
    if ($num >= 5) {
	$num = 1;
    }
    my $suffix='';
    foreach $i (1 .. $num) {
	my $version = rand 200;
	$version =~ s/\..*//;

	$suffix .= '.'.$version;
    }

    # most revision numbers begin with 1 though there are a few 2 and 3

    $num = rand 10;
    $num =~ s/\..*//;
    if ($num >= 3) {
	$num = 0;
    }
    $num ++;

    my $out = $num.$suffix;
    return $out;
}


sub get_tree_state {
    my ($bonsai_tree) = @_;

    return 'Open';
}

sub save_tree_state {
  my ($tree, $value) = @_;

  return ;
}


sub get_checkin_data {
    my ($bonsai_tree, $cvs_module, $cvs_branch, $date_min) = @_;

    my @out;
    my $time = $date_min;
    my $time_now = time();

    while ($time < $time_now) {

        my $next_checkin = rand (60*30);
        $next_checkin =~ s/\..*//;
        
        $time += $next_checkin;
    
        my (
            $rectype, $date, $author, $revision, 
            $file, $repository_dir, $log, 
            $sticky, $branch,
            $lines_added, $lines_removed,
            );

        # I do not use these Bonsai fields.

        $lines_added = '<ignore>';
        $lines_removed = '<ignore>';
        $sticky  = '<ignore>';
        $branch = '<ignore>';
        
        # In the simulation we usually have one single checkin but
        # sometimes we get a bunch of files at the same time.
        
        my $num = rand 10;
        $num =~ s/\..*//;
        if ($num >= 5) {
            $num = 1;
        }

        $author = pickone(@AUTHORS);
        foreach $i (1 .. $num) {
	
            $rectype = pickone(@RECTYPES);
            $repository_dir = pickone(@DIRNAMES);
            $revision = simulate_cvs_version();
            $file = pickone(@BASENAMES);
            $log = pickone(@LOGS);
            
            @tmp = (
                    $rectype, $time, $author,
                    $repository, $repository_dir, $file,
                    $revision,
                    $sticky, $branch,
                    $lines_added, $lines_removed, $log,
                    );
            
            push @out, [ @tmp ];
        }
    }
    
    my ($index) = {
	'type' => 0,
	'time' => 1,
	'author' => 2,
	'repository' => 3,
	'dir' => 4,
	'file' => 5,
	'rev' => 6,
	'sticky' => 7,
	'branch' => 8,
	'lines_added' => 9,
	'lines_removed' => 10,
	'log' => 11,
    };

    my $result = \@out;
    return ($result,  $index);
}


# This code looks like the code in
# ~/mozilla/webtools/tinderbox2/src/lib/TinderDB/VC_Bonsai.pm so I can
# checkhow BonsaiData::get_checkin_data is used.

sub test {

    my $last_cvs_data = time() - (2*24*60*60);
    my ($results, $i) = 
        BonsaiData::get_checkin_data(
                                     '<ignored>', 
                                     '<ignored>',
                                     '<ignored>',
                                     $last_cvs_data,
                                     );

    foreach $r (@{ $results }) {
        
        my ($time)   = $r->[$$i{'time'}];
        my ($author) = $r->[$$i{'author'}];
        my ($dir)    = $r->[$$i{'dir'}];
        my ($file)   = $r->[$$i{'file'}];
        my ($log)    = $r->[$$i{'log'}];
        
        print "time: $time author: $author\n".
            "dir: $dir file: $file\n".
            "log: $log\n".
            "\n";
        
    }

    return ;
}

1;

