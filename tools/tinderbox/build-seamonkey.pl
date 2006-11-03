#!/usr/bin/perl -w

require 5.003;

# This script has split some functions off into a util
# script so they can be re-used by other scripts.
require "build-seamonkey-util.pl";

use strict;

# "use strict" complains if we do not define these.
# They are not initialized here. The default values are after "__END__".
$TreeSpecific::name = $TreeSpecific::build_target = $TreeSpecific::checkout_target = $TreeSpecific::clobber_target = $::Version = undef;

$::Version = '$Revision: 1.104 $ ';

{    
    TinderUtils::Setup();
    tree_specific_overides();

    # This code assumes that the build process will alter the mtime of the
    # "build directory" (i.e. WINNT_5.2_Dep, Darwin_8.1.0_Clbr, etc.) by
    # dumping a log file or some such in the directory. If that's not the
    # case, then this "feature" won't work.
    if (defined($Settings::BuildInterval) && $Settings::BuildInterval > 0) {
        print STDERR "Build interval of $Settings::BuildInterval seconds requested\n";
        my $lastBuilt = (stat($Settings::DirName))[9]; ## Magic 9 is st_mtime
        my $now = time();

        if (($now - $Settings::BuildInterval) < $lastBuilt) {
            print STDERR 'Last built at ' . scalar(localtime($lastBuilt)) . 
             ', ' . ($now - $lastBuilt) . " seconds ago; not building.\n";
            TinderUtils::stop_tinderbox(reason => 'Build interval not expired');
        }
    }

    TinderUtils::Build();
}

# End of main
#======================================================================



sub tree_specific_overides {

	$TreeSpecific::name = 'mozilla';
	$TreeSpecific::build_target = 'build_all_depend';
	$TreeSpecific::checkout_target = 'checkout';
	$TreeSpecific::clobber_target = 'distclean';
	$TreeSpecific::extrafiles = '';
	
}

    
