#!/usr/bin/perl
#

#
# This script gets called after a full mozilla build & test.
# Use this to build and test an embedded or commercial branding of Mozilla.
#
# ./build-seamonkey-utils.pl will call PostMozilla::main() after
# a successful build and testing of mozilla.  This package can report
# status via the $TinderUtils::build_status variable.  Yeah this is a hack,
# but it works for now.  Feel free to improve this mechanism to properly
# return values & stuff.  -mcafee
#

use strict;

package PostMozilla;

sub checkout {
  my ($mozilla_build_dir) = @_;

  # chdir to build directory
  chdir "$mozilla_build_dir";

  # checkout galeon source
  $ENV{CVSROOT} = ":pserver:anonymous\@anoncvs.gnome.org:/cvs/gnome";

  my $status = TinderUtils::run_shell_command("cvs checkout galeon");

  # hack in the galeon prefs, if needed

  return $status;
}


sub main {
  my ($mozilla_build_dir) = @_;

  TinderUtils::print_log "Post-Mozilla build goes here.\n";

  my $post_status = 'success';  # Success until we report a failure.
  my $status = 0;
  my $galeon_alive_test = 0;
  my $galeon_test8_test = 0;
  my $galeon_dir = "$mozilla_build_dir/galeon";

  # Checkout/update the galeon code.
  $status = checkout($mozilla_build_dir);
  if ($status != 0) {
  	$post_status = 'busted';
  }

  # Build galeon if we passed the checkout command.
  if ($post_status ne 'busted') {
	# Build galeon.

	chdir $galeon_dir;
	
	# Make sure we have a configure
	unless (-e "configure") {
	  $status = TinderUtils::run_shell_command("./autogen.sh");
	}

	# Not sure how to only do this when we need to.
	# Force a configure for now.
	$status = TinderUtils::run_shell_command("./configure --sysconfdir=/etc --with-mozilla-libs=$mozilla_build_dir/mozilla/dist/bin --with-mozilla-includes=$mozilla_build_dir/mozilla/dist/include --with-nspr-includes=$mozilla_build_dir/mozilla/dist/include/nspr --with-mozilla-home=$mozilla_build_dir/mozilla/dist/bin");
	
	TinderUtils::DeleteBinary("$galeon_dir/src/galeon-bin");
	$status = TinderUtils::run_shell_command("gmake");
	
	if ($status != 0) {
	  $post_status = 'busted';
	} elsif (not TinderUtils::BinaryExists("$galeon_dir/src/galeon-bin")) {
	  TinderUtils::print_log "Error: binary not found: $galeon_dir/src/galeon-bin\n";
	  $post_status = 'busted';
	} else {
	  $post_status = 'success';
	}
  }

  #
  # Need a way to bypass initial setup dialog stuff, or
  # we'll need to manually run this by hand the first time.
  #
  # Also, have to set crash_recovery=0 in ~/.gnome/galeon to avoid
  # getting the crash dialog after the alive test times out.
  # Looks like --disable-crash-dialog is not implemented yet.
  #

  # Test galeon, about:blank
  if ($galeon_alive_test and $post_status eq 'success') {
	$post_status = TinderUtils::AliveTest("GaleonAliveTest",
										  "$galeon_dir/src",
										  "galeon",
										  "about:blank",
										  45);
  }

  # Test galeon, test8
  if ($galeon_test8_test and $post_status eq 'success') {
	$post_status = TinderUtils::AliveTest("GaleonTest8Test",
										  "$galeon_dir/src",
										  "galeon",
										  "resource:///res/samples/test8.html",
										  45);
  }

  # Pass our status back to calling script.
  return $post_status;
}

# Need to end with a true value, (since we're using "require").
1;
