# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Tomas Kopal <Tomas.Kopal@altap.cz>

package Bugzilla::DB;

use strict;

use DBI;

# Inherit the DB class from DBI::db and Exporter
# Note that we inherit from Exporter just to allow the old, deprecated
# interface to work. If it gets removed, the Exporter class can be removed
# from this list.
use base qw(Exporter DBI::db);

%Bugzilla::DB::EXPORT_TAGS =
  (
   deprecated => [qw(SendSQL SqlQuote
                     MoreSQLData FetchSQLData FetchOneColumn
                     PushGlobalSQLState PopGlobalSQLState)
                 ],
);
Exporter::export_ok_tags('deprecated');

use Bugzilla::Config qw(:DEFAULT :db);
use Bugzilla::Util;
use Bugzilla::Error;

# All this code is backwards compat fu. As such, its a bit ugly. Note the
# circular dependencies on Bugzilla.pm
# This is old cruft which will be removed, so theres not much use in
# having a separate package for it, or otherwise trying to avoid the circular
# dependency

# XXX - mod_perl
# These use |our| instead of |my| because they need to be cleared from
# Bugzilla.pm. See bug 192531 for details.
our $_current_sth;
our @SQLStateStack = ();
sub SendSQL {
    my ($str) = @_;

    $_current_sth = Bugzilla->dbh->prepare($str);

    $_current_sth->execute;

    # This is really really ugly, but its what we get for not doing
    # error checking for 5 years. See bug 189446 and bug 192531
    $_current_sth->{RaiseError} = 0;
}

# Its much much better to use bound params instead of this
sub SqlQuote {
    my ($str) = @_;

    # Backwards compat code
    return "''" if not defined $str;

    my $res = Bugzilla->dbh->quote($str);

    trick_taint($res);

    return $res;
}

# XXX - mod_perl
my $_fetchahead;
sub MoreSQLData {
    return 1 if defined $_fetchahead;

    if ($_fetchahead = $_current_sth->fetchrow_arrayref()) {
        return 1;
    }
    return 0;
}

sub FetchSQLData {
    if (defined $_fetchahead) {
        my @result = @$_fetchahead;
        undef $_fetchahead;
        return @result;
    }

    return $_current_sth->fetchrow_array;
}

sub FetchOneColumn {
    my @row = FetchSQLData();
    return $row[0];
}

sub PushGlobalSQLState() {
    push @SQLStateStack, $_current_sth;
    push @SQLStateStack, $_fetchahead;
}

sub PopGlobalSQLState() {
    die ("PopGlobalSQLState: stack underflow") if ( scalar(@SQLStateStack) < 1 );
    $_fetchahead = pop @SQLStateStack;
    $_current_sth = pop @SQLStateStack;
}

# MODERN CODE BELOW

sub connect_shadow {
    die "Tried to connect to non-existent shadowdb" unless Param('shadowdb');

    return _connect($db_driver, Param("shadowdbhost"),
                    Param('shadowdb'), Param("shadowdbport"),
                    Param("shadowdbsock"), $db_user, $db_pass);
}

sub connect_main (;$) {
    my ($no_db_name) = @_;
    my $connect_to_db = $db_name;
    $connect_to_db = "" if $no_db_name;
    return _connect($db_driver, $db_host, $connect_to_db, $db_port,
                    $db_sock, $db_user, $db_pass);
}

sub _connect {
    my ($driver, $host, $dbname, $port, $sock, $user, $pass) = @_;

    # DB specific module have the same name as DB driver, here we
    # just make sure we are not case sensitive
    (my $db_module = $driver) =~ s/(\w+)/\u\L$1/g;
    my $pkg_module = "Bugzilla::DB::" . $db_module;

    # do the actual import
    eval ("require $pkg_module")
        || die ("'$db_module' is not a valid choice for \$db_driver in "
                . " localconfig: " . $@);

    # instantiate the correct DB specific module
    my $dbh = $pkg_module->new($user, $pass, $host, $dbname, $port, $sock);

    return $dbh;
}

sub _handle_error {
    require Carp;

    # Cut down the error string to a reasonable size
    $_[0] = substr($_[0], 0, 2000) . ' ... ' . substr($_[0], -2000)
        if length($_[0]) > 4000;
    $_[0] = Carp::longmess($_[0]);
    return 0; # Now let DBI handle raising the error
}

