# -*- Mode: perl; indent-tabs-mode: nil -*-

# FileStructure.pm - The lookup for where different file/directories,
# for each tree, are stored on the filesystem.  Local system
# administrator may need to put different trees onto different disk
# partitions and this will require making directory structure in
# get_filename() less regular then we have defined it here.

# $Revision: 1.10 $ 
# $Date: 2003/08/17 02:13:17 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/default_conf/FileStructure.pm,v $ 
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





package FileStructure;

# This package must not use any tinderbox specific libraries.  It is
# intended to be a base class.

$VERSION = '#tinder_version#';

# the url to the tinderbox server binary directory

$URL_BIN = ($TinderConfig::URL_BIN ||
            "http://tinderbox.mozilla.org/cgibin");


# the url to the tinderbox server HTML directory

$URL_HTML = ($TinderConfig::URL_HTML ||
             "http://tinderbox.mozilla.org/");


%URLS = (

	 # url to main tinderbox executable

	 'tinderd' => "$URL_BIN/tinder.cgi",

         # url to show notices for users who can not view popup windows.

         'shownote' => "$URL_BIN/shownote.cgi",

         'admintree' => "$URL_BIN/admintree.cgi",
         
         'addnote' => "$URL_BIN/addnote.cgi",

         'gunzip' => "$URL_BIN/gunzip.cgi",

         'treepages' => "$URL_HTML",
         
         'regenerate' => "$URL_BIN/regenerate.cgi",  

	);


# the full path name tinderbox will use to access the tinderbox
# servers root data directory

$TINDERBOX_HTML_DIR = ($TinderConfig::TINDERBOX_HTML_DIR || 
                  "/usr/apache/cgibin/webtools/tinderbox");

$TINDERBOX_DATA_DIR = ($TinderConfig::TINDERBOX_DATA_DIR || 
                  "/usr/apache/cgibin/webtools/tinderbox");

$TINDERBOX_GZLOG_DIR = ($TinderConfig::TINDERBOX_GZLOG_DIR || 
                  "/usr/apache/cgibin/webtools/tinderbox");

$GLOBAL_INDEX_FILE = ($TinderConfig::GLOBAL_INDEX_FILE ||
		      "index.html");

$LOCK_FILE = ($TinderConfig::LOCK_FILE ||
              "/usr/apache/cgibin/webtools/tinderbox/tinderd.lock");

$CGIBIN_DIR = ($TinderConfig::TINDERBOX_CGIBIN_DIR ||
               "/usr/apache/cgibin/webtools/tinderbox");

$GIF_URL = ($TinderConfig::GIF_URL ||
            "http://tinderbox.mozilla.org/gif");


# the default page for a tree
$DEFAULT_HTML_PAGE = $TinderConfig::DEFAULT_HTML_PAGE || 'index.html';

# The lookup for where different file/directories are stored on the
# filesystem.  Local system administrator may need to put different
# trees onto different disk partitions and this will require making
# get_filename() less regular then we have defined it here.

sub get_filename {
  my ($tree, $file,) = @_;

  (TreeData::tree_exists($tree)) ||
    die("tree: $tree does not exist\n");    

  my ($html_tree_dir) = "$TINDERBOX_HTML_DIR/$tree";
  my ($data_tree_dir) = "$TINDERBOX_DATA_DIR/$tree";

  # all the file names this program uses appear below

  my %all_files = 
    (

     # where the database files are stored on disk. For debugging it
     # is useful to have this inside the webservers document root so
     # that all browsers can examine the raw data.  For security you
     # may want this outside the webservers document root, so that the
     # raw data can not be seen.

     'TinderDB_Dir'=> "$data_tree_dir/db",

     # header data files are placed in this directory.  Same issues
     # with document root as above.

     'TinderHeader_Dir'=> "$data_tree_dir/h",
     
     # setting it like this lets you have each tree have a different
     # set of administrators.

     'passwd' => "$data_tree_dir/h/passwd.DBdat",

     # setting it like this lets you have one set of adminstrators for
     # all trees.

     #'passwd' => "$TINDERBOX_DATA_DIR/passwd.DBdat",

     # The build log files will be turned into html and stored
     # here.  Lets have just one big full/brief directory for
     # all the trees this will make cleanup easier and no one
     # goes browsing through the tree specific log dir anyway.
     
     'full-log' => "$TINDERBOX_GZLOG_DIR/full",
     
     'brief-log' => "$TINDERBOX_GZLOG_DIR/brief",
     
     # where the binary files mailed inside the build log files will
     # be placed.
     
     'build_bin_dir' => "$html_tree_dir/bin",
     
     # the per tree time stamp file to ensure all updates are at least 
     # $TinderDB::TABLE_SPACING apart
     
     'update_time_stamp'  => "$data_tree_dir/db/Mail.Build.time.stamp",
     
     # where the tree specific generated html pages are placed
     # in this directory
     
     'tree_HTML' => $html_tree_dir,
     
     'tree_URL' => "$URL_HTML/$tree",

     # the index file to all summary pages and the status page for
     # this tree.

     'index'=> "$html_tree_dir/index.html",
     
     # there are automated bots who need the header data, they extract
     # it from this file.

     'alltree_headers' => "$html_tree_dir/alltree_headers.html",

    );

  my $out = $all_files{$file};

  ($out) ||
    die("error in function FileStructure::all_files: ".
        "file: $file does not exist\n");    

  # Restrict the characters allowed in a file name to a known safe
  # set.

  $out = main::extract_filename_chars($out);

  return $out;
}


1;
