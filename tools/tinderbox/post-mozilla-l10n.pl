# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Firefox localization build automation.
#
# The Initial Developer of the Original Code is
# Benjamin Smedberg <bsmedberg@covad.net>
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

use strict;

package PostMozilla;

sub print_log {
    my ($text) = @_;
    print LOCLOG $text;
    print $text;
}

sub run_shell_command {
    my ($shell_command) = @_;
    local $_;

    my $status = 0;
    chomp($shell_command);
    print_log "$shell_command\n";
    open CMD, "$shell_command $Settings::TieStderr |" or die "open: $!";
    print_log $_ while <CMD>;
    close CMD or $status = 1;
    return $status;
}

sub mail_locale_started_message {
    my ($start_time, $locale) = @_;
    my $msg_log = "build_start_msg.tmp";
    open LOCLOG, ">$msg_log";

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';

    print_log "\n";
    print_log "tinderbox: tree: $Settings::BuildTree\n";
    print_log "tinderbox: builddate: $start_time\n";
    print_log "tinderbox: status: building\n";
    print_log "tinderbox: build: $Settings::BuildName $locale\n";
    print_log "tinderbox: errorparser: $platform\n";
    print_log "tinderbox: buildfamily: $platform\n";
    print_log "tinderbox: version: $::Version\n";
    print_log "tinderbox: END\n";
    print_log "\n";

    close LOCLOG;

    if ($Settings::blat ne "" && $Settings::use_blat) {
        system("$Settings::blat $msg_log -t $Settings::Tinderbox_server");
    } else {
        system "$Settings::mail $Settings::Tinderbox_server "
            ." < $msg_log";
    }
    unlink "$msg_log";
}

sub mail_locale_finished_message {
    my ($start_time, $build_status, $logfile, $locale) = @_;

    # Rewrite LOG to OUTLOG, shortening lines.
    open OUTLOG, ">$logfile.last" or die "Unable to open logfile, $logfile: $!";

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';

    # Put the status at the top of the log, so the server will not
    # have to search through the entire log to find it.
    print OUTLOG "\n";
    print OUTLOG "tinderbox: tree: $Settings::BuildTree\n";
    print OUTLOG "tinderbox: builddate: $start_time\n";
    print OUTLOG "tinderbox: status: $build_status\n";
    print OUTLOG "tinderbox: build: $Settings::BuildName $locale\n";
    print OUTLOG "tinderbox: errorparser: $platform\n";
    print OUTLOG "tinderbox: buildfamily: $platform\n";
    print OUTLOG "tinderbox: version: $::Version\n";
    print OUTLOG "tinderbox: utilsversion: $::UtilsVersion\n";
    print OUTLOG "tinderbox: logcompression: $Settings::LogCompression\n";
    print OUTLOG "tinderbox: logencoding: $Settings::LogEncoding\n";
    print OUTLOG "tinderbox: END\n";

    if ($Settings::LogCompression eq 'gzip') {
        open GZIPLOG, "gzip -c $logfile |" or die "Couldn't open gzip'd logfile: $!\n";
        TinderUtils::encode_log(\*GZIPLOG, \*OUTLOG);
        close GZIPLOG;
    }
    elsif ($Settings::LogCompression eq 'bzip2') {
        open BZ2LOG, "bzip2 -c $logfile |" or die "Couldn't open bzip2'd logfile: $!\n";
        TinderUtils::encode_log(\*BZ2LOG, \*OUTLOG);
        close BZ2LOG;
    }
    else {
        open LOCLOG, "$logfile" or die "Couldn't open logfile, $logfile: $!";
        TinderUtils::encode_log(\*LOCLOG, \*OUTLOG);
        close LOCLOG;
    }    
    close OUTLOG;
    unlink($logfile);

    # If on Windows, make sure the log mail has unix lineendings, or
    # we'll confuse the log scraper.
    if ($platform eq 'windows') {
        open(IN,"$logfile.last") || die ("$logfile.last: $!\n");
        open(OUT,">$logfile.new") || die ("$logfile.new: $!\n");
        while (<IN>) {
            s/\r\n$/\n/;
	    print OUT "$_";
        } 
        close(IN);
        close(OUT);
        File::Copy::move("$logfile.new", "$logfile.last") or die("move: $!\n");
    }

    if ($Settings::ReportStatus and $Settings::ReportFinalStatus) {
        if ($Settings::blat ne "" && $Settings::use_blat) {
            system("$Settings::blat $logfile.last -t $Settings::Tinderbox_server");
        } else {
            system "$Settings::mail $Settings::Tinderbox_server "
                ." < $logfile.last";
        }
    }
}

sub main {
    my ($mozilla_build_dir) = @_;

    TinderUtils::print_log "Building locales:\n";

    my $srcdir = "$mozilla_build_dir/${Settings::Topsrcdir}";
    my $objdir = $srcdir;
    if ($Settings::ObjDir ne '') {
        $objdir .= "/${Settings::ObjDir}";
    }

    unless (open(ALL_LOCALES, "<$srcdir/browser/locales/all-locales")) {
        TinderUtils::print_log "Error: Couldn't read all-locales.\n";
        return (("testfailed"));
    }

    chdir $mozilla_build_dir;

    my @locales = <ALL_LOCALES>;
    chomp @locales;

    close ALL_LOCALES;

    my $start_time = TinderUtils::adjust_start_time(time());
    foreach my $locale (@locales) {
        mail_locale_started_message($start_time, $locale);

        my $logfile = "$Settings::DirName-$locale.log";
        my $tinderstatus = 'success';
        open LOCLOG, ">$logfile";

        my $status = run_shell_command "$Settings::Make -C $objdir/browser/locales installers-$locale";
        if ($status != 0) {
            $tinderstatus = 'busted';
        }

        $status = run_shell_command "$^X $srcdir/toolkit/locales/compare-locales.pl $srcdir/toolkit/locales/en-US $srcdir/toolkit/locales/$locale";
        if ($tinderstatus eq 'success' && $status != 0) {
            $tinderstatus = 'testfailed';
        }

        $status = run_shell_command "$^X $srcdir/toolkit/locales/compare-locales.pl $srcdir/browser/locales/en-US $srcdir/browser/locales/$locale";
        if ($tinderstatus eq 'success' && $status != 0) {
            $tinderstatus = 'testfailed';
        }
        
        close LOCLOG;

        mail_locale_finished_message($start_time, $tinderstatus, $logfile, $locale);
    }

    return (('success'));
}

1;
