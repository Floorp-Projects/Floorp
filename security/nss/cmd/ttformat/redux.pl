#
# redux.pl -- general nss trace data extraction
#
# syntax: redux.pl 
#
# redux.pl reads a file of formatted trace table records from stdin
# The trace records are formatted by nssilock.c 
# redux.pl parses the lines and accumulates data in a hash
# When finished with stdin, redux.pl traverses the hash and emits
# the accumulated data.
# 
# Operation:
# read stdin, accumulate in a hash by file, line, type.
# traverse the hash, reporting data.
#
# raw data format:
#   thredid op ltype callTime heldTime lock line file
#
# Notes:
# After running redux.pl, sort the report on column 4 in decending sequence
# to see where the lock contention is.
#
#
# -----------------------------------------------------------------------
use Getopt::Std;

getopts("h") || die "redux.pl: unrecognized command option";


# -----------------------------------------------------------------------
# read stdin to exhaustion
while ( <STDIN> ) {
   $recordCount++;
#   next if ($recordCount < 36000 ); # skip initialization records
   chomp;
   ($thredid, $op, $ltype, $callTime, $heldTime, $lock, $line, $file) = split;

# select out un-interesting lines
#   next if (( $callTime < $opt_c ) && ( $heldTime < $opt_h ));
#   print $_, "\n";

# count general stats
   $interesting++;

# format the key
   $hashKey = $file ." ". $line ." ". $ltype;

# Update the data in the hash entry 
   $theData = $theHash{$hashKey}; # read it if it already exists
   ( $hCount, $hcallTime, $hheldTime, $hcallMax, $hheldMax ) = split(/\s+/, $theData );
   $hCount++;
   $hcallTime += $callTime;
   $hheldTime += $heldTime;
   $hcallMax  =  ( $hcallMax > $callTime )? $hcallMax : $callTime;
   $hheldMax  =  ( $hheldMax > $heldTime )? $hheldMax : $heldTime;

# Write theData back to the hash
   $theData = $hCount." ".$hcallTime." ".$hheldTime." ".$hcallMax." ".$hheldMax;
   $theHash{$hashKey} = $theData;
} # end while()

# -----------------------------------------------------------------------
# traverse theHash
   printf("%-16s %6s %-16s %8s %8s %8s %8s %8s\n", 
      "File","line","ltype","hits","calltim","heldtim","callmax","heldmax" );
while (($hashKey,$theData) = each(%theHash)) {
   $hashElements++;
   ($file, $line, $ltype) = split(/\s+/, $hashKey );
   ( $hCount, $hcallTime, $hheldTime, $hcallMax, $hheldMax ) = split(/\s+/, $theData );
   printf("%-16s %6d %-16s %8d %8d %8d %8d %8d\n", 
       $file, $line, $ltype, $hCount, $hcallTime, $hheldTime, $hcallMax, $hheldMax );
} # end while()

# -----------------------------------------------------------------------
# dump global statistics
printf ("Record count: %d\n", $recordCount );
printf("Interesting: %d, HashElements: %d\n", $interesting, $hashElements);
