# -*- Mode: perl; indent-tabs-mode: nil -*-

# VCDisplay.pm - describes how tinderbox should prepeare URLS to
# show the source tree allow VC queries.  Currently there are two
# implementations one for users of the bonsai system (cvsquery,
# cvsguess, cvsblame) and one for users who have no web based access
# to their version control repository.  In the future I will make a
# VCDisplay module for CVSWeb.


# $Revision: 1.7 $ 
# $Date: 2002/12/10 19:35:50 $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
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
