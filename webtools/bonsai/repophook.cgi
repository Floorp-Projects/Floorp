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

print "Content-type: text/html

<HTML>";

CheckPassword($::FORM{'password'});

my $startfrom = ParseTimeAndCheck(FormData('startfrom'));

Lock();
LoadTreeConfig();
LoadDirList();
LoadCheckins();
@::CheckInList = ();


$| = 1;

ConnectToDatabase();

print "<TITLE> Rebooting, please wait...</TITLE>

<H1>Recreating the hook</H1>

<h3>$::TreeInfo{$::TreeID}->{'description'}</h3>

<p>
Searching for first checkin after " . SqlFmtClock($startfrom) . "...<p>\n";

my $branch = $::TreeInfo{$::TreeID}->{'branch'};
print "<p> $branch <p> \n";

my $sqlstring = "select type, UNIX_TIMESTAMP(ci_when), people.who, repositories.repository, dirs.dir, files.file, revision, stickytag, branches.branch, addedlines, removedlines, descs.description from checkins,people,repositories,dirs,files,branches,descs where people.id=whoid and repositories.id=repositoryid and dirs.id=dirid and files.id=fileid and branches.id=branchid and descs.id=descid and branches.branch='$branch' and ci_when>='" . SqlFmtClock($startfrom) . "' order by ci_when;";
print "<p> $sqlstring <p>\n";
SendSQL("$sqlstring");

my ($change, $date, $who, $repos, $dir, $file, $rev, $sticky, $branch, $linesa, $linesr, $log);
my ($lastchange, $lastdate, $lastwho, $lastrepos, $lastdir, $lastrev, $laststicky, $lastbranch, $lastlinesa, $lastlinesr, $lastlog);
my ($id, $info, $lastdate, @files, @fullinfo);
my ($d, $f, $okdir, $full);
my ($r);
$lastdate = "";
$lastdir = "";
@files = ();
@fullinfo = ();
while (($change, $date, $who, $repos, $dir, $file, $rev, $sticky, $branch, $linesa, $linesr, $log) = FetchSQLData()) {
#	print "<p>$change $date $who $repos $dir $file $rev $sticky $branch $linesa $linesr $log<p>\n ";
if (($date ne $lastdate && $lastdate ne "") || ($dir ne $lastdir && $lastdir ne "")) {

	$okdir = 0;
LEGALDIR:
	foreach $d (sort( grep(!/\*$/, @::LegalDirs))) {
		if ($lastdir =~ m!^$d\b!) {
			$okdir = 1;
			last LEGALDIR;
		}
	}
	if ($okdir) {
		print "<br>";
		print "$lastchange $lastdate $lastwho $lastrepos <br> $lastdir ";
		print "<br>";
		foreach $f (@files) { print "$f ";}
		print " <br>$lastrev $laststicky $lastbranch $lastlinesa $lastlinesr <br>$lastlog";
		print "\n<br>--------------------------------------------------------<br>\n";
		$r++;
		$id = "::checkin_${lastdate}_$r";
		push @::CheckInList, $id;
	
		$info = eval("\\\%$id");
		%$info = (
		person	  => $lastwho,
		date	  => $lastdate,
		dir	  => $lastdir,
		files	  => join('!NeXt!', @files),
		'log'	  => MarkUpText(html_quote(trim($lastlog))),
		treeopen => $::TreeOpen,
		fullinfo => join('!NeXt!', @fullinfo)
		);
	}

@files = ();
@fullinfo = ();
}
$lastchange = $change;
$lastdate = $date;
$lastwho = $who;
$lastrepos = $repos;
$lastdir = $dir;
$lastrev = $rev;
$laststicky = $sticky;
$lastbranch = $branch;
$lastlinesa = $linesa;
$lastlinesr = $linesr;
$lastlog = $log;

if (!($file=~/Tag:/ || ($file=~/$branch/) && ($branch) )) {
push @files, $file;
push @fullinfo, "$file|$rev|$linesa|$linesr|";
}
}

WriteCheckins();
Unlock();

print "<p>OK, done. \n";

PutsTrailer();
