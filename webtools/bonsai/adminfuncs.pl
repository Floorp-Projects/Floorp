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

require 'globals.pl';

use Mail::Internet;
use Mail::Header;

sub MakeHookList {
     my ($checkin, $person, %people, @addrs);

     # First, empty the arrays
     undef %people; undef @addrs;

     foreach $checkin (@::CheckInList) {
          my $info = eval("\\\%$checkin");

          $people{$$info{'person'}} = 1;
     }

     foreach $person (sort(keys(%people))) {
          push @addrs, EmailFromUsername($person);
     }
     return @addrs;
}


sub SendHookMail {
     my ($filename) = @_;
     my $hooklist = join(', ', MakeHookList());
     my (%substs, %headers, $body, $mail);
     local *MAIL;

     $pathname = DataDir() . "/$filename";

print 'a';
     return unless $hooklist;
print 'b';
     return unless -f $pathname;
print 'c';
     return unless open(MAIL, "< $pathname");
print 'd';
     $mail = join("", <MAIL>);
print 'e';
     close (MAIL);
print 'f';

     %substs = ();
print 'g';
     $substs{'hooklist'} = $hooklist;
print 'h';
     $mail = PerformSubsts($mail, \%substs);
print 'i';

     %headers = ParseMailHeaders($mail);
print 'j';
     %headers = CleanMailHeaders(%headers);
print 'k';
     $body = FindMailBody($mail);
print 'l';

     my $mail_relay = Param("mailrelay");
print 'm';
     my $mailer = Mail::Mailer->new("smtp", Server => $mail_relay);
print 'n';
     $mailer->open(\%headers)
          or warn "Can't send hook mail: $!\n";
print 'o';
     print $mailer "$body\n";
print 'p';
     $mailer->close();
print 'q';
}


sub AdminOpenTree {
     my ($lastgood, $clearp) = @_;

     return if $::TreeOpen;

     $::LastGoodTimeStamp = $lastgood;
     $::TreeOpen = 1;

     PickNewBatchID();

     if ($clearp) {
          SendHookMail('treeopened');
          @::CheckInList = ();
     } else {
          SendHookMail('treeopenedsamehook');
     }

     Log("Tree opened.  \$::LastGoodTimeStamp is " .
         MyFmtClock($::LastGoodTimeStamp));
}


sub AdminCloseTree {
     my ($closetime) = @_;

     return unless $::TreeOpen;

     $::CloseTimeStamp = $closetime;
     $::TreeOpen = 0;
     SendHookMail('treeclosed');
     Log("Tree $::TreeID closed.  \$::CloseTimeStamp is " .
         MyFmtClock($::CloseTimeStamp));
}
