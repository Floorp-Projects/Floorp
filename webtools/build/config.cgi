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
  if ($query->param('nspr_option') eq 'tip') {
    $query->param(-name=>'MOZ_WITH_NSPR',
		  -values=>['@OBJDIR@/nspr']);
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
  my ($saveopts) = '';
  foreach $param ($query->param()) {
    if ($param =~ /^(MOZ_|--)/) {
      next if $query->param($param) eq '';
      $saveopts .= "$param=".$query->param($param).'&';
    }
  }

  print qq(    
    <HTML>
    <HEAD>
      <TITLE>Configurator Script Preview</TITLE>
    </HEAD>
    <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">

	<form action='config.cgi' method='get'>);

    foreach $param ($query->param()) {
      if ($param =~ /^(MOZ_|--)/) {
		next if $query->param($param) eq '';
		print "<input type='hidden' name=$param value='".$query->param($param)."'>\n";
      }
	}

  print qq(
    <table cellspacing=2 cellpading=0 border=0 width=500><tr><td>

    <font size='+1' face='Helvetica,Arial'><b>
    Configurator Script Preview</b></font>
    </td></tr><tr></tr><tr><td>
    Check the script to make sure your options are correct. When you are done,
    save this script to <code><b>\$HOME/.mozmyconfig.sh</b></code>.*
    </td></tr></table>

    <table cellpadding=0 cellspacing=1><tr><td>
	<input type='submit' value='Save this script'>
	</td></tr></table>

    <table cellspacing=2 cellpading=0 border=0><tr><td>
	<table bgcolor="#FF0000" cellspacing=0 cellpadding=2 border=0><tr valign=middle><td align=center>
    <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=10 width="500" border=0><tr><td>
    <pre>);

  &print_script;

  print qq(</pre>
	   </td></tr></table>
	   </td></tr></table>
	   </td></tr></table>

    <table cellpadding=0 cellspacing=1><tr><td>
	<input type='submit' value='Save this script'>
	</td></tr></table>

<table cellspacing=0 cellpadding=0>
<tr><td colspan=3>
After the script is saved,build the tree with,
</td></tr><tr><td>&nbsp;</td><td>
  1.</td><td> <code>cd mozilla</code>
</td></tr><tr><td></td><td>
  2.</td><td> <code>gmake -f client.mk</code><br>
</td></tr><tr><td></td><td>
</td><td>     (default targets = "<code>checkout build</code>")
</td></tr>
<tr></tr><tr><td colspan=3>
Steps to run the viewer,
</td></tr><tr><td></td><td>
  1.</td><td> <code>cd &lt;objdir&gt;</code>
</td></tr><tr><td></td><td>
  2.</td><td> <code>gmake run_viewer</code>
</td></tr><tr><td>&nbsp;</td></tr><tr><td colspan=3>
* The build searches for this script in the following places,
</td></tr><tr><td></td><td></td><td>
   If <code>\$MOZ_MYCONFIG</code> is set, use that file,
</td></tr><tr><td></td><td></td><td>
   else try <code>&lt;objdir&gt;/myconfig.sh</code>
</td></tr><tr><td></td><td></td><td>
   else try <code>&lt;topsrcdir&gt/myconfig.sh</code>
</td></tr><tr><td></td><td></td><td>
   else try <code>\$HOME/.mozmyconfig.sh</code>
</td></tr></table>
<hr>
           Send questions or comments to 
           &lt;<a href="mailto:slamm\@netscape.com?subject=About the Build Configurator">slamm\@netcape.com</a>&gt;.
	</form>

	  );
}

sub print_script {

  print "# sh\n";
  print "# Build configuration script\n";
  print "#\n";
  print "# See http://www.mozilla.org/build/unix.html for build instructions.\n";
  print "#\n";
  print "\n";

  print "# Options for client.mk.\n";
  print "# (client.mk also understands some 'configure' options.)\n";
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

    <FORM action='config.cgi' method='POST'>
    <INPUT Type='hidden' name='preview' value='yes'>

    This form produces a script that you can save and use to configure your
    mozilla build. If this form does not have some options you want, you can
	add them to the script later.

    <table><tr><td>
	<input type="Submit" value="Preview Build Script">
    </td></tr></table>

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

    <!-- Threads -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>Threads:</b></font><br>
    </td></tr><tr><td>
    NSPR and mozilla can both be built with or without
    pthreads (POSIX threads). <br>
    Check
    <a href="http://www.mozilla.org/docs/refList/refNSPR/platforms.html">
    the NSPR supported platforms</a>
    to see if you can choose this option.<p>
    <input type="checkbox" name="--with-pthreads" value="yes">
    Build both NSPR and mozilla with pthreads<br>
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
    (Sets <code>USE_PTHREADS=1</code> for nspr, 
    and <code>--with-pthreads</code> for mozilla client.)
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
       <table><tr><td>
	   <input type="Submit" value="Preview Build Script">
       </td></tr></table>
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
