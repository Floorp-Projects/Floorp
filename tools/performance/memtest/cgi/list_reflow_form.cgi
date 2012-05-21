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
    $host_server = "ftp.mozilla.org";
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

# error check for user selecting in both fields ------------------------------------

$number_of_values = @key_value_pairs;
 
if ($number_of_values != 2) {
    print "Content-type: text/html", "\n\n";

    print "<HTML>", "\n";
    print "<HEAD><TITLE>Plotting Error</TITLE><HEAD>", "\n";
    print "<BODY>", "\n";
    print "<H1>", "Plotting Error", "</H1>", "<HR>", "\n";
    print "Please make a selection from each field in the yellow from.", "\n";
    print "</BODY></HTML>", "\n";
    }

# create variables to point to directories containg the data files ---------------

else {
    ($month, $day, $year) = split (/\//, $form_data{"date"});
    $dir_date = "$year$month$day";
    $data_in = $form_data{"data"};
    if ($data_in eq "Reflows per URL") {
	$data_list = "results";
        }
    elsif ($data_in eq "Detailed Results") {
	$data_list = "reflow.out";
        }

    $output_location = join ("/", "ftp:/", $host_server, "pub/data/reflows", $dir_date, $data_list);

# send text to users browser----------------------------------------------------------

    print "Location: $output_location", "\n\n";

    }
#end if

exit (0);












