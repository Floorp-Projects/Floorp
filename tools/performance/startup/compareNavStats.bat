@rem = ('
@perl compareNavStats.bat %1 %2 %3 %4 %5 %6
@goto done
@rem ');

#<perl>
if ( @ARGV + 0 ) {
   # Get numbers from args.
   @desc = ( "first", "second" );
   $sum1 = $ARGV[0] + $ARGV[1] + $ARGV[2];
   $sum2 = $ARGV[3] + $ARGV[4] + $ARGV[5];
} else {
   # Get numbers from stdin.
   @args = ();
   @desc = ();
   while (<>) {
     print;
     if ( /^----- (.*) -----/ ) {
        $#desc += 1;
        @desc[ $#desc ] = $1;
     }
     if ( /^(\d+\.?\d*)\:\s*Navigator Window visible now/ ) {   
        $#args += 1;
        @args[ $#args ] = $1;
     }
   }
   $sum1 = $args[0] + $args[1] + $args[2];
   $sum2 = $args[3] + $args[4] + $args[5];
}
$avg1 = $sum1/3;
$avg2 = $sum2/3;
$diff = $avg1 - $avg2;
printf "%s=%f %s=%f diff=%f (%f%% %s)\n", $desc[0], $avg1, $desc[1], $avg2, $diff, abs(($diff/$avg1)*100), ($diff>=0 ? "faster" : "slower");
#</perl>

@rem = ('
:done
@rem ');
