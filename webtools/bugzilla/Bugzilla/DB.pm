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
use Bugzilla::DB::Schema;
use Bugzilla::User;

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

#####################################################################
# Connection Methods
#####################################################################

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
our @_abstract_methods = qw(REQUIRED_VERSION PROGRAM_NAME
                            new sql_regexp sql_not_regexp sql_limit sql_to_days
                            sql_date_format sql_interval
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

sub sql_position {
    my ($self, $fragment, $text) = @_;

    return "POSITION($fragment IN $text)";
}

sub sql_group_by {
    my ($self, $needed_columns, $optional_columns) = @_;

    my $expression = "GROUP BY $needed_columns";
    $expression .= ", " . $optional_columns if defined($optional_columns);
    
    return $expression;
}

sub sql_string_concat {
    my ($self, @params) = @_;
    
    return '(' . join(' || ', @params) . ')';
}

sub sql_fulltext_search {
    my ($self, $column, $text) = @_;

    # This is as close as we can get to doing full text search using
    # standard ANSI SQL, without real full text search support. DB specific
    # modules shoud override this, as this will be always much slower.

    # the text is already sql-quoted, so we need to remove the quotes first
    my $quote = substr($self->quote(''), 0, 1);
    $text = $1 if ($text =~ /^$quote(.*)$quote$/);

    # make the string lowercase to do case insensitive search
    my $lower_text = lc($text);

    # split the text we search for to separate words
    my @words = split(/\s+/, $lower_text);

    # search for occurence of all specified words in the column
    return "CASE WHEN (LOWER($column) LIKE ${quote}%" .
           join("%${quote} AND LOWER($column) LIKE ${quote}%", @words) .
           "%${quote}) THEN 1 ELSE 0 END";
}

#####################################################################
# General Info Methods
#####################################################################

# XXX - Needs to be documented.
sub bz_server_version {
    my ($self) = @_;
    return $self->get_info(18); # SQL_DBMS_VER
}

sub bz_last_key {
    my ($self, $table, $column) = @_;

    return $self->last_insert_id($db_name, undef, $table, $column);
}

sub bz_get_field_defs {
    my ($self) = @_;

    my $extra = "";
    if (!UserInGroup(Param('timetrackinggroup'))) {
        $extra = "AND name NOT IN ('estimated_time', 'remaining_time', " .
                 "'work_time', 'percentage_complete', 'deadline')";
    }

    my @fields;
    my $sth = $self->prepare("SELECT name, description FROM fielddefs
                              WHERE obsolete = 0 $extra
                              ORDER BY sortkey");
    $sth->execute();
    while (my $field_ref = $sth->fetchrow_hashref()) {
        push(@fields, $field_ref);
    }
    return(@fields);
}

#####################################################################
# Database Setup
#####################################################################

sub bz_setup_database {
    my ($self) = @_;

    # Get a list of the existing tables (if any) in the database
    my $table_sth = $self->table_info(undef, undef, undef, "TABLE");
    my @current_tables =
        @{$self->selectcol_arrayref($table_sth, { Columns => [3] })};

    my @desired_tables = $self->_bz_schema->get_table_list();

    foreach my $table_name (@desired_tables) {
        next if grep /^$table_name$/, @current_tables;
        print "Creating table $table_name ...\n";

        my @table_sql = $self->_bz_schema->get_table_ddl($table_name);
        foreach my $sql_statement (@table_sql) {
            $self->do($sql_statement);
        }
    }
}

#####################################################################
# Schema Modification Methods
#####################################################################

# XXX - Need to make this cross-db compatible
# XXX - This shouldn't print stuff to stdout
sub bz_add_field ($$$) {
    my ($self, $table, $field, $definition) = @_;

    my $ref = $self->bz_get_field_def($table, $field);
    return if $ref; # already added?

    print "Adding new field $field to table $table ...\n";
    $self->do("ALTER TABLE $table
              ADD COLUMN $field $definition");
}

# XXX - Need to make this cross-db compatible
# XXX - This shouldn't print stuff to stdout
sub bz_change_field_type ($$$) {
    my ($self, $table, $field, $newtype) = @_;

    my $ref = $self->bz_get_field_def($table, $field);

    my $oldtype = $ref->[1];
    if (! $ref->[2]) {
        $oldtype .= qq{ not null};
    }
    if ($ref->[4]) {
        $oldtype .= qq{ default "$ref->[4]"};
    }

    if ($oldtype ne $newtype) {
        print "Updating field type $field in table $table ...\n";
        print "old: $oldtype\n";
        print "new: $newtype\n";
        $self->do("ALTER TABLE $table
                  CHANGE $field
                  $field $newtype");
    }
}

# XXX - Need to make this cross-db compatible
# XXX - This shouldn't print stuff to stdout
sub bz_drop_field ($$) {
    my ($self, $table, $field) = @_;

    my $ref = $self->bz_get_field_def($table, $field);
    return unless $ref; # already dropped?

    print "Deleting unused field $field from table $table ...\n";
    $self->do("ALTER TABLE $table
              DROP COLUMN $field");
}

# XXX - Needs to be made cross-db compatible
sub bz_drop_table_indexes ($) {
    my ($self, $table) = @_;
    my %seen;

    # get the list of indexes
    my $sth = $self->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    # drop each index
    while ( my $ref = $sth->fetchrow_arrayref) {

      # note that some indexes are described by multiple rows in the
      # index table, so we may have already dropped the index described
      # in the current row.
      next if exists $seen{$$ref[2]};

      if ($$ref[2] eq 'PRIMARY') {
          # The syntax for dropping a PRIMARY KEY is different
          # from the normal DROP INDEX syntax.
          $self->do("ALTER TABLE $table DROP PRIMARY KEY");
      }
      else {
          $self->do("ALTER TABLE $table DROP INDEX $$ref[2]");
      }
      $seen{$$ref[2]} = 1;

    }
}

# XXX - Needs to be made cross-db compatible
sub bz_rename_field ($$$) {
    my ($self, $table, $field, $newname) = @_;

    my $ref = $self->bz_get_field_def($table, $field);
    return unless $ref; # already renamed?

    if ($$ref[1] ne $newname) {
        print "Updating field $field in table $table ...\n";
        my $type = $$ref[1];
        $type .= " NOT NULL" if !$$ref[2];
        $type .= " auto_increment" if $$ref[5] =~ /auto_increment/;
        $self->do("ALTER TABLE $table
                  CHANGE $field
                  $newname $type");
    }
}

#####################################################################
# Schema Information Methods
#####################################################################

sub _bz_schema {
    my ($self) = @_;
    return $self->{private_bz_schema} if exists $self->{private_bz_schema};
    $self->{private_bz_schema} = 
        Bugzilla::DB::Schema->new($self->MODULE_NAME);
    return $self->{private_bz_schema};
}

# XXX - Needs to be made cross-db compatible.
sub bz_get_field_def ($$) {
    my ($self, $table, $field) = @_;
    my $sth = $self->prepare("SHOW COLUMNS FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[0] ne $field;
        return $ref;
   }
}

# XXX - Needs to be made cross-db compatible
sub bz_get_index_count ($) {
    my ($self, $table) = @_;

    my $sth = $self->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    if ( $sth->rows == -1 ) {
      die ("Unexpected response while counting indexes in $table:" .
           " \$sth->rows == -1");
    }

    return ($sth->rows);
}

# XXX - Needs to be made cross-db compatible.
sub bz_get_index_def ($$) {
    my ($self, $table, $field) = @_;
    my $sth = $self->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[4] ne $field;
        return $ref;
   }
}

# XXX - Should be updated to use _bz_real_schema when we have that,
#       if we ever need it.
sub bz_table_columns {
    my ($self, $table) = @_;
    return $self->_bz_schema->get_table_columns($table);
}

# XXX - Needs to be made cross-db compatible
sub bz_table_exists ($) {
   my ($self, $table) = @_;
   my $exists = 0;
   my $sth = $self->prepare("SHOW TABLES");
   $sth->execute;
   while (my ($dbtable) = $sth->fetchrow_array ) {
      if ($dbtable eq $table) {
         $exists = 1;
      }
   }
   return $exists;
}

#####################################################################
# Transaction Methods
#####################################################################

sub bz_start_transaction {
    my ($self) = @_;

    if ($self->{private_bz_in_transaction}) {
        ThrowCodeError("nested_transaction");
    } else {
        # Turn AutoCommit off and start a new transaction
        $self->begin_work();
        $self->{private_bz_in_transaction} = 1;
    }
}

sub bz_commit_transaction {
    my ($self) = @_;

    if (!$self->{private_bz_in_transaction}) {
        ThrowCodeError("not_in_transaction");
    } else {
        $self->commit();

        $self->{private_bz_in_transaction} = 0;
    }
}

sub bz_rollback_transaction {
    my ($self) = @_;

    if (!$self->{private_bz_in_transaction}) {
        ThrowCodeError("not_in_transaction");
    } else {
        $self->rollback();

        $self->{private_bz_in_transaction} = 0;
    }
}

#####################################################################
# Subclass Helpers
#####################################################################

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

  # Schema Changes
  $dbh->bz_add_field($table, $column, $definition);
  $dbh->bz_change_field_type($table, $column, $newtype);
  $dbh->bz_drop_field($table, $column);
  $dbh->bz_drop_table_indexes($table);
  $dbh->bz_rename_field($table, $column, $newname);

  # Schema Information
  my @fields    = $dbh->bz_get_field_defs();
  my @fieldinfo = $dbh->bz_get_field_def($table, $column);
  my @indexinfo = $dbh->bz_get_index_def($table, $field);
  my $exists    = $dbh->bz_table_exists($table);

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

=head1 CONSTANTS

Subclasses of Bugzilla::DB are required to define certain constants. These
constants are required to be subroutines or "use constant" variables.

=over 4

=item C<REQUIRED_VERSION>

This is the minimum required version of the database server that the
Bugzilla::DB subclass requires. It should be in a format suitable for
passing to vers_cmp during installation.

=item C<PROGRAM_NAME>

The human-readable name of this database. For example, for MySQL, this
would be 'MySQL.' You should not depend on this variable to know what
type of database you are running on; this is intended merely for displaying
to the admin to let them know what DB they're running.

=item C<MODULE_NAME>

The name of the Bugzilla::DB module that we are. For example, for the MySQL
Bugzilla::DB module, this would be "Mysql." For PostgreSQL it would be "Pg."

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

=head1 ABSTRACT METHODS

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

=item C<sql_position>

 Description: Outputs proper SQL syntax determinig position of a substring
              (fragment) withing a string (text). Note: if the substring or
              text are string constants, they must be properly quoted
              (e.g. "'pattern'").
 Params:      $fragment = the string fragment we are searching for (scalar)
              $text = the text to search (scalar)
 Returns:     formatted SQL for substring search (scalar)

=item C<sql_group_by>

 Description: Outputs proper SQL syntax for grouping the result of a query.
              For ANSI SQL databases, we need to group by all columns we are
              querying for (except for columns used in aggregate functions).
              Some databases require (or even allow) to specify only one
              or few columns if the result is uniquely defined. For those
              databases, the default implementation needs to be overloaded.
 Params:      $needed_columns = string with comma separated list of columns
              we need to group by to get expected result (scalar)
              $optional_columns = string with comma separated list of all
              other columns we are querying for, but which are not in the
              required list.
 Returns:     formatted SQL for row grouping (scalar)

=item C<sql_string_concat>

 Description: Returns SQL syntax for concatenating multiple strings (constants
              or values from table columns) together.
 Params:      @params = array of column names or strings to concatenate
 Returns:     formatted SQL for concatenating specified strings

=item C<sql_fulltext_search>

 Description: Returns SQL syntax for performing a full text search for
              specified text on a given column.
              There is a ANSI SQL version of this method implemented using
              LIKE operator, but it's not a real full text search. DB specific
              modules shoud override this, as this generic implementation will
              be always much slower. This generic implementation returns
              'relevance' as 0 for no match, or 1 for a match.
 Params:      $column = name of column to search (scalar)
              $text = text to search for (scalar)
 Returns:     formatted SQL for for full text search

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

=head1 IMPLEMENTED METHODS

These methods are implemented in Bugzilla::DB, and only need
to be implemented in subclasses if you need to override them for 
database-compatibility reasons.

=over 4

=head2 General Information Methods

These methods return information about data in the database.

=over 4

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

=head2 Schema Modification Methods

These methods modify the current Bugzilla schema.

=over 4

=item C<bz_add_field>

 Description: Adds a new column to a table in the database. Prints out
              a brief statement that it did so, to stdout.
 Params:      $table = the table where the column is being added
              $column = the name of the new column
              $definition = SQL for defining the new column
 Returns:     none

=item C<bz_change_field_type>

 Description: Changes the data type of a column in a table. Prints out 
              the changes being made to stdout. If the new type is the 
              same as the old type, the function returns without changing
              anything.
 Params:      $table = the table where the column is
              $column = the name of the column you want to change
              $newtype = the new data type of the column, in SQL format.
 Returns:     none

=item C<bz_drop_field>

 Description: Removes a column from a database table. If the column 
              doesn't exist, we return without doing anything. If we do
              anything, we print a short message to stdout about the change.
 Params:      $table = The table where the column is
              $column = The name of the column you want to drop
 Returns:     none

=item C<bz_drop_table_indexes>

 Description: Drops all indexes on a given table.
 Params:      $table = the table on which you wish to remove all indexes
 Returns:     none

=item C<bz_rename_field>

 Description: Renames a column in a database table. If the column 
              doesn't exist, or if the new name is the same as the 
              old name, we return without changing anything.
 Params:      $table = the table where the column is that you want to rename
              $column = the name of the column you want to rename
              $newname = the new name of the column
 Returns:     none

=head2 Schema Information Methods

These methods return info about the current Bugzilla database schema.

=over 4

=item C<bz_get_field_defs>

 Description: Returns a list of all the "bug" fields in Bugzilla. The list
              contains hashes, with a 'name' key and a 'description' key.
 Params:      none
 Returns:     List of all the "bug" fields

=item C<bz_get_field_def>

 Description: Returns information about a column in a table in the database.
 Params:      $table = name of table containing the column (scalar)
              $column = column you want info about (scalar)
 Returns:     An reference to an array containing information about the
              field, with the following information in each array place:
              0 = column name
              1 = column type
              2 = 'YES' if the column can be NULL, empty string otherwise
              3 = The type of key, either 'MUL', 'UNI', 'PRI, or ''.
              4 = The default value
              5 = An "extra" column, per MySQL docs. Don't use it.
              If the column does not exist, the function returns undef.

=item C<bz_get_index_count>

 Description: Returns the number of indexes on a certain table.
 Params:      $table = the table that you want to count indexes on
 Returns:     The number of indexes on the table.

=item C<bz_get_index_def($table, $field)>

 Description: Returns information about an index on a table in the database.
 Params:      $table = name of table containing the index (scalar)
              $field = name of a field that the index is on (scalar)
 Returns:     A reference to an array containing information about the
              index, with the following information in each array place:
              0 = name of the table that the index is on
              1 = 0 if unique, 1 if not unique
              2 = name of the index
              3 = seq_in_index (either 1 or 0)
              4 = Name of ONE column that the index is on
              5 = 'Collation' of the index. Usually 'A'.
              6 = Cardinality. Either a number or undef.
              7 = sub_part. Usually undef. Sometimes 1.
              8 = "packed". Usually undef.
              9 = comments. Usually an empty string. Sometimes 'FULLTEXT'.
              If the index does not exist, the function returns undef.

=item C<bz_table_exists>

 Description: Returns whether or not the specified table exists in the DB.
 Params:      $table = the name of the table you're checking the existence
                       of (scalar)
 Returns:     A true value if the table exists, a false value otherwise.

=head2 Transaction Methods

These methods deal with the starting and stopping of transactions 
in the database.

=over 4

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

=head1 SUBCLASS HELPERS

Methods in this class are intended to be used by subclasses to help them
with their functions.

=over 4

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
