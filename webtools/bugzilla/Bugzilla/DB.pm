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

package Bugzilla::DB;

use strict;

use DBI;

use base qw(Exporter);

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

# All this code is backwards compat fu. As such, its a bit ugly. Note the
# circular dependancies on Bugzilla.pm
# This is old cruft which will be removed, so theres not much use in
# having a separate package for it, or otherwise trying to avoid the circular
# dependancy

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

    my $dsn = "DBI:mysql:host=" . Param("shadowdbhost") .
      ";database=" . Param('shadowdb') . ";port=" . Param("shadowdbport");

    $dsn .= ";mysql_socket=" . Param("shadowdbsock") if Param('shadowdbsock');

    return _connect($dsn);
}

sub connect_main {
    my $dsn = "DBI:mysql:host=$db_host;database=$db_name;port=$db_port";

    $dsn .= ";mysql_socket=$db_sock" if $db_sock;

    return _connect($dsn);
}

sub _connect {
    my ($dsn) = @_;

    # connect using our known info to the specified db
    # Apache::DBI will cache this when using mod_perl
    my $dbh = DBI->connect($dsn,
                           $db_user,
                           $db_pass,
                           { RaiseError => 1,
                             PrintError => 0,
                             ShowErrorStatement => 1,
                             HandleError => \&_handle_error,
                             FetchHashKeyName => 'NAME_lc',
                             TaintIn => 1,
                           });

    return $dbh;
}

sub _handle_error {
    require Carp;

    $_[0] = Carp::longmess($_[0]);
    return 0; # Now let DBI handle raising the error
}

my $cached_server_version;
sub server_version {
    return $cached_server_version if defined($cached_server_version);
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare('SELECT VERSION()');
    $sth->execute();
    ($cached_server_version) = $sth->fetchrow_array();
    return $cached_server_version;
}

1;

__END__

=head1 NAME

Bugzilla::DB - Database access routines, using L<DBI>

=head1 SYNOPSIS

  my $dbh = Bugzilla::DB->connect_main;
  my $shadow = Bugzilla::DB->connect_shadow;

  SendSQL("SELECT COUNT(*) FROM bugs");
  my $cnt = FetchOneColumn();

=head1 DESCRIPTION

This allows creation of a database handle to connect to the Bugzilla database.
This should never be done directly; all users should use the L<Bugzilla> module
to access the current C<dbh> instead.

Access to the old SendSQL-based database routines are also provided by
importing the C<:deprecated> tag. These routines should not be used in new
code.

=head1 CONNECTION

A new database handle to the required database can be created using this
module. This is normally done by the L<Bugzilla> module, and so these routines
should not be called from anywhere else.

=over 4

=item C<connect_main>

Connects to the main database, returning a new dbh.

=item C<connect_shadow>

Connects to the shadow database, returning a new dbh. This routine C<die>s if
no shadow database is configured.

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
