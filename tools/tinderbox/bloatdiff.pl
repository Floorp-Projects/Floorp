#!/usr/bonsaitools/bin/perl -w

$OLDFILE = $ARGV[0];
$NEWFILE = $ARGV[1];

sub processFile {
    local ($FILENAME, $map) = @_;
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
    local ($key) = @_;
    local $oldLeaks = $oldMap{$key}{leaked};
    local $newLeaks = $newMap{$key}{leaked};
    local $percentLeaks = 0;
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
    local ($key) = @_;
    local $newBloat = $newMap{$key}{objTotal} * $newMap{$key}{size};
    local $percentBloat = 0;
    local $oldSize = $oldMap{$key}{size};
    if (defined($oldSize)) {
        local $oldTotal = $oldMap{$key}{objTotal} || 0;
        local $oldBloat = $oldTotal * $oldSize;
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

if ($NEWFILE =~ /all\-(\d+)\-(\d+)\-(\d+)\-(\d{2})(\d{2})(\d{2}).txt/) {
    $summary = "delta-$1-$2-$3-$4$5$6.txt";
}
else {
    local ($sec,$min,$hour,$mday,$mon,$year) = localtime(time);
    $summary = sprintf "delta-%s-%02d-%02d-%02d%02d%02d.txt",
                       ($year, $mon+1, $mday, $hour, $min, $sec);
}

open(SF, ">$summary");
print SF "Bloat/Leak Delta Report\n";
print SF "Current file:  $NEWFILE\n";
print SF "Previous file: $OLDFILE\n";
print SF "--------------------------------------------------------------------------\n";

%keys = ();
@keys = sort { $newMap{$b}{leaked} <=> $newMap{$a}{leaked}
               ||  $newMap{$b}{size} * $newMap{$b}{objTotal}
               <=> $newMap{$a}{size} * $newMap{$a}{objTotal}} keys %newMap;
printf SF "%-20s %10s  %10s %10s  %10s\n", ("CLASS", "LEAKS", "delta", "BLOAT", "delta");
printf SF "--------------------------------------------------------------------------\n";
foreach $key (@keys) {
    local ($newLeaks, $percentLeaks) = leaksDelta($key);
    local ($newBloat, $percentBloat) = bloatDelta($key);
    printf SF "%-20s %10s %10.2f%% %10s %10.2f%%\n",
    ($key, $newLeaks, $percentLeaks, $newBloat, $percentBloat);
}

close(SF);

################################################################################

printf "<pre><font size=-1><a href=$summary>\nleaks: %10d %+8.2f%%\n", leaksDelta("TOTAL");
printf "bloat: %10d %+8.2f%%\n</a></font></pre>\n", bloatDelta("TOTAL");

