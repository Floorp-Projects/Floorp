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
# Things to do:
#   * Get pull by branch working
#
#
use strict;

package PostMozilla;

sub checkout {
  my ($mozilla_build_dir) = @_;

  # chdir to build directory
  chdir "$mozilla_build_dir";

  # Next, do the checkout:
  print "Settings::CVS = $Settings::CVS\n";
  my $status = TinderUtils::run_shell_command("$Settings::CVS checkout mozilla/chimera");

  # hack in the chimera prefs, if needed

  return $status;
}


sub main {
  my ($mozilla_build_dir) = @_;

  TinderUtils::print_log "Starting chimera build.\n";

  # Pending a config file, stuff things here.
  my $post_status = 'success';  # Success until we report a failure.
  my $status = 0;  # 0 = success
  # No tests for now, since Chimera can't open a URL passed on the command line.
  my $chimera_alive_test = 1;
  my $chimera_test8_test = 0;
  my $chimera_dir = "$mozilla_build_dir/mozilla/chimera";
  my $embedding_dir = "$mozilla_build_dir/mozilla/embedding/config";
  my $chimera_binary = "Navigator";

  unless ($Settings::TestOnly) {
    # Checkout/update the chimera code.
    #$status = checkout($mozilla_build_dir);
    TinderUtils::print_log "Status from checkout: $status\n";
    if ($status != 0) {
      $post_status = 'busted';
    }
    
    # Build chimera if we passed the checkout command.
    if ($post_status ne 'busted') {
      # Build embedding/config.
      
      chdir $embedding_dir;
      
      if ($status == 0) {
        $status = TinderUtils::run_shell_command("make");
        TinderUtils::print_log "Status from make: $status\n";
      }
      
      #
      # Build chimera.
      #
      
      chdir $chimera_dir;
      
      if ($status == 0) {
        TinderUtils::print_log "Deleting binary...\n";
        TinderUtils::DeleteBinary("$chimera_dir/build/Navigator.app/Contents/MacOS/$chimera_binary");
        
        # Always do a clean build; gecko dependencies don't work correctly
        # for Chimera.
        
        TinderUtils::print_log "Clobbering chimera...\n";
        TinderUtils::run_shell_command("pbxbuild -buildstyle \"Deployment\" clean");
        
        my $foo = Cwd::getcwd();
        TinderUtils::print_log "cwd = $foo\n";
        
        # opt = Deployment, debug = Development.
        # Add   -target NavigatorStatic   for static build.
        $status = TinderUtils::run_shell_command("pbxbuild -buildstyle \"Deployment\"");
        TinderUtils::print_log "Status from pbxbuild: $status\n";
      }
    
      TinderUtils::print_log "Testing build status...\n";
      if ($status != 0) {
        TinderUtils::print_log "busted, pbxbuild status non-zero\n";
        $post_status = 'busted';
      } elsif (not TinderUtils::BinaryExists("$chimera_dir/build/Navigator.app/Contents/MacOS/$chimera_binary")) {
        TinderUtils::print_log "Error: binary not found: $chimera_dir/build/Navigator.app/Contents/MacOS/$chimera_binary\n";
        $post_status = 'busted';
      } else {
        $post_status = 'success';
      }
    }  
  } # TestOnly


  # Test chimera, about:blank
  if ($chimera_alive_test and $post_status eq 'success') {
    $post_status = TinderUtils::AliveTest("ChimeraAliveTest",
                                          "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                          "Navigator",
                                          "about:blank",
                                          60);
  }

  # Test chimera, test8
  # resource:///res/samples/test8.html
  if ($chimera_test8_test and $post_status eq 'success') {
    $post_status = TinderUtils::AliveTest("ChimeraTest8Test",
                                          "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                          "Navigator",
                                          "http://lxr.mozilla.org/seamonkey/source/webshell/tests/viewer/samples/test8.html",
                                          60);
  }
  
  # Pass our status back to calling script.
  return $post_status;
}

# Need to end with a true value, (since we're using "require").
1;
