# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Start up Mozilla, test passes if Mozilla is still alive
# after $Settings::AliveTestTimeout (seconds).
#
sub AliveTest {
    my ($build_dir, $binary) = @_;
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/MozillaAliveTest.log";
    my $args = [$binary];
    local $_;

    # Print out testname
    Util::print_log("\n\nRunning AliveTest test ...\n");

    # Debug
    Util::print_log("build_dir = $build_dir ...\n");
    Util::print_log("binary_dir = $binary_dir ...\n");
    Util::print_log("binary = $binary ...\n");

    # Print out timeout.
    Util::print_log("Timeout = $Settings::AliveTestTimeout seconds.\n");

    my $result = Util::run_cmd($build_dir, $binary_dir, $args,
                               $Settings::AliveTestTimeout);

    Util::print_logfile($binary_log, "AliveTest");

    if ($result->{timed_out}) {
	Util::print_log("AliveTest: $binary_basename successfully stayed up".
	                " for $Settings::AliveTestTimeout seconds.\n");
	return 'success';
    } else {
	Util::print_test_errors($result, $binary_basename);
	Util::print_log("AliveTest: did not time out\n");
	return 'success';
    }
}

1;
