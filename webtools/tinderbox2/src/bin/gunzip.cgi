#!#perl# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:10:43 $ 
# $Author: mcafee%netscape.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/bin/gunzip.cgi,v $ 
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



# Tinderbox libraries
use lib '#tinder_libdir#';

use TreeData;
use FileStructure;
use HTMLPopUp;
use Utils;




sub parse_args {
  my (%form) = HTMLPopUp::split_cgi_args();

  # take care of the informational arguments
  
  if(grep /version/, keys %form) {
    print "$0: Version: $VERSION\n";
    exit 0;  
  }
  
  if (grep /help/, keys %form) {
    usage();
  }
  
  my ($tree) = $form{'tree'};

  (TreeData::tree_exists($tree)) ||
    die("tree: $tree does not exist\n");    

  my ($log_type);
  my ($log_file);
  if ($log_file = $form{'brief-log'}) {
    $log_type = "brief-log";
  }  elsif ($log_file = $form{'full-log'}) {
    $log_type = "full-log";
  }
  
  ($log_type) ||
    die("Must specify either 'full-log' or 'brief-log'\n");
  
  my ($zipped_file) = (FileStructure::get_filename($tree, $log_type).
                       "/$log_file.html.gz");

  $LOG_TYPE = $log_type;
  $ZIPPED_FILE = $zipped_file;

  return 1;
}



# --------------------main-------------------------
{
  set_static_vars();
  get_env();
  parse_args();

  print "Content-type: text/html\n\n";

  my (@cmd) = (@GUNZIP, $ZIPPED_FILE);
  system(@cmd);
  ($?) && die("Could not run: '@cmd'\n");
  
  exit 0;
}
