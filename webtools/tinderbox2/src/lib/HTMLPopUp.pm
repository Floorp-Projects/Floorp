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


# $Revision: 1.26 $ 
# $Date: 2003/08/16 18:29:00 $ 
# $Author: kestes%walrus.com $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 




package HTMLPopUp;

# Load standard perl libraries


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use Utils;
use FileStructure;

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

@ISA=($IMPLS);

$POPUP = HTMLPopUp->new();

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

$EMPTY_TABLE_CELL ="&nbsp;";

# default settings for the popup_windows in HTML.pm

$DEFAULT_POPUP_TITLE = '';

# these varaibles are a hack, I need them to make the popup window
# auto size correctly.  I wish there was a javascript way to autosize.

$DEFAULT_POPUP_HEIGHT = 160;
$DEFAULT_POPUP_WIDTH = 225;

if (defined($TinderConfig::ADD_TEXT_BROWSER_STRINGS)) {
    $ADD_TEXT_BROWSER_STRINGS = $TinderConfig::ADD_TEXT_BROWSER_STRINGS;
} else {
    $ADD_TEXT_BROWSER_STRINGS = 1;
}

#-----------------------------------------------------------
# You should not need to configure anything below this line
#-----------------------------------------------------------

sub new {

  my $type = shift;
  my %params = @_;
  my $self = {};
  bless $self, $type;
}



# People who use the text browser 'links'
# (http://artax.karlin.mff.cuni.cz/~mikulas/links/) would like to
# see colors in the tinderbox table cells. Links will not render
# background colors but it will render foreground colors. So we
# add characters in the same color as the background just for
# these browsers, others will not see these characters because
# they will disapear into the background color.

sub text_browser_color_string {
    my ($cell_color, $char) = @_;

    if (!($ADD_TEXT_BROWSER_STRINGS)) {
        return "";
    }

    my $cell_options;
    if ( $cell_color ) {
        $cell_options = "color=$cell_color";
    }

    my $out = (
                
                "<font $cell_options>".
                "$char".
                "</font>".
                
                "");

    return $out;
}


# Turn a time in 'time() format' into a string suitable for html
# printing. eg '05/31&nbsp;14:59'

sub timeHTML {
  my ($t) = @_;
  my ($sec,$minute,$hour,$mday,$mon,$ignore) = localtime($t);
  my ($out) = sprintf("%02d/%02d&nbsp;%02d:%02d",$mon+1,$mday,$hour,$minute);
  return $out;
}

# Turn a time in 'time() format' into a string suitable for html
# printing using the GMT timezone. eg '05/31&nbsp;14:59'

sub gmtimeHTML {
  my ($t) = @_;
  my ($sec,$minute,$hour,$mday,$mon,$ignore) = gmtime($t);
  my ($out) = sprintf("%02d/%02d&nbsp;%02d:%02d",$mon+1,$mday,$hour,$minute);
  return $out;
}


# primative parsing of CGI arguments into a hash.

# These argments are desgined to be pased to the cgi script via the
# webserver.  The can also be passed via command line interface
# ('name=value') for testing.


