#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


#______________________________________________________________________________________
# these variables will need to be changed to fit the host machine configuration/directories.
#
# this is where the linux.dat files live;  currently /home/usr/ftp/pub/data/memtests
    $directory_root = "/u/curt/reflow/results/";
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
#$query_string = "first_date=04%2F17%2F01&compare_date=04%2F12%2F01";

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

# error check for user selecting both fields ------------------------------------

$number_of_values = @key_value_pairs;
 
if ($number_of_values != 2) {
    print "Content-type: text/html", "\n\n";

    print "<HTML>", "\n";
    print "<HEAD><TITLE>Plotting Error</TITLE><HEAD>", "\n";
    print "<BODY>", "\n";
    print "<H1>", "Plotting Error", "</H1>", "<HR>", "\n";
    print "Please make a selection from each of the date fields in the green form.", "\n";
    print "</BODY></HTML>", "\n";
    }

# create variables to point to directories containg the reflow.dat files ---------------

else {
    ($month, $day, $year) = split (/\//, $form_data{"first_date"});
    $first_date = "$year$month$day";
    ($month, $day, $year) = split (/\//, $form_data{"compare_date"});
    $compare_date = "$year$month$day";
    $first_date_dir = join ("/",$directory_root , $test_list, $first_date, "reflow.dat");
    $compare_date_dir = join ("/", $directory_root, $test_list, $compare_date, "reflow.dat");
    

# generate gnuplot graph --------------------------------------------------------------

    $process_id = $$;
    $output_file = join ("", $results_dir, "/plots_tmp/", $process_id, ".png");
    $output_location = join ("", "http://", $host_server, "/plots_tmp/", $process_id, ".png");
    open(GNUPLOT, "|$gnuplot");
    print GNUPLOT <<gnuplot_Commands_Done;

    set term png color
    set output '$output_file'
    set title 'Reflow counts of selcted URL's on $test_list_in list on $form_data{"first_date"} and $form_data{"compare_date"}'
    set xlabel 'URLs'
    set ylabel 'Reflows'
    set key top left Right title 'Legend' box -1
    plot "$compare_date_dir" using 1:2 title "Reflows on $form_data{"compare_date"}" with line 1,"$first_date_dir" using 1:2 title "Reflows on $form_data{"first_date"}" with line 3
      
gnuplot_Commands_Done

    close(GNUPLOT);

# send graph to users browser----------------------------------------------------------

    print "Location: $output_location", "\n\n";

    }
#end if

exit (0);












