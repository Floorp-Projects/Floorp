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
# Copyright (C) 2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s): Dan Mosedale <dmose@mozilla.org>
#                 Terry Weissman <terry@mozilla.org>
#                 Dave Miller <justdave@syndicomm.com>

# This object models a set of relations between one item and a group
# of other items.  An example is the set of relations between one bug
# and the users CCed on that bug.  Currently, the relation objects are
# expected to be bugzilla userids.  However, this could and perhaps
# should be generalized to work with non userid objects, such as
# keywords associated with a bug.  That shouldn't be hard to do; it
# might involve turning this into a virtual base class, and having
# UserSet and KeywordSet types that inherit from it.

use diagnostics;
use strict;

# Everything that uses RelationSet should already have globals.pl loaded
# so we don't want to load it here.  Doing so causes a loop in Perl because
# globals.pl turns around and does a 'use RelationSet'
# See http://bugzilla.mozilla.org/show_bug.cgi?id=72862
#require "globals.pl";

package RelationSet;
use CGI::Carp qw(fatalsToBrowser);

# create a new empty RelationSet
#
sub new {
  my $type = shift();

  # create a ref to an empty hash and bless it
  #
  my $self = {};
  bless $self, $type;

  # construct from a comma-delimited string
  # 
  if ($#_ == 0) {
    $self->mergeFromString($_[0]);
  }
  # unless this was a constructor for an empty list, somebody screwed up.
  #
  elsif ( $#_ != -1 ) {
    confess("invalid number of arguments");
  }

  # bless as a RelationSet
  #
  return $self;
}

# Assumes that the set of relations "FROM $table WHERE $constantSql and 
# $column = $value" is currently represented by $self, and this set should
# be updated to look like $other.  
#
# Returns an array of two strings, one INSERT and one DELETE, which will
# make this change.  Either or both strings may be the empty string,
# meaning that no INSERT or DELETE or both (respectively) need to be done.
#
# THE CALLER IS RESPONSIBLE FOR ANY DESIRED LOCKING AND/OR CONSISTENCY 
# CHECKS (not to mention doing the SendSQL() calls).
#
sub generateSqlDeltas {
  ($#_ == 5) || confess("invalid number of arguments");
  my ( $self, # instance ptr to set representing the existing state
       $endState, # instance ptr to set representing the desired state
       $table, # table where these relations are kept
       $invariantName, # column held const for a RelationSet (often "bug_id")
       $invariantValue, # what to hold the above column constant at
       $columnName # the column which varies (often a userid)
     ) = @_;

  # construct the insert list by finding relations which exist in the
  # end state but not the current state.
  #
  my @endStateRelations = keys(%$endState);
  my @insertList = ();
  foreach ( @endStateRelations ) {
    push ( @insertList, $_ ) if ( ! exists $$self{"$_"} );
  }

  # we've built the list.  If it's non-null, add required sql chrome.
  #
  my $sqlInsert="";
  if ( $#insertList > -1 ) {
    $sqlInsert = "INSERT INTO $table ($invariantName, $columnName) VALUES " .
      join (",", 
            map ( "($invariantValue, $_)" , @insertList ) 
           );
  }
     
  # construct the delete list by seeing which relations exist in the
  # current state but not the end state
  #
  my @selfRelations = keys(%$self);
  my @deleteList = ();
  foreach ( @selfRelations ) {
    push (@deleteList, $_) if ( ! exists $$endState{"$_"} );
  }

  # we've built the list.  if it's non-empty, add required sql chrome.
  #
  my $sqlDelete = "";
  if ( $#deleteList > -1 ) {
    $sqlDelete = "DELETE FROM $table WHERE $invariantName = $invariantValue " .
      "AND $columnName IN ( " . join (",", @deleteList) . " )";
  } 

  return ($sqlInsert, $sqlDelete);
}

# compare the current object with another.  
#
sub isEqual {
  ($#_ == 1) || confess("invalid number of arguments");
  my $self = shift();
  my $other = shift();

  # get arrays of the keys for faster processing
  #
  my @selfRelations = keys(%$self);
  my @otherRelations = keys(%$other);
 
  # make sure the arrays are the same size
  #
  return 0 if ( $#selfRelations != $#otherRelations );

  # bail out if any of the elements are different
  #
  foreach my $relation ( @selfRelations ) {
    return 0 if ( !exists $$other{$relation})
  }

  # we made it!
  #
  return 1;

}

# merge the results of a SQL command into this set
#
sub mergeFromDB {
  ( $#_ == 1 ) || confess("invalid number of arguments");
  my $self = shift();

  &::SendSQL(shift());
  while (my @row = &::FetchSQLData()) {
    $$self{$row[0]} = 1;
  }

  return;
}

# merge a set in string form into this set
#
sub mergeFromString {
  ($#_ == 1) || confess("invalid number of arguments");
  my $self = shift();

  # do the merge
  #
  foreach my $person (split(/[ ,]/, shift())) {
    if ($person ne "") {
      $$self{&::DBNameToIdAndCheck($person)} = 1;
    }
  }
}

# remove a set in string form from this set
#
sub removeItemsInString {
  ($#_ == 1) || confess("invalid number of arguments");
  my $self = shift();

  # do the merge
  #
  foreach my $person (split(/[ ,]/, shift())) {
    if ($person ne "") {
      my $dbid = &::DBNameToIdAndCheck($person);
      if (exists $$self{$dbid}) {
        delete $$self{$dbid};
      }
    }
  }
}

# remove a set in array form from this set
#
sub removeItemsInArray {
  ($#_ > 0) || confess("invalid number of arguments");
  my $self = shift();

  # do the merge
  #
  while (my $person = shift()) {
    if ($person ne "") {
      my $dbid = &::DBNameToIdAndCheck($person);
      if (exists $$self{$dbid}) {
        delete $$self{$dbid};
      }
    }
  }
}

# return the number of elements in this set
#
sub size {
  my $self = shift();

  my @k = keys(%$self);
  return $#k++;
}

# return this set in array form
#
sub toArray {
  my $self= shift();

  return keys(%$self);
}

# return this set as an array of strings
#
sub toArrayOfStrings {
  ($#_ == 0) || confess("invalid number of arguments");
  my $self = shift();

  my @result = ();
  foreach my $i ( keys %$self ) {
    push @result, &::DBID_to_name($i);
  }

  return sort { lc($a) cmp lc($b) } @result;
}  

# return this set in string form (comma-separated and sorted)
#
sub toString {
  ($#_ == 0) || confess("invalid number of arguments");
  my $self = shift();

  my @result = ();
  foreach my $i ( keys %$self ) {
    push @result, &::DBID_to_name($i);
  }

  return join(',', sort(@result));
}  

1;
