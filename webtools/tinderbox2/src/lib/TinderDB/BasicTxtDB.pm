# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# This file provides common implementations for TinderDB's.  Most DB's
# read a set of update files to gather new data and save their state
# as a Dump of the $DATABASE reference.


# $Revision: 1.12 $ 
# $Date: 2003/08/17 01:45:10 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/BasicTxtDB.pm,v $ 
# $Name:  $ 



# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


package TinderDB::BasicTxtDB;


# Tinderbox libraries

use lib '#tinder_libdir#';

use FileStructure;
use Persistence;


$VERSION = ( qw $Revision: 1.12 $ )[1];


# To help preserve the database in the event of a serious system
# problem we save the database to a different file name then change
# the name of the file to the database file name.  This allows us to
# recover from a program crash during the save operation since we
# always have at least one copy of the data arround.

sub new {

  # In the future we may wish to put the tree name, a reference to the
  # database and a reference to the metadata into the object but this
  # would make it harder to push into @TinderDB::HTML_COLUMNS at
  # compile time AND this a GLOBAL list of implementations to use so
  # needs to be kept clear of tree specific data.  How to connect the
  # object returned by TinderDB::loadtree_db($tree) with the object
  # used by TinderDB::Build::loadtree_db($tree) is not clear to me, we
  # would loose much of the simplicity of the wrappers with strange
  # calling sequences. Also now the TinderDB object is quite complex
  # it needs to be indexed by: module, tree, value.  The build module
  # uses the fact that STATUS can be peeked at when the TinderDB
  # object is not arround.

  my $type = shift;
  my %params = @_;
  my $self = {};
  bless $self, $type;
}


# we contuct the prefix for all update files in this function.  Note
# that the namespace gets its name in the prefix.

sub update_file {
  my ($self, $tree,) = @_;
  my ($file) = (FileStructure::get_filename($tree, 'TinderDB_Dir').
                "/".ref($self).".Update");
  $file =~ s![^/:]+::!!;
  return $file;
}

# we contuct the prefix for all DB files in this function.  Note that
# that the namespace gets its name in the DB name.

sub db_file {
  my ($self, $tree,) = @_;
  my ($file) = (FileStructure::get_filename($tree, 'TinderDB_Dir').
                "/".ref($self).".DBdat");
  $file =~ s![^/:]+::!!;
  return $file;
}


sub unlink_files {
  my ($self, @files) = @_;

  foreach $file (@files) {

    # This may be the output of a glob, make it taint safe.
    $full_file = main::extract_safe_filename($file);

    unlink ("$full_file") ||
      die("Could not remove filename: '$full_file': $!\n");

    # since we have 'required' these files we need to forget that they
    # were loaded.

    delete $INC{"$full_file"};
  }

  return ;  
}


sub readdir_file_prefix {

# return the list of all files in $dir which begin with the prefix
# $prefix in sorted (ascii' betically) order.  This is important since
# we often deal with files whos suffix is time().$pid.

  my ($self, $dir, $prefix) = @_;

  (-d $dir) ||
    return ;

  opendir(DIR, $dir) ||
    die("Could not open directory: $dir, $!\n");
  
  my (@dir_list) = readdir(DIR);
  
  closedir(DIR) ||
    die("Could not close directory: $dir, $!\n");
  
  # sort the file names by the order of the starttimes, oldest updates
  # first.  This is encoded by the digits on the end of the filename.
  
  my (@sorted_files) = sort grep ( /^$prefix/, @dir_list );

  # make it taint safe.

  my (@untainted_files) = map { main::extract_safe_filename("$dir/$_") } 
  @sorted_files;

  return @untainted_files;
}



# load the DB, since the save may have been interrupted we try and
# recover if there is a good DB file available.

sub loadtree_db {
  my ($self, $tree) = @_;

  (TreeData::tree_exists($tree)) ||
    die("Tree: $tree, not defined.");
  
  my ($filename) = $self->db_file($tree);
  my $dirname= File::Basename::dirname($filename);
  my $basename = File::Basename::basename($filename);
  my (@sorted_files) = $self->readdir_file_prefix( $dirname, $basename);
  
  if ( !(-r $filename) && scalar(@sorted_files)) {

    # might have had an incomplete write, take most recent tmpfile.
    rename("$dirname/$sorted_files[$#sorted_files]", $filename) ||
      die("Could not rename: $sorted_files[$#sorted_files] ".
          "to $filename: $!\n");
  }
  
  # if we are running for the first time there may not be any data for
  # us to read.
  
  (-r $filename) || return ;

  my ($r) = Persistence::load_structure($filename);

  # Put the data into the correct namespace.

  # If you can find a way to do this without the eval please help.  The
  # perl parser evaluates all my attempts to mean something else.

  my $namespace = ref($self);
  my $str = ( 
             " ( \$$namespace"."::DATABASE{'$tree'}, ".
             "   \$$namespace"."::METADATA{'$tree'}, ) ".
             '= @{ $r } ;'
            );
  eval $str;

  # ignore unlink errors, cleaning up the directory is not important. 
  
  my @extra_files = grep {!/^${filename}$/} @sorted_files;
  foreach $file (@extra_files) {
    $file = main::extract_safe_filename($file);
    unlink($file);
  }

  return ;
}



# Save the DB data to a file in a safe fashion. 

# This needs to be called from the implementation of apply_db_updates
# since each update of the database should imply a save.

sub savetree_db {
  my ($self, $tree) = @_;

  (TreeData::tree_exists($tree)) ||
    die("Tree: $tree, not defined.");

  my ($data_file) = $self->db_file($tree);

  my ($name_space) = ref($self);
  my ($name_db) = "${name_space}::DATABASE";
  my ($name_meta) = "${name_space}::METADATA";
  Persistence::save_structure( 
                              [ 
                               $ { $name_db }{$tree}, 
                               $ { $name_meta }{$tree} 
                              ],
                              $data_file
                             );

  return ;
}

# -------------------------------------------------------------- 

# These functions assume that the data is stored in a structure which
# looks like:
# 		$DATABASE{$tree}{$time};

# so really everything above the dotted line is an implementation
# about how the database is stored on the disk, this part is an
# implementation of the DATABASE datastructure.



# remove all records from the database which are older then last_time.

sub trim_db_history {
  my ($self, $tree,) = (@_);

  my ($last_time) =  $main::TIME - $TinderDB::TRIM_SECONDS;

  # sort numerically ascending
  my (@times) = sort {$a <=> $b} keys %{ $DATABASE{$tree} };
  foreach $time (@times) {
    ($time >= $last_time) && last;

    delete $DATABASE{$tree}{$time};
  }

  return ;
}

# return a list of all the times where an even occured.

sub event_times_vec {
  my ($self, $start_time, $end_time, $tree) = (@_);

  my ($name_space) = ref($self);
  my ($name_db) = "${name_space}::DATABASE";

  # sort numerically descending
  my (@times) = sort {$b <=> $a} keys %{ $ { $name_db }{$tree} };

  my @out;
  foreach $time (@times) {
    ($time <= $start_time) || next;
    ($time <= $end_time) && last;
    push @out, $time;
  }

  return @out;
}

# where can people attach notices to?
# Really this is the names the columns produced by this DB

# the default for columns is not to allow notices, unless they have
# installed all the hooks they need and override this function.

sub notice_association {
    return '';
}


1;
