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
use Bug;
require "CGI.pl";
$::lockcount = 0;

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

if ( !defined $::FORM{'buglist'} ) {
  print "Content-type: text/html\n\n";
  PutHeader("Move Bugs");
  print "To move bugs, perform a ";
  print "<A HREF=\"query.cgi\">query</A> and change several bugs at once.\n";
  PutFooter();
  exit;
}

confirm_login();
my $exporter = $::COOKIE{"Bugzilla_login"};
if (! $exporter =~ /(lchaing\@netscape.com|leger\@netscape.com|endico\@mozilla.org|dmose\@mozilla.org)/) {
  print "Content-type: text/html\n\n";
  PutHeader("Move Bugs");
  print "<P>You do not have permission to move bugs<P>\n";
  PutFooter();
  exit;
}

my $xml = "";
$xml .= Bug::XML_Header( Param("urlbase"), $::param{'version'}, 
                         Param("maintainer"), $exporter );
foreach my $id (split(/:/, $::FORM{'buglist'})) {
  my $bug = new Bug($id, $::userid);
  $xml .= $bug->emitXML;
}
$xml .= Bug::XML_Footer;

my $host = Param("urlbase");
$host =~ s#http://([^/]+)/.*#$1#;
my $to = "endico\@localhost";
my $msg = "To: $to\n";
$msg .= "From: Bugzilla <bugzilla\@$host>\n";
$msg .= "Subject: Moving bugs $::FORM{'buglist'}\n\n";
$msg .= $xml . "\n";

open(SENDMAIL,
  "|/usr/lib/sendmail -ODeliveryMode=background -t") ||
    die "Can't open sendmail";

print SENDMAIL $msg;
close SENDMAIL;

my $buglist = $::FORM{'buglist'};
$buglist =~ s/:/,/g;
my $logstr = "XML: bugs $buglist sent to $to";
Log($logstr);

print "Content-type: text/html\n\n";
PutHeader("Moved Bugs");
print "<P>Bugs $buglist were moved to $to.<P>";
print "<P>(This function incomplete. You must close these bugs yourself.)<P>";
PutFooter();
