#!/usr/bonsaitools/bin/perl -w
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
# Copyright (C) 2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s): Dan Mosedale <dmose@mozilla.org>
#

# mysqld-watcher.pl - a script that watches the running instance of
# mysqld and kills off any long-running SELECTs against the shadow_db
# 
use diagnostics;
use strict;

require "globals.pl";

# some configurables: 

# length of time before a thread is eligible to be killed, in seconds
#
my $long_query_time = 600;
#
# the From header for any messages sent out
#
my $mail_from = "root\@lounge.mozilla.org";
#
# mail transfer agent.  this should probably really be converted to a Param().
#
my $mta_program = "/usr/lib/sendmail -t -ODeliveryMode=deferred";

# and STDIN is where we get the info about running threads
#
close(STDIN);
open(STDIN, "/usr/bonsaitools/bin/mysqladmin processlist |");

# iterate through the running threads
#
my @LONGEST = (0,0,0,0,0,0,0,0,0);
while ( <STDIN> ) { 
    my @F = split(/\|/);

    # if this line is not the correct number of fields, or if the thread-id
    # field contains Id, skip this line.  both these cases indicate that this
    # line contains pretty-printing gunk and not thread info.
    #
    next if ( $#F != 9 || $F[1] =~ /Id/); 

    if ( $F[4] =~ /shadow_bugs/             # shadowbugs database in use
         && $F[5] =~ /Query/                # this is actually a query
         && $F[6] > $long_query_time        # this query has taken too long
         && $F[8] =~ /(select|SELECT)/      # only kill a select
         && $F[6] > $LONGEST[6] ) {         # the longest running query seen
        @LONGEST = @F;
    }
}

# if we found anything, kill the database thread and send mail about it
#
if ($LONGEST[6] != 0) {

    system ("/usr/bonsaitools/bin/mysqladmin", "kill", $LONGEST[1]);

    # fire off an email telling the maintainer that we had to kill a thread
    #
    sendEmail($mail_from, Param("maintainer"), 
	     "long running MySQL thread killed",
	     join(" ", @LONGEST) . "\n");
}

# send an email message
#
# should perhaps be moved to somewhere more global for use in bugzilla as a 
# whole; should also do more error-checking
#
sub sendEmail($$$$) {
    ($#_ == 3) || die("sendEmail: invalid number of arguments");
    my ($from, $to, $subject, $body) = @_;

    open(MTA, "|$mta_program");
    print MTA "From: $from\n";
    print MTA "To: $to\n";
    print MTA "Subject: $subject\n";
    print MTA "\n";
    print MTA $body;
    print MTA "\n";
    close(MTA);
   
}
