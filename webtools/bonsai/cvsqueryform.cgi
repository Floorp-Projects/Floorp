#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

# Query the CVS database.
#
require 'lloydcgi.pl';
require 'cvsmenu.pl';
require 'utils.pl';


$|=1;

$CVS_ROOT = $form{"cvsroot"};

print "Content-type: text/html\n\n";

require 'modules.pl';


EmitHtmlHeader("CVS Query Form", $CVS_ROOT);


print "
<p>
<FORM METHOD=GET ACTION='cvsquery.cgi'>
<p>
<TABLE BORDER CELLPADDING=8 CELLSPACING=0>
";


#
# module selector
#
print "
<TR><TH ALIGN=RIGHT>Module:</TH>
<TD>
<SELECT name='module' size=5>
";


#
# check to see if there are multple repositories
#
@reposList = &getRepositoryList();
$bMultiRepos = (@reposList > 1);

#
# This code sucks, I should rewrite it to be shorter
#
if( $form{module} eq 'all' || $form{module} eq '' ){
    print "<OPTION SELECTED VALUE='all'>All Files in the Repository\n";
    if( $bMultiRepos ){
        print "<OPTION VALUE='allrepositories'>All Files in all Repositories\n";
    }
}
elsif( $form{module} eq 'allrepositories' ){
    print "<OPTION VALUE='all'>All Files in the Repository\n";
    if( $bMultiRepos ){
        print "<OPTION SELECTED VALUE='allrepositories'>All Files in all Repositories\n";
    }
}
else {
    print "<OPTION VALUE='all'>All Files in the Repository\n";
    if( $bMultiRepos ){
        print "<OPTION VALUE='allrepositories'>All Files in all Repositories\n";
    }
    print "<OPTION SELECTED VALUE='$form{module}'>$form{module}\n";
}

#
# Print out all the Different Modules
#
for $k  (sort( keys( %$modules ) ) ){
    print "<OPTION value='$k'>$k\n";
}

print "</SELECT></td>\n";
print "<td rowspan=2>";
cvsmenu();
print "</td></tr>";

#
# Branch
#
if( defined $form{branch} ){
    $b = $form{branch};
}
else {
    $b = "HEAD";
}
print "<tr>
<th align=right>Branch:</th>
<td> <input type=text name=branch value='$b' size=25><br>\n" .
regexpradio('branchtype') .
      "<br>(leaving this field empty will show you checkins on both
<tt>HEAD</tt> and branches)
</td></tr>";

#
# Query by directory
#
print "
<tr>
<th align=right>Directory:</th>
<td colspan=2>
<input type=text name=dir value='$form{dir}' size=45><br>
(you can list multiple directories)
</td>
</tr>
";

print "
<tr>
<th align=right>File:</th>
<td colspan=2>
<input type=text name=file value='$form{file}' size=45><br>" .
regexpradio('filetype') . "
</td>
</tr>
";


#
# Who
#
print "
<tr>
<th align=right>Who:</th>
<td colspan=2> <input type=text name=who value='$form{who}' size=45><br>" .
regexpradio('whotype') . "
</td>
</tr>";


#
# Log contains
#
#print "
#<br>
#<nobr><b>Log contains:</b>
#<input type=text name=logexpr size=45></nobr>(you can use <a href=cvsregexp.html>regular expressions</a>)\n";


#
# Sort order
#
print "
<tr>
<th align=right>Sort By:</th>
<td colspan=2>
<SELECT name='sortby'>
<OPTION" . &sortTest("Date") . ">Date
<OPTION" . &sortTest("Who") . ">Who
<OPTION" . &sortTest("File") . ">File
<OPTION" . &sortTest("Change Size") . ">Change Size
</SELECT>
</td>
</tr>
";

#
# Print the date selector
#

$CVS_REPOS_SUFFIX = $CVS_ROOT;
$CVS_REPOS_SUFFIX =~ s:/:_:g;

$startdate = fetchCachedStartDate($CVS_ROOT);

if ($form{date} eq "") {
    $form{date} = "hours";
}

