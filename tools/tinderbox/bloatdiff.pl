#!/usr/bonsaitools/bin/perl -w

$OLDFILE = $ARGV[0];
$NEWFILE = $ARGV[1];

sub processFile {
    my ($FILENAME, $map) = @_;
    open(FH, $FILENAME);
    while (<FH>) {
        if (/^\s+(\d+) (\w+)\s+([\-\d]+)\s+([\-\d]+)\s+([\-\d]+)\s+([\-\d]+) \(\s+([\-\d\.]+) \+\/\-\s+([\-\d\.]+)\)\s+([\-\d]+)\s+([\-\d]+) \(\s+([\-\d\.]+) \+\/\-\s+([\-\d\.]+)\)/) {
            $name = $2;
            $size = $3;
            $leaked = $4;
            $objTotal = $5;
            $objRem = $6;
            $objMean = $7;
            $objStdDev = $8;
            $refTotal = $9;
            $refRem = $10;
            $refMean = $11;
            $refStdDev = $12;
            $$map{$name} = { name=>$name,
                             size=>$size,
                             leaked=>$leaked,
                             objTotal=>$objTotal,
                             objRem=>$objRem,
                             objMean=>$objMean,
                             objStdDev=>$objStdDev,
                             refTotal=>$refTotal,
                             refRem=>$refRem,
                             refMean=>$refMean,
                             refStdDev=>$refStdDev };
        }
    }
    close(FH);
}

%oldMap = ();
processFile($OLDFILE, \%oldMap);

%newMap = ();
processFile($NEWFILE, \%newMap);

################################################################################

sub leaksDelta {
    my ($key) = @_;
    my $oldLeaks = $oldMap{$key}{leaked};
    my $newLeaks = $newMap{$key}{leaked};
    my $percentLeaks = 0;
    if (defined($oldLeaks)) {
        if ($oldLeaks == 0) {
            if ($newLeaks != 0) {
                # there weren't any leaks before, but now there are!
                $percentLeaks = 9999999.99;
            }
        }
        else {
            $percentLeaks = ($newLeaks - $oldLeaks) / $oldLeaks * 100;
        }
    }
    # else we had no record of this class before
    return ($newLeaks, $percentLeaks);
}
    
################################################################################

sub bloatDelta {
    my ($key) = @_;
    my $newBloat = $newMap{$key}{objTotal} * $newMap{$key}{size};
    my $percentBloat = 0;
    my $oldSize = $oldMap{$key}{size};
    if (defined($oldSize)) {
        my $oldTotal = $oldMap{$key}{objTotal} || 0;
        my $oldBloat = $oldTotal * $oldSize;
        if ($oldBloat == 0) {
            if ($newBloat != 0) {
                # this class wasn't used before, but now it is
                $percentBloat = 9999999.99;
            }
        }
        else {
            $percentBloat = ($newBloat - $oldBloat) / $oldBloat * 100;
        }
    }
    # else we had no record of this class before
    return ($newBloat, $percentBloat);
}

################################################################################

print "Bloat/Leak Delta Report\n";
print "Current file:  $NEWFILE\n";
print "Previous file: $OLDFILE\n";
print "--------------------------------------------------------------------------\n";

%keys = ();
@keys = sort { $newMap{$b}{leaked} <=> $newMap{$a}{leaked}
               ||  $newMap{$b}{size} * $newMap{$b}{objTotal}
               <=> $newMap{$a}{size} * $newMap{$a}{objTotal}} keys %newMap;
printf "%-20s %10s  %10s %10s  %10s\n", ("CLASS", "LEAKS", "delta", "BLOAT", "delta");
printf "--------------------------------------------------------------------------\n";
foreach $key (@keys) {
    my ($newLeaks, $percentLeaks) = leaksDelta($key);
    my ($newBloat, $percentBloat) = bloatDelta($key);
    printf "%-20s %10s %10.2f%% %10s %10.2f%%\n",
    ($key, $newLeaks, $percentLeaks, $newBloat, $percentBloat);
}

################################################################################

