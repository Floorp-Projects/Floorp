# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderHeader::BasicTxtHeader - a simple text file implementation of
# the TinderHeader functionality ( new(), savetree_header(),
# gettree_header(), ).


# $Revision: 1.8 $ 
# $Date: 2003/08/17 01:39:21 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderHeader/BasicTxtHeader.pm,v $ 
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





package TinderHeader::BasicTxtHeader;


# Tinderbox libraries

use lib '#tinder_libdir#';

use FileStructure;
use Persistence;

$VERSION = ( qw $Revision: 1.8 $ )[1];


sub new {
  my $type = shift;
  my %params = @_;
  my $self = {};

  bless $self, $type;

  return  $self;
}



# the file which store the header information.

sub db_file {
  my ($self, $tree,) = @_;
  my ($file) = (FileStructure::get_filename($tree, 'TinderHeader_Dir').
                "/".ref($self).".DBdat");
  $file =~ s![^/:]+::!!;
  return $file;
}


# this is called by the admintree program.  We expect tree
# administration to be a rare event and each change should be
# independent and noncontradictory, so we do not worry about locking
# during the updates but we make an effort to keep the updates atomic.
# Notice that this is in sharp contrast to savetree_db() which will
# never be called unless the tinder.cgi has a global lock out.


sub savetree_header {
  my ($self, $tree, $value) = @_;

  my ($filename) = $self->db_file($tree);
  my ($name_space) = ref($self);
  my ($value_ref) = \$value;

  Persistence::save_structure($value_ref, $filename);

  return ;
}


# return the "$value" which was saved into the file

sub gettree_header {
  my ($self, $tree) = @_;
  my ($name_space) = ref($self);
  my ($filename) = $self->db_file($tree);

  (-r $filename) || 
    return ;
  
  my ($value_ref) = Persistence::load_structure($filename);
  my ($value) = ${ $value_ref };

  return $value;
}

1;
