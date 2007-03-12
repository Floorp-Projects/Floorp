# -*- Mode: perl; indent-tabs-mode: nil -*-

#
# These generic DBI routines should live somewhere common
#

use strict;
use DBI;
use POSIX qw(strftime mktime);
use Time::Local;

# Assume that treedata.pl & tbglobals.pl are already loaded
my $db = undef;

sub ConnectToDatabase($) {
    my ($tree) = @_;

    if (!defined $db) {
        my ($dsn);
        my $viewvc_dbdriver = $::global_treedata->{$tree}->{viewvc_dbdriver}; 
        my $viewvc_dbname = $::global_treedata->{$tree}->{viewvc_dbname}; 
        my $viewvc_dbhost = $::global_treedata->{$tree}->{viewvc_dbhost}; 
        my $viewvc_dbport = $::global_treedata->{$tree}->{viewvc_dbport}; 
        my $viewvc_dbuser = $::global_treedata->{$tree}->{viewvc_dbuser}; 
        my $viewvc_dbpasswd = $::global_treedata->{$tree}->{viewvc_dbpasswd}; 

        $dsn = "DBI:${viewvc_dbdriver}:database=${viewvc_dbname};";
        $dsn .= "host=${viewvc_dbhost};"
            if (defined($viewvc_dbhost) && "$viewvc_dbhost" ne "");
        $dsn .= "port=${viewvc_dbport};" 
            if (defined($viewvc_dbport) && "$viewvc_dbport" ne "");

#        DBI->trace(1, "/tmp/dbi.out");

        $db = DBI->connect($dsn, $viewvc_dbuser, $viewvc_dbpasswd)
            || die "Can't connect to database server.";
    }
}

sub DisconnectFromDatabase {
    if (defined $db) {
        $db->disconnect();
        undef $db;
    }
}

sub SendSQL {
    my ($query_ref, $str, @bind_values) = (@_);
    my $status = 0;

    $$query_ref = $db->prepare($str) || $status++;
    if ($status) {
        print STDERR "SendSQL prepare error: '$str' with values (";
        foreach my $v (@bind_values) {
            print STDERR "'" . &shell_escape($v) . "', ";
        }
        print STDERR ") :: " . $db->errstr . "\n";
        die "Cannot prepare SQL query. Please contact system administrator.\n";
    }

    $$query_ref->execute(@bind_values) || $status++;
    if ($status) {
        print STDERR "SendSQL execute error: '$str' with values (";
        foreach my $v (@bind_values) {
            print STDERR "'" . &shell_escape($v) . "', ";
        }
        print STDERR ") :: " . $$query_ref->errstr . "\n";
        die "Cannot execute SQL query. Please contact system administrator.\n";
    }
}

sub FetchSQLData($) {
    my ($query_ref) = (@_);
    return $$query_ref->fetchrow_array();
}


sub FetchOneColumn($) {
    my ($query_ref) = @_;
    my @row = &FetchSQLData($query_ref);
    return $row[0];
}

sub formatSqlTime {
    my ($date) = @_;
    my $time = strftime("%Y/%m/%d %T", gmtime($date));
    return $time;
}

# Use this function to workaround the fact that perl
# assumes that seconds since epoch are always in localtime
sub GMTtoLocaltime() {
    my ($time) = @_;
    my @timeData = localtime(time);
    my $diff = mktime(gmtime($time)) - mktime(localtime($time));
    # Account for DST
    $diff -= 3600 if ($timeData[8]);
    return $time - $diff;
}

#
# ViewVC uses the same table layout as bonsai. Yippie!
#

sub query_checkins($) {
    my ($tree, %mod_map) = @_;
    my @bind_values;
    my $currentquery;

    &ConnectToDatabase($tree);

    my $qstring = "SELECT type, UNIX_TIMESTAMP(ci_when), people.who, " .
        "repositories.repository, dirs.dir, files.file, revision, " .
        "stickytag, branches.branch, addedlines, removedlines, " .
        "descs.description FROM checkins,people,repositories,dirs,files," .
        "branches,descs WHERE people.id=whoid AND " .
        "repositories.id=repositoryid AND dirs.id=dirid AND " .
        "files.id=fileid AND branches.id=branchid AND descs.id=descid";

    if (defined($::query_module) && $::query_module ne 'allrepositories') {
        $qstring .= " AND repositories.repository = ?";
        push(@bind_values, $::query_module);
    }

    if (defined($::query_date_min) && $::query_date_min) {
        $qstring .= " AND ci_when >= ?";
        push(@bind_values, &formatSqlTime($::query_date_min));
    }
    if (defined($::query_date_max) && $::query_date_max) {
        $qstring .= " AND ci_when <= ?";
        push(@bind_values, &formatSqlTime($::query_date_max));
    }

#    print "Query: $qstring\n";
#    print "values: @bind_values\n";
    &SendSQL(\$currentquery, $qstring, @bind_values);

    my $lastlog = 0;
    my (@row, $ci, $rev, $result);
    while (@row = &FetchSQLData(\$currentquery)) {
#print "<pre>";
        $ci = [];
        for (my $i=0 ; $i<=$::CI_LOG ; $i++) {
            if ($i == $::CI_DATE) {
                $ci->[$i] = &GMTtoLocaltime($row[$i]);
            } else {
                $ci->[$i] = $row[$i];
            }
        }
#print "</pre>\n";


        my $key = "$ci->[$::CI_DIR]/$ci->[$::CI_FILE]";
#        if (IsHidden("$ci->[$::CI_REPOSITORY]/$key")) {
#            next;
#        }

        next if ($key =~ m@^CVSROOT/@);

        if (defined($::query_logexpr) && 
            $::query_logexpr ne '' &&
            !($ci->[$::CI_LOG] =~ /$::query_logexpr/i) ){
            next;
        }

        push( @$result, $ci );
    }

    &DisconnectFromDatabase();

    return $result;
}

1;