print "
<tr>
<th align=right valign=top><br>Date:</th>
<td colspan=2>
<table BORDER=0 CELLSPACING=0 CELLPADDING=0>
<tr>
<td><input type=radio name=date " . &dateTest("hours") . "></td>
<td>In the last <input type=text name=hours value=2 size=4> hours</td>
</tr><tr>
<td><input type=radio name=date " . &dateTest("day") . "></td>
<td>In the last day</td>
</tr><tr>
<td><input type=radio name=date " . &dateTest("week") . "></td>
<td>In the last week</td>
</tr><tr>
<td><input type=radio name=date " . &dateTest("month") . "></td>
<td>In the last month</td>
</tr><tr>
<td><input type=radio name=date " . &dateTest("all") . "></td>
<td>Since the beginning of time (which happens to be <TT><NOBR>$startdate</NOBR></TT> currently)</td>
</tr><tr>
<td><input type=radio name=date " . &dateTest("explicit") . "></td>
<td><table BORDER=0 CELLPADDING=0 CELLPSPACING=0>
<tr>
<TD VALIGN=TOP ALIGN=RIGHT NOWRAP>
Between <input type=text name=mindate value='$form{mindate}' size=25></td>
<td valign=top rowspan=2>You can use the form
<B><TT><NOBR>mm/dd/yy hh:mm:ss</NOBR></TT></B> or a Unix <TT>time_t</TT>
(seconds since the Epoch.)
</td>
</tr>
<tr>
<td VALIGN=TOP ALIGN=RIGHT NOWRAP>
 and <input type=text name=maxdate '$form{maxdate}' size=25></td>
</tr>
</table>
</td>
</tr>
</table>
</tr>
";

print "
<tr>
<th><BR></th>
<td colspan=2>
<INPUT TYPE=HIDDEN NAME=cvsroot VALUE='$CVS_ROOT'>
<INPUT TYPE=SUBMIT VALUE='Run Query'>
</td>
</tr>
</table>
</FORM>";

sub sortTest {
    if( $_[0] eq $form{sortby} ){
        return " SELECTED";
    }
    else {
        return "";
    }
}

refigureStartDateIfNecessary($CVS_ROOT);

sub dateTest {
    if( $_[0] eq $form{date} ){
        return " CHECKED value=$_[0]";
    }
    else {
        return "value=$_[0]";
    }
}

sub regexpradio {
    my($name) = @_;
    my $c1,$c2,$c3;
    $c1 = $c2 = $c3 = "";
    if( $form{$name} eq 'regexp'){
        $c2 = "checked";
    }
    elsif( $form{$name} eq 'notregexp'){
        $c3 = "checked";
    }
    else {
        $c1 = "checked";
    }
    return "
<input type=radio name=$name value=match $c1>Exact match
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<input type=radio name=$name value=regexp $c2><a href=cvsregexp.html>Regular&nbsp;expression</a>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<input type=radio name=$name value=notregexp $c3>Doesn't&nbsp;match&nbsp;<a href=cvsregexp.html>Reg&nbsp;Exp</a>";
}


sub fetchCachedStartDate {
    my ($repository) = @_;
    open(CACHE, "<data/cachedstartdates") || return "unknown";
    while (<CACHE>) {
        chop();
        my($rep,$date,$cachedate) = split(/\|/);
        if ($rep eq $repository) {
            $rememberedcachedate = $cachedate;
            return $date;
        }
    }
    return "unknown";
}

sub refigureStartDateIfNecessary {
    my ($repository) = @_;
    my $now = time();
    if ((defined $rememberedcachedate) &&
        $now - $rememberedcachedate < 24*60*60 &&
        $rememberedcachedate < $now) {
        return;
    }

    my $db = ConnectToDatabase();

    my $query = $db->prepare( "select min(when) from checkins,repositories where repositories.id = repositoryid and repository = '$CVS_ROOT'") || die $DBD::mysql::db_errstr;

    my @row = $query->fetchrow();
    my $startdate = $row[0];
    if ($startdate eq "") {
        $startdate = "nonexistant";
    }
    open(OUTCACHE, ">data/cachedstartdates.$$") || die "Can't open output date cache file";
    if (open(INCACHE, "<data/cachedstartdates")) {
        while (<INCACHE>) {
            chop();
            my($rep,$date,$cachedate) = split(/\|/);
            if ($rep ne $repository) {
                print OUTCACHE "$_\n";
            }
        }
        close INCACHE;
    }
    print OUTCACHE "$repository|$startdate|$now\n";
    close OUTCACHE;
    rename "data/cachedstartdates.$$", "data/cachedstartdates";
}
