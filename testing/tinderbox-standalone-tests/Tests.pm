# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package Tests;

use Util;
use strict;

my $objdir = '';

sub global_prefs {
    my ($binary, $embed_binary, $build_dir) = @_;

    my $binary_basename       = File::Basename::basename($binary);
    my $binary_dir            = File::Basename::dirname($binary);
    my $embed_binary_basename = File::Basename::basename($embed_binary);
    my $embed_binary_dir      = File::Basename::dirname($embed_binary);

    my $test_result = 'success';

    # Windows needs this for file: urls.
    my $win32_build_dir = $build_dir;
    if ($Settings::OS =~ /^WIN/ && $win32_build_dir !~ m/^.:\//) {
        chomp($win32_build_dir = `cygpath -w $win32_build_dir`);
        $win32_build_dir =~ s/\\/\//g;
    }

    #
    # Set prefs to run tests properly.
    #
    # Chances are we will be timing these tests.  Bring gettime() into memory
    # by calling it once, before any tests run.
    Time::PossiblyHiRes::getTime();

    my $pref_file = 'prefs.js';
    
    # Some tests need browser.dom.window.dump.enabled set to true, so
    # that JS dump() will work in optimized builds.
    Prefs::set_pref($pref_file, 'browser.dom.window.dump.enabled', 'true');
    
    # Set security prefs to allow us to close our own window,
    # pageloader test (and possibly other tests) needs this on.
    Prefs::set_pref($pref_file, 'dom.allow_scripts_to_close_windows', 'true');

    # Set security prefs to allow us to resize our windows.
    # DHTML and Tgfx perf tests (and possibly other tests) need this off.
    Prefs::set_pref($pref_file, 'dom.disable_window_flip', 'false');

    # Set prefs to allow us to move, resize, and raise/lower the
    # current window. Tgfx needs this.
    Prefs::set_pref($pref_file, 'dom.disable_window_flip', 'false');
    Prefs::set_pref($pref_file, 'dom.disable_window_move_resize', 'false');

    # Suppress firefox's popup blocking
    if ($Settings::BinaryName =~ /^firefox/) {
        Prefs::set_pref($pref_file, 'dom.disable_open_during_load', 'false');

        # Suppress default browser dialog
        Prefs::set_pref($pref_file, 'browser.shell.checkDefaultBrowser', 'false');

        # Suppress session restore dialog
        Prefs::set_pref($pref_file, 'browser.sessionstore.resume_from_crash', 'false');

        # Suppress automatic safe mode after crashes
        Prefs::set_pref($pref_file, 'toolkit.startup.max_resumed_crashes', -1);
    }
    elsif ($Settings::BinaryName eq 'Camino') {
        Prefs::set_pref($pref_file, 'camino.check_default_browser', 'false');
    }

    # Suppress security warnings for QA test.
    if ($Settings::QATest) {
        Prefs::set_pref($pref_file, 'security.warn_submit_insecure', 'true');
    }
    
    #
    # Assume that we want to test modern skin for all tests.
    #
    if (system("\\grep -s general.skins.selectedSkin \"$pref_file\" > /dev/null")) {
        Util::print_log("Setting general.skins.selectedSkin to modern/1.0\n");
        open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
        print PREFS "user_pref(\"general.skins.selectedSkin\", \"modern/1.0\");\n";
        close PREFS;
    } else {
        Util::print_log("Modern skin already set.\n");
    }

    if ($Settings::BinaryName eq 'Camino') {
      # stdout will be block-buffered and will not be flushed when the test
      # timeout expires and the process is killed, this would make tests
      # appear to fail.
      $ENV{'MOZ_UNBUFFERED_STDIO'} = 1;
    }
}


1;
