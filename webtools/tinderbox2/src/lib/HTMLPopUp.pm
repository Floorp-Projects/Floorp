# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTML.pm - a lightweight replacement for cgi.pm and general html/cgi
# processing.  This provides popup windows and a more highly
# structured interface for linking then CGI.pm does.  The popup window
# code is easily changed to use different technology if needed.  The
# parts of tinderbox which need user input via forms all use the
# standard perl library CGI.pm.

# Each function here builds an $out string.  If there are bugs in the
# code you can put your breakpoint on the return statement and look at
# the completed string before it is returned.


# $Revision: 1.5 $ 
# $Date: 2000/11/09 19:30:45 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/HTMLPopUp.pm,v $ 
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




package HTMLPopUp;

# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use Utils;

# Pick how you wish to the Tinderbox popup windows to be implemented:
# Uncomment only one HTMLPopUp implementation.

$IMPLS = ( ($TinderConfig::PopUpImpl) ||
           (
            #          'HTMLPopUp::MozillaLayers',
			'HTMLPopUp::MajorCoolWindow',
            #          'HTMLPopUp::None',
           )
         );

main::require_modules($IMPLS);


# The code at these sites look interesting.  Perhaps I will add them as
# an alternative implementation someday.


# 1) http://www.webreference.com/dhtml/hiermenus/

# 2) Top Navigational Bar II
#    from Dynamic Drive DHTML code library (http://dynamicdrive.com)
#    including navbar.js and dhtmllib.js
#    Copyright 1999 by Mike Hall.
#    Web address: http://www.brainjar.com



$VERSION = '#tinder_version#';


# It is sometimes useful for debugging to set $EMPTY_TABLE_CELL to XXX
# to make the empty cells visible and to ensure all emtpy cells use
# this variable to define them, otherwise we run the risk of having
# empty cells with nothing at all in them.

# If there are only spaces or there is nothing in the cell then the
# browser prints nothing, not even a color.  It prints a color if you
# use '<br>' as the cell contents. However a ' ' inside <pre> should
# be more portable BUT this causes the cell hights to get bigger.  The
# HTML standard says to use "&nbsp;" to signify an empty cell but I
# find this hard to ready so I set a variable.

#  $EMPTY_TABLE_CELL = "XXX";
  $EMPTY_TABLE_CELL = "&nbsp;";

# default settings for the popup_windows in HTML.pm

$DEFAULT_POPUP_TITLE = '';

# these varaibles are a hack, I need them to make the popup window
# auto size correctly.  I wish there was a javascript way to autosize.

$DEFAULT_POPUP_HEIGHT = 150;
$DEFAULT_POPUP_WIDTH = 350;



#-----------------------------------------------------------
# You should not need to configure anything below this line
#-----------------------------------------------------------



# Turn a time in 'time() format' into a string suitable for html
# printing. eg '05/31&nbsp;14:59'

sub timeHTML {
  my ($t) = @_;
  my ($sec,$minute,$hour,$mday,$mon,$ignore) = localtime($t);
  my ($out) = sprintf("%02d/%02d&nbsp;%02d:%02d",$mon+1,$mday,$hour,$minute);
  return $out;
}


# primative parsing of CGI arguments into a hash.

# These argments are desgined to be pased to the cgi script via the
# webserver.  The can also be passed via command line interface
# ('name=value') for testing.