# List of abstract methods we are checking the derived class implements
our @_abstract_methods = qw(new sql_regexp sql_not_regexp sql_limit
                            sql_to_days sql_date_format sql_interval
                            bz_lock_tables bz_unlock_tables);

# This overriden import method will check implementation of inherited classes
# for missing implementation of abstract methods
# See http://perlmonks.thepen.com/44265.html
sub import {
    my $pkg = shift;

    # do not check this module
    if ($pkg ne __PACKAGE__) {
        # make sure all abstract methods are implemented
        foreach my $meth (@_abstract_methods) {
            $pkg->can($meth)
                or croak("Class $pkg does not define method $meth");
        }
    }

    # Now we want to call our superclass implementation.
    # If our superclass is Exporter, which is using caller() to find
    # a namespace to populate, we need to adjust for this extra call.
    # All this can go when we stop using deprecated functions.
    my $is_exporter = $pkg->isa('Exporter');
    $Exporter::ExportLevel++ if $is_exporter;
    $pkg->SUPER::import(@_);
    $Exporter::ExportLevel-- if $is_exporter;
}

# note that when multiple databases are supported, version number does not
# make sense anymore (as it is DB-dependent). This needs to be removed in
# the future and places where it's used fixed.
my $cached_server_version;
sub server_version {
    return $cached_server_version if defined($cached_server_version);
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare('SELECT VERSION()');
    $sth->execute();
    ($cached_server_version) = $sth->fetchrow_array();
    return $cached_server_version;
}

