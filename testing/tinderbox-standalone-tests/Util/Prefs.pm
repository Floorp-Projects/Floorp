# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package Prefs;

sub set_pref {
  my ($pref_file, $pref, $value) = @_;
  # Make sure we get rid of whatever value was there,
  # to allow for resetting prefs
  system ("\\grep -v \\\"$pref\\\" '$pref_file' > '$pref_file.new'");
  File::Copy::move("$pref_file.new", "$pref_file") or die("move: $!\n");

  Util::print_log("Setting $pref to $value\n");
  open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
  print PREFS "user_pref(\"$pref\", $value);\n";
  close PREFS;
}

sub get_prefs_file {
  my ($pref_file, $profile_dir);

  if ($Settings::UseMozillaProfile) {
    # Profile directory.  This lives in way-different places
    # depending on the OS.
    #
    my $profiledir = get_profile_dir($build_dir);

    #
    # Make sure we have a profile to run tests.  This is assumed to be called
    # $Settings::MozProfileName and will live in $build_dir/.mozilla.
    # Also assuming only one profile here.
    #
    my $cp_result = 0;

    unless (-d "$profiledir") {
      Util::print_log("No profile found, creating profile.\n");
      $cp_result = create_profile($build_dir, $binary_dir, $binary);
    } else {
      Util::print_log("Found profile.\n");

      # Recreate profile if we have $Settings::CleanProfile set.
      if ($Settings::CleanProfile) {
        my $deletedir = $profiledir;

        Util::print_log("Creating clean profile ...\n");
        Util::print_log("Deleting $deletedir ...\n");
        File::Path::rmtree([$deletedir], 0, 0);
        if (-e "$deletedir") {
          Util::print_log("Error: rmtree([$deletedir], 0, 0) failed.\n");
        }
        $cp_result = create_profile($build_dir, $binary_dir, $binary);
      }
    }

    # Set status, in case create profile failed.
    if ($cp_result) {
      # We should check $cp_result->{exit_value} too, except
      # semi-single-profile landing made 0 the success value (which is
      # good), so we now have inconsistent expected results.
      if (not $cp_result->{timed_out}) {
        $test_result = "success";
      } else {
		Util::print_log("cp_result failed\n");
        $test_result = "testfailed";
        Util::print_log("Error: create profile failed.\n");
      }
    }

    # Call get_profile_dir again, so it can find the extension-salted
    # profile directory under the profile root.

    $profiledir = get_profile_dir($build_dir);

    #
    # Find the prefs file, remember we have that random string now
    # e.g. <build-dir>/.mozilla/default/uldx6pyb.slt/prefs.js
    # so File::Path::find will find the prefs.js file.
    ##
    ($pref_file, $profile_dir) = find_pref_file($profiledir);

    #XXX this is ugly and hacky 
    $test_result = 'testfailed' unless $pref_file;;
    if (!$pref_file) { Util::print_log("no pref file found\n"); }

  } elsif($Settings::BinaryName eq "TestGtkEmbed") {
    Util::print_log("Using TestGtkEmbed profile\n");
    
    $pref_file   = "$build_dir/.TestGtkEmbed/TestGtkEmbed/prefs.js";
    $profile_dir = "$build_dir";

    # Create empty prefs file if needed
    #unless (-e $pref_file) {
    #  system("mkdir -p $build_dir/.TestGtkEmbed/TestGtkEmbed");
    #  system("touch $pref_file");
    #}

    # Run TestGtkEmbed to generate proper pref file.
    # This should only need to be run the first time for a given tree.
    unless (-e $pref_file) {
      $test_result = AliveTest("EmbedAliveTest_profile", $build_dir,
                               ["$embed_binary_dir/$embed_binary_basename"],
                               $Settings::EmbedTestTimeout);
    }
  }
  return $pref_file;
}

#
# Given profile directory, find pref file hidden in salt directory.
# profile $Settings::MozProfileName must exist before calling this sub.
#
sub find_pref_file {
    my $profile_dir = shift;

    # default to *nix
    my $pref_file = "prefs.js";

    unless (-e $profile_dir) {
        print_log "ERROR: profile $profile_dir does not exist\n";
        #XXX should make 'run_all_tests' throw a 'testfailed' exception
        # and just skip all the continual checking for $test_result
        return; # empty list
    }

    my $found = undef;
    my $sub = sub {$pref_file = $File::Find::name, $found++ if $pref_file eq $_};
    File::Find::find($sub, $profile_dir);
    unless ($found) {
        print_log "ERROR: couldn't find prefs.js in $profile_dir\n";
        return; # empty list
    }

    # Find full profile_dir while we're at it.
    $profile_dir = File::Basename::dirname($pref_file);

    print_log "profile dir = $profile_dir\n";
    print_log "prefs.js    = $pref_file\n";

    return ($pref_file, $profile_dir);
}

1;
