#! /tools/ns/bin/perl5
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


# You need to put this in your CVSROOT directory, and check it in.  (Change the
# first line above to point to a real live perl5.)  Add "dolog.pl" to
# CVSROOT/checkoutlist, and check it in. Then, add a line to your
# CVSROOT/loginfo file that says something like:
#
#      ALL      $CVSROOT/CVSROOT/dolog.pl -r /cvsroot bonsai-checkin-daemon@my.bonsai.machine
#
# or if you do not want to use SMTP at all, add:
#
#      ALL      ( $CVSROOT/CVSROOT/dolog.pl -r /cvsroot -n | /bonsai/handleCheckinMail.pl )
#
# Replace "/cvsroot" with the name of the CVS root directory, and
# "my.bonsai.machine" with the name of the machine Bonsai runs on.
# Now, on my.bonsai.machine, add a mail alias so that mail sent to
# "bonsai-checkin-daemon" will get piped to handleCheckinMail.pl.

use Socket;

$username = $ENV{"CVS_USER"} || getlogin || (getpwuid($<))[0] || "nobody";
$envcvsroot = $ENV{'CVSROOT'};
$cvsroot = $envcvsroot;
$flag_debug = 0;
$flag_tagcmd = 0;
$repository = '';
$repository_tag = '';
$mailhost = 'localhost';
$rlogcommand = '/usr/bin/rlog';
$output2mail = 1;

@mailto = ();
@changed_files = ();
@added_files = ();
@removed_files = ();
@log_lines = ();
@outlist = ();

$STATE_NONE    = 0;
$STATE_CHANGED = 1;
$STATE_ADDED   = 2;
$STATE_REMOVED = 3;
$STATE_LOG     = 4;

&process_args;

if ($flag_debug) {
    print STDERR "----------------------------------------------\n";
    print STDERR "LOGINFO:\n";
    print STDERR " pwd:" . `pwd` . "\n";
    print STDERR " Args @ARGV\n";
    print STDERR " CVSROOT: $cvsroot\n";
    print STDERR " who: $username\n";
    print STDERR " Repository: $repository\n";
    print STDERR " mailto: @mailto\n";
    print STDERR "----------------------------------------------\n";
}

if ($flag_tagcmd) {
    &process_tag_command;
} else {
    &get_loginfo;
    &process_cvs_info;
}

if ($flag_debug) {
    print STDERR "----------------------------------------------\n";
    print STDERR @outlist;
    print STDERR "----------------------------------------------\n";
}

if ($output2mail) {
    &mail_notification;
} else {
    &stdout_notification;
}

0;

sub process_args {
    while (@ARGV) {
        $arg = shift @ARGV;

        if ($arg eq '-d') {
            $flag_debug = 1;
            print STDERR "Debug turned on...\n";
        } elsif ($arg eq '-r') {
            $cvsroot = shift @ARGV;
        } elsif ($arg eq '-t') {
            $flag_tagcmd = 1;
            last;              # Keep the rest in ARGV; they're handled later.
        } elsif ($arg eq '-h') {
            $mailhost = shift @ARGV;
        } elsif ($arg eq '-n') {
            $output2mail = 0;
        } else {
            push(@mailto, $arg);
        }
    }
    if ($repository eq '') {
        open(REP, "<CVS/Repository");
        $repository = <REP>;
        chop($repository);
        close(REP);
    }
    $repository =~ s:^$cvsroot/::;
    $repository =~ s:^$envcvsroot/::;

    if (!$flag_tagcmd) {
        if (open(REP, "<CVS/Tag")) {
            $repository_tag = <REP>;
            chop($repository_tag);
            close(REP);
        }
    }
}

sub get_loginfo {

    if ($flag_debug) {
        print STDERR "----------------------------------------------\n";
    }

    # Iterate over the body of the message collecting information.
    #
    while (<STDIN>) {
        chop;                  # Drop the newline

        if ($flag_debug) {
            print STDERR "$_\n";
        }

        if (/^In directory/) {
            next;
        }

        if (/^Modified Files/) { $state = $STATE_CHANGED; next; }
        if (/^Added Files/)    { $state = $STATE_ADDED;   next; }
        if (/^Removed Files/)  { $state = $STATE_REMOVED; next; }
        if (/^Log Message/)    { $state = $STATE_LOG;     next; }

        s/^[ \t\n]+//;         # delete leading whitespace
        s/[ \t\n]+$//;         # delete trailing whitespace

        if ($state == $STATE_CHANGED && !(/^Tag:/)) { push(@changed_files, split); }
        if ($state == $STATE_ADDED && !(/^Tag:/))   { push(@added_files,   split); }
        if ($state == $STATE_REMOVED && !(/^Tag:/)) { push(@removed_files, split); }
        if ($state == $STATE_LOG)     { push(@log_lines,     $_); }
    }

    if ($flag_debug) {
        print STDERR "----------------------------------------------\n"
                     . "changed files: @changed_files\n"
                     . "added files: @added_files\n"
                     . "removed files: @removed_files\n";
        print STDERR "----------------------------------------------\n";
    }

}

