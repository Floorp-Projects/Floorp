# -*- Mode: perl; indent-tabs-mode: nil -*-

# FileStructure.pm - The lookup for where different file/directories,
# for each tree, are stored on the filesystem.  Local system
# administrator may need to put different trees onto different disk
# partitions and this will require making get_filename() less regular
# then we have defined it here.

# $Revision: 1.5 $ 
# $Date: 2000/09/18 19:26:22 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Attic/FileStructure.pm,v $ 
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





package FileStructure;

$VERSION = '#tinder_version#';



# I wish to do away with images.  They do not add much to the display
# and they make a bunch of clutter.

%IMAGES = (
  'flames'    => '1afi003r.gif',
  'star'      => 'star.gif',
);



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

	);

# the full path name tinderbox will use to access the tinderbox
# servers root data directory

$TINDERBOX_DIR = ($TinderConfig::TINDERBOX_DIR || 
                  "/usr/apache/cgibin/webtools/tinderbox");

$GLOBAL_INDEX_FILE = ($TinderConfig::GLOBAL_INDEX_FILE ||
		      "index.html");

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

  my ($tree_dir) = "$TINDERBOX_DIR/$tree";

  # all the file names this program uses appear below

  my %all_files = 
    (

     # the build log files will be turned into html and stored
     # here.  Lets have just one big full/brief directory for
     # all the trees this will make cleanup easier and no one
     # goes browsing through the tree specific log dir anyway.
     
     'full-log' => "$TINDERBOX_DIR/full",
     
     'brief-log' => "$TINDERBOX_DIR/brief",
     
     # where the binary files mailed inside the log files will
     # be placed.
     
     'build_bin_dir' => "$tree_dir/bin",
     
     # the per tree time stamp file to ensure all updates are at least 
     # $MIN_TABLE_SPACING apart
     
     'update_time_stamp'  => "$tree_dir/db/Mail.time.stamp",
     
     # where the tree specific generated html pages are placed
     # in this directory
     
     'tree_HTML' => $tree_dir,
     
     'tree_URL' => "$URL_HTML/$tree",
     
     # where the database files are stored on disk
     
     'TinderDB_Dir'=> "$tree_dir/db",
     
     # header data files
     
     'TinderHeader_Dir'=> "$tree_dir/h",
     
     # the index file for this tree
     
     'index'=> "$tree_dir/index.html",
     
     # access to the administration page

     # this version lets you have each tree have a different set of
     # administrators.
     
     'passwd' => "$tree_dir/h/passwd.DBdat",
     
     # this version lets you have one set of adminstrators for all
     # trees.
     
     #'passwd' => "$TINDERBOX_DIR/passwd.DBdat",

     # there are automated bots who need the header data, they extract
     # it from this file.

     'alltree_headers' => "$tree_dir/alltree_headers.html",

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
