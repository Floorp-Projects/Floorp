#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

use strict;

require 'CGI.pl';

use vars qw($CloseTimeStamp);

print "Content-type: text/html\n\n";
LoadCheckins();

my $maxsize = 400;

PutsHeader("Beancounter central", "Meaningless checkin statistics");

print "
<TABLE BORDER CELLSPACING=2><TR>
<TH>Tree closed</TH>
<TH>Number<BR>of<BR>people<BR>making<BR>changes</TH>
<TH COLSPAN=2>Number of checkins</TH>
</TR>\n";

my @list = ();
my $globstr = DataDir() . '/batch-*[0-9].pl';

foreach my $i (glob($globstr )) {
     if ($i =~ /(\d+)/) {
          push @list, $1;
     }
}

@list = sort { $b <=> $a } @list;
my $first = 1;
my $biggest = 1;
my %minfo;  # meaninglesss info

foreach my $i (@list) {
     my $batch = DataDir() . "/batch-$i.pl";
     require $batch;

     $minfo{$i}{num} = scalar @::CheckInList;
     $biggest = $minfo{$i}{num} if ($minfo{$i}{num} > $biggest);
     if ($first) {
          $minfo{$i}{donetime} = "Current hook";
          $first = 0;
     } else {
          $minfo{$i}{donetime} = MyFmtClock($::CloseTimeStamp);
     }

     my %people = ();
     foreach my $checkin (@::CheckInList) {
          my $info = eval("\\\%$checkin");
          $people{$$info{'person'}} = 1;
     }
     $minfo{$i}{numpeople} = scalar keys(%people);
}


foreach my $i (@list) {
     print "<tr>\n";
     print "<TD>$minfo{$i}{donetime}</TD>\n";
     print "<TD ALIGN=RIGHT>$minfo{$i}{numpeople}</TD>\n";
     print "<TD ALIGN=RIGHT>$minfo{$i}{num}</TD>\n";
     printf "<TD><table WIDTH=%d bgcolor=green>\n",
           ($minfo{$i}{num} * $maxsize) / $biggest;
     print "<tr><td>&nbsp;</td></tr></table></TD>\n";
     print "</TR>\n";
}

print "</table>\n";
PutsTrailer();
exit;
