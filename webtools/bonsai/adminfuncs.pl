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

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub adminfuncs_pl_sillyness {
    my $zz;
    $zz = $::TreeID;
}

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

     my $pathname = DataDir() . "/$filename";

     return unless $hooklist;
     return unless -f $pathname;
     return unless open(MAIL, "< $pathname");
     $mail = join("", <MAIL>);
     close (MAIL);

     %substs = ();
     $substs{'hooklist'} = $hooklist;
     $mail = PerformSubsts($mail, \%substs);

     %headers = ParseMailHeaders($mail);
     %headers = CleanMailHeaders(%headers);
     $body = FindMailBody($mail);

     my $mail_relay = Param("mailrelay");
     my $mailer = Mail::Mailer->new("smtp", Server => $mail_relay);
     $mailer->open(\%headers)
          or warn "Can't send hook mail: $!\n";
     print $mailer "$body\n";
     $mailer->close();
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