sub split_cgi_args {

  my (%form) = ();

  # store users email address in a cookie so that they do not have to
  # type it into every notice form.

  %COOKIE_JAR = ();

  ($ENV{'HTTP_COOKIE'}) &&
    ( %COOKIE_JAR = split('[;=] *',$ENV{'HTTP_COOKIE'}) );

  my ($str) = $ENV{"QUERY_STRING"};
  
  if ($ENV{"REQUEST_METHOD"} eq 'POST') {
    # slurp whole file
    my ($old_irs) = $/;
    undef $/;

    my (@str) = <>;

    $/ = $old_irs;

    $str = "@str";
  }
  my ($s) = unescapeURL($str);
  $s =~ tr/+/ /;

  if ($s) {

    # run with CGI arguments
    
    for $pair (split(/[&;]/, $str )) {
      my ($key, $value) = split(/=/, $pair,2);
      $form{$key} = $value;
    }
    
    # if we are being run by a webserver we are not in daemon_mode.
    
    delete ($form{'daemon-mode'});

  } else {

    # run with argv arguments

    foreach $pair (@ARGV) {
      $pair =~ s/^(-)+//;
      my ($key, $value) = split(/=/, $pair, 2);
      $form{$key} = $value;
    }

  }

  return %form;
}






# the functions:
#
#     sub escapeHTML {
#     sub unescapeHTML {
#     sub unescapeURL {
#     sub escapeURL {
#     sub parse_params {

# are taken directly from ./lib/perl5/CGI.pm
# $CGI::revision = '\$\I\d\: CGI.pm,v 1.1.1.1 1998/10/08 20:23:02 lstein Exp $';
# $CGI::VERSION='2.43';

# I added the URL to the end of the functions named: escapeURL,
# unescapeURL for clarity.

# I modified escapeHTML() to escape the "'" as well so that my popup
# windows code do not see a "'" in their input as this confuses them.


# Escape HTML 
sub escapeHTML {
    my($self,$toencode) = @_;
    $toencode = $self unless ref($self);
    return undef unless defined($toencode);
    return $toencode if ref($self) && $self->{'dontescape'};

    $toencode=~s/&/&amp;/g;
    $toencode=~s/\"/&quot;/g;
    $toencode=~s/\'/&\#039;/g;
    $toencode=~s/>/&gt;/g;
    $toencode=~s/</&lt;/g;
    return $toencode;
}

# unescape HTML 
sub unescapeHTML {
    my $string = ref($_[0]) ? $_[1] : $_[0];
    return undef unless defined($string);
    # thanks to Randal Schwartz for the correct solution to this one
    $string=~ s[&(.*?);]{
	local $_ = $1;
	/^amp$/i	? "&" :
	/^quot$/i	? '"' :
        /^gt$/i		? ">" :
	/^lt$/i		? "<" :
	/^#(\d+)$/	? chr($1) :
	/^#x([0-9a-f]+)$/i ? chr(hex($1)) :
	$_
	}gex;
    return $string;
}

# unescape URL-encoded data
sub unescapeURL {
    shift() if ref($_[0]);
    my $todecode = shift;
    return undef unless defined($todecode);
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

# URL-encode data
sub escapeURL {
    shift() if ref($_[0]) || $_[0] eq $DefaultClass;
    my $toencode = shift;
    return undef unless defined($toencode);
    $toencode=~s/([^a-zA-Z0-9_.-])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}


sub parse_params {
    my($self,$tosplit) = @_;
    my(@pairs) = split(/[&;]/,$tosplit);
    my($param,$value);
    foreach (@pairs) {
	($param,$value) = split('=',$_,2);
	$param = unescapeURL($param);
	$value = unescapeURL($value);
	push (@PARAM,$value);
    }
}

1;


# call like this
#
#    page_header(
#                'title'=>""
#                'refresh'=>""
#               );
#
# to return a string which will create page headers for the html page




# call the function like this 
#
# Link(
#	  "statuslinetxt"=>"", 
#	  "windowtxt"=>"", 
#	  "linktxt"=>"", 
#	  "name"=>"", 
#	  "href"=>"",
#
# (arguments with defaults)
#
#	  "windowtitle"=>"", 
#	  "windowheight"=>"", 
#	  "windowwidth"=>"",
#	 );

# to return a string which will create a html link with an associated
# popup window and name tag.



# defines java strucutres (string tables) in the html page after all
# the link calls have been completed.

# define_structures()

