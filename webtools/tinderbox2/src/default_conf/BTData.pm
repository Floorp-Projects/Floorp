# -*- Mode: perl; indent-tabs-mode: nil -*-

# BTData.pm - the configuration file which describes the local Bug
# Tracking system and its relationship to the tinderbox trees.


# $Revision: 1.13 $ 
# $Date: 2003/08/17 02:13:16 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/default_conf/BTData.pm,v $ 
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


# This package is used for configuring the Generic Bug Tracking system
# module.  I belive that this will handle most bug tracking systems.
# We assume that bugs are stored in a database (each bug has a known
# list of field names) and that a subset of the fields will be of
# interest to the tinderbox users (tinderbox is not the correct place
# to display any sort of long comment field).  Tinderbox will display
# the bug number and a popup window showing relevant fields describing
# this bug.  If the user wishes more information they may click on
# the link and be taken directly to the bugtracking system for more
# information about the bug.  Bugs have their state change as the bug
# is worked on and users of tinderbox wish to see information about
# the bug as the state changes.  We divide state changes into two
# types: 'Progress', 'Slippage'. Most important is to get a feel for
# the number of bugs which move into bad/backward states ('REOPENED').

# Users will certainly need to configure the tables in this module for
# their needs. Additionally users need to define how to convert
# information about each bug to the correct tinderbox tree that this
# bug belongs.  This is handled by defining the update2tree() function
# as appropriate.



package BTData;

# This package must not use any tinderbox specific libraries.  It is
# intended to be a base class.


$VERSION = '#tinder_version#';



# a URL to the bug tracking systems main page.

$BT_URL	= ($TinderConfig::BT_URL || 
           'http://bugzilla.mozilla.org/');


# Ticket variable names match this pattern

# All variables seem to start with a capital letter.

# Note: there are some standard bugzilla variables with unusual
# characters in them:
#
#       Bug#: 1999
#       OS/Version: 

# Some AIM variable names have spaces ' ' in them, we will convert
# these into underscores '_' after the mail is parsed.

$VAR_PATTERN = '[A-Z][a-zA-Z0-9._/ \#\-]*'; 



# the name of the bug tracking field which shows bug_id

#$BUGID_FIELD_NAME = 'Ticket_#';
$BUGID_FIELD_NAME = 'Bug#';


# the name of the bug tracking field which shows progress/slippage.

$STATUS_FIELD_NAME = 'Status';


# The values of the status field wich denote that the ticket is moving
# forward.  Notice that this list may not be complete as we are only
# interested in displaying Developer progress. If the ticket is moving
# through QA tinderbox may not be the correct place to see that
# change.  In particular newly opend tickets are not particularly
# interesting when monitoring the development process.

# All status values are converted to lower case for ease of
# processing.  Each value of this table corresponds to a bug column in
# the tinderbox status page. You may have as many bug columns as you
# like.  If you wish to indicate that certain states are possible but
# should not be displayed then indicate the state with a null string.

%STATUS_PROGRESS = (
		    'ASSIGNED' => 'Progress',

                    # QA action states

                    'RESOLVED' => 'Progress',
		    'VERIFIED' => 'Progress',
                    'CLOSED' => 'Progress',
                    
                    # Developer action states

                    'FIXED' => 'Progress',
                    'INVALID' => 'Progress',
                    'WONTFIX' => 'Progress',
                    'LATER' => 'Progress',
                    'REMIND' => 'Progress',
                    'DUPLICATE' => 'Progress',
                    'WORKSFORME' => 'Progress',

		    'REOPENED' => 'Slippage',
		    'FAILED' => 'Slippage',
		    'OPENED' => 'Slippage',
		    'NEW' => 'Slippage',
		   );







# Uncomment only the fields you wish displayed in the popup window,
# The fields will be displayed in the order they are listed here.
# Only uncomment fields which are interesting. Fields which are empty
# will still be displayed.

