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
# Contributor(s): Dave Miller <davem00@aol.com>
#                 Gayathri Swaminath <gayathrik00@aol.com>
#                 Jeroen Ruigrok van der Werven <asmodai@wxs.nl>
#                 Dave Lawrence <dkl@redhat.com>
#                 Tomas Kopal <Tomas.Kopal@altap.cz>

=head1 NAME

Bugzilla::DB::Mysql - Bugzilla database compatibility layer for MySQL

=head1 DESCRIPTION

This module overrides methods of the Bugzilla::DB module with MySQL specific
implementation. It is instantiated by the Bugzilla::DB module and should never
be used directly.

For interface details see L<Bugzilla::DB> and L<DBI>.

=cut

package Bugzilla::DB::Mysql;

use strict;

use Bugzilla::Error;

# This module extends the DB interface via inheritance
use base qw(Bugzilla::DB);

use constant REQUIRED_VERSION => '3.23.41';
use constant PROGRAM_NAME => 'MySQL';
use constant MODULE_NAME  => 'Mysql';

sub new {
    my ($class, $user, $pass, $host, $dbname, $port, $sock) = @_;

    # construct the DSN from the parameters we got
    my $dsn = "DBI:mysql:host=$host;database=$dbname";
    $dsn .= ";port=$port" if $port;
    $dsn .= ";mysql_socket=$sock" if $sock;
    
    my $self = $class->db_new($dsn, $user, $pass);

    # all class local variables stored in DBI derived class needs to have
    # a prefix 'private_'. See DBI documentation.
    $self->{private_bz_tables_locked} = 0;

    bless ($self, $class);
    
    return $self;
}

# when last_insert_id() is supported on MySQL by lowest DBI/DBD version
# required by Bugzilla, this implementation can be removed.
sub bz_last_key {
    my ($self) = @_;

    my ($last_insert_id) = $self->selectrow_array('SELECT LAST_INSERT_ID()');

    return $last_insert_id;
}

sub sql_regexp {
    return "REGEXP";
}

sub sql_not_regexp {
    return "NOT REGEXP";
}

sub sql_limit {
    my ($self, $limit, $offset) = @_;

    if (defined($offset)) {
        return "LIMIT $offset, $limit";
    } else {
        return "LIMIT $limit";
    }
}

sub sql_string_concat {
    my ($self, @params) = @_;
    
    return 'CONCAT(' . join(', ', @params) . ')';
}

sub sql_fulltext_search {
    my ($self, $column, $text) = @_;

    return "MATCH($column) AGAINST($text)";
}

sub sql_to_days {
    my ($self, $date) = @_;

    return "TO_DAYS($date)";
}

sub sql_date_format {
    my ($self, $date, $format) = @_;

    $format = "%Y.%m.%d %H:%i:%s" if !$format;
    
    return "DATE_FORMAT($date, " . $self->quote($format) . ")";
}

sub sql_interval {
    my ($self, $interval) = @_;
    
    return "INTERVAL $interval";
}

sub sql_position {
    my ($self, $fragment, $text) = @_;

    # mysql 4.0.1 and lower do not support CAST
    # mysql 3.*.* had a case-sensitive INSTR
    # (checksetup has a check for unsupported versions)
    my $server_version = $self->bz_server_version;
    if ($server_version =~ /^3\./) {
        return "INSTR($text, $fragment)";
    } else {
        return "INSTR(CAST($text AS BINARY), CAST($fragment AS BINARY))";
    }
}

sub sql_group_by {
    my ($self, $needed_columns, $optional_columns) = @_;

    # MySQL allows to specify all columns as ANSI SQL requires, but also
    # allow you to specify just minimal subset to get unique result.
    # According to MySQL documentation, the less columns you specify
    # the faster the query runs.
    return "GROUP BY $needed_columns";
}


sub bz_lock_tables {
    my ($self, @tables) = @_;

    # Check first if there was no lock before
    if ($self->{private_bz_tables_locked}) {
        ThrowCodeError("already_locked");
    } else {
        $self->do('LOCK TABLE ' . join(', ', @tables)); 
    
        $self->{private_bz_tables_locked} = 1;
    }
}

sub bz_unlock_tables {
    my ($self, $abort) = @_;
    
    # Check first if there was previous matching lock
    if (!$self->{private_bz_tables_locked}) {
        # Abort is allowed even without previous lock for error handling
        return if $abort;
        ThrowCodeError("no_matching_lock");
    } else {
        $self->do("UNLOCK TABLES");
    
        $self->{private_bz_tables_locked} = 0;
    }
}

# As Bugzilla currently runs on MyISAM storage, which does not supprt
# transactions, these functions die when called.
# Maybe we should just ignore these calls for now, but as we are not
# using transactions in MySQL yet, this just hints the developers.
sub bz_start_transaction {
    die("Attempt to start transaction on DB without transaction support");
}

