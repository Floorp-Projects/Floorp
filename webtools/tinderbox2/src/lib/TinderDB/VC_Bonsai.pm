# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# NOT WORKING - please port, following VC_CVS as a guide.

# TinderDB::VC_Bonsai - methods to query the bonsai interface to CVS
# Version Control system and find the users who have checked changes
# into the tree recently and renders this information into HTML for
# the status page.  This module depends on TreeData and is one of the
# few which can see its internal datastructures.  The
# TinderHeader::TreeState is queried so that users will be displayed
# in the color which represents the state of the tree at the time they
# checked their code in.




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


# We need this empty package namespace for our dependency analysis, it
# gets confused if there is not a package name which matches the file
# name and in this case the file is one of several possible
# implementations.

package TinderDB::VC_Bonsai;

package TinderDB::VC;

$VERSION = ( qw $Revision: 1.2 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);


sub html_legend {
  my ($out)='';

# print all the possible tree states in a cell with the color

  $out .= "\t<td align=right valign=top>\n";
  $out .= "\t<table>\n";
  
  $out .= ("\t\t<thead><tr><td align=center>".
           "VC Cell Colors".
           "</td></tr></thead>\n");

  foreach $state (keys %TinderConfig::TREE_STATE2COLOR) {
    my $color = TinderConfig::TREE_STATE2COLOR{$state};
    $out .= ("\t\t<tr bgcolor=\"$color\">".
             "<td>Tree state: $state</td></tr>\n");
  }

  $out .= "\t</table>\n";
  $out .= "\t</td>\n";


  return ($out);  
}


sub html_header {
  return ("\t<th>VC checkins</th>\n");
}


use lib "../bonsai";

require 'globals.pl';

$F_DEBUG=1;


$days = 2;

if ($ARGV[0] eq "-days") {
    shift @ARGV;
    $days = shift @ARGV;
}

$tree = $ARGV[0];

open(SEMFILE, ">>$tree/buildwho.sem") || die "Couldn't open semaphore file!";
if (!flock(SEMFILE, 2 + 4)) {   # 2 means "lock"; 4 means "fail immediately if
                                # lock already taken".
    print "buildwho.pl: Another process is currently building the database.\n";
    exit(0);
}

require "$tree/treedata.pl";

if( $cvs_root eq '' ){
    $CVS_ROOT = '/m/src';
}
else {
    $CVS_ROOT = $cvs_root;
}

$CVS_REPOS_SUFIX = $CVS_ROOT;
$CVS_REPOS_SUFIX =~ s/\//_/g;
    
$CHECKIN_DATA_FILE = "/d/webdocs/projects/bonsai/data/checkinlog${CVS_REPOS_SUFIX}";
$CHECKIN_INDEX_FILE = "/d/webdocs/projects/bonsai/data/index${CVS_REPOS_SUFIX}";

require 'cvsquery.pl';

print "cvsroot='$CVS_ROOT'\n";

&build_who;

flock(SEMFILE, 8);              # '8' is magic 'unlock' const.
close SEMFILE;


sub build_who {
    open(BUILDLOG, "<$tree/build.dat" );
    $line = <BUILDLOG>;
    close(BUILDLOG);

    #($j,$query_date_min) = split(/\|/, $line);
    $query_date_min = time - (60 * 60 * 24 * $days);

    if( $F_DEBUG ){
        print "Minimum date: $query_date_min\n";
    }

    $query_module=$cvs_module;
    $query_branch=$cvs_branch;

    $result = &query_checkins;

    
    $last_who='';
    $last_date=0;
    open(WHOLOG, ">$tree/who.dat" );
    for $ci (@$result) {
        if( $ci->[$CI_DATE] != $last_date || $ci->[$CI_WHO] != $last_who ){
            print WHOLOG "$ci->[$CI_DATE]|$ci->[$CI_WHO]\n";
        }
        $last_who=$ci->[$CI_WHO];
        $last_date=$ci->[$CI_DATE];
    }
    close( WHOLOG );
}



sub update_history {
#execute: 
#cvs -d $d_args -a -c -D $begin_date $module

#parse:
  my ($type, $month_day, $time, $time_zone, 
      $author, $revision, $basename, $dirname, @ignore)

  = split (/ +/, $cvs_line[1])

}


# Check to see if anyone checked in during time slot.
#   ex.  has_who_list(1);    # Check for checkins in most recent time slot.
#   ex.  has_who_list(1,5);  # Check range of times.
sub has_who_list {
  my ($time1, $time2) = @_;

  if (not defined(@who_check_list)) {
    # Build a static array of true/false values for each time slot.
    $who_check_list[$time_count] = 0;
    my ($t) = 1;
    for (; $t<=$time_count; $t++) {
      $who_check_list[$t] = 1 if each %{$who_list->[$t]};
    }
  }
  if ($time2) {
    for ($ii=$time1; $ii<=$time2; $ii++) {	 
      return 1 if $who_check_list[$ii]
    }
    return 0
  } else {
    return $who_check_list[$time1]; 
  }
}



sub load_who {
  my ($who_list, $td) = @_;
  my $d, $w, $i, $bfound;
  
  open(WHOLOG, "<$td->{name}/who.dat");
  while (<WHOLOG>) {
    $i = $time_count;
    chop;
    ($d,$w) = split /\|/;
    $bfound = 0;
    while ($i > 0 and not $bfound) {
      if ($d <= $build_time_times->[$i]) {
        $who_list->[$i+1]->{$w} = 1;
        $bfound = 1;
      }
      else {
        $i--;
      }
    }
  }

  # Ignore the last one
  #
  if ($time_count > 0) {
    $who_list->[$time_count] = {};
  }
}



sub html_row {
"<tr align=center> @row </tr>\n";

  # Guilty
  #
  print '<td align=center>';
  for $who (sort keys %{$who_list->[$tt]} ){
    $qr = &who_menu($td1, $build_time_times->[$tt],
                    $build_time_times->[$tt-1],$who);
    $who =~ s/%.*$//;
    print "  ${qr}$who</a>\n";
  }
  print '</td>';
   
}


1;