sub bz_get_field_defs {
    my ($self) = @_;

    my $extra = "";
    if (!&::UserInGroup(Param('timetrackinggroup'))) {
        $extra = "WHERE name NOT IN ('estimated time', 'remaining_time', " .
                 "'work_time', 'percentage_complete', 'deadline')";
    }

    my @fields;
    my $sth = $self->prepare("SELECT name, description
                                FROM fielddefs $extra
                            ORDER BY sortkey");
    $sth->execute();
    while (my $field_ref = $sth->fetchrow_hashref()) {
        push(@fields, $field_ref);
    }
    return(@fields);
}

sub bz_last_key {
    my ($self, $table, $column) = @_;

    return $self->last_insert_id($db_name, undef, $table, $column);
}

sub bz_start_transaction {
    my ($self) = @_;

    if ($self->{private_bz_in_transaction}) {
        carp("Can't start transaction within another transaction");
        ThrowCodeError("nested_transaction");
    } else {
        # Turn AutoCommit off and start a new transaction
        $self->begin_work();

        $self->{privateprivate_bz_in_transaction} = 1;
    }
}

sub bz_commit_transaction {
    my ($self) = @_;

    if (!$self->{private_bz_in_transaction}) {
        carp("Can't commit without a transaction");
        ThrowCodeError("not_in_transaction");
    } else {
        $self->commit();

        $self->{private_bz_in_transaction} = 0;
    }
}

sub bz_rollback_transaction {
    my ($self) = @_;

    if (!$self->{private_bz_in_transaction}) {
        carp("Can't rollback without a transaction");
        ThrowCodeError("not_in_transaction");
    } else {
        $self->rollback();

        $self->{private_bz_in_transaction} = 0;
    }
}

sub db_new {
    my ($class, $dsn, $user, $pass, $attributes) = @_;

    # set up default attributes used to connect to the database
    # (if not defined by DB specific implementation)
    $attributes = { RaiseError => 0,
                    AutoCommit => 1,
                    PrintError => 0,
                    ShowErrorStatement => 1,
                    HandleError => \&_handle_error,
                    TaintIn => 1,
                    FetchHashKeyName => 'NAME',  
                    # Note: NAME_lc causes crash on ActiveState Perl
                    # 5.8.4 (see Bug 253696)
                    # XXX - This will likely cause problems in DB
                    # back ends that twiddle column case (Oracle?)
                  } if (!defined($attributes));

    # connect using our known info to the specified db
    # Apache::DBI will cache this when using mod_perl
    my $self = DBI->connect($dsn, $user, $pass, $attributes)
        or die "\nCan't connect to the database.\nError: $DBI::errstr\n"
        . "  Is your database installed and up and running?\n  Do you have"
        . "the correct username and password selected in localconfig?\n\n";

    # RaiseError was only set to 0 so that we could catch the 
    # above "die" condition.
    $self->{RaiseError} = 1;

    # class variables
    $self->{private_bz_in_transaction} = 0;

    bless ($self, $class);

    return $self;
}

1;

__END__

=head1 NAME

Bugzilla::DB - Database access routines, using L<DBI>

=head1 SYNOPSIS

  # Obtain db handle
  use Bugzilla::DB;
  my $dbh = Bugzilla->dbh;

  # prepare a query using DB methods
  my $sth = $dbh->prepare("SELECT " .
                          $dbh->sql_date_format("creation_ts", "%Y%m%d") .
                          " FROM bugs WHERE bug_status != 'RESOLVED' " .
                          $dbh->sql_limit(1));

  # Execute the query
  $sth->execute;
  
  # Get the results
  my @result = $sth->fetchrow_array;

  # Schema Information
  my @fields = $dbh->bz_get_field_defs();

=head1 DESCRIPTION

Functions in this module allows creation of a database handle to connect
to the Bugzilla database. This should never be done directly; all users
should use the L<Bugzilla> module to access the current C<dbh> instead.

This module also contains methods extending the returned handle with
functionality which is different between databases allowing for easy
customization for particular database via inheritance. These methods
should be always preffered over hard-coding SQL commands.

Access to the old SendSQL-based database routines are also provided by
importing the C<:deprecated> tag. These routines should not be used in new
code.

=head1 CONNECTION

A new database handle to the required database can be created using this
module. This is normally done by the L<Bugzilla> module, and so these routines
should not be called from anywhere else.

=head2 Functions

=over 4

=item C<connect_main>

 Description: Function to connect to the main database, returning a new
              database handle.
 Params:      $no_db_name (optional) - If true, Connect to the database 
                  server, but don't connect to a specific database. This 
                  is only used when creating a database. After you create
                  the database, you should re-create a new Bugzilla::DB object
                  without using this parameter. 
 Returns:     new instance of the DB class

=item C<connect_shadow>

 Description: Function to connect to the shadow database, returning a new
              database handle.
              This routine C<die>s if no shadow database is configured.
 Params:      none
 Returns:     new instance of the DB class

=item C<_connect>

 Description: Internal function, creates and returns a new, connected
              instance of the correct DB class.
              This routine C<die>s if no driver is specified.
 Params:      $driver = name of the database driver to use
              $host = host running the database we are connecting to
              $dbname = name of the database to connect to
              $port = port the database is listening on
              $sock = socket the database is listening on
              $user = username used to log in to the database
              $pass = password used to log in to the database
 Returns:     new instance of the DB class

=item C<_handle_error>

 Description: Function passed to the DBI::connect call for error handling.
              It shortens the error for printing.

=item C<import>

 Description: Overrides the standard import method to check that derived class
              implements all required abstract methods. Also calls original
              implementation in its super class.

=back

=head2 Methods

Note: Methods which can be implemented generically for all DBs are implemented in
this module. If needed, they can be overriden with DB specific code.
Methods which do not have standard implementation are abstract and must
be implemented for all supported databases separately.
To avoid confusion with standard DBI methods, all methods returning string with
formatted SQL command have prefix C<sql_>. All other methods have prefix C<bz_>.

=over 4

=item C<new>

 Description: Constructor
              Abstract method, should be overriden by database specific code.
 Params:      $user = username used to log in to the database
              $pass = password used to log in to the database
              $host = host running the database we are connecting to
              $dbname = name of the database to connect to
              $port = port the database is listening on
              $sock = socket the database is listening on
 Returns:     new instance of the DB class
 Note:        The constructor should create a DSN from the parameters provided and
              then call C<db_new()> method of its super class to create a new
              class instance. See C<db_new> description in this module. As per
              DBI documentation, all class variables must be prefixed with
              "private_". See L<DBI>.

=item C<sql_regexp>

 Description: Outputs SQL regular expression operator for POSIX regex
              searches in format suitable for a given database.
              Abstract method, should be overriden by database specific code.
 Params:      none
 Returns:     formatted SQL for regular expression search (e.g. REGEXP)
              (scalar)

=item C<sql_not_regexp>

 Description: Outputs SQL regular expression operator for negative POSIX
              regex searches in format suitable for a given database.
              Abstract method, should be overriden by database specific code.
 Params:      none
 Returns:     formatted SQL for negative regular expression search
              (e.g. NOT REGEXP) (scalar)

=item C<sql_limit>

 Description: Returns SQL syntax for limiting results to some number of rows
              with optional offset if not starting from the begining.
              Abstract method, should be overriden by database specific code.
 Params:      $limit = number of rows to return from query (scalar)
              $offset = number of rows to skip prior counting (scalar)
 Returns:     formatted SQL for limiting number of rows returned from query
              with optional offset (e.g. LIMIT 1, 1) (scalar)

=item C<sql_to_days>

 Description: Outputs SQL syntax for converting date to Julian days.
              Abstract method, should be overriden by database specific code.
 Params:      $date = date to convert to days
 Returns:     formatted SQL for returning date fields in Julian days. (scalar)

=item C<sql_date_format>

 Description: Outputs SQL syntax for formatting dates.
              Abstract method, should be overriden by database specific code.
 Params:      $date = date or name of date type column (scalar)
              $format = format string for date output (scalar)
              (%Y = year, four digits, %y = year, two digits, %m = month,
               %d = day, %a = weekday name, 3 letters, %H = hour 00-23,
               %i = minute, %s = second)
 Returns:     formatted SQL for date formatting (scalar)

=item C<sql_interval>

 Description: Outputs proper SQL syntax for a time interval function.
              Abstract method, should be overriden by database specific code.
 Params:      $interval = the time interval requested (e.g. '30 minutes')
              (scalar)
 Returns:     formatted SQL for interval function (scalar)

=item C<bz_lock_tables>

 Description: Performs a table lock operation on specified tables.
              If the underlying database supports transactions, it should also
              implicitly start a new transaction.
              Abstract method, should be overriden by database specific code.
 Params:      @tables = list of names of tables to lock in MySQL
              notation (ex. 'bugs AS bugs2 READ', 'logincookies WRITE')
 Returns:     none

=item C<bz_unlock_tables>

 Description: Performs a table unlock operation
              If the underlying database supports transactions, it should also
              implicitly commit or rollback the transaction.
              Also, this function should allow to be called with the abort flag
              set even without locking tables first without raising an error
              to simplify error handling.
              Abstract method, should be overriden by database specific code.
 Params:      $abort = UNLOCK_ABORT (true, 1) if the operation on locked tables
              failed (if transactions are supported, the action will be rolled
              back). False (0) or no param if the operation succeeded.
 Returns:     none

=item C<bz_last_key>

 Description: Returns the last serial number, usually from a previous INSERT.
              Must be executed directly following the relevant INSERT.
              This base implementation uses DBI->last_insert_id. If the
              DBD supports it, it is the preffered way to obtain the last
              serial index. If it is not supported, the DB specific code
              needs to override it with DB specific code.
 Params:      $table = name of table containing serial column (scalar)
              $column = name of column containing serial data type (scalar)
 Returns:     Last inserted ID (scalar)

=item C<bz_get_field_defs>

 Description: Returns a list of all the "bug" fields in Bugzilla. The list
              contains hashes, with a 'name' key and a 'description' key.
 Params:      none
 Returns:     List of all the "bug" fields

=item C<bz_start_transaction>

 Description: Starts a transaction if supported by the database being used
 Params:      none
 Returns:     none

=item C<bz_commit_transaction>

 Description: Ends a transaction, commiting all changes, if supported by
              the database being used
 Params:      none
 Returns:     none

=item C<bz_rollback_transaction>

 Description: Ends a transaction, rolling back all changes, if supported by
              the database being used
 Params:      none
 Returns:     none

=item C<db_new>

 Description: Constructor
 Params:      $dsn = database connection string
              $user = username used to log in to the database
              $pass = password used to log in to the database
              $attributes = set of attributes for DB connection (optional)
 Returns:     new instance of the DB class
 Note:        the name of this constructor is not new, as that would make
              our check for implementation of new() by derived class useles.

=back

=head1 DEPRECATED ROUTINES

Several database routines are deprecated. They should not be used in new code,
and so are not documented.

=over 4

=item *

SendSQL

=item *

SqlQuote

=item *

MoreSQLData

=item *

FetchSQLData

=item *

FetchOneColumn

=item *

PushGlobalSQLState

=item *

PopGlobalSQLState

=back

=head1 SEE ALSO

L<DBI>

=cut
