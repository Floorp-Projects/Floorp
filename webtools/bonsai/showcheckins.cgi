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
use vars qw(@TreeList);

print "Content-type: text/html\n\n";

Lock();
LoadCheckins();
LoadTreeConfig();
Unlock();

my %peoplearray = ();
my @list = ();
my $versioninfo = '';
my $tweak = $::FORM{'tweak'};
my $delta_size = 1;
my ($title, $head, $subhead) = ('', '', '');
my ($checkin, $info);

sub BreakBig {
     my ($str) = @_;
     my $result = '';

     while (length($str) > 20) {
          my $head = substr($str, 0, 19);
          my $w = rindex($head, "/");

          $w = 19 if ($w < 0);
          $result .= substr($str, 0, $w++) . "<br>";
          $str = substr($str, $w);
     }
     return $result . $str;
}


if (exists($::FORM{'person'})) {
     my $escaped_person = html_quote($::FORM{'person'});
     $title = $head = "Checkins for $escaped_person";

     foreach $checkin (@::CheckInList) {
          $info = eval("\\\%$checkin");
          push @list, $checkin
               if ($$info{'person'} eq $::FORM{'person'});
     }
} elsif (exists($::FORM{'mindate'}) || exists($::FORM{'maxdate'})) {
     my ($min, $max) = (0, 1<<30);

     $title = "Checkins";
     if (exists($::FORM{'mindate'})) {
          $title .= " since " . MyFmtClock($min = $::FORM{'mindate'});
          $title .= " and" if (exists($::FORM{'maxdate'}));
     }
     $title .= " before" . MyFmtClock($max = $::FORM{'maxdate'})
          if (exists($::FORM{'maxdate'}));
     $head = $title;

     foreach $checkin (@::CheckInList) {
          $info = eval("\\\%$checkin");
          push @list, $checkin
               if (($$info{'date'} >= $min) && ($$info{'date'} <= $max));
     }
} else {
     $title = $head = "All Checkins";
     @list = @::CheckInList;
}

my $treepart = '';
$treepart = "&treeid=$::TreeID"    if ($::TreeID ne "default");
my $branchpart = '';
$branchpart = "&branch=$::TreeInfo{$::TreeID}{branch}"
     if ($::TreeInfo{$::TreeID}{branch});

$subhead .= "<br><font color=red>
These checkins are <em>not</em> from <a 
href='showcheckins.cgi?$treepart$branchpart'>the current 
hook</a>!</font><br>"
     if (Param('readonly'));
$subhead .= "View a <a href=\"viewold.cgi?" . BatchIdPart('?') . "&target=showcheckins\">different 
day's checkins</a>.<br>";

PutsHeader($title, $head, $subhead);

$::FORM{'sort'} = 'date' unless $::FORM{'sort'};

print "
(Current sort is by <tt>$::FORM{'sort'}</tt>; click on a column header
to sort by that column.)";

my @fields = split(/,/, $::FORM{'sort'});

sub Compare {
     my $rval = 0;
     my $key;
     my $aref = eval("\\\%$a");
     my $bref = eval("\\\%$b");

     foreach $key (@fields) {
         if ($key eq 'date') {
             $rval = $$bref{$key} cmp $$aref{$key};
         } else {
             $rval = $$aref{$key} cmp $$bref{$key};
         }
         return $rval unless ($rval == 0);
     }
     return $rval;
}

my $total_added = 0;
my $total_removed = 0;

#
# Calculate delta information
#
CHECKIN:
foreach my $infoname (@list) {
     $info = eval("\\\%$infoname");
     $$info{added} = 0;
     $$info{removed} = 0;

     if (exists($$info{'fullinfo'})) {
          my @fullinfos = split(/!NeXt!/, $$info{'fullinfo'});
INFO:
          foreach my $fullinfo (@fullinfos) {
               my ($file, $version, $addlines, $removelines, $sticky)
                    = split(/\|/, $fullinfo);

               # Skip binary files
               next INFO if (($file =~ /\.gif$/) ||
                             ($file =~ /\.bmp$/) ||
                             ($sticky =~ /-kb/));

               if ($addlines) {
                   $$info{added} += $addlines;
               }
               if ($removelines) {
                   $$info{removed} += $removelines;
               }
          }
     }

     $$info{'lines_changed'} =
          sprintf("%7d", 1000000 - ($$info{added} - $$info{removed}));

     $total_added += $$info{added};
     $total_removed += $$info{removed};
}


# Sort that puppy...
@list = sort Compare @list;

# $::buffer contains the arguments that we were called with, it is
# initialized by CGI.pl
my $otherparams;
($otherparams = $::buffer) =~ s/[&?]sort=[^&]*//g;

sub NewSort {
     my ($key) = @_;
     my @sort_keys = grep(!/^$key$/, split(/,/, $::FORM{'sort'}));
     unshift(@sort_keys, $key);

     return $otherparams . "&sort=" . join(',', @sort_keys);
}



#
# Print the table...
#

print "<FORM method=get action=\"dotweak.cgi\">\n" if $tweak;
print "<TABLE border cellspacing=2>\n<TR ALIGN=LEFT>\n\n";
print "<TH></TH>\n" if $tweak;

