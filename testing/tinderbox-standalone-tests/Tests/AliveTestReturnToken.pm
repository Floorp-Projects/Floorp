# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Same as AliveTest, but look for a token in the log and return
# the value.  (used for startup iteration test).
sub AliveTestReturnToken {
    my ($test_name, $build_dir, $args, $timeout_secs, $token, $delimiter) = @_;
    my $status;
    my $rv = 0;

    # Same as in AliveTest, needs to match in order to find the log file.
    my $binary = @$args[0];
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/$test_name.log";

    $status = AliveTest($test_name, $build_dir, $args, $timeout_secs);

    # Look for and return token
    if ($status) {
        $rv = Util::extract_token_from_file($binary_log, $token, $delimiter);
        chomp($rv);
        chop($rv) if ($rv =~ /\r$/); # cygwin perl doesn't chomp dos-newlinesproperly so use chop.
        if ($rv) {
            print "AliveTestReturnToken: token value = $rv\n";
        }
    }

    return $rv;
}

return 1;
