 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2006
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>

# Utility functions (based on those from Bugzilla's checksetup.pl to 
# create new fields and indicies to the schema when upgrading an installation.

package Litmus::DBTools;

use strict;

#########################################################################
sub new {
	my ($class, $dbh) = @_;
	my $self = {};
	$self->{'dbh'} = $dbh;
	bless($self);
	return $self;
}

#########################################################################
sub ChangeFieldType {
    my ($self, $table, $field, $newtype) = @_;

    my $ref = $self->GetFieldDef($table, $field);
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

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
#        'not null' should be passed as part of the call to ChangeFieldType()
#        $newtype .= " NOT NULL" if $$ref[3];
        $self->{'dbh'}->do("ALTER TABLE $table
                  CHANGE $field
                  $field $newtype");
    }
}

#########################################################################
sub RenameTable {
    my ($self, $table, $newname) = @_;

    my $ref = $self->TableExists($table);
    return unless $ref; # already renamed?

    $self->{'dbh'}->do("ALTER TABLE $table
                  RENAME TO $newname");
}

#########################################################################
sub RenameField {
    my ($self, $table, $field, $newname) = @_;

    my $ref = $self->GetFieldDef($table, $field);
    return unless $ref; # already fixed?
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

    if ($$ref[1] ne $newname) {
        print "Updating field $field in table $table ...\n";
        my $type = $$ref[1];
        $type .= " NOT NULL" if !$$ref[2];
        $type .= " auto_increment" if $$ref[5] =~ /auto_increment/;
        $self->{'dbh'}->do("ALTER TABLE $table
                  CHANGE $field
                  $newname $type");
    }
}

#########################################################################
sub AddField {
    my ($self, $table, $field, $definition) = @_;

    my $ref = $self->GetFieldDef($table, $field);
    return if $ref; # already added?

    print "Adding new field $field to table $table ...\n";
    $self->{'dbh'}->do("ALTER TABLE $table
              ADD COLUMN $field $definition");
}

#########################################################################
sub AddKey {
    my ($self, $table, $field, $definition) = @_;

    my $ref = $self->GetIndexDef($table, $field);
    return if $ref; # already added?

    print "Adding new key $field to table $table ...\n";
    $self->{'dbh'}->do("ALTER TABLE $table
              ADD KEY $field $definition");
}

#########################################################################
sub AddUniqueKey {
    my ($self, $table, $field, $definition) = @_;

    my $ref = $self->GetIndexDef($table, $field);
    return if $ref; # already added?

    print "Adding new key $field to table $table ...\n";
    $self->{'dbh'}->do("ALTER TABLE $table
              ADD UNIQUE KEY $field $definition");
}

#########################################################################
sub AddFullText {
    my ($self, $table, $index, $definition) = @_;

    my $ref = $self->GetIndexDef($table, $index);
    return if $ref; # already added?

    print "Adding new fulltext $index to table $table ...\n";
    $self->{'dbh'}->do("ALTER TABLE $table
              ADD FULLTEXT $index $definition");
}


#########################################################################
sub DropField {
    my ($self, $table, $field) = @_;

    my $ref = $self->GetFieldDef($table, $field);
    return unless $ref; # already dropped?

    print "Deleting unused field $field from table $table ...\n";
    $self->{'dbh'}->do("ALTER TABLE $table
              DROP COLUMN $field");
}

#########################################################################
sub DropTable {
    my ($self, $table, $field) = @_;

    my $ref = $self->TableExists($table);
    return unless $ref; # already dropped?

    print "Deleting unused table $table ...\n";
    $self->{'dbh'}->do("DROP TABLE $table");
}

#########################################################################
# this uses a mysql specific command. 
sub TableExists {
   my ($self, $table) = @_;
   my @tables;
   my $dbtable;
   my $exists = 0;
   my $sth = $self->{'dbh'}->prepare("SHOW TABLES");
   $sth->execute;
   while ( ($dbtable) = $sth->fetchrow_array ) {
      if ($dbtable eq $table) {
         $exists = 1;
      } 
   } 
   return $exists;
} 

#########################################################################
sub GetFieldDef {
    my ($self, $table, $field) = @_;
    my $sth = $self->{'dbh'}->prepare("SHOW COLUMNS FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[0] ne $field;
        return $ref;
   }
}

#########################################################################
sub GetIndexDef {
    my ($self, $table, $field) = @_;
    my $sth = $self->{'dbh'}->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[2] ne $field;
        return $ref;
   }
}

#########################################################################
sub CountIndexes {
    my ($self, $table) = @_;
    
    my $sth = $self->{'dbh'}->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    if ( $sth->rows == -1 ) {
      die ("Unexpected response while counting indexes in $table:" .
           " \$sth->rows == -1");
    }
    
    return ($sth->rows);
}

#########################################################################
sub DropIndex {
    my ($self, $table, $index) = @_;

    my $ref = $self->GetIndexDef($table, $index);
    return unless $ref; # no matching index?

    $self->{'dbh'}->do("ALTER TABLE $table
                  DROP INDEX $index");
}

#########################################################################
sub DropIndexes {
    my ($self, $table) = @_;
    my %SEEN;

    # get the list of indexes
    #
    my $sth = $self->{'dbh'}->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    # drop each index
    #
    while ( my $ref = $sth->fetchrow_arrayref) {
      
      # note that some indexes are described by multiple rows in the
      # index table, so we may have already dropped the index described
      # in the current row.
      # 
      next if exists $SEEN{$$ref[2]};

      if ($$ref[2] eq 'PRIMARY') {
          # The syntax for dropping a PRIMARY KEY is different
          # from the normal DROP INDEX syntax.
          $self->{'dbh'}->do("ALTER TABLE $table DROP PRIMARY KEY"); 
      }
      else {
          $self->{'dbh'}->do("ALTER TABLE $table DROP INDEX $$ref[2]");
      }
      $SEEN{$$ref[2]} = 1;

    }
}

1;