sub process_cvs_info {
    local($d,$fn,$rev,$mod_time,$sticky,$tag,$stat,@d,$l,$rcsfile);
    if (!open(ENT, "<CVS/Entries.Log")) {
        open(ENT, "<CVS/Entries");
    }
    $time = time;
    while (<ENT>) {
        chop;
        ($d,$fn,$rev,$mod_time,$sticky,$tag) = split(/\//);
        $stat = 'C';
        for $i (@changed_files, "BEATME.NOW", @added_files) {
            if ($i eq "BEATME.NOW") { $stat = 'A'; }
            if ($i eq $fn) {
                $rcsfile = shell_escape("$envcvsroot/$repository/$fn,v");
                if (! -r $rcsfile) {
                    $rcsfile = shell_escape("$envcvsroot/$repository/Attic/$fn,v");
                }
                open(LOG, "$rlogcommand -N -r$rev $rcsfile |")
                        || print STDERR "dolog.pl: Couldn't run rlog\n";
                while (<LOG>) {
                    if (/^date:.* author: ([^;]*);.*/) {
                        $username = $1;
                        if (/lines: \+([0-9]*) -([0-9]*)/) {
                            $lines_added = $1;
                            $lines_removed = $2;
                        }
                    }
                }
                close(LOG);
                push(@outlist,
                     ("$stat|$time|$username|$cvsroot|$repository|$fn|$rev|$sticky|$tag|$lines_added|$lines_removed\n"));
            }
        }
    }
    close(ENT);

    for $i (@removed_files) {
        push(@outlist,
             ("R|$time|$username|$cvsroot|$repository|$i|||$repository_tag\n"));
    }

    push(@outlist, "LOGCOMMENT\n");
    push(@outlist, join("\n",@log_lines));
    push(@outlist, "\n:ENDLOGCOMMENT\n");
}


sub process_tag_command {
    local($str,$part,$time);
    $time = time;
    $str = "Tag|$cvsroot|$time";
    while (@ARGV) {
        $part = shift @ARGV;
        $str .= "|" . $part;
    }
    push(@outlist, ("$str\n"));
}



sub do_commitinfo {
}




sub get_response_code {
    my ($expecting) = @_;
#   if ($flag_debug) {
#       print STDERR "SMTP: Waiting for code $expecting\n";
#   }
    while (1) {
        my $line = <S>;
#       if ($flag_debug) {
#           print STDERR "SMTP: $line";
#       }
        if ($line =~ /^[0-9]*-/) {
            next;
        }
        if ($line =~ /(^[0-9]*) /) {
            my $code = $1;
            if ($code == $expecting) {
#               if ($flag_debug) {
#                   print STDERR "SMTP: got it.\n";
#               }
                return;
            }
            die "Bad response from SMTP -- $line";
        }
    }
}




sub mail_notification {
    chop(my $hostname = `hostname`);

    my ($remote,$port, $iaddr, $paddr, $proto, $line);

    $remote  = $mailhost;
    $port    = 25;
    if ($port =~ /\D/) { $port = getservbyname($port, 'tcp') }
    die "No port" unless $port;
    $iaddr   = inet_aton($remote)               || die "no host: $remote";
    $paddr   = sockaddr_in($port, $iaddr);

    $proto   = getprotobyname('tcp');
    socket(S, PF_INET, SOCK_STREAM, $proto)  || die "socket: $!";
    connect(S, $paddr)    || die "connect: $!";
    select(S); $| = 1; select(STDOUT);

    get_response_code(220);
    print S "HELO $hostname\n";
    get_response_code(250);
    print S "MAIL FROM: bonsai-daemon\@$hostname\n";
    get_response_code(250);
    foreach $i (@mailto) {
        print S "RCPT TO: $i\n";
        get_response_code(250);
    }
    print S "DATA\n";
    get_response_code(354);
    # Get one line starting with "354 ".
    if ($flag_tagcmd) {
        print S "Subject:  cvs tag in $repository\n";
    } else {
        print S "Subject:  cvs commit to $repository\n";
    }
    print S "\n";
    print S @outlist, "\n";
    print S ".\n";
    get_response_code(250);
    print S "QUIT\n";
    close(S);
}

sub stdout_notification { 
    chop(my $hostname = `hostname`);

    print  "MAIL FROM: bonsai-daemon\@$hostname\n";
    print  "RCPT TO: root\@localhost\n";
    print  "DATA\n";
    if ($flag_tagcmd) {
        print  "Subject:  cvs tag in $repository\n";
    } else {
        print  "Subject:  cvs commit to $repository\n";
    }
    print  "\n";
    print  @outlist, "\n";
    print  ".\n";
}

# Quotify a string, suitable for invoking a shell process
sub shell_escape {
    my ($file) = @_;
    $file =~ s/([ \"\'\?\$\&\|\!<>\(\)\[\]\;\:])/\\$1/g;
    return $file;
}

