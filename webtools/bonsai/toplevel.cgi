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

require 'CGI.pl';

use strict;

sub StupidFuncToShutUpWarningsByUsingVarsAgain {
    my $z;
    $z = $::CloseTimeStamp;
    $z = $::LastGoodTimeStamp;
    $z = $::MOTD;
    $z = $::WhiteBoard;
    $z = $::TreeList;
}

print "Content-type: text/html\nRefresh: 300\n\n";

PutsHeader("Bonsai -- the art of effectively controlling trees",
           "Bonsai", "CVS Tree Control");

print "<IMG ALIGN=right SRC=bonsai.gif>";


Lock();
LoadCheckins();
LoadMOTD();
LoadWhiteboard();
LoadTreeConfig();
Unlock();


my $openword;
if ($::TreeOpen) {
    $openword = '<b><FONT SIZE=+2>OPEN</FONT></B>';
} else {
    $openword = '<b><FONT SIZE=+3 COLOR=RED>CLOSED</FONT></B>';
}


print "

<FORM name=treeform>
<H3>
<SELECT name=treeid size=1 onchange='submit();'>
";


foreach my $tree (@::TreeList) {
     unless (exists $::TreeInfo{$tree}{nobonsai}) {
          my $c = ($tree eq $::TreeID) ? 'Selected' : '';
          print "<OPTION VALUE=\"$tree\" $c>$::TreeInfo{$tree}{description}\n";
    }
}

print "</SELECT></H3></FORM>\n";


my $treepart = '';
$treepart = "&treeid=$::TreeID"
    if ($::TreeID ne "default");
my $branchpart='';
$branchpart="&branch=$::TreeInfo{$::TreeID}{branch}"
    if $::TreeInfo{$::TreeID}{branch};
if (Param('readonly')) {
    print "<div style='border: 2px dotted grey; padding: 2px'><h2><font color=red>
Be aware that this is <em>not</em> the <a href='toplevel.cgi?$treepart$branchpart'>current 
hook!</a></font></h2>\n";
} else {
    print "<div><tt>" . time2str("%Y-%m-%d %T %Z", time())."</tt>:";
}
print " The tree is currently $openword<br>\n";
unless ($::TreeOpen) {
    print "The tree has been closed since <tt>" .
         MyFmtClock($::CloseTimeStamp) . "</tt>.<BR>\n";
}

print "</div>The last known good tree had a timestamp of <tt>";
print time2str("%Y-%m-%d %T %Z", $::LastGoodTimeStamp) . "</tt>.<br>";
print "<hr><pre variable>$::MOTD</pre><hr>";
print "<br clear=all>";

my $bid_part = BatchIdPart('?');
print "<b><a href=editwhiteboard.cgi$bid_part>
Free-for-all whiteboard:</a></b>
<pre>" . html_quote($::WhiteBoard) . "</pre><hr>\n";


my %username;
my %checkincount;
my %closedcheckin;
my %fullname;
my %curcontact;

foreach my $checkin (@::CheckInList) {
    my $info = eval("\\\%$checkin");
    my $addr = EmailFromUsername($info->{'person'});

    $username{$addr} = $info->{'person'};
    if (!exists $checkincount{$addr}) {
        $checkincount{$addr} = 1;
    } else {
        $checkincount{$addr}++;
    }
    if (!$info->{'treeopen'}) {
        if (!defined $closedcheckin{$addr}) {
            $closedcheckin{$addr} = 1;
        } else {
            $closedcheckin{$addr}++;
        }
    }
}


my $ldaperror = 0;

