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
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>
#                 Ian Hickson <py8ieh=mozbot@bath.ac.uk>

package Mails;
use strict;
use Carp;

# User must declare the following package global variables:
#  $Mails::owner = \'e-mail address of owner';
#  $Mails::smtphost = 'name of SMTP server';
#  $Mails::debug = \&function to print debug messages # better solutions welcome

# send mail to the owner
sub mailowner {
    my ($subject, $text) = @_;
    &$Mails::debug('I am going to mail the owner!!!');
    return &sendmail($$Mails::owner, $0, $subject, $text);
}

sub RFC822time {
    # Returns today's date as an RFC822 compliant string with the
    # exception that the year is returned as four digits.  In my
    # extremely valuable opinion RFC822 was wrong to specify the year
    # as two digits.  Many email systems generate four-digit years.

    # Today is defined as the first parameter, if given, or else the
    # value that time() gives.

    my ($tsec,$tmin,$thour,$tmday,$tmon,$tyear,$twday,$tyday,$tisdst) = gmtime(shift || time());
    $tyear += 1900; # as mentioned above, this is not RFC822 compliant, but is Y2K-safe.
    $tsec = "0$tsec" if $tsec < 10;
    $tmin = "0$tmin" if $tmin < 10;
    $thour = "0$thour" if $thour < 10;
    $tmon = ('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec')[$tmon];
    $twday = ('Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat')[$twday];
    return "$twday, $tmday $tmon $tyear $thour:$tmin:$tsec GMT";

}

sub sendmail {
    my ($to, $from, $subject, $text, $sig) = (@_, $0);
    eval {
        use Net::SMTP;
        my $date = &RFC822time();
        my $smtp = Net::SMTP->new($Mails::smtphost) or confess("Could not create SMTP connection to $Mails::smtphost! Giving Up");
        $smtp->mail($ENV{USER}); # XXX ?
        $smtp->to($to);
        $smtp->data(<<end);
X-Mailer: $0, Mails.pm; $$Mails::owner
To: $to
From: $from
Subject: $subject
Date: $date

$text
--
$sig
end
        $smtp->quit;
    } or do {
        &$Mails::debug('Failed to send e-mail.');
        &$Mails::debug($@);
        &$Mails::debug('-'x40);
        &$Mails::debug("To: $to");
        &$Mails::debug("From: $from");
        &$Mails::debug("Subject: $subject");
        &$Mails::debug("\n$text\n-- \n$sig");
        &$Mails::debug('-'x40);
        return 0;
    };
    return 1;
}


##########################################################
####  The Mails ##########################################
##########################################################

sub ServerDown {
    my ($server, $port, $nick, $ircname, $username) = @_;
    return &mailowner("Help! I can't talk to $server:$port!", <<end);

Hello Sir or Madam!

I'm afraid I could not connect to the IRC server. I tried, and will
try and try again (unless you kill me...) but it was fruitless.

Could you kick the IRC server for me? Give it a right ol' booting.
And hit the network connection while you are at it, would you please?

Thanks.

Here is what I was trying to connect to:

   Server: $server
     Port: $port
     Nick: $nick
  Ircname: $ircname
 Username: $username

Hope that helps.

Cheers,
end
}

sub ServerUp {
    my ($server) = @_;
    return &mailowner("Woohoo! $server let me in!", <<end);

Helo again.

You'll be happy to know that everything turned out for the better.

Seeya later,
end
}

sub NickShortage {
    my ($cfgfile, $hostname, $port, $username, $ircname, @nicks) = @_;
    local $" = "\n   ";
    return &mailowner('There is a nick shortage!', <<end);

Hello Sir or Madam.

I could not find an unused nick on IRC.

I tried all of these:
   @nicks

If you like you could add some more nicks manually by
editing my configuration file, "$cfgfile"... *hint* *hint*

Here is what I think I am connected to:

    Hostname: $hostname
    Port: $port
    Username: $username
    IRC Name: $ircname

I'll e-mail you again when I manage to get on.

Seeya,
end
}

sub NickOk {
    my ($nick) = @_;
    return &mailowner("It's ok, I'm now using $nick as my nick.", <<end);

Hello again.

You'll be happy to know that everything turned out for the better.

Seeya later,
end
}

1; # end
