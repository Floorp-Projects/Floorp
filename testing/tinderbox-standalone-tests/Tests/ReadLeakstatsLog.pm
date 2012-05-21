# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Page loader (-f option):
# If you are building optimized, you need to add
#   --enable-trace-malloc --enable-perf-metrics
# to turn the pageloader code on.  If you are building debug you only
# need
#   --enable-trace-malloc
#

sub ReadLeakstatsLog($) {
    my ($filename) = @_;
    my $leaks = 0;
    my $leaked_allocs = 0;
    my $mhs = 0;
    my $bytes = 0;
    my $allocs = 0;

    open LEAKSTATS, "$filename"
        or die "unable to open $filename";
    while (<LEAKSTATS>) {
        chop;
        my $line = $_;
        if ($line =~ /Leaks: (\d+) bytes, (\d+) allocations/) {
            $leaks = $1;
            $leaked_allocs = $2;
        } elsif ($line =~ /Maximum Heap Size: (\d+) bytes/) {
            $mhs = $1;
        } elsif ($line =~ /(\d+) bytes were allocated in (\d+) allocations./) {
            $bytes = $1;
            $allocs = $2;
        }
    }

    return {
        'leaks' => $leaks,
        'leaked_allocs' => $leaked_allocs,
        'mhs' => $mhs,
        'bytes' => $bytes,
        'allocs' => $allocs
        };
}

1;
