#!/usr/bin/perl -w

require 5.005;

# This script has split some functions off into a util
# script so they can be re-used by other scripts.
require "build-seamonkey-util.pl";

use strict;

# "use strict" complains if we do not define these.
# They are not initialized here. The default values are after "__END__".
$TreeSpecific::checkout_command = $::Version = undef;

$::Version = '$Revision: 1.93 $ ';

{
    TinderUtils::Setup();
    tree_specific_overides();
    TinderUtils::Build();
}

# End of main
#======================================================================



sub tree_specific_overides {
    $ENV{CVSROOT} = ":pserver:$ENV{USER}%netscape.com\@cvs.mozilla.org:/cvsroot";

	$TreeSpecific::checkout_command = "aaaa";
}

    
