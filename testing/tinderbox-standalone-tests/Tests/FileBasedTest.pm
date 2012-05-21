# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Run a generic test that writes output to stdout, save that output to a
# file, parse the file looking for failure token and report status based
# on that.  A hack, but should be useful for many tests.
#
# test_name = Name of test we're gonna run, in $Settings::DistBin.
# testExecString = How to run the test
# testTimeoutSec = Timeout for hung tests, minimum test time.
# statusToken = What string to look for in test output to
#     determine test status.
# statusTokenMeansPass = Default use of status token is to look for
#     failure string.  If this is set to 1, then invert logic to look for
#     success string.
#
# timeout_is_ok = Don't report test failure if test times out.
#
# Note: I tried to merge this function with AliveTest(),
#       the process flow control got too confusing :(  -mcafee
#
sub FileBasedTest {
    my ($test_name, $build_dir, $binary_dir, $test_args, $timeout_secs,
        $status_token, $status_token_means_pass, $timeout_is_ok) = @_;
    local $_;

    # Assume the app is the first argument in the array.
    my ($binary_basename) = @$test_args[0];
    my $binary_log = "$build_dir/$test_name.log";

    # Print out test name
    Util::print_log("\n\nRunning $test_name ...\n");

    my $result = Util::run_cmd($build_dir, $binary_dir, $test_args,
                               $timeout_secs);

    Util::print_logfile($binary_log, $result->{output});
    Util::print_logfile($binary_log, $test_name);

    if (($result->{timed_out}) and (!$timeout_is_ok)) {
      Util::print_log("Error: $test_name timed out after $timeout_secs seconds.\n");
      return 'testfailed';
    } elsif ($result->{exit_value} != 0) {
      Util::print_log("Error: $test_name exited with status $result->{exit_value}\n");
      Util::print_test_errors($result, $test_name);
      return 'testfailed';
    } else {
      Util::print_log("$test_name exited normally\n");
    }

    my $found_token = file_has_token($binary_log, $status_token);
    if ($found_token) {
        Util::print_log("Found status token in log file: $status_token\n");
    } else {
        Util::print_log("Status token, $status_token, not found\n");
    }

    if (($status_token_means_pass and $found_token) or
        (not $status_token_means_pass and not $found_token)) {
        return 'success';
    } else {
        Util::print_log("Error: $test_name has failed.\n");
        return 'testfailed';
    }
} # FileBasedTest

1;
