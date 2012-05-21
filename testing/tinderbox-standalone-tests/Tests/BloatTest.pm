# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Page loader/cycling mechanism (mozilla -f option):
# If you are building optimized, you need to add
#   --enable-logrefcnt
# to turn the pageloader code on.  These are on by default for debug.
#
sub BloatTest {
    my ($binary, $build_dir, $bloat_args, $bloatdiff_label, $timeout_secs) = @_;
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/bloat-cur.log";
    my $old_binary_log = "$build_dir/bloat-prev.log";
    local $_;

    rename($binary_log, $old_binary_log);

    unless (-e "$binary_dir/bloaturls.txt") {
        Util::print_log("Error: bloaturls.txt does not exist.\n");
        return 'testfailed';
    }

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';
    # If on Windows, make sure the urls file has dos lineendings, or
    # mozilla won't parse the file correctly
    if ($platform eq 'windows') {
	my $bu = "$binary_dir/bloaturls.txt";
        open(IN,"$bu") || die ("$bu: $!\n");
        open(OUT,">$bu.new") || die ("$bu.new: $!\n");
        while (<IN>) {
	    if (! m/\r\n$/) {
                s/\n$/\r\n/;
            }
	    print OUT "$_";
        } 
        close(IN);
        close(OUT);
        File::Copy::move("$bu.new", "$bu") or die("move: $!\n");
    }

    $ENV{XPCOM_MEM_BLOAT_LOG} = 1; # Turn on ref counting to track leaks.

    # Build up binary command, look for profile.
    my @args = ($binary_basename);
    unless (($Settings::MozProfileName eq "") or 
            ($Settings::BinaryName eq "TestGtkEmbed")) {
        @args = (@args, "-P", $Settings::MozProfileName);
    }
    @args = (@args, @$bloat_args);

    my $result = Util::run_cmd($build_dir, $binary_dir, \@args, $timeout_secs);
    $ENV{XPCOM_MEM_BLOAT_LOG} = 0;
    delete $ENV{XPCOM_MEM_BLOAT_LOG};

    Util::print_logfile($binary_log, "$bloatdiff_label bloat test");

    if ($result->{timed_out}) {
        Util::print_log("Error: bloat test timed out after"
            ." $timeout_secs seconds.\n");
        return 'testfailed';
    } elsif ($result->{exit_value}) {
        Util::print_test_errors($result, $binary_basename);
        Util::print_log("Error: bloat test failed.\n");
        return 'testfailed';
    }

    # Print out bloatdiff stats, also look for TOTAL line for leak/bloat #s.
    Util::print_log("<a href=#bloat>\n######################## BLOAT STATISTICS\n");
    my $found_total_line = 0;
    my @total_line_array;
    open DIFF, "perl $build_dir/../bloatdiff.pl $build_dir/bloat-prev.log $binary_log $bloatdiff_label|"
        or die "Unable to run bloatdiff.pl";
    while (<DIFF>) {
        Util::print_log($_);

        # Look for fist TOTAL line
        unless ($found_total_line) {
            if (/TOTAL/) {
                @total_line_array = split(" ", $_);
                $found_total_line = 1;
            }
        }
    }
    close DIFF;
    Util::print_log("######################## END BLOAT STATISTICS\n</a>\n");


    # Scrape for leak/bloat totals from TOTAL line
    # TOTAL 23 0% 876224
    my $leaks = $total_line_array[1];
    my $bloat = $total_line_array[3];

    Util::print_log("leaks = $leaks\n");
    Util::print_log("bloat = $bloat\n");

    # Figure out what the label prefix is.
    my $label_prefix = "";
    my $testname_prefix = "";
    unless($bloatdiff_label eq "") {
        $label_prefix = "$bloatdiff_label ";
        $testname_prefix = "$bloatdiff_label" . "_";
    }

    # Figure out testnames to send to server
    my $leaks_testname = "refcnt_leaks";
    my $bloat_testname = "refcnt_bloat";
    unless($bloatdiff_label eq "") {
        $leaks_testname = $testname_prefix . "refcnt_leaks";
        $bloat_testname = $testname_prefix . "refcnt_bloat";
    }

    # Figure out testname labels
    my $leaks_testname_label = "refcnt Leaks";
    my $bloat_testname_label = "refcnt Bloat";
    unless($bloatdiff_label eq "") {
        $leaks_testname_label = $label_prefix . $leaks_testname_label;
        $bloat_testname_label = $label_prefix . $bloat_testname_label;
    }

    my $embed_prefix = "";
    if($Settings::BinaryName eq "TestGtkEmbed") {
      $embed_prefix = "m";
    }

    Util::print_log_test_result_bytes($leaks_testname, $leaks_testname_label, 
                                $leaks,
                                $label_prefix . $embed_prefix . 'RLk', 3);

    if($Settings::TestsPhoneHome) {
        # Report numbers to server.
        Util::send_results_to_server($leaks, "--", $leaks_testname);
    }

    return 'success';
}

1;
