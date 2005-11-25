use strict;
my ($freeBytes, $freeCount);
my ($usedBytes, $usedCount);
my ($uncommFreeBytes, $uncommFreeCount);
my ($freeAtEndBytes, $freeAtEndCount);
my ($overheadBytes, $overheadCount);
my ($holeBytes, $holeCount, @hole);
my ($commBytes, $uncommBytes);
# track prev address of allocation to detect holes
# Track begin and end address of contiguous block
my ($nextAddr) = 0;
my $holeTolerance = 0;

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
        ($uncommFreeBytes, $uncommFreeCount) = 0;
        ($freeAtEndBytes, $freeAtEndCount) = 0;
        ($overheadBytes, $overheadCount) = 0;
        ($holeBytes, $holeCount) = 0;
        ($commBytes, $uncommBytes) = 0;
        $heading = $1;
        @hole = ();
        next;
    }
    if (/END HEAPDUMP/) {
        # Print results of heapdump
        results();
        next;
    }
    # look for blocks that are used or free
    if (/BEGIN heap/) {
        next;
    }

    if (/END heap/) {
        # Reset nextAddr for overhead detection
        $nextAddr = 0;

        # See if the previous heap ended with a free block
        if ($prevFree) {
            $freeAtEndBytes += $prevFree;
            $freeAtEndCount++;
        }
        $prevFree = 0;
        next;
    }

    if (/REGION ([0-9A-Fa-f]*) : *overhead ([0-9]*) committed ([0-9]*) uncommitted ([0-9]*)/) {
        # Reset nextAddr for overhead detection
        $nextAddr = 0;

        # See if the previous heap ended with a free block
        if ($prevFree) {
            $freeAtEndBytes += $prevFree;
            $freeAtEndCount++;
        }
        $prevFree = 0;

        $commBytes += $3;
        $uncommBytes += $4;
        $overheadBytes += $2;
        $overheadCount++;
        next;
    }

    if (/ *FREE ([0-9A-Fa-f]*) : *([0-9]*) overhead *([0-9]*)/)
    {
        $freeCount++;
        $freeBytes += $2;
        $overheadCount++;
        $overheadBytes += $3;
        # This is a free. Notes it size. If the this is the end of the heap,
        # this is a candidate for compaction.
        $prevFree = $2;
    }
    elsif (/ *USED ([0-9A-Fa-f]*) : *([0-9]*) overhead *([0-9]*)/)
    {
        $usedCount++;
        $usedBytes += $2;
        $overheadCount++;
        $overheadBytes += $3;
        # This wasn't a free
        $prevFree = 0;
    }
    elsif (/ *---- ([0-9A-Fa-f]*) : *([0-9]*) overhead *([0-9]*)/)
    {
        $uncommFreeCount++;
        $uncommFreeBytes += $2;
        # these won't have any overhead
        # we shouldn't view this as a free as we could shed this and
        # reduce our VmSize
        $prevFree = $2;
    }
    else {
        next;
    }
    my $addr = hex $1;
    my $size = $2;
    my $overhead = $3;

    if ($nextAddr && $addr-$nextAddr > $holeTolerance) {
        # found a hole. This is usally alignment overhead
        $holeCount ++;
        $holeBytes += $addr - $nextAddr;
    }
    $nextAddr = $addr + $size + $overhead;
}

sub results()
{
    printf "Heap statistics : $heading\n";
    printf "------------------------------------------------------------\n";
    printf "USED               : %8.2f K in %6d blocks\n", toK($usedBytes), $usedCount;
    printf "FREE               : %8.2f K in %6d blocks\n", toK($freeBytes), $freeCount;
    printf "Uncommitted FREE   : %8.2f K in %6d blocks\n", toK($uncommFreeBytes), $uncommFreeCount;
    printf "Overhead           : %8.2f K in %6d blocks\n", toK($overheadBytes), $overheadCount;
    printf "Alignment overhead : %8.2f K in %6d blocks\n", toK($holeBytes), $holeCount;
    printf "             Total : %8.2f K\n", toK($freeBytes+$usedBytes+$uncommFreeBytes+$overheadBytes+$holeBytes);
    printf "FREE at heap end   : %8.2f K in %6d blocks - %5.2f%% of FREE\n", toK($freeAtEndBytes), $freeAtEndCount, $freeAtEndBytes/($freeBytes+$uncommFreeBytes)*100;
    printf "\n";
    printf "Total Commit    : %8.2f K\n", toK($commBytes);
    printf "Total Uncommit  : %8.2f K\n", toK($uncommBytes);
    printf "          Total : %8.2f K\n", toK($uncommBytes + $commBytes);
}

sub toK()
{
    my $bytes = shift;
    return $bytes / 1024;
}
