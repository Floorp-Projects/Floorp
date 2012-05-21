#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


#______________________________________________________________________________________
# these variables will need to be changed to fit the host machine configuration/directories.
#
# this is where the linux.dat files live;  currently /home/usr/ftp/pub/data/memtests
    $directory_root = "/u/twalker/memtest/results/daily";
#
    $host_server = "smoketest1";
#
# note: the /plots_tmp directory under $results_dir will need to be cleaned out periodically
# because this is where the .png files will be put on creation (it could become a memory hog)
    $results_dir = "/usr/local/apache/htdocs";

# this is where gnuplot app lives
    $gnuplot = "/usr/bin/gnuplot";
#
#--------------------------------------------------------------------------------------

# take in form info and convert it to usable variables --------------------------------
   
$query_string = $ENV{'QUERY_STRING'};

@key_value_pairs = split (/&/, $query_string);

foreach $key_value (@key_value_pairs)
   {       
   ($key, $value) = split (/=/, $key_value);
   $value =~ tr/+/ /;
   $value =~ s/%([\dA-Fa-f][\dA-Fa-f])/pack ("C", hex ($1))/eg;
   $form_data{$key} = $value;
   @list_keys = keys(%form_data);
   @values_keys = values(%form_data);
   }

# error check for user selecting all three fields ------------------------------------

$number_of_values = @key_value_pairs;
 
if ($number_of_values != 3) {
    print "Content-type: text/html", "\n\n";

    print "<HTML>", "\n";
    print "<HEAD><TITLE>Plotting Error</TITLE><HEAD>", "\n";
    print "<BODY>", "\n";
    print "<H1>", "Plotting Error", "</H1>", "<HR>", "\n";
    print "Please close this window and make a selection from each of the three fields.", "\n";
    print "</BODY></HTML>", "\n";
    }

# create variables to point to directories containg the linux.dat files ---------------

else {
    ($month, $day, $year) = split (/\//, $form_data{"first_date"});
    $first_date = "$year$month$day";
    ($month, $day, $year) = split (/\//, $form_data{"compare_date"});
    $compare_date = "$year$month$day";
    $test_list_in = $form_data{"test_list"};
    if ($test_list_in eq "large to small") {
	$test_list = "static41x3_gtol";
        }
    elsif ($test_list_in eq "small to large") {
	$test_list = "static41x3_ltog";
        }
    elsif ($test_list_in eq "plain text") {
	$test_list = "static_vanillaX100";
        }
    elsif ($test_list_in eq "memory hog") {
	$test_list = "static_iplanetX100";
        }

    open(FIRST_DATE_DIR,"$directory_root/$test_list/$first_date/linux.dat") || die "could not create directory\n";
    open(COMPARE_DATE_DIR,"$directory_root/$test_list/$compare_date/linux.dat") || die "could not create directory\n";
    $first_date_dir = join ("/",$directory_root , $test_list, $first_date, "linux.dat");
    $compare_date_dir = join ("/", $directory_root, $test_list, $compare_date, "linux.dat"); 

# get slope of first date line---------------------------------------------------------

    while (<FIRST_DATE_DIR>) { 
	$line = $_;
        }
    @slope_array_first = split (' ',$line);
    $slope_first = $slope_array_first[1];
    $label_placement_first = $slope_array_first[5];
    $line_formula_first = join (" ", $slope_array_first[3], $slope_array_first[2], $slope_array_first[1], $slope_array_first[4], $slope_array_first[5]);
    chop($slope_first);chop($slope_first);chop($slope_first);
    close (FIRST_DATE_DIR);

# get slope of compare date line---------------------------------------------------------

    while (<COMPARE_DATE_DIR>) { 
	$line = $_;
        }
    @slope_array_compare = split (' ',$line);
    $slope_compare = $slope_array_compare[1];
    $label_placement_compare = $slope_array_compare[5];
    $line_formula_compare = join (" ", $slope_array_compare[3], $slope_array_compare[2], $slope_array_compare[1], $slope_array_compare[4], $slope_array_compare[5]);
    chop($slope_compare);chop($slope_compare);chop($slope_compare); 
    close(COMPARE_DATE_DIR);

# generate gnuplot graph --------------------------------------------------------------

    $process_id = $$;
    $output_file = join ("", $results_dir, "/plots_tmp/", $process_id, ".png");
    $output_location = join ("", "http://", $host_server, "/plots_tmp/", $process_id, ".png");
    open(GNUPLOT, "|$gnuplot");
    print GNUPLOT <<gnuplot_Commands_Done;

    set term png color
    set output '$output_file'
    set title 'Gross Dynamic Footprints of $test_list_in list on $form_data{"first_date"} and $form_data{"compare_date"}'
    set xlabel 'URLs'
    set ylabel 'KB'
    set key bottom right Right title 'Legend' box -1
    set label '$slope_first KB/URL' at 5, $label_placement_first right rotate
    set label '$slope_compare KB/URL' at 10, $label_placement_compare right rotate
    plot "$compare_date_dir" using 1:2 title "VM size on $form_data{"compare_date"}" with line 1,$line_formula_compare title "Slope of $form_data{"compare_date"}" with line 7,"$first_date_dir" using 1:2 title "VM size on $form_data{"first_date"}" with line 3,$line_formula_first title "Slope of $form_data{"first_date"}" with line 9,"$compare_date_dir" using 1:4 title "Code size on $form_data{"compare_date"}" with line 2,"$first_date_dir" using 1:4 title "Code size on $form_data{"first_date"}" with line 8
      
gnuplot_Commands_Done

    close(GNUPLOT);

# send graph to users browser----------------------------------------------------------

    print "Location: $output_location", "\n\n";

    }
#end if

exit (0);