sub split_cgi_args {
    my %args = @_;
    my @cgi_remove_arg = @{ $args{'cgi_remove_args'} };


  my (%form) = ();

  # store users email address in a cookie so that they do not have to
  # type it into every notice form.

  %COOKIE_JAR = ();

  ($ENV{'HTTP_COOKIE'}) &&
    ( %COOKIE_JAR = split('[;=] *',$ENV{'HTTP_COOKIE'}) );

  my ($str) = $ENV{"QUERY_STRING"};
  
  if ( 
       ($ENV{"REQUEST_METHOD"}) && 
       ($ENV{"REQUEST_METHOD"} eq 'POST') &&
       1) 
  {
    # slurp whole file
    my ($old_irs) = $/;
    undef $/;

    my (@str) = <>;

    $/ = $old_irs;

    $str = "@str";
  }
  my ($s) = unescapeURL($str);
 if ($s) {
     $s =~ tr/+/ /;
 }

  if ($s) {

    # run with CGI arguments
    
    for $pair (split(/[&;]/, $str )) {
      my ($key, $value) = split(/=/, $pair,2);
      $form{$key} = $value;
    }

    # if we are being run by a webserver some options are not allowed
    # even if the user asks for them.

    if (@cgi_remove_args) {
        delete @form{@cgi_remove_args};
    }
    
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

# Run tinderbox again to regenerate the pages.  I put this function
# here because we need to bypass the security arangements.  Normally
# you can not run --daemon-mode from an arbitrary CGI script.  This
# part of the code (split_cgi_args) is the only part which knows which
# Environmental variables are checked for security.

sub regenerate_HTML_pages {

    # When we get here set_static_vars() should have been run so we
    # will Observe taint-mode for this system call

    # we make sure to delete the HTTP server variables since we do not
    # want our child to believe it was run by a webserver, even if we were.

    my $old_query_string =  $ENV{"QUERY_STRING"};
    my $old_request_method = $ENV{"REQUEST_METHOD"};

    $ENV{"QUERY_STRING"} = '';
    $ENV{"REQUEST_METHOD"} = '';

    system(
           $FileStructure::CGIBIN_DIR.'tinder.cgi', 
           '--daemon-mode',
           );

    $ENV{"QUERY_STRING"} = $old_query_string;
    $ENV{"REQUEST_METHOD"} = $old_request_method;

    return 0;
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

# I put a copy of package CGI::Util; at the bottom of this file so
# that all users can use this library and I do not have to maintain the
# escape code. Not all versions of perl have one and I am trying to keep
# this  code portable to most perl versions.


# Escape HTML 
sub escapeHTML {
    my($self,$toencode) = @_;
    $toencode = $self unless ref($self);
    return undef unless defined($toencode);
    return $toencode if ref($self) && $self->{'dontescape'};

    $toencode=~s/&/&amp;/g;
    $toencode=~s/\"/&quot;/g;
    $toencode=~s/>/&gt;/g;
    $toencode=~s/</&lt;/g;


	# I need to escape the apostrophe because that character is 
	# used as a java script terminator in my code.

	# use an acute accent instead of an apostrophe because netscape 
	# does not get the escape correct and will not render my 
	# popups with the escaped apostrophe any other character seems
	# to work though.  The escape for apostrope is #039. 

    $toencode=~s/\'/&acute;/g;

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
    return CGI::Util_Internal::unescape(@_);
}

# URL-encode data
sub escapeURL {
    return CGI::Util_Internal::escape(@_);
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
#	  "linktxt"=>"text to usually show", 
#         "alt_linktxt" => Alternative text to display if there 
#                           is no popup library installed and the text 
#                           should be different from the normal linktxt
#	  "name"=>"so that other links can point here", 
#	  "href"=>"where this link will go to",
#	  "windowtxt"=>"the contents of the popup window", 
#
# (These are deprecated: arguments with defaults)
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

# call the implemenation defined functions, OO notionation looks
# peculiar for this package so we do this instead.

sub page_header {
    return $POPUP->SUPER::page_header(@_);
}

sub Link {
    return $POPUP->SUPER::Link(@_);
}


sub define_structures {
    return $POPUP->SUPER::define_structures(@_);
}


#-----------------------------------------

# This is just a copy of CGI::Util I changed the package name so that
# it does not cause a conflict for users who have this library.  I do
# not wish to force users to update their perl just to get this
# library.  Only modern CGI.pm comes with this file, modern perl
# includes it.


package CGI::Util_Internal;

use strict;
use vars qw($VERSION @EXPORT_OK @ISA $EBCDIC @A2E @E2A);
#require Exporter;
#@ISA = qw(Exporter);
#@EXPORT_OK = qw(rearrange make_attributes unescape escape expires);

$VERSION = '1.3';

$EBCDIC = "\t" ne "\011";
if ($EBCDIC) {
  # (ord('^') == 95) for codepage 1047 as on os390, vmesa
  @A2E = (
   0,  1,  2,  3, 55, 45, 46, 47, 22,  5, 21, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 60, 61, 50, 38, 24, 25, 63, 39, 28, 29, 30, 31,
  64, 90,127,123, 91,108, 80,125, 77, 93, 92, 78,107, 96, 75, 97,
 240,241,242,243,244,245,246,247,248,249,122, 94, 76,126,110,111,
 124,193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,
 215,216,217,226,227,228,229,230,231,232,233,173,224,189, 95,109,
 121,129,130,131,132,133,134,135,136,137,145,146,147,148,149,150,
 151,152,153,162,163,164,165,166,167,168,169,192, 79,208,161,  7,
  32, 33, 34, 35, 36, 37,  6, 23, 40, 41, 42, 43, 44,  9, 10, 27,
  48, 49, 26, 51, 52, 53, 54,  8, 56, 57, 58, 59,  4, 20, 62,255,
  65,170, 74,177,159,178,106,181,187,180,154,138,176,202,175,188,
 144,143,234,250,190,160,182,179,157,218,155,139,183,184,185,171,
 100,101, 98,102, 99,103,158,104,116,113,114,115,120,117,118,119,
 172,105,237,238,235,239,236,191,128,253,254,251,252,186,174, 89,
  68, 69, 66, 70, 67, 71,156, 72, 84, 81, 82, 83, 88, 85, 86, 87,
 140, 73,205,206,203,207,204,225,112,221,222,219,220,141,142,223
	 );
  @E2A = (
   0,  1,  2,  3,156,  9,134,127,151,141,142, 11, 12, 13, 14, 15,
  16, 17, 18, 19,157, 10,  8,135, 24, 25,146,143, 28, 29, 30, 31,
 128,129,130,131,132,133, 23, 27,136,137,138,139,140,  5,  6,  7,
 144,145, 22,147,148,149,150,  4,152,153,154,155, 20, 21,158, 26,
  32,160,226,228,224,225,227,229,231,241,162, 46, 60, 40, 43,124,
  38,233,234,235,232,237,238,239,236,223, 33, 36, 42, 41, 59, 94,
  45, 47,194,196,192,193,195,197,199,209,166, 44, 37, 95, 62, 63,
 248,201,202,203,200,205,206,207,204, 96, 58, 35, 64, 39, 61, 34,
 216, 97, 98, 99,100,101,102,103,104,105,171,187,240,253,254,177,
 176,106,107,108,109,110,111,112,113,114,170,186,230,184,198,164,
 181,126,115,116,117,118,119,120,121,122,161,191,208, 91,222,174,
 172,163,165,183,169,167,182,188,189,190,221,168,175, 93,180,215,
 123, 65, 66, 67, 68, 69, 70, 71, 72, 73,173,244,246,242,243,245,
 125, 74, 75, 76, 77, 78, 79, 80, 81, 82,185,251,252,249,250,255,
  92,247, 83, 84, 85, 86, 87, 88, 89, 90,178,212,214,210,211,213,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57,179,219,220,217,218,159
	 );
  if (ord('^') == 106) { # as in the BS2000 posix-bc coded character set
     $A2E[91] = 187;   $A2E[92] = 188;  $A2E[94] = 106;  $A2E[96] = 74;
     $A2E[123] = 251;  $A2E[125] = 253; $A2E[126] = 255; $A2E[159] = 95;
     $A2E[162] = 176;  $A2E[166] = 208; $A2E[168] = 121; $A2E[172] = 186;
     $A2E[175] = 161;  $A2E[217] = 224; $A2E[219] = 221; $A2E[221] = 173;
     $A2E[249] = 192;
 
     $E2A[74] = 96;   $E2A[95] = 159;  $E2A[106] = 94;  $E2A[121] = 168;
     $E2A[161] = 175; $E2A[173] = 221; $E2A[176] = 162; $E2A[186] = 172;
     $E2A[187] = 91;  $E2A[188] = 92;  $E2A[192] = 249; $E2A[208] = 166;
     $E2A[221] = 219; $E2A[224] = 217; $E2A[251] = 123; $E2A[253] = 125;
     $E2A[255] = 126;
 }
  elsif (ord('^') == 176) { # as in codepage 037 on os400
     $A2E[10] = 37;  $A2E[91] = 186;  $A2E[93] = 187; $A2E[94] = 176;
     $A2E[133] = 21; $A2E[168] = 189; $A2E[172] = 95; $A2E[221] = 173;
 
     $E2A[21] = 133; $E2A[37] = 10;  $E2A[95] = 172; $E2A[173] = 221;
     $E2A[176] = 94; $E2A[186] = 91; $E2A[187] = 93; $E2A[189] = 168;
   }
}

# Smart rearrangement of parameters to allow named parameter
# calling.  We do the rearangement if:
# the first parameter begins with a -
sub rearrange {
    my($order,@param) = @_;
    return () unless @param;

    if (ref($param[0]) eq 'HASH') {
	@param = %{$param[0]};
    } else {
	return @param 
	    unless (defined($param[0]) && substr($param[0],0,1) eq '-');
    }

    # map parameters into positional indices
    my ($i,%pos);
    $i = 0;
    foreach (@$order) {
	foreach (ref($_) eq 'ARRAY' ? @$_ : $_) { $pos{lc($_)} = $i; }
	$i++;
    }

    my (@result,%leftover);
    $#result = $#$order;  # preextend
    while (@param) {
	my $key = lc(shift(@param));
	$key =~ s/^\-//;
	if (exists $pos{$key}) {
	    $result[$pos{$key}] = shift(@param);
	} else {
	    $leftover{$key} = shift(@param);
	}
    }

    push (@result,make_attributes(\%leftover,1)) if %leftover;
    @result;
}

sub make_attributes {
    my $attr = shift;
    return () unless $attr && ref($attr) && ref($attr) eq 'HASH';
    my $escape = shift || 0;
    my(@att);
    foreach (keys %{$attr}) {
	my($key) = $_;
	$key=~s/^\-//;     # get rid of initial - if present

	# old way: breaks EBCDIC!
	# $key=~tr/A-Z_/a-z-/; # parameters are lower case, use dashes

	($key="\L$key") =~ tr/_/-/; # parameters are lower case, use dashes

	my $value = $escape ? simple_escape($attr->{$_}) : $attr->{$_};
	push(@att,defined($attr->{$_}) ? qq/$key="$value"/ : qq/$key/);
    }
    return @att;
}

sub simple_escape {
  return unless defined(my $toencode = shift);
  $toencode =~ s{&}{&amp;}gso;
  $toencode =~ s{<}{&lt;}gso;
  $toencode =~ s{>}{&gt;}gso;
  $toencode =~ s{\"}{&quot;}gso;
# Doesn't work.  Can't work.  forget it.
#  $toencode =~ s{\x8b}{&#139;}gso;
#  $toencode =~ s{\x9b}{&#155;}gso;
  $toencode;
}

sub utf8_chr ($) {
        my $c = shift(@_);

        if ($c < 0x80) {
                return sprintf("%c", $c);
        } elsif ($c < 0x800) {
                return sprintf("%c%c", 0xc0 | ($c >> 6), 0x80 | ($c & 0x3f));
        } elsif ($c < 0x10000) {
                return sprintf("%c%c%c",
                                           0xe0 |  ($c >> 12),
                                           0x80 | (($c >>  6) & 0x3f),
                                           0x80 | ( $c          & 0x3f));
        } elsif ($c < 0x200000) {
                return sprintf("%c%c%c%c",
                                           0xf0 |  ($c >> 18),
                                           0x80 | (($c >> 12) & 0x3f),
                                           0x80 | (($c >>  6) & 0x3f),
                                           0x80 | ( $c          & 0x3f));
        } elsif ($c < 0x4000000) {
                return sprintf("%c%c%c%c%c",
                                           0xf8 |  ($c >> 24),
                                           0x80 | (($c >> 18) & 0x3f),
                                           0x80 | (($c >> 12) & 0x3f),
                                           0x80 | (($c >>  6) & 0x3f),
                                           0x80 | ( $c          & 0x3f));

        } elsif ($c < 0x80000000) {
                return sprintf("%c%c%c%c%c%c",
                                           0xfe |  ($c >> 30),
                                           0x80 | (($c >> 24) & 0x3f),
                                           0x80 | (($c >> 18) & 0x3f),
                                           0x80 | (($c >> 12) & 0x3f),
                                           0x80 | (($c >> 6)  & 0x3f),
                                           0x80 | ( $c          & 0x3f));
        } else {
                return utf8(0xfffd);
        }
}

# unescape URL-encoded data
sub unescape {
  shift() if @_ > 1 and (ref($_[0]) || (defined $_[1] && $_[0] eq $CGI::DefaultClass));
  my $todecode = shift;
  return undef unless defined($todecode);
  $todecode =~ tr/+/ /;       # pluses become spaces
    $EBCDIC = "\t" ne "\011";
    if ($EBCDIC) {
      $todecode =~ s/%([0-9a-fA-F]{2})/chr $A2E[hex($1)]/ge;
    } else {
      $todecode =~ s/%(?:([0-9a-fA-F]{2})|u([0-9a-fA-F]{4}))/
	defined($1)? chr hex($1) : utf8_chr(hex($2))/ge;
    }
  return $todecode;
}

# URL-encode data
sub escape {
  shift() if @_ > 1 and ( ref($_[0]) || (defined $_[1] && $_[0] eq $CGI::DefaultClass));
  my $toencode = shift;
  return undef unless defined($toencode);
    if ($EBCDIC) {
      $toencode=~s/([^a-zA-Z0-9_.-])/uc sprintf("%%%02x",$E2A[ord($1)])/eg;
    } else {
      $toencode=~s/([^a-zA-Z0-9_.-])/uc sprintf("%%%02x",ord($1))/eg;
    }
  return $toencode;
}

# This internal routine creates date strings suitable for use in
# cookies and HTTP headers.  (They differ, unfortunately.)
# Thanks to Mark Fisher for this.
sub expires {
    my($time,$format) = @_;
    $format ||= 'http';

    my(@MON)=qw/Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec/;
    my(@WDAY) = qw/Sun Mon Tue Wed Thu Fri Sat/;

    # pass through preformatted dates for the sake of expire_calc()
    $time = expire_calc($time);
    return $time unless $time =~ /^\d+$/;

    # make HTTP/cookie date string from GMT'ed time
    # (cookies use '-' as date separator, HTTP uses ' ')
    my($sc) = ' ';
    $sc = '-' if $format eq "cookie";
    my($sec,$min,$hour,$mday,$mon,$year,$wday) = gmtime($time);
    $year += 1900;
    return sprintf("%s, %02d$sc%s$sc%04d %02d:%02d:%02d GMT",
                   $WDAY[$wday],$mday,$MON[$mon],$year,$hour,$min,$sec);
}

# This internal routine creates an expires time exactly some number of
# hours from the current time.  It incorporates modifications from 
# Mark Fisher.
sub expire_calc {
    my($time) = @_;
    my(%mult) = ('s'=>1,
                 'm'=>60,
                 'h'=>60*60,
                 'd'=>60*60*24,
                 'M'=>60*60*24*30,
                 'y'=>60*60*24*365);
    # format for time can be in any of the forms...
    # "now" -- expire immediately
    # "+180s" -- in 180 seconds
    # "+2m" -- in 2 minutes
    # "+12h" -- in 12 hours
    # "+1d"  -- in 1 day
    # "+3M"  -- in 3 months
    # "+2y"  -- in 2 years
    # "-3m"  -- 3 minutes ago(!)
    # If you don't supply one of these forms, we assume you are
    # specifying the date yourself
    my($offset);
    if (!$time || (lc($time) eq 'now')) {
        $offset = 0;
    } elsif ($time=~/^\d+/) {
        return $time;
    } elsif ($time=~/^([+-]?(?:\d+|\d*\.\d*))([mhdMy]?)/) {
        $offset = ($mult{$2} || 1)*$1;
    } else {
        return $time;
    }
    return (time+$offset);
}

1;

__END__

=head1 NAME

CGI::Util - Internal utilities used by CGI module

=head1 SYNOPSIS

none

=head1 DESCRIPTION

no public subroutines

=head1 AUTHOR INFORMATION

Copyright 1995-1998, Lincoln D. Stein.  All rights reserved.  

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Address bug reports and comments to: lstein@cshl.org.  When sending
bug reports, please provide the version of CGI.pm, the version of
Perl, the name and version of your Web server, and the name and
version of the operating system you are using.  If the problem is even
remotely browser dependent, please provide information about the
affected browers as well.

=head1 SEE ALSO

L<CGI>

=cut



1;
