# -*- Mode: perl; indent-tabs-mode: nil -*-

# TinderConfig - 



# $Revision: 1.8 $ 
# $Date: 2000/09/18 19:32:46 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Attic/TinderConfig.pm,v $ 
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




package TinderConfig;


# the url to the tinderbox server binary directory

$URL_BIN =  "http://tinderbox.mozilla.org/cgibin";


# the url to the tinderbox server HTML directory

$URL_HTML = "http://tinderbox.mozilla.org/";

# the full path name tinderbox will use to access the tinderbox
# servers root data directory where the html will be written.

$TINDERBOX_DIR = "/usr/apache/cgibin/webtools/tinderbox";

# The top level tinderbox index file. Change this if you wish to
# provide your own index file for tinderboxs web pages.

$GLOBAL_INDEX_FILE = "index.html";

# where errors are loged
  
$ERROR_LOG = "/var/log/tinderbox/log";
  
# where the daemon mode lock (for all trees) is placed
$LOCK_FILE = "/usr/apache/cgibin/webtools/tinderbox/tinderd.lock";

# the time between auto refreshes for all pages in seconds.
  
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
	   'TinderDB::Notice',
	   'TinderDB::Build',
	  );

# What border should the status legends use?  new browers allow us to
# frame the parts of the legend without putting a border arround the
# individual cells.

#$DB_LEGEND_BORDER = "border rules=none";
$DB_LEGEND_BORDER = "";

# finest spacing on html page (in minutes), this resticts the
# minimum time between builds (to this value plus 5 minutes).

$DB_MIN_TABLE_SPACING = 5;

# number of times a database can be updated before its contents must
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
	       
	       # this is not implemented yet
	       #'TinderHeader::Image,
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
                        'Image' => "",
                        'MOTD' => "",
                        'TreeState' => "Open",
			
                       );



# pick one display system if your VC system can display via a web
# server then VCDisplay module that you wish to use, otherwise pick
# 'None'.

$VCDisplayImpl = (
		  'VCDisplay::None',
		  #'VCDisplay::Bonsai',
		 );


# pick one method for storting data, Data::Dumper is slow but text
# files allows great debugging capabilities and Storable (not yet
# tested) which is much faster but binary format.

$PersistenceImpl = (
                    'Persistence::Dumper',
                    # 'Persistence::Storable',
                   );


# If you your using VCDisplay:Bonsai we need to know how to make HMTL
# to point to the bonsai CGI programs.

$BONSAI_URL = "http://tinderbox.mozilla.org/bonsai";

# the default number of hours shown on the status page

$DEFAULT_DISPLAY_HOURS = 6;

# The default page for a tree, used in several types of href links to
# 'return to the tree'.

#$DEFAULT_HTML_PAGE = 'index.html';
$DEFAULT_HTML_PAGE = 'status.html';

# the amount of time rmlogs keeps logs on file

$BRIEF_LOG_TRIM_DAYS = 1;
$FULL_LOG_TRIM_DAYS = 7;

1;
