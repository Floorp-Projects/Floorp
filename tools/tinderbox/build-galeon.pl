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


sub main {
  TinderUtils::print_log "Post-Mozilla build goes here.\n";

  my $post_status = 'success';  # Success until we report a failure.
  my $status = 0;
  my $galeon_alive_test = 1;
  my $galeon_test8_test = 1;
  my $galeon_dir = "/u/mcafee/gnome/galeon";

  # Hack.  This needs to auto-pull into the tbox build directory.
  chdir "/u/mcafee/gnome";
  $status = TinderUtils::run_shell_command "checkout";
  if ($status != 0) {
	$post_status = 'busted';
  }

  # Build galeon if we passed the checkout command.
  if ($post_status ne 'busted') {
	# Build galeon.

	#
	# Hack.  There should be a configure step in here.
	# We need to point at mozilla installation, etc.
	#

	chdir $galeon_dir;
	TinderUtils::DeleteBinary("$galeon_dir/src/galeon-bin");
	$status = TinderUtils::run_shell_command "gmake";
	
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

  # Test galeon, about:blank
  if ($galeon_alive_test and $post_status eq 'success') {
	$post_status = TinderUtils::AliveTest("GaleonAliveTest",
										  "$galeon_dir/src",
										  "galeon --disable-crash-dialog",
										  "about:blank",
										  45);
  }

  # Test galeon, test8
  if ($galeon_test8_test and $post_status eq 'success') {
	$post_status = TinderUtils::AliveTest("GaleonTest8Test",
										  "$galeon_dir/src",
										  "galeon --disable-crash-dialog",
										  "resource:///res/samples/test8.html",
										  45);
  }

  # Pass our status back to calling script.
  return $post_status;
}

# Need to end with a true value, (since we're using "require").
1;
