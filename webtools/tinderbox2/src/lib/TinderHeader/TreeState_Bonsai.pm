# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderHeader::TreeState_Bonsai - an interface to the bonasi VC
# managment system this module will get the current tree state and set
# the current tree state using the bonsai conventions when we have
# implemented it.  Currently it is just holding code from the old
# version of tinderbox which will help us code the correct version.

# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:17:19 $ 
# $Author: mcafee%netscape.com $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 


package TinderDB::TreeState;

$VERSION = ( qw $Revision: 1.1 $ )[1];

sub gettree_header {
  my $line, $treestate;

  open(BID, "<../bonsai/data/$bonsai_tree/batchid.pl")
    or die("can't open batchid<br>");
  $line = <BID>;
  close(BID);

  if ($line =~ m/'(\d+)'/) {
      $bid = $1;
  } else {
      return ;
  }

  open(BATCH, "<../bonsai/data/$bonsai_tree/batch-${bid}.pl")
    or die("can't open batch-${bid}.pl<br>");
  while ($line = <BATCH>) { 
    if ($line =~ /^\$::TreeOpen = '(\d+)';/) {
        $treestate = $1;
        last;
    }
  }
  close(BATCH);

  $treestate = ($treestate ? 'OPEN' : 'CLOSED');

  return $treestate;
}


1;

