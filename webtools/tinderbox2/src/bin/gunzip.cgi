#!#perl# #perlflags# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# gunzip.cgi - This cgi script will gunzip a file and send the result
# to standard out in a form that a webserver can display.  Filenames
# are passed in via an abbreviated form.  It is assumed that all files
# are either brief or full log files which are stored in known
# Tinderbox directories.  The file id is the basename of the file
# without the '.gz.html' extension.


# $Revision: 1.10 $ 
# $Date: 2003/08/04 17:14:57 $ 
# $Author: kestes%walrus.com $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 



# Tinderbox libraries
use lib '#tinder_libdir#';

use TinderConfig;
use TreeData;
use FileStructure;
use HTMLPopUp;
use Utils;






sub usage {


    my $usage =<<EOF;

$0	[--version] [--help]  
$0	tree=treename [brief-log=fileid][full-log=fileid]


Informational Arguments


--version	Print version information for this program

--help		Show this usage page


Synopsis


This cgi script will gunzip a file and send the result to standard out
in a form that a webserver can display.  Filenames are passed in via
an abbreviated form.  It is assumed that all files are either brief or
full log files which are stored in known Tinderbox directories.  The
file id is the basename of the file without the '.gz.html' extension.


EOF

    print $usage;
    exit 0;

} # usage


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

  # tree is safe, untaint it.
  $tree =~ m/(.*)/;
  $tree = $1;

  my ($log_type);
  my ($log_file);
  if ($log_file = $form{'brief-log'}) {
    $log_type = "brief-log";
  }  elsif ($log_file = $form{'full-log'}) {
    $log_type = "full-log";
  }

  # untaint the log_file, we do not use letters in the file name yet
  # but it does us no harm to allow for future expansion.

  $log_file =~ m/([0-9a-zA-Z\.]*)/;
  $log_file = $1;

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
  chk_security();

  print "Content-type: text/html\n\n";

  # To ensure that we do not have security problems:
  # 1) we ensure that the log file exists 
  # 2) we run system with a list argument.

  if (-f $ZIPPED_FILE) {
    my (@cmd) = (@GUNZIP, $ZIPPED_FILE);
    system(@cmd);
    ($?) && die("Could not run: '@cmd' : $! : waitstatus $? \n");
  } else {
    print "Could not find file: $ZIPPED_FILE\n";
  }

  exit 0;
}
