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
     $#start += 1;
     @start[ $#start ] = $1;
  }
  if ( /^(\d+\.?\d*)\:\s*nsXULWindow loaded and visible/ ) {   
      $#end += 1;
      @end[ $#end ] = $1;
      # Ignore the first two windows (hidden window and first window).
      if ( $#end > 2 ) {
         # Display simple ascii graph.
         $len = int ( ( $end[$#end] - $start[$#end] + 0.005 ) * 100 );
         print '*' x ($len%80), " ", $end[$#end]-$start[$#end],"\n";
      }
  }
}
$sum = 0.0;
# Ignore the first two windows (hidden window and first window).
foreach $i (3..$#start) {
  $sum += $end[$i] - $start[$i];
}
$avg = $sum/($#start-2);
printf "avg window open time for %d windows: %f\n", $#start-2, $avg;
#</perl>

@rem = ('
:done
@rem ');
