# -*- Mode: perl; indent-tabs-mode: nil -*-


# TinderConfig - Global configuration file containing the major
# customizable settings.


# $Revision: 1.8 $ 
# $Date: 2001/03/26 14:01:38 $ 
# $Author: kestes%tradinglinx.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/default_conf/TinderConfig.pm,v $ 
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
#	 kestes@tradinglinx.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 




package TinderConfig;

# This package must not use any tinderbox specific libraries.  It is
# intended to be a base class.



# How do we run the unzip command?

@GZIP = ("/usr/local/bin/gzip",);

@GUNZIP = ("/usr/local/bin/gzip", "--uncompress", "--to-stdout",);


# The GNU UUDECODE will use these arugments, Solaris uudecode is
# different. 

@UUDECODE = ("/usr/local/bin/uudecode", "-o",);


# The url to the tinderbox server binary directory

$URL_BIN =  "http://tinderbox.mozilla.org/cgibin";


# The url to the tinderbox server HTML directory

$URL_HTML = "http://tinderbox.mozilla.org/";

# The full path name tinderbox will use to access the tinderbox
# servers root data directory where the html will be written.

$TINDERBOX_HTML_DIR = "/home/httpd/html/tinderbox";

# The full path name tinderbox will use to access the tinderbox
# servers root data directory where the data will be written.  For
# debugging you may wish to make this the same as the
# $TINDERBOX_HTML_DIR and set Persistence::Dumper.  This setting will
# allow a browser can look at the internal data structures. For
# production use it is more secure to keep internal tinderbox data
# outside of the HTML tree so that the webserver can not send the
# internal data over the network.

$TINDERBOX_DATA_DIR = "/home/httpd/html/tinderbox";

# The top level tinderbox index file. Change this if you wish to
# provide your own index file for tinderboxs web pages.

$GLOBAL_INDEX_FILE = "index.html";

# Error log filename:

$ERROR_LOG = "/var/log/tinderbox2/log";
  
# Where the daemon mode lock (for all trees) is placed
$LOCK_FILE = $TINDERBOX_HTML_DIR."/tinderd.lock";

# The time between auto refreshes for all pages in seconds.

$REFRESH_TIME = (60 * 15);


# Pick how you wish to the Tinderbox popup windows to be implemented:
# Uncomment only one HTMLPopUp implementation.

# MajorCoolWindow: Should be portable to all browsers
# MozillaLayers:   Will not display on any browser other then Netscape
# None:            A null implementation which will not use any popups
#                  provide no popup windows. Use this if you do not run
#                  JavaScript in your browsers.
 
$PopUpImpl = (
	      # 'HTMLPopUp::MozillaLayers',
	      'HTMLPopUp::MajorCoolWindow',
	      # 'HTMLPopUp::None',
	     );


# Use the DB implementations you wish to use.

# These uses determine the columns of the build page and their
# include order is the order in which the columns are displayed.

# The main choice of implementations is how to gather information
# about checkins.  You can currently choose whether you are using
# bonsai or are using CVS raw.

@DBImpl = (
	   'TinderDB::Time',
	   'TinderDB::VC_CVS',
#	   'TinderDB::VC_Bonsai',
	   'TinderDB::Notice',
	   'TinderDB::BT_Generic',
	   'TinderDB::Build',
	  );

# What border should the status legends use?  new browers allow us to
# frame the parts of the legend without putting a border arround the
# individual cells.

#$DB_LEGEND_BORDER = "border rules=none";
$DB_LEGEND_BORDER = "";

# Spacing on html page (in minutes), this resticts the
# minimum time between builds (to this value plus 5 minutes).

$DB_TABLE_SPACING = 5;

# Number of times a database can be updated before its contents must
# be trimmed of old data.  This scan of the database is used to
# collect data about average build time so we do not want it
# performed too infrequently.

$DB_MAX_UPDATES_SINCE_TRIM = 50;

# Number of seconds to keep in Database, older data will be trimmed
# away.

$DB_TRIM_SECONDS = (60 * 60 * 24 * 8);

@HeaderImpl = (
	       'TinderHeader::Build',
	       'TinderHeader::IgnoreBuilds',
	       'TinderHeader::MOTD',
	       
	       # TinderDB::VC_Bonsai provides a
	       # TinderHeader::TreeState implementation,
	       # so comment out the TreeSTate if using
	       # VC_Bonsai. Most VC implementations will
	       # not have a State file in the version
	       # control system.
	       
	       'TinderHeader::TreeState',
	      );

# Each of the TinderHeader method appears on the left side of this
# hash and gets a default value.  You must have a default value for
# every header even if you do not use an implementation for it.

%HEADER2DEFAULT_HTML = (
                        # the build module has one piece of info
                        # which goes in the header, our best guess 
                        # as to when the tree broke.
			
                        'Build' => "",
                        'IgnoreBuilds' => "",
                        'MOTD' => "",
                        'TreeState' => "Open",
			
                       );



# Pick one display system if your VC system can display via a web
# server then VCDisplay module that you wish to use, otherwise pick
# 'None'.

$VCDisplayImpl = (
		  'VCDisplay::None',
		  #'VCDisplay::Bonsai',
		 );


# Pick one method for storting data, Data::Dumper is slow but text
# files allows great debugging capabilities and Storable, availible
# from CPAN, which is a much faster binary format.

# If you are worried about security you should use Storable because
# Dumper uses and Eval to load the new code it is concievable that the
# code could be forced to perform unwanted actions.

$PersistenceImpl = (
                    'Persistence::Dumper',
                    # 'Persistence::Storable',
                   );


# If you your using VCDisplay:Bonsai we need to know how to make HMTL
# to point to the bonsai CGI programs.

$BONSAI_URL = "http://tinderbox.mozilla.org/bonsai";

# If we query bonsai data we need to know the directory which bonsai
# is installed in.

$BONSAI_DIR = "/home/httpd/cgi-bin/bonsai";


# If you your using BT_Generic we need to know how to make HMTL
# to point to the bug tracking CGI programs.

$BT_URL	= 'http://bugzilla.mozilla.org/';

# The default number of hours shown on the status page

$DEFAULT_DISPLAY_HOURS = 6;

# The default page for a tree, used in several types of href links to
# 'return to the tree'.

#$DEFAULT_HTML_PAGE = 'index.html';
$DEFAULT_HTML_PAGE = 'status.html';

# The amount of time rmlogs keeps logs on file

$BRIEF_LOG_TRIM_DAYS = 7;
$FULL_LOG_TRIM_DAYS = 7;

# Should we write performance data to the log file?
# zero means no, one means yes.

$LOG_PERFORMANCE = 0;

# Define IP addresses/domain names which are allowed to run the
# administrative functions.

$ADMINISTRATIVE_NETWORK_PAT = ( 
                                '(^10\.10\.[0-9\.]*$)|'.
                                '(^207\.200\.81\.[0-9\.]*)$|'.
                                '(\.mozilla\.org$)|'.
                                '(\.netscape\.com$)|'.
                                '(^localhost$)'
                              );


1;