sub bz_commit_transaction {
    die("Attempt to commit transaction on DB without transaction support");
}

sub bz_rollback_transaction {
    die("Attempt to rollback transaction on DB without transaction support");
}

#####################################################################
# Database Setup
#####################################################################

sub bz_setup_database {
    my ($self) = @_;

    # Figure out if any existing tables are of type ISAM and convert them
    # to type MyISAM if so.  ISAM tables are deprecated in MySQL 3.23,
    # which Bugzilla now requires, and they don't support more than 16
    # indexes per table, which Bugzilla needs.
    my $sth = $self->prepare("SHOW TABLE STATUS");
    $sth->execute;
    my @isam_tables = ();
    while (my ($name, $type) = $sth->fetchrow_array) {
        push(@isam_tables, $name) if $type eq "ISAM";
    }

    if(scalar(@isam_tables)) {
        print "One or more of the tables in your existing MySQL database are\n"
              . "of type ISAM. ISAM tables are deprecated in MySQL 3.23 and\n"
              . "don't support more than 16 indexes per table, which \n"
              . "Bugzilla needs.\n  Converting your ISAM tables to type"
              . " MyISAM:\n\n";
        foreach my $table (@isam_tables) {
            print "Converting table $table... ";
            $self->do("ALTER TABLE $table TYPE = MYISAM");
            print "done.\n";
        }
        print "\nISAM->MyISAM table conversion done.\n\n";
    }

    # Versions of Bugzilla before the existence of Bugzilla::DB::Schema did 
    # not provide explicit names for the table indexes. This means
    # that our upgrades will not be reliable, because we look for the name
    # of the index, not what fields it is on, when doing upgrades.
    # (using the name is much better for cross-database compatibility 
    # and general reliability). It's also very important that our
    # Schema object be consistent with what is on the disk.
    #
    # While we're at it, we also fix some inconsistent index naming
    # from the original checkin of Bugzilla::DB::Schema.

    # We check for the existence of a particular "short name" index that
    # has existed at least since Bugzilla 2.8, and probably earlier.
    # For fixing the inconsistent naming of Schema indexes,
    # we also check for one of those inconsistently-named indexes.
    my @tables = $self->bz_table_list_real();
    if ( scalar(@tables) && 
         ($self->bz_index_info_real('bugs', 'assigned_to') ||
          $self->bz_index_info_real('flags', 'flags_bidattid_idx')) )
    {
        my $bug_count = $self->selectrow_array("SELECT COUNT(*) FROM bugs");
        # We estimate one minute for each 3000 bugs, plus 3 minutes just
        # to handle basic MySQL stuff.
        my $rename_time = int($bug_count / 3000) + 3;
        # If we're going to take longer than 5 minutes, we let the user know
        # and allow them to abort.
        if ($rename_time > 5) {
            print "\nWe are about to rename old indexes.\n"
                  . "The estimated time to complete renaming is "
                  . "$rename_time minutes.\n"
                  . "You cannot interrupt this action once it has begun.\n"
                  . "If you would like to cancel, press Ctrl-C now..."
                  . " (Waiting 45 seconds...)\n\n";
            # Wait 45 seconds for them to respond.
            sleep(45);
        }
        print "Renaming indexes...\n";

        # We can't be interrupted, because of how the "if"
        # works above.
        local $SIG{INT}  = 'IGNORE';
        local $SIG{TERM} = 'IGNORE';
        local $SIG{PIPE} = 'IGNORE';

        # Certain indexes had names in Schema that did not easily conform
        # to a standard. We store those names here, so that they
        # can be properly renamed.
        my $bad_names = {
            bugs_activity => ('bugs_activity_bugid_idx',
                'bugs_activity_bugwhen_idx'),
            longdescs => ('longdescs_bugid_idx',
               'longdescs_bugwhen_idx'),
            flags => ('flags_bidattid_idx'),
            flaginclusions => ('flaginclusions_tpcid_idx'),
            flagexclusions => ('flagexclusions_tpc_id_idx'),
            profiles_activity => ('profiles_activity_when_idx'),
            group_control_map => ('group_control_map_gid_idx')
            # series_categories is dealt with below, not here.
        };

        # The series table is broken and needs to have one index
        # dropped before we begin the renaming, because it had a
        # useless index on it that would cause a naming conflict here.
        if (grep($_ eq 'series', @tables)) {
            my $dropname;
            # This is what the bad index was called before Schema.
            if ($self->bz_index_info_real('series', 'creator_2')) {
                $dropname = 'creator_2';
            }
            # This is what the bad index is called in Schema.
            elsif ($self->bz_index_info_real('series', 'series_creator_idx')) {
                    $dropname = 'series_creator_idx';
            }

            if ($dropname) {
                print "Removing the useless index $dropname on the"
                      . " series table...\n";
                my @drop = $self->_bz_schema->get_drop_index_ddl(
                    'series', $dropname);
                foreach my $sql (@drop) {
                    $self->do($sql);
                }
            }
        }

        # The email_setting table also had the same problem.
        if( grep($_ eq 'email_setting', @tables) 
            && $self->bz_index_info_real('email_setting', 
                                         'email_settings_user_id_idx') ) 
        {
            print "Removing the useless index email_settings_user_id_idx\n"
                  . "  on the email_setting table...\n";
            my @drop = $self->_bz_schema->get_drop_index_ddl('email_setting',
                'email_settings_user_id_idx');
            $self->do($_) foreach (@drop);
        }

        # Go through all the tables.
        foreach my $table (@tables) {
            # And go through all the columns on each table.
            my @columns = $self->bz_table_columns_real($table);

            # We also want to fix the silly naming of unique indexes
            # that happened when we first checked-in Bugzilla::DB::Schema.
            if ($table eq 'series_categories') {
                # The series_categories index had a nonstandard name.
                push(@columns, 'series_cats_unique_idx');
            } 
            elsif ($table eq 'email_setting') { 
                # The email_setting table had a similar problem.
                push(@columns, 'email_settings_unique_idx');
            }
            else {
                push(@columns, "${table}_unique_idx");
            }
            # And this is how we fix the other inconsistent Schema naming.
            push(@columns, $bad_names->{$table})
                if (exists $bad_names->{$table});
            foreach my $column (@columns) {
                # If we have an index named after this column, it's an 
                # old-style-name index.
                # This will miss PRIMARY KEY indexes, but that's OK 
                # because we aren't renaming them.
                if (my $index = $self->bz_index_info_real($table, $column)) {
                    # Fix the name to fit in with the new naming scheme.
                    my $new_name = $table . "_" .
                                   $index->{FIELDS}->[0] . "_idx";
                    print "Renaming index $column to to $new_name...\n";
                    # Unfortunately, MySQL has no way to rename an index. :-(
                    # So we have to drop and recreate the indexes.
                    my @drop = $self->_bz_schema->get_drop_index_ddl(
                        $table, $column);
                    my @add = $self->_bz_schema->get_add_index_ddl(
                        $table, $new_name, $index);
                    $self->do($_) foreach (@drop);
                    $self->do($_) foreach (@add);
                } # if
            } # foreach column
        } # foreach table
    } # if old-name indexes

    $self->SUPER::bz_setup_database();
}


