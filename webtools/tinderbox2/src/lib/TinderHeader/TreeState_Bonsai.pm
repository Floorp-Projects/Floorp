# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderHeader::TreeState_Bonsai - an interface to the bonasi VC
# managment system this module will get the current tree state and set
# the current tree state using the bonsai conventions when we have
# implemented it.  

# $Revision: 1.5 $ 
# $Date: 2001/07/20 19:05:19 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderHeader/TreeState_Bonsai.pm,v $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 


package TinderHeader::TreeState;

# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use BonsaiData;



$VERSION = ( qw $Revision: 1.5 $ )[1];

# load the simple name of this module into TinderHeader so we can
# track the implementations provided.

$TinderHeader::NAMES2OBJS{ 'TreeState' } = 
  TinderHeader::TreeState->new();



sub new {
  my $type = shift;
  my %params = @_;
  my $self = {};

  bless $self, $type;

  return  $self;
}


sub gettree_header {
  my ($self, $tree,) = @_;

  my ($treestate) = BonsaiData::get_tree_state($tree);

  return $treestate;
}

sub savetree_header {

  # NULL implementation.

  #  The tree state must be changed via the bonsai tool. Yhere is no
  #  programatic way to change the state.

}

1;

