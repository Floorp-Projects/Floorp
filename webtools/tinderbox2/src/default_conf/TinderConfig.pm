# -*- Mode: perl; indent-tabs-mode: nil -*-


# TinderConfig - Global configuration file containing the major
# customizable settings.


# $Revision: 1.49 $ 
# $Date: 2003/08/16 18:37:44 $ 
# $Author: kestes%walrus.com $ 
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
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 




package TinderConfig;

# This package must not use any Tinderbox specific libraries.  It is
# intended to be a base class.



# set the path for Tinderbox.

$ENV{'PATH'}=  (
                '/bin'.
':/home/kestes/mozilla/webtools/tinderbox2/build/test/vcsim'.
                ':/usr/bin'.
                ':/usr/local/bin'.
                ':/opt/gnu/bin/'.
                '');

# How do we run the unzip command?
# All log files are stored in compressed format.

@GZIP = ("gzip",);

@GUNZIP = ("gzip", "--uncompress", "--to-stdout",);


# The GNU UUDECODE will use these arguments, Solaris uudecode is
# different. UUDECODE is only used if the build machines send binary
# files inside the build log messages.  This option is probably not
# used by most Tinderbox installations.

@UUDECODE = ("/error/notinstalled/uudecode", "-o",);

# The user/group ids which tinderbox will run as. Hopefully these
# integers are out of the restricted range (bigger is safer, bigger
# then 100 is ideal but bigger than 25 is recommended.).

$TINDERBOX_UID=3310;
$TINDERBOX_GID=3310;
$TINDERBOX_UID=500;
$TINDERBOX_GID=100;

# The url to the tinderbox server binary directory

$URL_BIN = "http://lounge.mozilla.org/cgi-bin/cgiwrap/cgiwrap_exe/tbox";


# The url to the tinderbox server HTML directory

$URL_HTML = "http://lounge.mozilla.org/tinderbox2";
$URL_HTML = "file:///tmp/tinderbox2";

# The full path name tinderbox will use to access the tinderbox
# servers root data directory where the html will be written.

#$TINDERBOX_HTML_DIR = "/home/httpd/html/tinderbox";
$TINDERBOX_HTML_DIR = "/opt/apache/htdocs/tinderbox2";
$TINDERBOX_HTML_DIR = "/tmp/tinderbox2";

# The full path name tinderbox will use to access the tinderbox
# servers root data directory where the data will be written.  For
# debugging you may wish to make this the same as the
# $TINDERBOX_HTML_DIR and set Persistence::Dumper.  This setting will
# allow a browser can look at the internal data structures. For
# production use it is more secure to keep internal tinderbox data
# outside of the HTML tree so that the web server can not send the
# internal data over the network.

#$TINDERBOX_DATA_DIR = "/home/httpd/html/tinderbox";
#$TINDERBOX_DATA_DIR = "/var/spool/tinderbox";
$TINDERBOX_DATA_DIR = "/export2/tbox2-data";
$TINDERBOX_DATA_DIR = "/tmp/tinderbox2/tbox2-data";

# Where to store the compressed HTML converted log files. Typically
# this is either the DATA_DIR or the HTML dir, though it can be
# elsewhere.  This is a directory tinderbox will write to and will use
# lots of space.

#$TINDERBOX_GZLOG_DIR = $TINDERBOX_DATA_DIR;
$TINDERBOX_GZLOG_DIR = $TINDERBOX_HTML_DIR;


# The full path name tinderbox will use to access the tinderbox
# cgi scripts.

$TINDERBOX_CGIBIN_DIR = "/opt/tbox/public_html/cgi-bin/";

# The top level tinderbox index file. Change this if you wish to
# provide your own index file for tinderboxes web pages.

$GLOBAL_INDEX_FILE = "index.html";

# Tinderbox can be run without any images.  Set GIF_URL to null to
# disable images. If you install the traditional tinderbox images on
# your web server then set GIF_URL to the url needed to find the
# images. Images are used for the Notices and in the BuildStatus
# 'header_background_gif'.

$GIF_URL = 'http://lounge.mozilla.org/tinderbox2/gif';
$GIF_URL = 'file:////home/kestes/mozilla/webtools/tinderbox2/src/gif/';


# Error log filename:

$ERROR_LOG = "/var/log/tinderbox2/tinderbox2.log";
$ERROR_LOG = "/tmp/tinderbox2/tinderbox2.log";
  
# Where the daemon mode lock (for all trees) is placed
$LOCK_FILE = $TINDERBOX_HTML_DIR."/tinderd.lock";

# The time between auto refreshes for all pages in seconds.

$REFRESH_TIME = (60 * 15);


# Pick how you wish to the Tinderbox popup windows to be implemented:
# Uncomment only one HTMLPopUp implementation.

