# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::DatabaseHelper::DBI;
use strict;
use vars qw(@ISA);
use PLIF::DatabaseHelper;
@ISA = qw(PLIF::DatabaseHelper);
1;

sub databaseType {
    return qw(mysql);
}

sub tableExists {
    my $self = shift;
    my($app, $database, $table) = @_;
    return defined($database->execute('SHOW TABLES LIKE ?', $table)->row);
}

sub columnExists {
    my $self = shift;
    my($app, $database, $table, $columnName) = @_;
    $self->assert($table !~ /`/o, 1, 'Internal error: An invalid table name was passed to columnExists (it contained a \'`\' character)'); #'); # reset font-lock
    my $columns = $database->execute("SHOW COLUMNS FROM `$table`");
    while (my @columnDefinition = $columns->row) {
        if ($columnDefinition[0] eq $columnName) {
            return @columnDefinition;
        }
    }
    return undef;
}

# some routines used during setup to actually add the database
sub setupDatabaseName {
    # XXX MySQL specific -- should we create a MySQL specific DatabaseHelper?
    return 'mysql';
}

sub setupVerifyVersion {
    my $self = shift;
    my($app, $database) = @_;
    # check version and call $self->error(1, ''); if it is too low.
    # XXX currently we do not check version numbers (do nothing)
    #
    # my $version = $database->execute("SELECT VERSION()")->row;
    # $version will have the form major.minor.revision-log, where
    # major, minor and revision are decimal numbers, and '-log' is
    # present if logging in enabled.
    #
    # For MySQL, we should probably require 3.23.41... because that's
    # what I'm using to develop with.
}

sub setupCreateUser {
    my $self = shift;
    my($app, $database, $username, $password, $hostname, $name) = @_;
    # all done below in the setupSetRights() method
}

sub setupCreateDatabase {
    my $self = shift;
    my($app, $database, $name) = @_;
    $self->assert($name !~ /`/o, 1, 'Internal error: An invalid database name was passed to setupCreateDatabase (it contained a \'`\' character)'); #'); # reset font-lock
    $database->execute("CREATE DATABASE IF NOT EXISTS `$name`");
}

sub setupSetRights {
    my $self = shift;
    my($app, $database, $username, $password, $hostname, $name) = @_;
    # XXX MySQL specific -- should we create a MySQL specific DatabaseHelper?
    $self->assert($name !~ /`/o, 1, 'Internal error: An invalid database name was passed to setupSetRights (it contained a \'`\' character)'); #'); # reset font-lock
    $self->assert($username !~ /`/o, 1, 'Internal error: An invalid username was passed to setupSetRights (it contained a \'`\' character)'); #'); # reset font-lock
    $self->assert($hostname !~ /`/o, 1, 'Internal error: An invalid hostname was passed to setupSetRights (it contained a \'`\' character)'); #'); # reset font-lock
    $database->execute("GRANT ALTER, INDEX, SELECT, CREATE, INSERT, DELETE, UPDATE, DROP, REFERENCES ON `$name`.* TO `$username`\@`$hostname` IDENTIFIED BY ?", $password);
}


=over time i would expect the following to be implemented:

 *********************
 **** WARNING!!!! ****
 *********************

 The following section is only licensed as MPL and not MPL/GPL.
 It is copied from the Bugzilla 2.x codebase for our information only.

###########################################################################
# Detect changed local settings
###########################################################################

sub GetIndexDef ($$)
{
    my ($table, $field) = @_;
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[2] ne $field;
        return $ref;
   }
}

sub CountIndexes ($)
{
    my ($table) = @_;
    
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    if ( $sth->rows == -1 ) {
      die ("Unexpected response while counting indexes in $table:" .
           " \$sth->rows == -1");
    }
    
    return ($sth->rows);
}

sub DropIndexes ($)
{
    my ($table) = @_;
    my %SEEN;

    # get the list of indexes
    #
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    # drop each index
    #
    while ( my $ref = $sth->fetchrow_arrayref) {
      
      # note that some indexes are described by multiple rows in the
      # index table, so we may have already dropped the index described
      # in the current row.
      # 
      next if exists $SEEN{$$ref[2]};

      my $dropSth = $dbh->prepare("ALTER TABLE $table DROP INDEX $$ref[2]");
      $dropSth->execute;
      $dropSth->finish;
      $SEEN{$$ref[2]} = 1;

    }

}
#
# Check if the enums in the bugs table return the same values that are defined
# in the various locally changeable variables. If this is true, then alter the
# table definition.
#

sub CheckEnumField ($$@)
{
    my ($table, $field, @against) = @_;

    my $ref = GetFieldDef($table, $field);
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";
    
    $_ = "enum('" . join("','", @against) . "')";
    if ($$ref[1] ne $_) {
        print "Updating field $field in table $table ...\n";
        $_ .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $field $_");
        $::regenerateshadow = 1;
    }
}


###########################################################################
# Update the tables to the current definition
###########################################################################

#
# As time passes, fields in tables get deleted, added, changed and so on.
# So we need some helper subroutines to make this possible:
#

sub ChangeFieldType ($$$)
{
    my ($table, $field, $newtype) = @_;

    my $ref = GetFieldDef($table, $field);
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

    my $oldtype = $ref->[1];
    if ($ref->[4]) {
        $oldtype .= qq{ default "$ref->[4]"};
    }

    if ($oldtype ne $newtype) {
        print "Updating field type $field in table $table ...\n";
        print "old: $oldtype\n";
        print "new: $newtype\n";
        $newtype .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $field $newtype");
    }
}

sub RenameField ($$$)
{
    my ($table, $field, $newname) = @_;

    my $ref = GetFieldDef($table, $field);
    return unless $ref; # already fixed?
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

    if ($$ref[1] ne $newname) {
        print "Updating field $field in table $table ...\n";
        my $type = $$ref[1];
        $type .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $newname $type");
    }
}

sub AddField ($$$)
{
    my ($table, $field, $definition) = @_;

    my $ref = GetFieldDef($table, $field);
    return if $ref; # already added?

    print "Adding new field $field to table $table ...\n";
    $dbh->do("ALTER TABLE $table
              ADD COLUMN $field $definition");
}

sub DropField ($$)
{
    my ($table, $field) = @_;

    my $ref = GetFieldDef($table, $field);
    return unless $ref; # already dropped?

    print "Deleting unused field $field from table $table ...\n";
    $dbh->do("ALTER TABLE $table
              DROP COLUMN $field");
}

=cut
