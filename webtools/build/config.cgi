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


if ($query->param()) {
  &parse_params;

  if ($query->param(preview) eq "yes") {
    print "Content-type: text/html\n\n";
    &print_script_preview;
  } else {
    print "Content-type: text/saveas\n\n";
    &print_script;
  }
} else {
  print "Content-type: text/html\n\n";
  &print_configure_form;
}

sub parse_params {
  if ($query->param('nspr_option') eq 'userdefined') {
    $query->param(-name=>'--with-nspr',
		  -values=>[$query->param('nspr_dir')]);
  }
  if ($query->param('nspr_option') eq 'rpm') {
    $query->param(-name=>'--with-nspr',
		  -values=>['/usr']);
  }
  if ($query->param('debug_option') eq 'userdefined') {
    $query->param(-name=>'--enable-debug',
		  -values=>[$query->param('debug_dirs')]);
  }
  if ($query->param('debug_option') eq 'yes') {
    $query->param(-name=>'--enable-debug',
		  -values=>['yes']);
  }
}

sub print_script_preview {
  my ($saveoptions) = '';
  foreach $param ($query->param()) {
    if ($param =~ /^(MOZ_|--)/) {
      next if $query->param($param) eq '';
      $saveopts .= "$param=".$query->param($param).'&';
    }
  }
  chop($saveopts);

  print qq(    
    <HTML>
    <HEAD>
      <TITLE>myconfig.sh Preview</TITLE>
    </HEAD>
    <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">

    <font size='+1' face='Helvetica,Arial'><b>
    Configurator Script Preview</b></font><p>


Check the script to make sure your options are correct. When you are done,<br>
<a href="config.cgi?$saveopts">save this script</a> to <code><b>\$HOME/.mozmyconfig.sh</b></code>.*
<p>
Once you have saved the script, build the tree with,
<ol>
  <li><code>cd mozilla</code>
  <li><code>gmake -f client.mk</code><br>
      (default targets = "<code>checkout build</code>")
</ol>
    <table bgcolor="#FF0000" cellspacing=0 cellpadding=0><tr><td>
    <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=10 width="500"><tr><td>
    <pre>);

  &print_script;

  print qq(</pre>
	   </td></tr></table>
	   </td></tr></table>
<P>
* The build searches for this script in the following places,
<ul>
   If <code>\$MOZ_MYCONFIG</code> is set, use that file,<br>
   else try <code>&lt;objdir&gt;/myconfig.sh</code></br>
   else try <code>&lt;topsrcdir&gt/myconfig.sh</code><br>
   else try <code>\$HOME/.mozmyconfig.sh</code><br>
</ul>
<hr>
	   <p>
           Send questions or comments to 
           &lt;<a href="mailto:slamm\@netscape.com?subject=About the Build Configurator">slamm\@netcape.com</a>&gt;.
	  );
}

sub print_script {

  print "# sh\n";
  print "# Build configuration script\n";
  print "#\n";
  print "# See http://www.mozilla.org/build/unix.html for build instructions.\n";
  print "#\n";
  print "\n";

  print "# Options for client.mk\n";
  foreach $param ($query->param()) {
    if ($param =~ /^MOZ_/) {
      next if $query->param($param) eq '';
      print "mk_add_options $param=".$query->param($param)."\n";
      $need_blank_line = 1;
    }
  }
  print "\n" if $need_blank_line;
  print "# Options for 'configure' (same as command-line options).\n";
  foreach $param ($query->param()) {
    if ($param =~ /^--/) {
      next if $query->param($param) eq '';
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
    <input type="text" name="nspr_dir"> (omit trailing '<code>/lib</code>')<br>
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