# I am moving twards popup libraries which appear "on click" and away
# from the "on mouse over" libraries.  So consider some of these
# libraries deprecated. You may wish to run the test vcdisplay.tst to
# see samples of all the windows.

# Overlib : this library is a bit larger then the rest but I am
#            particularly fond of the portable and supported 
#	     windows it creates.
# MozillaClick: Should be bortable to all browsers written my the
#		netscape GUI guru <mgalli@netscape.com>
# MajorCoolWindow: Should be portable to all browsers, came from major cool. 
#                  Not recommended, on click seems to redirect.
# None:         A null implementation which will not use any popups
#               provide no popup windows. Use this if you do not run
#               JavaScript in your browsers.
 
$PopUpImpl = (

              'HTMLPopUp::OverLib',
	      # 'HTMLPopUp::MajorCoolWindow',
	      # 'HTMLPopUp::MozillaClick',
	      # 'HTMLPopUp::None',

              # ----- deprecated mouseover libraries -----

	      # 'HTMLPopUp::MozillaLayers',
              #'HTMLPopUp::MajorCoolPermanent',
	      # 'HTMLPopUp::PortableLayers',
	     );


# Use the DB implementations you wish to use.

# These uses determine the columns of the build page and their
# include order is the order in which the columns are displayed.

# The time Column can occur more then once if you like.


# The main choice of implementations is how to gather information
# about checkins.  You can currently choose whether you are using
# bonsai or are using CVS raw.

@DBImpl = (
	   'TinderDB::Time',

           # If you development spans multiple time zones you may wish
           # to display a time other then the time on the server. More
           # then one time column can be included if you
           # desire. Time_UTC displays the UTC time while Time_Local
           # will use JavaScript to compute the correct time for each
           # browser.

	   # 'TinderDB::Time_Local',
	   # 'TinderDB::Time_UTC',

           # The notice board is a special column, you may not wish to
           # include it. If you do not have a notice column you can
           # still have notices appear, they will just appear in the
           # other columns.

	   'TinderDB::Notice',

           # version control systems

	   'TinderDB::VC_CVS',
#	   'TinderDB::VC_Bonsai',
#          'TinderDB::VC_Perforce',

	   'TinderDB::Build',

           # Bug Tracking systems

#	   'TinderDB::BT_Generic',
#	   'TinderDB::BT_Req',

           # You may wish to have the time column on both sides of the
           # table.

	   # 'TinderDB::Time',
	  );

# What border should the status legends use?  new browsers allow us to
# frame the parts of the legend without putting a border around the
# individual cells.

#$DB_LEGEND_BORDER = "border rules=none";
$DB_LEGEND_BORDER = "";

# Should the vector of times, which represent the rows use a uniform
# spacing or should we put one row for each time we have data for.  It
# is recommended to set this to uniform vertical length anywhere on the
# table corresponds to the same amount of elapsed time.

$ROW_SPACING_DISIPLINE = (

                          # Use round numbers spaced 5 minutes apart.
                          # This keeps the cell box sizes independent
                          # of the amount of data.

                          #'uniform',

                          # Use build event times to create times column.
                          # This is traditional Tinderbox1 discipline.

                          'build_event_driven',

                          # Use all event times to create times column.

                          #'event_driven',

                          );

# Spacing on html page (in minutes), this restricts the
# minimum time between builds (to this value plus 5 minutes).
# I suggest 5 minutes.

$DB_TABLE_SPACING = 5;

# Number of times a database can be updated before its contents must
# be trimmed of old data.  This scan of the database is used to
# collect data about average build time so we do not want it
# performed too infrequently.

$DB_MAX_UPDATES_SINCE_TRIM = 50;

# Number of seconds to keep in Database, older data will be trimmed
# away and lost.

$DB_TRIM_SECONDS = (60 * 60 * 24 * 8);

# Enforce clock synchronization on the client machines.  Reject data
# which has been sent to the web server and the time stamp and current
# time are out side of the bounds.

# set this to determine the maximum transit time for client data.

$SECONDS_AGO_ACCEPTABLE = (60 * 60 * 10);

# set this to zero to enforce the client machines never having a
# faster clock then the server machine.

$SECONDS_FROM_NOW_ACCEPTABLE = (60 * 10);

# Setting this variable to true will enable extra characters in the
# tinderbox output which are the same color as the background color.
# This is helpful for users of text based browsers, since text based
# browsers can not render cell colors additional information needs to
# be encoded into the HTML page.  Some users may object to the use of
# extra and perhaps unneeded characters in an already wide table.  It
# is recommended to set this to 1 so that users can use a text browser
# if they choose.

$ADD_TEXT_BROWSER_STRINGS = 0;


