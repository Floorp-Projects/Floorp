#!/usr/bin/perl

#
# Script to time mozilla startup.
#

require 5.003;

require "gettime.pl";

use strict;
use Cwd;

# 
# mozilla file:/foo/startup-test.html?begin=T
#   where T = seconds since 1970, e.g.:
# mozilla file:/foo/startup-test.html?begin=999456977000
# 

{
	# Build up command string.
	my $cwd = Cwd::getcwd();
	my $time = Time::PossiblyHiRes::getTime();
	my $cmd = "mozilla \"file:$cwd/startup-test.html?begin=" . $time . "\"";	
	print "cmd = $cmd\n";

	# Run the command.
	exec $cmd;
}

