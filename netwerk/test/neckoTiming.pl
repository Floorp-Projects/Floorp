#neckoTiming.pl
#
# This file drives the Neck Page Load Timer.
# Written by John A. Taylor in 2001

use Strict;

my @pageList;
my @fileList;
my @timeList;
my @minTimes;
my @maxTimes;
my @avgTimes;
my $file_name; # = "PageList_all_pages.txt";
my $output_file = "timing_output.html";
my $test_loc; #  = " c:/builds/src/mozilla/dist/WIN32_O.OBJ/bin/TestPageLoad";
my $test_com;
my $num_tests=1;
my $num_pages=0;

if($#ARGV != 2){
  usage(@ARGV);
}

$num_tests = $ARGV[0];
$file_name = $ARGV[1];
$test_loc  = $ARGV[2];

push (@pageList, GetTestList($file_name));

my $i, $j, this;
for($i = 0; $i < $num_tests; $i++) {
  $j=0;
  foreach $page (@pageList) {
    my $start, $finish, $total;
    $test_com = $test_loc . " " . $page . " | ";
    printf "Testing (%d/%d): $page\n",$i*$num_pages+$j+1, $num_tests * $num_pages;
    #  $start = (times)[0];
    open (TEST, $test_com) 
      || die ("Could not find test program: $test_loc");
    while(<TEST>) {
      if(/>>PageLoadTime>>(\d*)>>/){
	print "$1\n";
	$this = $1/1000;
	$timeList[$j][$i] = $this;
	@avgTimes[$j] += $this;
	if($this > @maxTimes[$j] || $i == 0) {
	  @maxTimes[$j] = $this;
	}
	if($this < @minTimes[$j] || $i == 0) {
	  @minTimes[$j] = $this;
	}
      }
    }
    $j++;
  }
}

for($i = 0; $i < $j; $i++) {
  @avgTimes[$i] /= $num_tests;
}

PrintReport();
print "\nHTML formated report is in: timing_output.html\nin this directory\n";
exit;

my $num_cols = $num_tests + 4;
sub PrintReport {
  my $j=0;
  open (OUT, ">$output_file");
  print OUT "<HTML><BODY><H3>Necko Timing Test Results</H3>",
            "<b>Number of iterations: $num_tests</b>",
            "<TABLE border=1 cols=$num_cols><tr><td>Page Location</td><td>Avg Time</td>",
	    "<td>Max Time</td><td>Min Time</td><td colspan=$num_tests>Times ... </td>";

  foreach $page(@pageList) {
    #$loc=$page;
    #$max_time = @maxTimes[$j];
    #$min_time = @minTimes[$j];
    #$avg_time = @avgTimes[$j];
    #$time_1   = $timeList[$j][0];
    #write;
    printf OUT "<tr><td>$page</td><td>%d</td><td>@maxTimes[$j]</td><td>@minTimes[$j]</td>", @avgTimes[$j];
    #print "\n$page\t\t@avgTimes[$j]\t@maxTimes[$j]\t@minTimes[$j]\t";
    my $i;
    for($i=0; $i < $num_tests; $i++){
      print OUT "<td>$timeList[$j][$i]</td>";
    }
    print OUT "</tr>";
    $j++;
  }
  print OUT "</TABLE><H4><I>Report Done</I></H4></BODY></HTML>";
}

sub GetTestList {
    my ($list_file) = @_;
    my @retval = ();

    open (TESTLIST, $list_file) ||
      die("Could not find test list file: '$list_file': $!\n");

        while (<TESTLIST>) {
	  s/\n$//;
	  if (!(/\s*\#/)) {
	    # It's not a comment, so process it
	    push (@retval, $_);
	    $num_pages++;
	  }
        }

    close (TESTLIST);
    return @retval;
}

sub usage {
    print STDERR 
      ("\nusage: $0 NUM FILE LOC \n\n" .
       "NUM             Number of iterations you want\n" .
       "FILE            Location of input file containing test pages\n" .
       "LOC             Path (including executable) of TestPageLoad\n" .
       "                (should be in same directory as mozilla bin\n".
       "                  ex: /builds/mozilla/dist/bin/TestPageLoad )\n\n");
    exit (1);
}
