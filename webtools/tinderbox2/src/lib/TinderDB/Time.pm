# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::Time - the module responsible for generating the time
# column in the status page main table.

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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 



# $Revision: 1.12 $ 
# $Date: 2003/06/18 15:49:50 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/Time.pm,v $ 
# $Name:  $ 

package TinderDB::Time;

# Load standard perl libraries

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use VCDisplay;



$VERSION = ( qw $Revision: 1.12 $ )[1];


sub new {
  my $type = shift;
  my %params = @_;
  my $self = {};
  bless $self, $type;
}

# last_hour is specific to each time column in the table it stores the
# hour of the time we were computing between calls to
# status_table_row.  Using this data we can set the full date
# appropriately.

sub last_hour {
    my $self = shift;
    if (@_) { $self->{LASTHOUR} = shift }
    return $self->{LASTHOUR};
}


sub loadtree_db {
  return ;
}


sub savetree_db {
  return ;
}


sub trim_db_history {
  return ;
}


sub event_times_vec {
  return ;
}


sub apply_db_updates {
  return 0;
}



# where can people attach notices to?
# Really this is the names the columns produced by this DB

sub notice_association {
    return '';
}

sub status_table_header {
  return ("\t<th><b>Build Time</b></th>\n");
}


sub status_table_legend {

  return ;
}


sub status_table_start {
  my ($self, $row_times, $tree,) = @_;

  $self->last_hour(-1);
}



sub status_table_row {
  my ($self, $row_times, $row_index, $tree,) = @_;

  my $time = $row_times->[$row_index];
  my ($pretty_time) = HTMLPopUp::timeHTML($time);
  my ($hour) = ( $pretty_time =~ m/(\d\d):/ );

  # if it is a new hour or we have checkins for this period make a
  # link for a cvs query for all checkins since this time otherwise
  # just display the date.

  my ($query_link) = '';
  if ($self->last_hour() != $hour) {

    $query_link = VCDisplay::query(
				    'tree' => $tree,
				    'mindate' => $time,
				    'linktxt' => $pretty_time,
				   );
 } else {
    # remove the month/day
    $pretty_time =~ s/^.*&nbsp;//;

    $query_link = $pretty_time;
  }

  # the background for odd hours is a light grey, 
  # even hours have white.

  my $hour_color = '';
  if ($hour % 2) {
    $hour_color = "bgcolor=#e7e7e7";
  }

  my(@outrow) = ("\t<!-- Time: $pretty_time -->\n".
                 "\t\t<td align=right $hour_color>\n".
                 "\t\t\t$query_link\n".
                 "\t\t</td>\n");

  $self->last_hour($hour);
  return @outrow;
}


1;
__END__