print "
<TH><A HREF=\"showcheckins.cgi?${otherparams}&sort=date\">When</A>
<TH><A HREF=\"showcheckins.cgi?" . NewSort('treeopen') . "\">Tree state</A>
<TH><A HREF=\"showcheckins.cgi?" . NewSort('person') . "\">Who</A>
<TH><A HREF=\"showcheckins.cgi?" . NewSort('dir') . "\">Directory</A>
<TH><A HREF=\"showcheckins.cgi?" . NewSort('files') . "\">Files</A>
<TH><A HREF=\"showcheckins.cgi?" . NewSort('lines_changed') . 
     "\"><tt>+/-</tt></A>
<TH WIDTH=100%>Description
</TR>\n\n";


my $count = 0;
my $maxcount = 100;

foreach $checkin (@list) {
     $info = eval("\\\%$checkin");

     # Don't make tables too big, or toy computers will break.
     if ($count++ > $maxcount) {
          $count = 0;
          print "</TABLE>\n\n<TABLE border cellspacing=2>\n";
     }

     print "<TR>\n";
     print "<TD><INPUT TYPE=CHECKBOX NAME=\"$checkin\"></TD>\n" if $tweak;
     print "<TD><a href=editcheckin.cgi?id=$checkin" . BatchIdPart(). ">\n";
     print time2str("<font size=-1>%Y-%m-%d %H:%M</font>" , $$info{date}) .
          "</a></TD>\n";
     print "<TD>" . (($$info{treeopen})? "open": "CLOSED") . "\n";
     print "<br>$$info{notes}\n" if $$info{notes};

     $peoplearray{$$info{person}} = 1;
     print "<TD>". GenerateUserLookUp($$info{person}) . "</TD>\n";
     print "<TD><a href=\"cvsview2.cgi?" . 
           "root=$::TreeInfo{$::TreeID}{repository}&" .
           "subdir=$$info{dir}&" .
           "files=" . join('+', split(/!NeXt!/, $$info{files})) . "&" .
           "command=DIRECTORY$branchpart\">" .
           BreakBig($$info{dir}) .
           "</a></TD>\n";
     print "<TD>\n";
     foreach my $file (split(/!NeXt!/, $$info{files})) {
          print "  <a href=\"cvsview2.cgi?" .
                "root=$::TreeInfo{$::TreeID}{repository}&" .
                "subdir=$$info{dir}&" .
                "files=$file&" .
                "command=DIRECTORY$branchpart\">" .
                "$file</a>\n";
     }
     print "</td>\n";

     print "<TD><tt>+$$info{added}/-". abs($$info{removed}). "</tt></td>\n";
     foreach my $fullinfo (split(/!NeXt!/, $$info{'fullinfo'})) {
          my ($file, $version) = split(/\|/, $fullinfo);
          $versioninfo .= "$$info{person}|$$info{dir}|$file|$version,";
     }
     my $comment = $$info{'log'};
     $comment =~ s/\n/<br>/g;
     print "<TD WIDTH=100%>$comment</td>\n";
     print "</tr>\n\n";
}
print "</table>\n";

print scalar @list  . " checkins listed.
&nbsp;&nbsp;&nbsp; Lines changed <tt>($total_added/$total_removed)</tt>.\n";

sub IsSelected {
     my ($value) = @_;

     return "SELECTED" if ($value eq  $::TreeID);
     return "";
}

if ($tweak) {
     print "
<hr>
Check the checkins you wish to affect.  Then select one of the below options.
And type the magic word.  Then click on submit.
<P>
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
<INPUT TYPE=radio NAME=command VALUE=nuke>Delete these checkins.<BR>
<INPUT TYPE=radio NAME=command VALUE=setopen>Set the tree state on these checkins to be <B>Open</B>.<BR>
<INPUT TYPE=radio NAME=command VALUE=setclose>Set the tree state on these checkins to be <B>Closed</B>.<BR>
<INPUT TYPE=radio NAME=command VALUE=movetree>Move these checkins over to this tree:
<SELECT NAME=desttree SIZE=1>\n";

     foreach my $tree (@::TreeList) {
          print "<OPTION ". IsSelected($tree).
               " VALUE=$tree>$::TreeInfo{$tree}{description}\n"
                    unless $::TreeInfo{$tree}{nobonsai};
     }

     print "
</SELECT><P>
<B>Password:</B><INPUT NAME=password TYPE=password></td>
<BR>
<INPUT TYPE=SUBMIT VALUE=Submit>
</FORM>\n";
} else {
     print "
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<a href=showcheckins.cgi?$::buffer&tweak=1>Tweak some of these checkins.</a>
<br><br>
<FORM action='multidiff.cgi' method=post>
<INPUT TYPE='HIDDEN' name='allchanges' value = '$versioninfo'>
<INPUT TYPE=SUBMIT VALUE='Show me ALL the Diffs'>
</FORM>\n";
}

if (exists $::FORM{ltabbhack}) {
     print "<!-- StupidLloydHack " . join(',', sort(keys(%peoplearray))) .
          " -->\n";
     print "<!-- LloydHack2 $versioninfo -->\n";
}

PutsTrailer();


