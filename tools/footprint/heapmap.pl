use strict;
my ($freeBytes, $freeCount);
my ($usedBytes, $usedCount);
my ($freeAtEndBytes, $freeAtEndCount);
my ($overheadBytes, $overheadCount);
my ($holeBytes, $holeCount, @hole);
# track prev address of allocation to detect holes
# Track begin and end address of contiguous block
my ($nextAddr);
my $heapJump = 0;
my $holeTolerance = 100 * 1024;

# Heading for heap dump
my $heading;

# Notes the previous block size if it was a free to track freeAtEnd.
# If prev block was not a free, this would be set to zero.
my $prevFree = 0;

while(<>)
{
    if (/BEGIN HEAPDUMP : (.*)$/) {
        # Initialize all variables
        ($freeBytes, $freeCount) = 0;
        ($usedBytes, $usedCount) = 0;
        ($freeAtEndBytes, $freeAtEndCount) = 0;
        ($overheadBytes, $overheadCount) = 0;
        ($holeBytes, $holeCount) = 0;
        $heading = $1;
        @hole = ();
        next;
    }
    if (/END HEAPDUMP/) {
        # Print results of heapdump
        results();
    }
    # look for blocks that are used or free
    if (/Processing heap/) {
        # make sure the jump from one heap to another is not counted
        # as a hole
        $heapJump = 1;

        # See if the previous heap ended with a free block
        if ($prevFree) {
            $freeAtEndBytes += $prevFree;
            $freeAtEndCount++;
        }
        next;
    }
    elsif (/ *FREE block at ([0-9A-Fa-f]*) of size *([0-9]*) overhead *([0-9]*)/)
    {
        $freeCount++;
        $freeBytes += $2;
        $overheadCount++;
        $overheadBytes += $3;
        # This is a free. Notes it size. If the this is the end of the heap,
        # this is a candidate for compaction.
        $prevFree = $2;
    }
    elsif (/ *USED block at ([0-9A-Fa-f]*) of size *([0-9]*) overhead *([0-9]*)/)
    {
        $usedCount++;
        $usedBytes += $2;
        $overheadCount++;
        $overheadBytes += $3;
        # This wasn't a free
        $prevFree = 0;
    }
    else {
        next;
    }
    my $addr = hex $1;
    my $size = $2;
    my $overhead = $3;

    if (!$heapJump && defined($nextAddr) && $addr-$nextAddr > $holeTolerance) {
        # found a hole
        push @hole, $nextAddr;
        push @hole, $addr;
        $holeCount ++;
        $holeBytes += $addr - $nextAddr;
    }
    $nextAddr = $addr + $size + $overhead;
    $heapJump = 0;
}

sub results()
{
    printf "Heap statistics : $heading\n";
    printf "------------------------------------------------------------\n";
    printf "FREE            : %8.2f K in %d blocks\n", toK($freeBytes), $freeCount;
    printf "USED            : %8.2f K in %d blocks\n", toK($usedBytes), $usedCount;
    printf "Overhead        : %8.2f K in %d blocks\n", toK($overheadBytes), $overheadCount;
    printf "Total           : %8.2f K\n", toK($freeBytes+$usedBytes+$overheadBytes);
    printf "Hole            : %8.2f K in %d holes\n", toK($holeBytes), $holeCount;
    printf "Total w/hole    : %8.2f K\n", toK($freeBytes+$usedBytes+$overheadBytes+$holeBytes);
    printf "FREE at end of heap : %8.2f K in %d heaps - %5.2f%% of FREE\n", toK($freeAtEndBytes), $freeAtEndCount, $freeAtEndBytes/$freeBytes*100;
    for (my $i=0; $i<$holeCount; $i++) {
        printf "hole %08x - %08x : %8.2f K\n", $hole[$i], $hole[$i+1], toK($hole[$i+1] - $hole[$i]);
    }
    printf "\n";
}

sub toK()
{
    my $bytes = shift;
    return $bytes / 1024;
}
