# Post processor for timeline output
# Does a diff betn consecutive timeline outputs looking for
# big % differences

my $percent = 1; # anything over 1% of total is of interest to us
my $ignoreDllLoads = 0; # by default dont ignore dll timings

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
    # if significant time elapsed print it
    $diff = $cur - $prev;
    if ($diff > $t && (!$ignoreDllLoads || ! /PR_LoadLibrary/)) {
        printf "%4.2f%% %5.3f\n", $diff/$total*100, $diff;
    }
    print "\t$_";
    $prev = $cur;
}
