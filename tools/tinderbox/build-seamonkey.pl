#!/usr/bin/perl -w

require 5.003;

# This script has split some functions off into a util
# script so they can be re-used by other scripts.
require "build-seamonkey-util.pl";

use strict;

# "use strict" complains if we do not define these.
# They are not initialized here. The default values are after "__END__".
$TreeSpecific::name = $TreeSpecific::build_target = $TreeSpecific::checkout_target = $TreeSpecific::clobber_target = $::Version = undef;

$::Version = '$Revision: 1.99 $ ';

{    
    TinderUtils::Setup();
    tree_specific_overides();
    TinderUtils::Build();
}

# End of main
#======================================================================



sub tree_specific_overides {

	$TreeSpecific::name = 'mozilla';
	$TreeSpecific::build_target = 'alldep';
	$TreeSpecific::checkout_target = 'checkout';
	$TreeSpecific::clobber_target = 'clobber_all';
	
}

    
