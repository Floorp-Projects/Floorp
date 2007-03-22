# Post processor for timeline output
# Does a diff betn consecutive timeline outputs looking for
# big % differences

my $percent = 1; # anything over 1% of total is of interest to us
my $ignoreDllLoads = 0; # by default don't ignore dll timings

# Read in log to memory
my @lines = <>;

my $total = 0;
# Look for main1's timing
foreach (@lines) {
    if (/^([0-9\.]*): \.\.\.main1/) {
        $total = $1 + 0;
        print "Found main1 time: $total\n";
        last;
    }
}


my $prev = 0;
my $t = $total * $percent / 100;
foreach (@lines) {
    /^([0-9\.]*):/;
    $cur = $1;
    printdiff($cur, $prev, $_);
    if (/total: ([0-9\.]*)/) {
        # print how much % this was
        printf "%4.2f%%", $1/$total*100;
    }
    print "\t$_";
    $prev = $cur;
}


sub printdiff() {
    my $cur = shift;
    my $prev = shift;
    my $line = shift;
    my $diff = $cur - $prev;

    # Make sure we have a considerable difference
    if ($diff < $t) {
        return 0;
    }

    # If we are ignoring dlls and this is a dll line, return
    if ($ignoreDllLoads && $line =~ /PR_LoadLibrary/) {
        return 0;
    }

    # if significant time elapsed print it
    printf "%4.2f%% %5.3f\n", $diff/$total*100, $diff;

    return 1;
}
