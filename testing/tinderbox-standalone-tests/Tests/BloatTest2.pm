# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sub BloatTest2 {
    my ($binary, $build_dir, $timeout_secs) = @_;
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $PERL = $^X;
    if ($Settings::OS =~ /^WIN/ && $build_dir !~ m/^.:\//) {
        chomp($build_dir = `cygpath -w $build_dir`);
        $build_dir =~ s/\\/\//g;
        $PERL = "perl";
    }
    my $binary_log = "$build_dir/bloattest2.log";
    my $malloc_log = "$build_dir/malloc.log";
    my $sdleak_log = "$build_dir/sdleak.log";
    my $old_sdleak_log = "$build_dir/sdleak.log.old";
    my $leakstats_log = "$build_dir/leakstats.log";
    my $old_leakstats_log = "$build_dir/leakstats.log.old";
    my $sdleak_diff_log = "$build_dir/sdleak.diff.log";
    local $_;

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

    rename($sdleak_log, $old_sdleak_log);

    my @args;
    if($Settings::BinaryName eq "TestGtkEmbed" ||
       $Settings::BinaryName =~ /^firefox/) {
      @args = ($binary_basename, "-P", $Settings::MozProfileName,
               "resource:///res/bloatcycle.html",
               "--trace-malloc", $malloc_log);
    } else {
      @args = ($binary_basename, "-P", $Settings::MozProfileName,
               "-f", "bloaturls.txt",
               "--trace-malloc", $malloc_log);
    }

    # win32 builds crash on multiple runs when --shutdown-leaks is used
    @args = (@args, "--shutdown-leaks", $sdleak_log) unless $Settings::OS =~ /^WIN/;
    my $result = Util::run_cmd($build_dir, $binary_dir, \@args, $timeout_secs);

    Util::print_logfile($binary_log, "trace-malloc bloat test");

    if ($result->{timed_out}) {
        Util::print_log("Error: bloat test timed out after"
            ." $timeout_secs seconds.\n");
        return 'testfailed';
    } elsif ($result->{exit_value}) {
        Util::print_test_errors($result, $binary_basename);
        Util::print_log("Error: bloat test failed.\n");
        return 'testfailed';
    }

    rename($leakstats_log, $old_leakstats_log);

    if ($Settings::OS =~ /^WIN/) {
        @args = ("leakstats", $malloc_log);
    } else {
        @args = ("run-mozilla.sh", "./leakstats", $malloc_log);
    }
    $result = Util::run_cmd($build_dir, $binary_dir, \@args, $timeout_secs);
    Util::print_logfile($leakstats_log, "trace-malloc bloat test: leakstats");

    my $newstats = ReadLeakstatsLog($leakstats_log);
    my $oldstats;
    if (-e $old_leakstats_log) {
        $oldstats = ReadLeakstatsLog($old_leakstats_log);
    } else {
        $oldstats = $newstats;
    }
    my $leakchange = PercentChange($oldstats->{'leaks'}, $newstats->{'leaks'});
    my $mhschange = PercentChange($oldstats->{'mhs'}, $newstats->{'mhs'});

    my $leaks_testname_label   = "Leaks: total bytes 'malloc'ed and not 'free'd";
    my $maxheap_testname_label = "Maximum Heap: max (bytes 'malloc'ed - bytes 'free'd) over run";
    my $allocs_testname_label  = "Allocations: number of calls to 'malloc' and friends";

    my $embed_prefix = "";
    if($Settings::BinaryName eq "TestGtkEmbed") {
      $embed_prefix = "m";
    }

    my $leaks_testname       = "trace_malloc_leaks";
    Util::print_log_test_result_bytes($leaks_testname, $leaks_testname_label,
                                $newstats->{'leaks'},
                                $embed_prefix . 'Lk', 3);

    my $maxheap_testname       = "trace_malloc_maxheap";
    Util::print_log_test_result_bytes($maxheap_testname,
                                $maxheap_testname_label,
                                $newstats->{'mhs'},
                                $embed_prefix . 'MH', 3);

    my $allocs_testname       = "trace_malloc_allocs";
    Util::print_log_test_result_count($allocs_testname, $allocs_testname_label,
                                $newstats->{'allocs'},
                                $embed_prefix . 'A', 3);

    if ($Settings::TestsPhoneHome) {
        # Send results to server.
        Util::send_results_to_server($newstats->{'leaks'},  "--", $leaks_testname);
        Util::send_results_to_server($newstats->{'mhs'},    "--", $maxheap_testname);
        Util::send_results_to_server($newstats->{'allocs'}, "--", $allocs_testname);
    }

    if (-e $old_sdleak_log && -e $sdleak_log) {
        Util::print_logfile($old_leakstats_log, "previous run of trace-malloc bloat test leakstats");
        @args = ($PERL, "$build_dir/mozilla/tools/trace-malloc/diffbloatdump.pl", "--depth=15", $old_sdleak_log, $sdleak_log);
        $result = Util::run_cmd($build_dir, $binary_dir, \@args, $timeout_secs);
        Util::print_logfile($sdleak_diff_log, "trace-malloc leak stats differences");
    }

    return 'success';
}

1;