@DISPLAY_FIELDS = (

		   # Tinderbox created fields
		   
		   #'tinderbox_status',
		   #'tinderbox_bug_id',
		   #'tinderbox_tree',
		   #'tinderbox_bug_url',
		   
		   # Bugzilla fields
		   
		   'Bug#',
		   'Product',
		   'Version',
		   'Platform',
		   'OS/Version',
		   'Status',
		   'Resolution',
		   'Severity',
		   'Priority',
		   'Component',
		   'AssignedTo',
		   'ReportedBy',
		   #'URL',
		   #'Cc',
		   'Summary',
		   
		   
		   # AIM Fields 

		   # (these names are configurable and come from the
		   #  appearance of mail messages. We always convert
		   #  spaces to '_'.)

		   
#		   'Ticket_#',
#		   'Date_Open',
#		   'Product_Name',
#		   'Product_Sub_System',
#		   'Product_Version',
#		   'Severity',
#		   'Status',
#		   'Closed_Date',
#		   'User_Login',
#		   'E-Mail',
#		   'Support_Staff_Login',
#		   'Support_Staff_E-Mail',
#		   'Last_Modified_Date',
#		   'Short_Description',

		  );


# Given a pointer to a bug update hash, return the name of the tree to
# which this bug report belongs.  Typically this will be the contents
# of a field like 'Product', (if you have one tinderbox page for each
# product in your bug database) however some projects may be more
# compicated.  

# One example of a complex function to determine tree name would be if
# each of the product product types listed in the bug tracking data
# base refers to one development project, except for a particular
# feature/platform of one particular project which is being developed
# by a separate group of developers.  So the version control notion of
# trees (a set of modules on a branch) may not have a direct map into
# the bug tracking database at all times.

# This function should return the null list '()' if the bug report
# should be ignored by the tinderbox server.  The function returns a
# list of trees which should display the data about this bug update.
# Any trees which are not known by the tinderbox server will be
# ignored so it is fine to return a tree for every ticket parsed even
# if some tickets do not map to tinderbox trees.
# It is cleaner to ignore trees we do not know about in the mail processor 
# then to make the BTData module depend on the TreeData module.

sub update2tree {
  my ($tinderbox_ref) = @_;

  my ($out);

  $out = (
	  $tinderbox_ref->{'Product'}.
	  "");

  # It might be a good idea to call TreeData::tree_exists() and ensure
  # that this tree is valid, but this would make it harder for testing
  # using genbugs.

  return ($out);
}



# It would be great if all bug tracking systems allowed a simple
# conversion from bug id to url.  I doubt this will happen. Where it
# makes sense though, let other modules peek at this.
# If the bug tracker does not support URL's to a bug number,
# return a 'mailto: ' to someone who cares about the bug.

sub bug_id2bug_url {
  my ($bug_id) = @_;

  my ($out);

  # AIM can not accept bug numbers as URLS without encoding lots of
  # other junk (logged in user name, session id info). Just give a
  # 'mailto: ' for the link instead.

  #  $out = (
  #	  'mailto: '.
  #	  $tinderbox_ref{'User_Login'}.
  #	  "");

  # Bugzilla has an easy format for making URL's of bugs.

  $out = (
          $BT_URL.
          '/show_bug.cgi?id='.
	  $bug_id.
	  "");

  return $out;
}


# Given a bug record  return a URL ('href') to the bug. 
# this might work for a larger collection of trackers then the above.

sub rec2bug_url {
  my ($tinderbox_ref) = @_;
  my ($bug_id) = $tinderbox_ref->{$BTData::BUGID_FIELD_NAME};

  my ($out);

  $out = bug_id2bug_url($bug_id);

  return $out;
}


sub get_all_columns {

  my (@columns) = main::uniq( values %BTData::STATUS_PROGRESS );

  # If the first element is null ignore it.
  ($columns[0]) ||
    (shift @columns);

  return @columns;
}


sub is_status_valid {
  my ($status) = @_;

  my $out = defined($STATUS_PROGRESS{$status});

  return $out;
}

1;