@HeaderImpl = (
	       'TinderHeader::Build',
	       'TinderHeader::IgnoreBuilds',
	       'TinderHeader::MOTD',

	       # Specify how we set/view a global development
	       # state. Only uncomment one TinderHeader::TreeState.

               # Most VC implementations will not have a State file in
               # the version control system so use the generic version
               # if you do not use bonsai.

	       'TinderHeader::TreeState',

               # Get states from Bonsai tool and set the Bonsai states via
               # Tinderbox admin page.

#	       'TinderHeader::TreeState_Bonsai',

               # Get states from bonsai tool, do not set Bonsai states
               # via Tinderbox, but also allow new states which are
               # known only to Tinderbox which can be set on our pages
               # and which override Bonsai for Tinderboxes use.

#	       'TinderHeader::TreeState_Bonsai_Plus',

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
# 'None'. I would be very interested in a module to work with this
# bonsai work alike tool http://viewcvs.sourceforge.net/ but I do not
# have time not to investigate it.

$VCDisplayImpl = (
		  'VCDisplay::None',
		  #'VCDisplay::Bonsai',
		  #'VCDisplay::Perforce_P4DB',
		 );

# The name of the version control system as it should appear on the
# column heading.

#$VC_NAME = "CVS";
$VC_NAME = "Guilty";

# a regular expression to find bug ticket numbers in checkin comments.

$VC_BUGNUM_REGEXP = '(\d\d\d+)';

# Pick one method for storing data, Data::Dumper is slow but text
# files allows great debugging capabilities and Storable, available
# from CPAN, which is a much faster binary format.

# If you are worried about security you should use Storable because
# Dumper uses and Eval to load the new code it is conceivable that the
# code could be forced to perform unwanted actions.

$PersistenceImpl = (
                    #'Persistence::Dumper',
                    'Persistence::Storable',
                   );

# Do you wish the main status page to display the number of errors
# which the error parser found in the build logs? Use zero for no one
# for yes.

$DISPLAY_BUILD_ERRORS = 1;

# If you your using one of the VCDisplay modules we need to know how
# to make HTML to point to the bonsai CGI programs. We do not need to
# have this link point to the same webserver as tinderbox.

$BONSAI_URL = "http://bonsai.mozilla.org/";

$P4DB_URL =  "http://public.perforce.com/cgi-bin/p4db";

# If we query bonsai data (e.g., DBImpl above is set to Bonsai), we
# need to know the directory which bonsai is installed in. Tinderbox
# needs to be able to see the bonsai directories to get bonsai checkin
# and treestate data.

#$BONSAI_DIR = "/home/httpd/cgi-bin/bonsai";
$BONSAI_DIR = "/opt/apache/htdocs/webtools/bonsai";


# If you your using BT_Generic we need to know how to make HTML
# to point to the bug tracking CGI programs.

$BT_URL	= 'http://bugzilla.mozilla.org/';

# The name of the bug tracking system as it should appear on the
# column heading.

$BT_NAME = "Bugzilla";

# The default number of hours shown on the status page

$DEFAULT_DISPLAY_HOURS = 6;

# The default page for a tree, used in several types of href links to
# 'return to the tree'.

#$DEFAULT_HTML_PAGE = 'index.html';
$DEFAULT_HTML_PAGE = 'status.html';

# The indicator that a notice is available for a given notice cell is
# configurable.  Traditionally it is a star GIF however if you wish to
# run entirely without images I suggest you set it to "X" or "*".
# This is used in TinderDB::Notice.pm

#$NOTICE_AVAILABLE = "X";
$NOTICE_AVAILABLE = "<img src='$GIF_URL/star1.gif' border=0>";


# The amount of time rmlogs keeps logs on file

$BRIEF_LOG_TRIM_DAYS = 8;
$FULL_LOG_TRIM_DAYS = 8;

# Should we write performance data to the log file?
# zero means no, one means yes.
# This is useful to see if on average the time it takes to create the
# pages is longer then the time between runs of the pages.


$LOG_PERFORMANCE = 0;

# Define IP addresses/domain names which are allowed to run the
# administrative functions.  If set to '.*' then anyone who knows the
# password can administrate the tree, if set to a network pattern then
# users must run their browser on the correct IP address and know the
# correct password to administrate the tree.

#$ADMINISTRATIVE_NETWORK_PAT = ( 
#                                '(^127\.0\.0\.[0-9\.]*$)|'.
#                                '(^10\.10\.[0-9\.]*$)|'.
#                                '(^207\.200\.81\.[0-9\.]*)$|'.
#                                '(^172\.24\.127\.[0-9\.]*$)|'.
#                                '(\.mozilla\.org$)|'.
#                                '(\.netscape\.com$)|'.
#                                '(^localhost$)'
#                              );

$ADMINISTRATIVE_NETWORK_PAT = '.*';
 

1;
