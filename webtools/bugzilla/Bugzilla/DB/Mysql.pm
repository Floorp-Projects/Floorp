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
use Carp;

# This module extends the DB interface via inheritance
use base qw(Bugzilla::DB);

use constant REQUIRED_VERSION => '3.23.41';
use constant PROGRAM_NAME => 'MySQL';

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
    my ($self, $limit,$offset) = @_;

    if (defined($offset)) {
        return "LIMIT $offset, $limit";
    } else {
        return "LIMIT $limit";
    }
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

sub bz_lock_tables {
    my ($self, @tables) = @_;

    # Check first if there was no lock before
    if ($self->{private_bz_tables_locked}) {
        carp("Tables already locked");
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
        carp("No matching lock");
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

1;
