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

# Fire up our db connection
my $dbh = DBI->connect($main::dsn, $main::user, $main::pass)
    or die "Can't connect to the db: $DBI::errstr\n";
    
# At the time of writing, this query takes about 30 seconds to run.  (just fyi)
my $results_query = $dbh->prepare("
    SELECT 
        choices_results.choice_id, 
        count(results.id) AS total 
    FROM results 
    JOIN choices_results ON results.id = choices_results.result_id 
    WHERE results.application_id = ? 
    AND choices_results.choice_id in (
        SELECT choices.id 
        FROM choices 
        JOIN choices_collections ON choices_collections.choice_id = choices.id 
        JOIN collections ON collections.id = choices_collections.collection_id 
        JOIN applications_collections ON applications_collections.collection_id = collections.id 
        JOIN applications ON applications.id = applications_collections.application_id 
        AND applications.id = ?
        AND collections.id = ? 
        AND choices.type = ?
    ) 
    GROUP BY choices_results.choice_id;
");

my $insert_cache_query = $dbh->prepare("
    REPLACE INTO cache_choices_results 
    ( choice_id, results_total, application_id, collection_id, modified ) 
    VALUES
    (?, ?, ?, ?, NOW())
");

my $applications = get_current_applications();

# Main Loop
    foreach my $application ( @$applications ) {
        my $collections  = get_collections_for_application($application->{id});

        foreach my $collection ( @$collections ) {

            if ($collection->{description} =~ /Issue/) {
                $results_query->execute($application->{id},$application->{id}, $collection->{id}, 'issue');
                next;
            } elsif ($collection->{description} =~ /Intention/) {
                $results_query->execute($application->{id},$application->{id}, $collection->{id}, 'intention');
                next;
            } else {
                # Nothing good can come from this...
            }

        } continue {

            # Add rows to our cache table
            while ( my @row = $results_query->fetchrow_array ) {
                $insert_cache_query->execute($row[0], $row[1], $application->{id}, $collection->{id});
            }

        }
    }

# Finish Up
    $results_query->finish();
    $dbh->disconnect();

# We're done.


# ---- Sub-routines ----
# ----------------------

sub get_current_applications {
    # Pulling only visible rows is purely a speed consideration - feel free to remove it
    return $dbh->selectall_arrayref("SELECT * FROM applications WHERE visible=1 ORDER BY id", { Slice => {} });
}

sub get_collections_for_application {
    my $application = shift;
    # Pulling only visible rows is purely a speed consideration - feel free to remove it
    my $query = "
        SELECT collections.id, collections.description 
        FROM collections,applications_collections 
        WHERE collections.id=applications_collections.collection_id
        AND applications_collections.application_id=$application
    ";

    return $dbh->selectall_arrayref($query, { Slice => {} });
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