if (%checkincount) {
     my (@peoplelist, @list, $p, $i, $end, $checkins);
     my $ldapserver = Param('ldapserver');
     my $ldapport = Param('ldapport');

     print  "
The following people are on \"the hook\", since they have made
checkins to the tree since it last opened: <p>\n";

     @peoplelist = sort(keys %checkincount);
     
     @list = @peoplelist;
     while (1) {
          last if ($#list < 0);
          $end = 19;
          $end = $#list if ($end >= $#list);
          GetInfoForPeople(splice(@list, 0, $end + 1));
     }

     if ($ldaperror) {
         print "<font color=red>
Can't contact the directory server at $ldapserver:$ldapport</font>\n";
     }

     print "
<table border cellspacing=2>
<th colspan=2>Who</th><th>What</th>\n";
     print "<th>How to contact</th>\n" if $ldapserver;

     foreach $p (sort {uc($a) cmp uc($b)} @peoplelist) {
          my ($uname, $namepart, $extra) = ('', '', '');

          if (exists($closedcheckin{$p})) {
               $extra = " <font color=red>($closedcheckin{$p} while tree closed!)</font>";
          }

          $uname = $username{$p};
          ($namepart = $p) =~ s/\@.*//;
          $checkins = $checkincount{$p};

          print "<tr>\n";
          if ($fullname{$p}) {
              print "<td>$fullname{$p}</td>\n<td>";
          } else {
              print "<td colspan=2>";
          }
          print GenerateUserLookUp($uname, $namepart, $p) . "</td>\n";
          print "<td><a href=\"showcheckins.cgi?person=" . url_quote($uname);
          print BatchIdPart() . "\"> $checkins ";
          print Pluralize('change', $checkins) . "</a>$extra</td>\n";
          print "<td>$curcontact{$p}\n" if $ldapserver;
          print "</tr>\n\n";
     }

     print "</table>\n\n";

     
     $checkins = @::CheckInList;
     print Pluralize("$checkins checkin", $checkins) . ".<p>\n";

     my $mailaddr =
         join(',', @peoplelist) . "?subject=Hook%3a%20Build%20Problem";
     $mailaddr .= "&cc=$::TreeInfo{$::TreeID}{cchookmail}"
          if (exists($::TreeInfo{$::TreeID}{cchookmail}));


     print "
<a href=showcheckins.cgi" . BatchIdPart('?') . ">Show all checkins.</a><br>
<a href=\"mailto:$mailaddr\">Send mail to \"the hook\".</a><br>\n";

} else {
   print "Nobody seems to have made any changes since the tree opened.";
}


my $cvsqueryurl = "cvsqueryform.cgi?" .
    "cvsroot=$::TreeInfo{$::TreeID}{repository}" .
    "&module=$::TreeInfo{$::TreeID}{module}" .
    $branchpart;
my $bip = BatchIdPart('?');
my $tinderboxbase = Param('tinderboxbase');
my $tinderboxlink = '';
$tinderboxlink = "<a href=\"$tinderboxbase/showbuilds.cgi\">Tinderbox
        continuous builds</a><br>" if ($tinderboxbase);

my $otherrefs = Param('other_ref_urls');

print "
<hr>
<table>
<tr>
<th>Useful links </th><th width=10%></th><th>Help and Documentation</th>
</tr>
<tr>
<td valign=top>
<a href=\"$cvsqueryurl\"><b>CVS Query Tool</b></a><br>
<a href=\"switchtree.cgi$bip\">Switch to look at a different tree or branch</a><br>
$tinderboxlink
<a href=\"viewold.cgi$bip\">Time warp -- view a different day's hook.</a><br>
<a href=\"countcheckins.cgi$bip\">See some stupid statistics about recent checkins.</a><br>
<a href=\"admin.cgi$bip\">Administration menu.</a><br>
</td><td>
</td><td valign=top>
$otherrefs
</td>
</tr></table>
" ;

exit 0;



sub GetInfoForPeople {
     my (@peoplelist) = @_;
     my ($p, $query, $isempty);
     my $ldapserver = Param('ldapserver');
     my $ldapport = Param('ldapport');
     my $ldapcmd;

     $query = "(| ";
     $isempty = 1;

     foreach $p (@peoplelist) {
          $query .= "(mail=$p) ";
          $fullname{$p} = "";
          $curcontact{$p} = "";
     }

     $query .= ")";
     return if ($ldaperror || ($ldapserver eq ''));

     $ldapcmd = "./data/ldapsearch -b \"dc=netscape,dc=com\" " .
                "-h $ldapserver -p $ldapport -s sub " .
                "-S mail \"$query\" mail cn nscpcurcontactinfo";
     unless (open(LDAP, "$ldapcmd |")) {
          $ldaperror = 1;
     } else {
          my $doingcontactinfo = 0;
          my $curperson;
          while (<LDAP>) {
               chop;
               if ($doingcontactinfo) {
                    if (/^ (.*)$/) {
                         $curcontact{$curperson} .= "$1\n";
                         next;
                    }
                    $doingcontactinfo = 0;
               }
               if (/^mail: (.*\@.*)$/) {
                    $curperson = $1;
               } elsif (/^cn: (.*)$/) {
                    $fullname{$curperson} = $1;
               } elsif (/^nscpcurcontactinfo: (.*)$/) {
                    $curcontact{$curperson} = "$1\n";
                    $doingcontactinfo = 1;
               }
          }
          close(LDAP);
     }
}

