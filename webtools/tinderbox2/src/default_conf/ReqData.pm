# -*- Mode: perl; indent-tabs-mode: nil -*-

# ReqData.pm - the configuration file which describes the local
# configuration for the Req Bug Tracking system
# (http://www.draga.com/~jwise/minireq/) and its relationship to the
# tinderbox trees.


# $Revision: 1.5 $ 
# $Date: 2003/08/17 02:14:31 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/default_conf/ReqData.pm,v $ 
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
# this bug.  If the users wishes more information they may click on
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



package ReqData;

# This package must not use any tinderbox specific libraries.  It is
# intended to be a base class.


$VERSION = '#tinder_version#';



$REQ_URL = ($TinderConfig::REQ_URL || 
           'http://buildweb.reefedge.com/cgi-bin/req');

$REQ_HOME = ($TinderConfig::REQ_HOME || 
	     "/home/req");

$REQ_URL = "http://buildweb.reefedge.com/cgi-bin/";


$REQ_HOME = "/home/req";

# the name of the bug tracking field which shows bug_id

$BUGID_FIELD_NAME = 'Ticket_Num';

$STATUS_FIELD_NAME = 'Action';


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
		    'commented' => 'Progress',
		    'created' => 'Progress',
		    'given' => 'Progress',
		    'killed' => 'Progress',
		    'notified' => 'Progress',
		    'opened' => 'Progress',
		    'resolved' => 'Progress',
		    'stalled' => 'Progress',
		    'subject_changed' => 'Progress',
		    'taken' => 'Progress',
		    'untaken' => 'Progress',
		    'user_set' => 'Progress',
		    );


# Uncomment only the fields you wish displayed in the popup window,
# The fields will be displayed in the order they are listed here.
# Only uncomment fields which are interesting. Fields which are empty
# will still be displayed.

@DISPLAY_FIELDS = (
		   'Subject',
		   'Ticket_Num',
		   'Complete_Action',
		   'Author',
		   );


# turn a tree name into a Req queue name.

sub tree2queue {
    my ($tree_name) = @_;

    my $queue_name = lc($tree_name);
    $queue_name =~ s/^b-//; 

    return $queue_name;
}


# turn a tree name into the name of a its file.

sub tree2logfiles {
    my ($tree_name) = @_;
    
    my $queue_name = tree2queue($tree_name);

    my @req_logs;
    if ($tree_name eq 'ALL') {
        @req_logs = glob "$REQ_HOME/releng-*/etc/req-log";
        @req_logs =   map { main::extract_filename_chars($_) } @req_logs;
    }else{
        @req_logs = ("$REQ_HOME/releng-${queue_name}/etc/req-log");
    }

    return @req_logs; 
}


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

sub update2tree {
  my ($tinderbox_ref) = @_;

  my ($out);

  my @out = (
             'ALL', 
             $tinderbox_ref->{'Tree'},
             );
  
  # It might be a good idea to call TreeData::tree_exists() and ensure
  # that this tree is valid, but this would make it harder for testing
  # using genbugs.
  
  return (@out);
}





# Given a bug id  return a URL ('href') to the bug. 
# If the bug tracker does not support URL's to a bug number,
# return a 'mailto: ' to someone who cares about the bug.

sub bug_id2bug_url {
  my ($tinderbox_ref) = @_;


  $url = (
	  $REQ_URL.
	  "/".
	  $tinderbox_ref->{'Queue'}.
	  "-req/req.cgi/show/".
	  $tinderbox_ref->{'Ticket_Num'}.
	  "");

  return $url;
}


sub get_all_progress_states {

  my (@progress_states) = main::uniq( values %BTData::STATUS_PROGRESS );

  # If the first element is null ignore it.
  ($progress_states[0]) ||
    (shift @progress_states);

  return @progress_states;
}


sub is_status_valid {
  my ($status) = @_;


  # hard code for now
  return 1;

  my $out = defined($STATUS_PROGRESS{$status});

  return $out;
}

1;

