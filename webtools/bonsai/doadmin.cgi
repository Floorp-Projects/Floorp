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
require 'adminfuncs.pl';

print "Content-type: text/html\n\n";

CheckPassword(FormData('password'));

Lock();
LoadCheckins();

my $cmd = FormData('command');

if ($cmd eq 'close') {
     close_tree();
} elsif ($cmd eq 'open') {
     open_tree();
} elsif ($cmd eq 'tweaktimes') {
     edit_tree();
} elsif ($cmd eq 'editmotd') {
     edit_motd();
} elsif ($cmd eq 'changepassword') {
     change_passwd();
} else {
     error_screen('Invalid Command',
                  "<b>Invalid Command '<tt>$cmd</tt>'</b>");
}

PutsTrailer();
WriteCheckins();
Unlock();
exit 0;



sub error_screen {
     my ($title, $err_str) = @_;

     PutsHeader($title);
     print "\n<hr>\n$err_str\n\n";
     PutsTrailer();
     exit 0;
}


sub close_tree {
     my $sw = Param("software", 1);
     my $ti = Param("bonsai-treeinterest", 1);
     my $href = ConstructMailTo(EmailFromUsername($sw),
                                "The tree is now closed.");

     AdminCloseTree(ParseTimeAndCheck(FormData('closetimestamp')));

     PutsHeader("Clang!", "Clang!", "The tree is now closed.");
     print "
Mail has been sent notifying \"the hook\" and anyone subscribed to $ti.<p>

$href about the closure.<p>
";
}

sub open_tree {
     my $sw = Param("software", 1);
     my $ti = Param("bonsai-treeinterest", 1);
     my $href = ConstructMailTo(EmailFromUsername($sw),
                                "The tree is now open.");

     AdminOpenTree(ParseTimeAndCheck(FormData('lastgood')),
                   exists($::FORM{'doclear'}));

     PutsHeader("The floodgates are open.", "The floodgates are open.");
     print "
Mail has been sent notifying \"the hook\" and anyone subscribed to $ti.<p>

$href about the new status of the tree.<p>
";
}

sub edit_tree {
     $::LastGoodTimeStamp = ParseTimeAndCheck(FormData('lastgood'));
     $::CloseTimeStamp = ParseTimeAndCheck(FormData('lastclose'));

     PutsHeader("Let's do the time warp again...",
                "Times have been tweaked.");

     Log("Times tweaked: \$::LastGoodTimeStamp is " .
         MyFmtClock($::LastGoodTimeStamp) .
         ", closetime is " .
         MyFmtClock($::CloseTimeStamp));
}


sub edit_motd {
     LoadMOTD();

     unless (FormData('origmotd') eq $::MOTD) {
          error_screen("Oops!",
"<H1>Someone else has been here!</H1>

It looks like somebody else has changed the message-of-the-day.
Terry was too lazy to implement anything beyond detecting this
condition.  You'd best go start over -- go back to the top of Bonsai,
look at the current message-of-the-day, and decide if you still
want to make your edits.");
     }

     MailDiffs("message-of-the-day", $::MOTD, FormData('motd'));
     $::MOTD = FormData('motd');
     PutsHeader("New MOTD", "New MOTD",
                "The Message Of The Day has been changed.");
     WriteMOTD();
     Log("New motd: $::MOTD");
}

sub change_passwd {
     my ($outfile, $encoded);
     local *PASSWD;

     unless (FormData('newpassword') eq FormData('newpassword2')) {
          error_screen("Oops -- Mismatch!",
"The two passwords you typed didn't match.  Click <b>Back</b> and try again.");
     }

     if ($::FORM{'doglobal'}) {
          CheckGlobalPassword($::FORM{'password'});
          $outfile = 'data/passwd';
     } else {
          $outfile = DataDir() . '/treepasswd';
     }

     $encoded = crypt($::FORM{'newpassword'}, "aa");
     unless (open(PASSWD, ">$outfile")) {
          error_screen("Oops -- Couldn't write password file!",
                       "Couldn't open `<tt>$outfile</tt>': $!.");
     }
     print PASSWD "$encoded\n";
     close(PASSWD);
     chmod(0777, $outfile);

     PutsHeader('Locksmithing complete.', 'Password Changed.',
                'The new password is now in effect.');
     PutsTrailer();
     exit 0;
}
