#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Dawn Endico    <endico@mozilla.org>
#                 Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;
use RelationSet;
use XML::Dumper;
use vars %::COOKIE;
require "CGI.pl";

#$::lockcount = 0;
my $dump = new XML::Dumper;

my $exporter = $::COOKIE{"Bugzilla_login"};

sub QuoteXMLChars {
    $_[0] =~ s/</&lt;/g;
    $_[0] =~ s/>/&gt;/g;
    $_[0] =~ s/'/&apos;/g;
    $_[0] =~ s/"/&quot;/g;
    $_[0] =~ s/&/&amp;/g;
#    $_[0] =~ s/([\x80-\xFF])/&XmlUtf8Encode(ord($1))/ge;
    return($_[0]);
}

#if ($::FORM{'GoAheadAndLogIn'}) {
#    confirm_login();
#}

confirm_login();
ConnectToDatabase();
GetVersionTable();

if (!defined $::FORM{'id'} || $::FORM{'id'} !~ /^\s*\d+\s*$/) {
    print "Content-type: text/html\n\n";
    PutHeader("Export by bug number");
    print "<FORM METHOD=GET ACTION=\"export_bug.cgi\">\n";
    print "You may export a single bug by entering its bug id here: \n";
    print "<INPUT NAME=id>\n";
    print "<INPUT TYPE=\"submit\" VALUE=\"Send Me This Bug\">\n";
    print "</FORM>\n";
    PutFooter();
    exit;
}


$! = 0;

my $loginok = quietly_check_login();

my $id = $::FORM{'id'};

my $query = "
select
        bugs.bug_id,
        product,
        version,
        rep_platform,
        op_sys,
        bug_status,
        resolution,
        priority,
        bug_severity,
        component,
        assigned_to,
        reporter,
        bug_file_loc,
        short_desc,
	target_milestone,
	qa_contact,
	status_whiteboard,
        date_format(creation_ts,'%Y-%m-%d %H:%i'),
        groupset,
	delta_ts,
	sum(votes.count)
from bugs left join votes using(bug_id)
where bugs.bug_id = $id
and bugs.groupset & $::usergroupset = bugs.groupset
group by bugs.bug_id";

SendSQL($query);
my %bug;
my @row;


if (@row = FetchSQLData()) {
    my $count = 0;
    my %fields;
    foreach my $field ("bug_id", "product", "version", "rep_platform",
		       "op_sys", "bug_status", "resolution", "priority",
		       "bug_severity", "component", "assigned_to", "reporter",
		       "bug_file_loc", "short_desc", "target_milestone",
                       "qa_contact", "status_whiteboard", "creation_ts",
                       "groupset", "delta_ts", "votes") {
	$fields{$field} = shift @row;
	if ($fields{$field}) {
	    $bug{$field} = $fields{$field};
	}
	$count++;
    }
} else {
    SendSQL("select groupset from bugs where bug_id = $id");
    if (@row = FetchSQLData()) {
        print "Content-type: text/html\n\n";
        PutHeader("Export by bug number");
        print "<H1>Permission denied.</H1>\n";
        if ($loginok) {
            print "Sorry; you do not have the permissions necessary to see\n";
            print "bug $id.\n";
        } else {
            print "Sorry; bug $id can only be viewed when logged\n";
            print "into an account with the appropriate permissions.  To\n";
            print "see this bug, you must first\n";
            print "<a href=\"show_bug.cgi?id=$id&GoAheadAndLogIn=1\">";
            print "log in</a>.";
        }
    } else {
        print "Content-type: text/html\n\n";
        PutHeader("Export by bug number");
        print "<H1>Bug not found</H1>\n";
        print "There does not seem to be a bug numbered $id.\n";
    }
    PutFooter();
    exit;
}


