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

use CGI;

$query = new CGI;
$field_separator = "<<fs>>";
$configure_in = "configure.in";
$chrome_color = "#F0A000";
open(OPTIONS, "m4 webify-configure.m4 $configure_in|")
  or die "Error parsing configure.in\n";


if ($query->param()) {
  print "Content-type: text/html\n\n\n";
  print "<pre>";
  foreach $param ($query->param()) {

    if ($param =~ /^MOZ_/) {
      
    } elsif ($param =~ /^--/) {
      next if $query->param($param) eq "";
      print "ac_add_options $param";
      print "=".$query->param($param) if $query->param($param) ne "yes";
      print "\n";
    }
  }
  print "</pre>";
} else {
  print "Content-type: text/html\n\n\n";
  print qq(
	   <HTML>
	   <HEAD>
	   <TITLE>Configure Unix Mozilla build</TITLE>
	   </HEAD>
	   <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
	  );

  print qq(
	   <FORM action='config.cgi' method='POST'>
	   <table bgcolor="$chrome_color" cellspacing=0 cellpadding=0><tr><td>
	   <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=0><tr><td>
	   <table cellspacing=0 cellpadding=1>
);

  foreach (<OPTIONS>) {
    chomp;
    ($type, $prename, $name, $comment) = split /$field_separator/;
    ($dummy,$dummy2,$help) = split /\s+/, $comment, 3;
    #$help =~ s/\\\$/\$/g;

    if ($type eq "header") {
      &header_option($comment);
    } else {
      eval "&${type}_option(\"--$prename-$name\",\"$help\");";
    }
  }

  print qq(
	   </table>
	   </td></tr></table>
	   </td></tr></table>
	   <input type="Submit">
	   </form>
	  );
  print "\n</body>\n</html>\n";
}


sub bool_option {
  my ($name, $help) = @_;

  print qq(<tr><td align=right>
	   <INPUT type='checkbox' name='$name' value="yes">
	   </td><td>$name
	   </td><td>$help</td></tr>
	  );
}

sub string_option {
  my ($name, $help) = @_;

  print "<tr><td align=right>$name=</td><td align=left>";
  print "<INPUT type='text' name='$name'>";
  print "</td><td>$help</td></tr>\n";
}

sub bool_or_string_option {
  my ($name, $help) = @_;

  print "<tr><td align=right>";
  print "<INPUT type='checkbox' name='$name'>";
  print "</td><td>$name";
  print "</td><td>$help</td></tr>\n";
  print "<tr><td align=right>$name=</td><td align=left>";
  print "<INPUT type='text' name='$name'>";
  print "</td><td>$help</td></tr>\n";
}

sub header_option {
  my ($header) = @_;
  print qq(<tr bgcolor=$chrome_color><td colspan=3>
           <b><font face="Arial,Helvetica">
           $header
	   </font></b></td></tr>
	  );
}
