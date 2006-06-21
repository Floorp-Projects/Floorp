#!/usr/bin/perl -wT

# This script is designed to be used with the Mozilla Uninstall Survey
# It will connect to the database and pull results into a .csv for each product
# ("Mozilla Firefox 1.5", etc.).  The .csv's are saved to the specified directory.
# There is no output if everything goes well (this will probably be a cron script)
#   
# @author Wil Clouser <wclouser@mozilla.com>


# Fill in the correct values below
my $output_dir = '.';

my $dsn  = 'dbi:mysql:survey:localhost:3306';
my $user = '';
my $pass = '';

my $csv_dsn  = "dbi:CSV:f_dir=$output_dir;csv_eol=\n;";

# If this is true phone numbers and email addresses will be obscured (you probably
# want this)
my $privacy = 1;


# ---- End of configuration ----
# ------------------------------

use strict;
use DBI;

# Do some initial tests to make sure things don't break further on
    if (! -d $output_dir) {
        die "Destination is not a directory: $output_dir\n";
    }

    if (! -w $output_dir) {
        die "Cannot write to directory: $output_dir\n";
    }

    my $dbh = DBI->connect($dsn, $user, $pass)
        or die "Can't connect to the db: $DBI::errstr\n";
        
    my $csvh = DBI->connect($csv_dsn)
        or die "Can't connect to the db: $DBI::errstr\n";


# Setup some variables for use in the main loop
    my $applications = get_current_applications();
    
    my $results_query = $dbh->prepare("
        SELECT
            `results`.`id`,
            `results`.`created`,
            `intentions`.`description` as `intention`,
            `results`.`intention_text` as `intention_other`,
            `results`.`comments`
        FROM `results` 
        LEFT JOIN `intentions` ON `results`.`intention_id`=`intentions`.`id` 
        INNER JOIN `applications` ON `applications`.`id` = `results`.`application_id`
        WHERE
            `applications`.`name` LIKE ?
        AND 
            `applications`.`version` LIKE ?
        ORDER BY 
            `results`.`created` ASC");

    # table name is unimportant
    my $csv = $csvh->prepare("INSERT INTO testo VALUES(?,?,?,?,?)");

# Main Loop
    foreach my $apps ( @$applications ) {
        # Clean spaces from names.  In the future if there are other strange
        # characters, we'll probably want to replace them too.
        my $application_name = $apps->{name};
            $application_name =~ tr/ /_/;
        my $application_version = $apps->{version};
            $application_version =~ tr/ /_/;

        my $filename = "export-$application_name-$application_version.csv";

        # If the file doesn't exist, this will create it.  If it does exist, it will
        # be truncated to zero length (we've already checked the directory is
        # writable)
            open CSVFILE, ">$output_dir/$filename" or
                die "ERROR: Could not open file: $output_dir/$filename!";
            truncate CSVFILE,0;
            close CSVFILE;

        $csvh->{'csv_tables'}->{'testo'} = { 'file' => $filename, 'col_names' => ['id','created','intention','intention_other','comments']};

        $results_query->execute($apps->{name},$apps->{version});

        # grab results from the db and send to the csv
        while ( my @row = $results_query->fetchrow_array ) {
            if ($privacy) {
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
