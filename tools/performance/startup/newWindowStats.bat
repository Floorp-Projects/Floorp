@rem = ('
@perl newWindowStats.bat %1 %2 %3 %4 %5 %6
@goto done
@rem ');

#<perl>
# Get numbers from stdin.
@start = ();
@end   = ();
while (<>) {
  if ( /^(\d+\.?\d*)\:\s*nsXULWindow created/ ) {
     $startTime = $1;
  }
  if ( /^(\d+\.?\d*)\:\s*nsXULWindow loaded and visible/ ) {   
      $#start += 1;
      @start[ $#start ] = $startTime;
      $#end += 1;
      @end[ $#end ] = $1;
      # Ignore the first two windows (hidden window and first window).
      if ( $#start > 1 ) {
         # Display simple ascii graph.
         $len = int ( ( $end[$#end] - $start[$#start] + 0.005 ) * 100 );
         if ( $#start == 2 ) {
            # Shift graph left this many; presumes subsequent values will be greater.
            $first = $len - 1;
         }
         $len -= $first;
         print '*' x ($len%80), "\n";
      }
  }
}
$sum = 0.0;
# Ignore the first two windows (hidden window and first navigator window).
foreach $i (2..$#start) {
  $sum += $end[$i] - $start[$i];
}
$avg = $sum/($#start-1);
printf "avg window open time for %d windows: %f\n", $#start-1, $avg;
#</perl>

@rem = ('
:done
@rem ');
