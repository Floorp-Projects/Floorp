#! /usr/bin/perl
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
# Contributor(s): Mike Taylor <bear@code-bear.com>
#                 Christopher Seawood <cls@seawood.org>


# You need to put this in your CVSROOT directory, and check it in.  (Change the
# first line above to point to a real live perl5.)  Add "dolog.pl" to
# CVSROOT/checkoutlist, and check it in. Then, add a line to your
# CVSROOT/loginfo file that says something like:
#
#      ALL      $CVSROOT/CVSROOT/dolog.pl [-u ${USER}] -r /cvsroot bonsai-checkin-daemon@my.bonsai.machine
#
# or if you do not want to use SMTP at all, add:
#
#      ALL      ( $CVSROOT/CVSROOT/dolog.pl -r /cvsroot -n | /bonsai/handleCheckinMail.pl )
#
# Replace "/cvsroot" with the name of the CVS root directory, and
# "my.bonsai.machine" with the name of the machine Bonsai runs on.
# Now, on my.bonsai.machine, add a mail alias so that mail sent to
# "bonsai-checkin-daemon" will get piped to handleCheckinMail.pl.

use bytes;
use File::Basename;
use Mail::Mailer;

# Set use_sendmail = 0 to send mail via $mailhost using SMTP
$use_sendmail = 1;

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
@import_tags = ();
@import_new_files = ();
@import_changed_files = ();

$STATE_NONE    = 0;
$STATE_CHANGED = 1;
$STATE_ADDED   = 2;
$STATE_REMOVED = 3;
$STATE_LOG     = 4;
$STATE_IMPORT_STATUS = 5;
$STATE_IMPORT_TAGS  = 6;
$STATE_IMPORT_FILES  = 7;

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
    print STDERR "OUTLIST:\n";
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
        } elsif ($arg eq '-u') {
            $username = shift @ARGV;
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
    $state = $STATE_NONE;
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
        if ($state == $STATE_LOG && (m/^Status:$/)) { 
            push(@log_lines, $_);
            $state = $STATE_IMPORT_STATUS;
            next;
        }
        if ($state == $STATE_IMPORT_STATUS) {
            my ($itag, $istat, @rest);
            while (<STDIN>) {
                chomp;
                print STDERR "$_\n" if ($flag_debug);
                push(@log_lines, $_);
                if ($state == $STATE_IMPORT_STATUS) {
                    next if (m/^\s*$/);
                    if (m/^Vendor Tag:/) {
                        ($vendor_tag = $_) =~ s/^Vendor Tag:\s+([\w-]+).*/$1/;
                        $state = $STATE_IMPORT_TAGS;
                        next;
                    } else {
                        $state = $STATE_LOG;
                        last;
                    }
                }
                if ($state == $STATE_IMPORT_TAGS) {
                    if (m/^\s*$/) {
                        $state = $STATE_IMPORT_FILES;
                    } else {
                        ($itag = $_) =~ s/^(Release Tags:)?\s+([\w-]+).*/$2/;
                        push(@import_tags, $itag);
                    }
                    next;
                }
                if ($state == $STATE_IMPORT_FILES) {
                    if (m/^\s*$/) {
                        $state = $STATE_LOG;
                        last;
                    }
                    ($istat, @rest) = split(/ /, $_, 2);
                    if ($istat eq 'N') {
                        push(@import_new_files, @rest);
                    } elsif ($istat eq 'U') {
                        push(@import_changed_files, @rest);
                    } 
                    # Ignore everything else
                    next;
                }
            }
        }
        if ($state == $STATE_LOG)     { push(@log_lines,     $_); }
    }

    # If any of the filenames in the arrays below contain spaces,
    # things get broken later on in the code.
    # fix the filename array by using the get_filename sub.
    @fixed_changed_files = @{&get_filename("C", @changed_files)};
    @fixed_added_files   = @{&get_filename("A", @added_files)};
    @fixed_removed_files = @{&get_filename("R", @removed_files)};
    @fixed_import_new_files = @{&get_filename("I", @import_new_files)};
    @fixed_import_changed_files = @{&get_filename("I", @import_changed_files)};

    # now replace the old broken arrays with the new fixed arrays and
    # carry on.

    @changed_files = @fixed_changed_files;
    @added_files   = @fixed_added_files;
    @removed_files = @fixed_removed_files;
    @import_new_files = @fixed_import_new_files;
    @import_changed_files = @fixed_import_changed_files;
    
    if ($flag_debug) {
        print STDERR "----------------------------------------------\n"
                     . "changed files: @changed_files\n"
                     . "added files: @added_files\n"
                     . "removed files: @removed_files\n"
                     . "new imported files: @import_new_files\n"
                     . "changed imported files: @import_changed_files\n";
        print STDERR "----------------------------------------------\n";
    }

}

