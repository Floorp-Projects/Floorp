# -*- Mode: perl; indent-tabs-mode: nil -*-

# VCDisplay.pm - describes how tinderbox should prepeare URLS to
# show the source tree allow VC queries.  Currently there are two
# implementations one for users of the bonsai system (cvsquery,
# cvsguess, cvsblame) and one for users who have no web based access
# to their version control repository.  In the future I will make a
# VCDisplay module for CVSWeb.


# $Revision: 1.10 $ 
# $Date: 2003/08/17 01:44:07 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/VCDisplay.pm,v $ 
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




package VCDisplay;


# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use Utils;


# pick the VCDisplay module that you wish to use. 

$IMPLS = ( ($TinderConfig::VCDisplayImpl) ||
           (
            #'VCDisplay::None',
            'VCDisplay::Bonsai',
           )
         );

main::require_modules($IMPLS);


@ISA=($IMPLS);

$VCDisplay = VCDisplay->new();

$DEBUG = 1;



# The function query creates HTTP links for SQL queries to the Version
# Control (VC) system.  The protype system that this works for is
# BONSAI the netscape database which is fed data from CVS.  The
# arguments to query are:

#    %args = (
#	     'tree' => Which VC tree this query applies to 
#	     'mindate' => The oldest time which should be considered
#	     'maxdate' => The most recent time which should be considered
#	     'who' => The VC name of the individual who checked in the change
#	     'linktxt' => The text to display on the link to the query
#	     );
    

# The function guess creates HTTP links to the current version of a
# particular file at a specified line number.  This function is used
# when only the basename of the file is provided and some additional
# work must be done to figure out the file which was meant.  Most
# compilers only give the basename in their error messages and leave
# the determination of the directory to the user who is looking at the
# error log.

# arguments to guess are:

#    %args = (
#	     'tree' => Which VC tree this query applies to 
#	     'file' => The basename of the file to be displayed
#	     'line' => The line number of the file which is of interest, 
#                      some context above and below this line will be shown.
#	     'linktxt' => The text to display on the link to the query
#	     'alt_linktxt' => Alternative text to display if there is no web 
#				access to the VC system
#	     );
    

# The function source is used instead of guess when the full filename
# is know.  The arguments to guess and source are identical.  There is
# no code in tinderbox which uses this function.

sub new {

  my $type = shift;
  my %params = @_;
  my $self = {};
  bless $self, $type;
}

# call the implemenation defined functions, OO notionation looks
# peculiar for this package so we do this instead.

sub source {
    return $VCDisplay->SUPER::source(@_);
}

sub guess {
    return $VCDisplay->SUPER::guess(@_);
}

sub query {
    return $VCDisplay->SUPER::query(@_);
}



1;