#####################################################################
# MySQL-specific Database-Reading Methods
#####################################################################

=begin private

=head 1 MYSQL-SPECIFIC DATABASE-READING METHODS

These methods read information about the database from the disk,
instead of from a Schema object. They are only reliable for MySQL 
(see bug 285111 for the reasons why not all DBs use/have functions
like this), but that's OK because we only need them for 
backwards-compatibility anyway, for versions of Bugzilla before 2.20.

=over 4

=item C<bz_index_info_real($table, $index)>

 Description: Returns information about an index on a table in the database.
 Params:      $table = name of table containing the index
              $index = name of an index
 Returns:     An abstract index definition, always in hashref format.
              If the index does not exist, the function returns undef.
=cut
sub bz_index_info_real {
    my ($self, $table, $index) = @_;

    my $sth = $self->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    my @fields;
    my $index_type;
    # $raw_def will be an arrayref containing the following information:
    # 0 = name of the table that the index is on
    # 1 = 0 if unique, 1 if not unique
    # 2 = name of the index
    # 3 = seq_in_index (The order of the current field in the index).
    # 4 = Name of ONE column that the index is on
    # 5 = 'Collation' of the index. Usually 'A'.
    # 6 = Cardinality. Either a number or undef.
    # 7 = sub_part. Usually undef. Sometimes 1.
    # 8 = "packed". Usually undef.
    # MySQL 3
    # -------
    # 9 = comments. Usually an empty string. Sometimes 'FULLTEXT'.
    # MySQL 4
    # -------
    # 9 = Null. Sometimes undef, sometimes 'YES'.
    # 10 = Index_type. The type of the index. Usually either 'BTREE' or 'FULLTEXT'
    # 11 = 'Comment.' Usually undef.
    my $is_mysql3 = ($self->bz_server_version() =~ /^3/);
    my $index_type_loc = $is_mysql3 ? 9 : 10;
    while (my $raw_def = $sth->fetchrow_arrayref) {
        if ($raw_def->[2] eq $index) {
            push(@fields, $raw_def->[4]);
            # No index can be both UNIQUE and FULLTEXT, that's why
            # this is written this way.
            $index_type = $raw_def->[1] ? '' : 'UNIQUE';
            $index_type = $raw_def->[$index_type_loc] eq 'FULLTEXT'
                ? 'FULLTEXT' : $index_type;
        }
    }

    my $retval;
    if (scalar(@fields)) {
        $retval = {FIELDS => \@fields, TYPE => $index_type};
    }
    return $retval;
}

1;

__END__

=back

=end private

