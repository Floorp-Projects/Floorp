# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::Time_Local - A Java Script replacement for the Time.pm
# module. This module displays the time column in the local time (to
# the browser, not the server).

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



# $Revision: 1.3 $ 
# $Date: 2003/08/17 16:04:27 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/Time_Local.pm,v $ 
# $Name:  $ 

package TinderDB::Time_Local;

# Load standard perl libraries

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use VCDisplay;



$VERSION = ( qw $Revision: 1.3 $ )[1];


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

    my $out =<<'EOF';

    <noscript>Time (UTC)</noscript>

    <script type="text/javascript"><!-- 
        now = new Date();
        offset = now.getTimezoneOffset();
        document.write("Local Time (UTC");
        if (offset < 0) document.write("+");
        if (offset) document.write(-offset/60);
        document.write(")");

        function show_time(month, day, hours, mins, show_date) {
	    now.setMonth(month-1);
	    now.setDate(day);
	    now.setHours(hours);
   	    now.setMinutes(mins-offset);
	    if (show_date) 
		document.write( (now.getMonth()+1) + "/" + now.getDate());
	    document.write("&nbsp;");
	    document.write(now.getHours() + ":");
	    if (now.getMinutes() <= 9) document.write("0");
 	    document.write(now.getMinutes());
        }

    //--></script>
EOF

  return ("\t<th><b>$out</b></th>\n");
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
  my ($pretty_time) = HTMLPopUp::gmtimeHTML($time);
  my ($month, $day) = ( $pretty_time =~ m!(\d\d)/(\d\d)! );
  my ($hour, $min) = ( $pretty_time =~ m!(\d\d):(\d\d)! );

  # if it is a new hour or we have checkins for this period make a
  # link for a cvs query for all checkins since this time otherwise
  # just display the date.

  my $display_time = '';
  $display_time .= "\t\t<noscript>$pretty_time</noscript>\n";

  my ($query_link) = '';
  if ($self->last_hour() != $hour) {


      $display_time .= (
                        "\t\t<script type=\"text/javascript\">".
                        "show_time($month,$day,$hour,$min, 1);".
                        "</script>\n".
                        "");

      # First make a popup window which tells the server time and UTC
      # time. OTherwise these times might be hard for developers who
      # travel a bunch to figure out.

      # I would like to put the local browser time in this popup
      # window, but how? The VCDisplay popup window expects a static
      # string, I would need to change everything to allow part of the
      # string to be an interpolated JavaScript function. Perhaps I
      # could change the code to take a string OR a reference to a
      # list.  The list would contain either strings or
      # javascript. This would solve the problem, but is it worth it
      # for one tiny portion of the tinderbox code? For now just leave
      # out the local time.  Users can see that time anyway since it
      # is in the cell that they click on to get this window.

      my $vc_link = "\t\t".
          VCDisplay::query(
                           'tree' => $tree,
                           'mindate' => $time,
                           'linktxt' => "Check-ins at this time",
                           );
      
      my ($pretty_gmtime) = HTMLPopUp::gmtimeHTML($time); 
      my ($pretty_servertime) = HTMLPopUp::timeHTML($time);
      
      my $txt = (
                 "UTC: $pretty_gmtime<br>\n".
                 "Server Time: $pretty_servertime<br>\n".
                 "$vc_link<br>\n".
                 "");

      my $title = "Tinderbox Times";
      
      # Now make the link for the time cell.

      $query_link = VCDisplay::query(
                                     "windowtxt"=>$txt,
                                     "windowtitle" => $title,
                                     
                                     'tree' => $tree,
                                     'mindate' => $time,
                                     'linktxt' => $display_time,
                                     );

  } else {

      $display_time .= (
                        "\t\t<script type=\"text/javascript\">".
                        "show_time($month,$day,$hour,$min, 0);".
                        "</script>\n".
                        "");
      
      $query_link =   $display_time;
  }

  # the background for odd hours is a light grey, 
  # even hours have white.

  my $hour_color = '';
  if ($hour % 2) {
    $hour_color = "bgcolor=#e7e7e7";
  }

  my(@outrow) = ("\t<!-- Time_Local: $pretty_time -->\n".
                 "\t\t<td align=right $hour_color>\n".
                 $query_link.
                 "\t\t</td>\n");

  $self->last_hour($hour);
  return @outrow;
}


1;
__END__

