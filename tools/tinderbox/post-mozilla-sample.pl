#!/usr/bin/perl
#

#
# This script gets called after a full mozilla build & test.
# Use this to build and test an embedded or commercial branding of Mozilla.
#
# Edit, and rename this file to post-mozilla.pl to activate.
# ./build-seamonkey-utils.pl will call PostMozilla::main() after
# a successful build and testing of mozilla.  This package can report
# status via the $TinderUtils::build_status variable.  Yeah this is a hack,
# but it works for now.  Feel free to improve this mechanism to properly
# return values & stuff.  -mcafee
#

use strict;

package PostMozilla;


sub main {
  # Get build directory from caller.
  my ($mozilla_build_dir) = @_;
 
  
  TinderUtils::print_log "Post-Mozilla build goes here.\n";

  #
  # Build script goes here.
  #

  # Report some kind of status to parent script.
  #
  #	 {'busted', 'testfailed', 'success'}
  #

  # Report a fake success, for example's sake.
  # we return a list because sometimes we pass back a status *and*
  # a url to a binary location, e.g.:
  # return ('success', 'http://upload.host.org/path/to/binary');
  # tinderbox processes the binary url and links to it in the log.
  return ('success');
}

# Need to end with a true value, (since we're using "require").
1;
