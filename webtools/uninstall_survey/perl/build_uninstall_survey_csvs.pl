#!/usr/bin/perl -w

# This script is designed to be used with the Mozilla Uninstall Survey
# It will connect to the database and pull results into a .csv for each product
# ("Mozilla Firefox 1.5", etc.).  The .csv's are saved to the specified directory
# and, once created, updated incrementally (only new rows are pushed).
# There is no output if everything goes well (this will probably be a cron script)
#   
# @author Wil Clouser <wclouser@mozilla.com>

use strict;
use DBI;

# We bring variables from the config, and perl will complain
no warnings 'once';

# All configuration values should be in config.pl
my $config = "config.pl";

if (-r $config) {
    my $return = do "config.pl";
    exit_with_error("Couldn't parse configuration: $@") if $@;
    exit_with_error("Couldn't \"do\" configuration (read permissions?): $!") unless defined $return;
} else {
    exit_with_error("Can't read the configuration file ($config)");
}


# Do some initial tests to make sure things don't break further on
    if (! -d $main::output_dir) {
        die "Destination is not a directory: $main::output_dir\n";
    }

    if (! -w $main::output_dir) {
        die "Cannot write to directory: $main::output_dir\n";
    }

    my $dbh = DBI->connect($main::dsn, $main::user, $main::pass)
        or die "Can't connect to the db: $DBI::errstr\n";
        
    my $csvh = DBI->connect($main::csv_dsn)
        or die "Can't connect to the csv db: $DBI::errstr\n";


# Setup some variables for use in the main loop
    my $applications = get_current_applications();
    
    my $results_query = $dbh->prepare("
        SELECT
            `results`.`id`,
            `results`.`created`,
            `choices`.`description` as `intention`,
            `choices_results`.`other` as `intention_other`,
            `results`.`comments`
        FROM `results` 
        LEFT JOIN `choices_results` ON `results`.`id`=`choices_results`.`result_id` 
        INNER JOIN `choices` ON `choices_results`.`choice_id` = `choices`.`id`
        INNER JOIN `applications` ON `applications`.`id` = `results`.`application_id`
        WHERE
            `applications`.`name` LIKE ?
        AND 
            `applications`.`version` LIKE ?
        AND
            `results`.`id` > ?
        AND
            `choices`.`type` = 'intention'
        ORDER BY 
            `results`.`created` ASC");

    # table name is arbitrary
    my $csv = $csvh->prepare("INSERT INTO csv VALUES(?,?,?,?,?)");

# Main Loop
    foreach my $apps ( @$applications ) {
        # Clean spaces from names.  In the future if there are other strange
        # characters, we'll probably want to replace them too.
        my $application_name = $apps->{name};
            $application_name =~ tr/ /_/;
        my $application_version = $apps->{version};
            $application_version =~ tr/ /_/;

        my $filename = "export-$application_name"."_"."$application_version.csv";

        # Used for incremental additions.  Default to adding rows starting at zero
        my $maxid = 0;

        $csvh->{'csv_tables'}->{'csv'} = { 'file' => $filename, 'col_names' => ['id','created','intention','intention_other','comments']};

        # If the file doesn't exist, this will create it.
        if (! -f $filename) {
            open CSVFILE, ">$main::output_dir/$filename" or
                die "ERROR: Could not open file: $main::output_dir/$filename!";
            close CSVFILE;
        } else {
            # Pull out the max ID for an incremental update
            ($maxid) = $csvh->selectrow_array("SELECT MAX(id) FROM csv");

            # If the CSV is empty (but exists), this will be undefined.  In this case, start from zero
            $maxid = (defined $maxid) ? $maxid : 0;
        }

        $results_query->execute($apps->{name},$apps->{version}, $maxid);

        # grab results from the db and send to the csv
        while ( my @row = $results_query->fetchrow_array ) {
            if ($main::privacy) {
                # Pull out phone numbers and email addresses.  We have to compile the
                # right side of the substitution because of 'use strict;'
                if ($row[3]) {
                    $row[3] =~ s/([0-9]{3})[ .-]?[0-9]{4}/(defined $1 ? $1 : '')."-****"/ge;
                    $row[3] =~ s/\ ?(.+)?@(.+)?[.,](.+)?\ ?/(defined $1 ? $1 : '')."@****.".(defined $3 ? $3 : '')/ge;
                }

                if ($row[4]) {
                    $row[4] =~ s/([0-9]{3})[ .-]?[0-9]{4}/(defined $1 ? $1 : '')."-****"/ge;
                    $row[4] =~ s/\ ?(.+)?@(.+)?[.,](.+)?\ ?/(defined $1 ? $1 : '')."@****.".(defined $3 ? $3 : '')/ge;
                }
            }
            $csv->execute(@row);
        }
    }

# Finish Up
    $csv->finish();
    $results_query->finish();
    $csvh->disconnect();
    $dbh->disconnect();

# ---- Sub-routines ----
# ----------------------

sub get_current_applications {
    # Pulling only visible rows is purely a speed consideration - feel free to remove it
    return $dbh->selectall_arrayref("SELECT * FROM applications WHERE visible=1 ORDER BY id", { Slice => {} });
}

sub get_max_id {
    # Pulling only visible rows is purely a speed consideration - feel free to remove it
    return $dbh->selectall_arrayref("SELECT MAX(id) FROM applications WHERE visible=1 ORDER BY id", { Slice => {} });
}

sub exit_with_error {
    my $error = shift;
    print "\n$error\n";
    print "Exiting.\n";
    exit 1;
}
