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


package TinderDB::Time;

# Load standard perl libraries

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use VCDisplay;



$VERSION = ( qw $Revision: 1.4 $ )[1];

# Add an empty object, of this DB subclass, to end of the set of all
# HTML columns.  This registers the subclass with TinderDB and defines
# the order of the HTML columns.

push @TinderDB::HTML_COLUMNS, TinderDB::Time->new();


sub new {
  my $type = shift;
  my %params = @_;
  my $self = {};
  bless $self, $type;
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


sub apply_db_updates {
  return 0;
}


sub status_table_header {
  return ("\t<th><b>Build Time</b></th>\n");
}


sub status_table_legend {

  return ;
}


sub status_table_start {
  my ($self, $row_times, $tree,) = @_;

  $LAST_HOUR= -1;
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
  if ($LAST_HOUR != $hour) {

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

  my(@outrow) = ("\t<td align=right $hour_color>".
                 "$query_link</td>\n");

  $LAST_HOUR = $hour;
  return @outrow;
}


1;
__END__

