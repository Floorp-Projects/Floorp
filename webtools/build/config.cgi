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
# The Original Code is this file as it was released upon February 18, 1999.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

# config.cgi - Configure a mozilla build.
#   Outputs a form of configure options.
#   On submit, the cgi prints out a shell script that the user can
#   save to configure their build.

# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).
use CGI;

$query = new CGI;
$field_separator = '<<fs>>';
$configure_in    = 'mozilla/configure.in';
$chrome_color    = '#F0A000';
$ENV{CVSROOT}    = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';
#$ENV{PATH}       = "$ENV{PATH}:/usr/bin/ccs"; # for m4

print "Content-type: text/html\n\n\n";

if ($query->param()) {
  if ($query->param(preview) eq "yes") {
    &print_script_preview;
  } else {
    &print_script;
  }
} else {
  &print_configure_form;
}

sub print_script_preview {
  print qq(    
    <HTML>
    <HEAD>
      <TITLE>myconfig.sh Preview</TITLE>
    </HEAD>
    <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
    <table bgcolor="#FF0000" cellspacing=0 cellpadding=0><tr><td>
    <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=10 width="500"><tr><td>
    <pre>);

  &print_script;

  print qq(</pre>
	   </td></tr></table>
	   </td></tr></table>
	  );
}

sub print_script {

  print "# sh\n";
  print "# Build configuration script\n";
  print "#\n";
  print "# Save this script to $HOME/.mozmyconfig.sh,\n";
  print "# or one of the places listed below.\n";
  print "#\n";
  print "# The build searches for this script in the following places:\n";
  print "#   If \$MOZ_MYCONFIG is set, use that file,\n";
  print "#   else try <objdir>/myconfig.sh\n";
  print "#   else try <topsrcdir>/myconfig.sh\n";
  print "#   else try $HOME/.mozmyconfig.sh\n";
  print "#\n";
  print "\n";
  foreach $param ($query->param()) {
    if ($param =~ /^MOZ_/) {
      if ($query->param($param) ne '') {
	print "mk_add_options $param=".$query->param($param)."\n";
	$need_blank_line = 1;
      }
    }
  }
  print "\n" if $need_blank_line;
  if ($query->param('nspr_option') eq 'userdefined') {
    print "ac_add_options --with-nspr=".$query->param('nspr_dir')."\n";
  }
  if ($query->param('nspr_option') eq 'rpm') {
    print "ac_add_options --with-nspr=/usr\n";
  }
  if ($query->param('debug_option') eq 'userdefined') {
    print "ac_add_options --enable-debug=".$query->param('debug_dirs')."\n";
  }
  if ($query->param('debug_option') eq 'yes') {
    print "ac_add_options --enable-debug\n";
  }

  foreach $param ($query->param()) {
    if ($param =~ /^--/) {
      next if $query->param($param) eq "";
      print "ac_add_options $param";
      print "=".$query->param($param) if $query->param($param) ne "yes";
      print "\n";
    }
  }
}

sub print_configure_form {
  system "cvs co mozilla/configure.in";

  print qq(
    <HTML>
    <HEAD>
      <TITLE>Mozilla Unix Build Configurator</TITLE>
    </HEAD>
    <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
 
    <font size='+1' face='Helvetica,Arial'><b>
    Mozilla Unix Build Configurator</b></font><p>

    This form produces a script that you can save and use to configure your
    mozilla build.

    <FORM action='config.cgi' method='POST'>
    <INPUT Type='hidden' name='preview' value='yes'>

    <table bgcolor="$chrome_color" cellspacing=0 cellpadding=0><tr><td>
    <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=0><tr><td>
    <table cellspacing=0 cellpadding=1>

    <!-- Checkout options -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>Checkout options:</b></font><br>
    </td></tr><tr><td>
    <table cellpadding=0 cellspacing=0><tr><td>
    Module to checkout
    </td><td>
    <input type="text" name="MOZ_CO_MODULES"> (default is SeaMonkeyEditor)
    </td></tr><tr><td>
    Branch to checkout
    </td><td>
    <input type="text" name="MOZ_CO_BRANCH">  (default is HEAD)
    </td></tr></table>
    </td></tr>

    <!-- Object Directory -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>
    Object Directory:</b></font><br>
    </td></tr><tr><td>
    <input type="radio" name="MOZ_OBJDIR" value="\@TOPSRCDIR\@" checked>
    mozilla (i.e. In the source tree)<br>
    <input type="radio" name="MOZ_OBJDIR" value="\@TOPSRCDIR\@/obj-\@CONFIG_GUESS\@">
    mozilla/obj-\@CONFIG_GUESS\@ (e.g. <code>mozilla/obj-i686-pc-linux-gnu</code>)<br>
    <!-- Take this option out for now...
    <input type="radio" name="MOZ_OBJDIR" value="\@TOPSRCDIR\@/../obj-\@CONFIG_GUESS\@">
    mozilla/../obj-\@CONFIG_GUESS\@ (e.g. <code>mozilla/../obj-i686-pc-linux-gnu</code>)<br>
    -->
    </td></tr>

    <!-- NSPR -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>NSPR to use:</b></font><br>
    </td></tr><tr><td>
    <input type="radio" name="nspr_option" value="tip" checked>
    Build nspr from the tip<br>
    <input type="radio" name="nspr_option" value="userdefined">
    NSPR is installed in
    <input type="text" name="nspr_dir"><br>
    <input type="radio" name="nspr_option" value="rpm">
    NSPR is installed in /usr/lib (NSPR RPM installation)
    </td></tr>

    <!-- Debug -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>Debug option:</b></font><br>
    </td></tr><tr><td>
    <input type="radio" name="debug_option" value="yes" checked>
    Enable debugging<br>
    <input type="radio" name="debug_option" value="no">
    Disable debugging<br>
    <input type="radio" name="debug_option" value="userdefined">
    Enable debugging but only for the following directories: <br>
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
    <input type="text" name="debug_dirs" size=50> (comma separated, no spaces)<br>
    </td></tr>

    </table>
    </td></tr></table>
    </td></tr></table>

    <br>
    <font size=+1 face="Helvetica,Arial"><b>
    Options for `<code>configure</code>' script:</b></font><br>

    <table bgcolor="$chrome_color" cellspacing=0 cellpadding=0><tr><td>
    <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=0><tr><td>
    <table cellspacing=0 cellpadding=1>
  );

  open(OPTIONS, "m4 webify-configure.m4 $configure_in|")
    or die "Error parsing configure.in\n";

  foreach (<OPTIONS>) {
    chomp;
    ($type, $prename, $name, $comment) = split /$field_separator/;
    ($dummy,$dummy2,$help) = split /\s+/, $comment, 3;
    #$help =~ s/\\\$/\$/g;

    next if $name eq 'debug';

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
	   <input type="Submit" value="Preview Build Script">
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
