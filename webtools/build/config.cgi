#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

push @INC, '../bonsai';
require 'utils.pl';

use CGI;

$query = new CGI;

$field_separator = "<<fs>>";


$root = pickDefaultRepository();

validateRepository($CVS_ROOT);

open(OPTIONS, "m4 webify-configure.m4 configure.in|")
  or die "Error parsing configure.in\n";

print "Content-type: text/html\n\n<HTML>\n";
print "CVS_ROOT=$root\n";
print qq(
<HEAD>
<TITLE>Configure Unix Mozilla build</TITLE>
</HEAD>
<body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
);

if ($query->param()) {
  foreach $param ($query->param()) {
    print "$param=".$query->param($param)."<BR>\n";
  }
} else {
  print qq(
	   <FORM action='setup.cgi' method='get'>
	   <table>);

  foreach (<OPTIONS>) {
    chomp;
    ($option_type, $option_name, $option_comment) = split /$field_separator/;
    ($dummy,$dummy2,$help) = split /\s+/, $option_comment, 3;
    $help =~ s/\\\$/\$/g;

    if ($option_type eq 'enable') {
      bool_option("--enable-$option_name", $help);
    }
    elsif ($option_type eq 'disable') {
      bool_option("--disable-$option_name", $help);
    } 
    elsif ($option_type eq 'enable_string') {
      string_option("--enable-$option_name", $help);
    }
    elsif ($option_type eq 'with_string') {
      string_option("--with-$option_name", $help);
    }

  }

  print qq(
	   </table>
	   <input type="Submit">
	   </form>
	  );
}

print "\n</body>\n</html>\n";

sub bool_option {
  my ($name, $help) = @_;

  print "<tr><td align=right>";
  print "<INPUT type='checkbox' name='$name'>";
  print "</td><td>$name";
  print "</td><td>$help</td></tr>\n";
}

sub string_option {
  my ($name, $help) = @_;

  print "<tr><td align=right>$name=</td><td align=left>";
  print "<INPUT type='text' name='$name'>";
  print "</td><td>$help</td></tr>\n";
}
