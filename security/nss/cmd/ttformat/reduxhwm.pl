#
# reduxhwm.pl -- analyze highwatermark data in xxxLogfile 
#
# example interesting line in xxxLogfile
# 1026[8154da0]: selfserv: Launched thread in slot 37, highWaterMark: 63 
#
# 
#
while ( <STDIN> ) {
   chomp;
   ($proc, $who, $launched, $thread, $in, $slotx, $slot, $hwm, $highwatermark) = split;
   if ( $launched == "Launched" ) {
      next if ( $slot == 0 );
      $notInteresting++;
      if ( $hwmMax < $highwatermark ){
         $hwmMax = $highwatermark;
      }
      $hwmArray[$slot] += 1;
      $interesting++;
   }
} # end while()

printf ("Interesteing:    %d\n", $interesting );
printf ("Not Interesting: %d\n", $notInteresting - $interesting );

foreach $element (@hwmArray) {
    $percent = 100*($element / $interesting);
    $percentTotal += $percent;
    printf("Slot %2d: %d hits, %2.2f percent, %2.2f total percent\n", $i, $element, $percent, $percentTotal );
    $i++;
}
printf("Sum of percentages: %3.2f\n", $percentTotal );
# --- end ---