if ($bug{'short_desc'}) {
  $bug{'short_desc'} = QuoteXMLChars( $bug{'short_desc'} );
}
if (defined $bug{'status_whiteboard'}) {
  $bug{'status_whiteboard'} = QuoteXMLChars($bug{'status_whiteboard'});
}
$bug{'assigned_to'} = DBID_to_real_or_loginname($bug{'assigned_to'});
$bug{'reporter'} = DBID_to_real_or_loginname($bug{'reporter'});


my $ccSet = new RelationSet;
$ccSet->mergeFromDB("select who from cc where bug_id=$id");
my @cc = $ccSet->toArrayOfStrings();
if (@cc) {
  $bug{'cc'} = \@cc;
}

if (Param("useqacontact") && (defined $bug{'qa_contact'}) ) {
    my $name = $bug{'qa_contact'} > 0 ? DBID_to_name($bug{'qa_contact'}) : "";
    if ($name) {
      $bug{'qa_contact'} = $name;
    }
}

if (@::legal_keywords) {
    SendSQL("SELECT keyworddefs.name 
             FROM keyworddefs, keywords
             WHERE keywords.bug_id = $id AND keyworddefs.id = keywords.keywordid
             ORDER BY keyworddefs.name");
    my @list;
    while (MoreSQLData()) {
        push(@list, FetchOneColumn());
    }
    if (@list) {
      $bug{'keywords'} = html_quote(join(', ', @list));
    }
}

SendSQL("select attach_id, creation_ts, description from attachments where bug_id = $id");
my @attachments;
while (MoreSQLData()) {
  my ($attachid, $date, $desc) = (FetchSQLData());
  if ($date =~ /^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
      $date = "$3/$4/$2 $5:$6";
  my %attach;
  $attach{'attachid'} = $attachid;
  $attach{'date'} = $date;
  $attach{'desc'} = html_quote($desc);
  push @attachments, \%attach;
  }
}
if (@attachments) {
  $bug{'attachments'} = \@attachments;
}

SendSQL("select bug_id, who, bug_when, thetext from longdescs where bug_id = $id");
my @longdescs;
while (MoreSQLData()) {
  my ($bug_id, $who, $bug_when, $thetext) = (FetchSQLData());
  my %longdesc;
  $longdesc{'who'} = $who;
  $longdesc{'bug_when'} = $bug_when;
  $longdesc{'thetext'} = html_quote($thetext);
  push @longdescs, \%longdesc;
}
if (@longdescs) {
  $bug{'longdescs'} = \@longdescs;
}

sub EmitDependList {
    my ($myfield, $targetfield) = (@_);
    my @list;
    SendSQL("select dependencies.$targetfield, bugs.bug_status
 from dependencies, bugs
 where dependencies.$myfield = $id
   and bugs.bug_id = dependencies.$targetfield
 order by dependencies.$targetfield");
    while (MoreSQLData()) {
        my ($i, $stat) = (FetchSQLData());
        push @list, $i;
    }
    return @list;
}

if (Param("usedependencies")) {
    my @depends = EmitDependList("blocked", "dependson");
    if ( @depends ) {
      $bug{'dependson'} = \@depends;
    }
    my @blocks = EmitDependList("dependson", "blocked");
    if ( @blocks ) {
      $bug{'blocks'} = \@blocks;
    }
}

sub Log {
    my ($str) = (@_);
    Lock();
    open(FID, ">>data/maillog") || die "Can't write to data/maillog";
    print FID time2str("%D %H:%M", time()) . ": $str\n";
    close FID;
    Unlock();
}


sub Lock {
    if ($::lockcount <= 0) {
        $::lockcount = 0;
        if (!open(LOCKFID, ">>data/maillock")) {
            mkdir "data", 0777;
            chmod 0777, "data";
            open(LOCKFID, ">>data/maillock") || die "Can't open lockfile.";
        }
        my $val = flock(LOCKFID,2);
        if (!$val) { # '2' is magic 'exclusive lock' const.
            print "Content-type: text/html\n\n";
            print "Lock failed: $val\n";
        }
        chmod 0666, "data/maillock";
    }
    $::lockcount++;
}

sub Unlock {
    $::lockcount--;
    if ($::lockcount <= 0) {
        flock(LOCKFID,8);       # '8' is magic 'unlock' const.
        close LOCKFID;
    }
}

my $xml;
my $urlbase = Param("urlbase");
$xml = "<?xml version=\"1.0\" standalone=\"no\"?>\n";
$xml .= "<!DOCTYPE bugzilla SYSTEM \"$urlbase";
if (! ($urlbase =~ /.+\/$/)) {
  $xml .= "/";
}
$xml .= "bugzilla.dtd\">\n";
$xml .= "<bugzilla";
$xml .= " exporter=\"$exporter\"";
$xml .= " version=\"$::param{'version'}\"";
$xml .= " urlbase=\"$urlbase\"";
$xml .= " maintainer=\"" . Param("maintainer") ."\">\n";
$xml .= "<bug>\n";

foreach my $field ("bug_id", "urlbase", "bug_status", "product", "priority", 
  "version", "rep_platform", "assigned_to", "delta_ts", "component", 
  "reporter", "target_milestone", "bug_severity", "creation_ts", 
  "qa_contact", "op_sys", "resolution", "bug_file_loc", "short_desc", 
  "keywords", "status_whiteboard") {
    if ($bug{$field}) {
      $xml .= "  <$field>" . $bug{$field} . "</$field>\n";
    }
}


foreach my $field ("dependson", "blocks", "cc") {
  if (defined $bug{$field}) {
    for (my $i=0 ; $i < @{$bug{$field}} ; $i++) {
      $xml .= "  <$field>" . $bug{$field}[$i] . "</$field>\n";
    }
  }
}

if (defined $bug{'longdescs'}) {
  for (my $i=0 ; $i < @{$bug{'longdescs'}} ; $i++) {
    $xml .= "  <long_desc>\n"; 
    $xml .= "   <who>" . DBID_to_name($bug{'longdescs'}[$i]->{'who'}) 
                       . "</who>\n"; 
    $xml .= "   <bug_when>" . $bug{'longdescs'}[$i]->{'bug_when'} 
                            . "</bug_when>\n"; 
    $xml .= "   <thetext>" . QuoteXMLChars($bug{'longdescs'}[$i]->{'thetext'})
                           . "</thetext>\n"; 
    $xml .= "  </long_desc>\n"; 
  }
}

if (defined $bug{'attachments'}) {
  for (my $i=0 ; $i < @{$bug{'attachments'}} ; $i++) {
    $xml .= "  <attachment>\n"; 
    $xml .= "   <attachid>".$bug{'attachments'}[$i]->{'attachid'}."</attachid>\n"; 
    $xml .= "   <date>".$bug{'attachments'}[$i]->{'date'} . "</date>\n"; 
    $xml .= "   <desc>".$bug{'attachments'}[$i]->{'desc'} . "</desc>\n"; 
  # $xml .= "   <type>".$bug{'attachments'}[$i]->{'type'} . "</type>\n"; 
  # $xml .= "   <data>".$bug{'attachments'}[$i]->{'data'} . "</data>\n"; 
    $xml .= "  </attachment>\n"; 
  }
}

$xml .= "</bug>\n";
$xml .= "</bugzilla>\n";

print "Content-type: text/plain\n\n";
print $xml;

#my $msg = "To: $exporter\n";
#$msg = $msg . "From: Bugalicious <bugs\@bugzilla.mozilla.org>\n";
#$msg = $msg . "Subject: XML\n\n";
#$msg = $msg . $xml . "\n";

#open(SENDMAIL,
#     "|/usr/lib/sendmail -ODeliveryMode=deferred -t") ||
#       die "Can't open sendmail";

#print SENDMAIL $msg;
#close SENDMAIL;

#my $logstr = "XML: bug $id sent to $to";

#Log($logstr);

1;