sub get_filename {

    my ($state, @files) = @_;
    my @fixed_files;
    my $FILE_EXIST = 0;
    my $FILE_CHECKED = 0;
    my $file;
    my $partial_file;
    my $path;
    if ($flag_debug) {
        print STDERR "\n-- get_filename ------------------------\n";
    }
    foreach my $scalar (@files) {
        if ($FILE_CHECKED && ! $FILE_EXISTS) {
            $file = "$partial_file $scalar";
        } else{
            $file = $scalar;
        }
        if ($state eq "I") {
            $path = "$envcvsroot/$file";
        } elsif ($state eq "R") {
            $path = "$envcvsroot/$repository/Attic/$file";
        } else {
            $path = "$envcvsroot/$repository/$file";
        }
        if ($flag_debug) {
            print STDERR "changed file: $file\n";
            print STDERR "path: $path\n";
        }
        if (-r "$path,v") {
            push(@fixed_files, $file);
            $FILE_EXISTS = 1;
            $FILE_CHECKED = 1;
            if ($flag_debug){
                print STDERR "file exists\n";
            }
        } else {
            $partial_file = $file;
            $FILE_EXISTS = 0;
            $FILE_CHECKED = 1;
            if ($flag_debug) {
                print STDERR "file does not exist\n";
            }
        }
    }
    if ($flag_debug) {
        print STDERR "\@fixed_files: @fixed_files\n";
        print STDERR "-------------------------------------------\n\n";
    }
    return \@fixed_files;
}

sub process_cvs_info {
    local($d,$fn,$rev,$mod_time,$sticky,$tag,$stat,@d,$rcsfile);
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
                $rcsfile = "$envcvsroot/$repository/$fn,v";
                if (! -r $rcsfile) {
                    $rcsfile = "$envcvsroot/$repository/Attic/$fn,v";
                }
                $rlogcmd = "$rlogcommand -N -r$rev " . shell_escape($rcsfile);
                open(LOG, "$rlogcmd |")
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

    my $headrev;
    my $found_desc;
    # Process new imported files
    foreach $fn (@import_new_files) {
        my ($file, $dir, $suffix) = &fileparse($fn, ",v");
        $dir =~ s@/$@@;
        $found_desc = 0;
        $headrev = 0;
        $lines_added = 0;
        $lines_removed = 0;
        $rcsfile = "$envcvsroot/$dir/${file},v";
        if (! -r $rcsfile) {
            $rcsfile = "$envcvsroot/$dir/Attic/${file},v";
        }
        $rlogcmd = "$rlogcommand -N " . &shell_escape($rcsfile);
        open(LOG, "$rlogcmd |") || 
            print STDERR "dolog.pl: Couldn't run import rlog\n";
        while (<LOG>) {
            $found_desc++, next if (m/^description:$/);
            $headrev = $1 if (!$found_desc && m/^head: (\d+[\.\d+]+)$/);
            $rev = $1 if (m/^revision (\d+[\.\d+]+)$/);
            if (m/^date:.* author: ([^;]*);.*/) {
                $username = $1;
                if (m/lines: \+([0-9]*) -([0-9]*)/) {
                    $lines_added = $1;
                    $lines_removed = $2;
                }
                # Add the head revision entry
                if ($headrev eq $rev) {
                    push(@outlist,
                         ("A|$time|$username|$cvsroot|$dir|$file|$headrev|" .
                          "||$lines_added|$lines_removed\n"));
                    last;
                }
            }
        }
        close(LOG);
    }

    # Process changed imported files
    my $search_tag = $import_tags[0];
    my ($search_rev, $found_rev, $found_srev);
    foreach $fn (@import_changed_files) {
        my ($file, $dir, $suffix) = &fileparse($fn, ",v");
        $dir =~ s@/$@@;
        $found_desc = 0;
        $found_rev = 0;
        $found_srev = 0;
        $search_rev = '';
        $lines_added = 0;
        $lines_removed = 0;
        last if (!defined($search_tag));
        $rcsfile = "$envcvsroot/$dir/${file},v";
        if (! -r $rcsfile) {
            $rcsfile = "$envcvsroot/$dir/Attic/${file},v";
        }
        $rlogcmd = "$rlogcommand " . &shell_escape($rcsfile);
        open(LOG, "$rlogcmd |") ||
            print STDERR "dolog.pl: Couldn't run import rlog\n";
        while (<LOG>) {
            $found_desc++, next if (m/^description:$/);
            if (!$found_desc && m/^\s*$search_tag: (\d+[\.\d+]+)$/) {
                $search_rev = $1;
                $found_srev++;
                next;
            }
            if (!$found_desc && $found_srev && m/^\s*[\w-]+: $search_rev$/) {
                # Revision already exists so no actual changes
                # were made during this import, so do nothing
                last;
            }
            $found_rev++, next if ($found_srev && m/^revision $search_rev$/);
            if ($found_rev && m/^date:.* author: ([^;]*);.*/) {
                $username = $1;
                if (m/lines: \+([0-9]*) -([0-9]*)/) {
                    $lines_added = $1;
                    $lines_removed = $2;
                }
                push(@outlist,
                     ("C|$time|$username|$cvsroot|$dir|$file|$search_rev|" .
                      "$search_tag||$lines_added|$lines_removed\n"));
                last;
            }
        }
        close(LOG);
    }

    # make sure dolog has something to parse when it sends its load off
    if (!scalar(@log_lines)) {
        push @log_lines, "EMPTY LOG MESSAGE"; 
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

sub mail_notification {
    chop(my $hostname = `hostname`);
    my $mailer;
    if ($use_sendmail) {
        $mailer = Mail::Mailer->new("sendmail");
    } else {
        $mailer = Mail::Mailer->new("smtp", Server => $mailhost);
    }
    die("Failed to send mail notification\n") if !defined($mailer);
    my %headers;

    $headers{'From'} = "bonsai-daemon\@$hostname";
    $headers{'To'} = \@mailto;
    if ($flag_tagcmd) {
        $headers{'Subject'} = "cvs tag in $repository";
    } else {
        $headers{'Subject'} = "cvs commit to $repository";
    }
    $mailer->open(\%headers);
    print $mailer @outlist;
    $mailer->close;
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

