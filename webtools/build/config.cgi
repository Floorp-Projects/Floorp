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
$configure_in    = 'configure-mirror/mozilla/configure.in';
$chrome_color    = '#F0A000';
$CVSROOT         = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';
$ENV{PATH}       = "$ENV{PATH}:/opt/cvs-tools/bin:/usr/local/bin"; # for cvs & m4


if ($query->param()) {
  &parse_params;

  if ($query->param(preview) eq "1") {
    print "Content-type: text/html\n\n";
    &print_script_preview;
    exit 0;
  } elsif ($query->param(saveas) eq "1") {
    print "Content-type: text/saveas\n\n";
    &print_script;
    exit 0;
  }
}
print "Content-type: text/html\n\n";
&print_configure_form;

## End main program
#########################################################

sub parse_params {
  if ($query->param('MOZ_OBJDIR') eq '@TOPSRCDIR@') {
    $query->param(-name=>'MOZ_OBJDIR',
		  -values=>['']);
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
    <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">);


  print qq(
    <TABLE BGCOLOR="#000000" WIDTH="100%" BORDER=0 CELLPADDING=0 CELLSPACING=0>
    <TR><TD>
      <A HREF="http://www.mozilla.org/">
      <IMG SRC="http://www.mozilla.org/images/mozilla-banner.gif" ALT="" BORDER=0 WIDTH=600 HEIGHT=58>
      </A>
    </TD></TR></TABLE>

    <table cellspacing=2 cellpading=0 border=0 width=600><tr><td>

    <font size='+1' face='Helvetica,Arial'><b>
    Configurator Script Preview</b></font>
    </td></tr><tr></tr><tr><td>
    Check the script to make sure the options are correct.
    Then, save this script as <code><b>~/.mozconfig</b></code>.
    </td></tr></table>

    <form action='.mozconfig' method='get'>
    <input type='hidden' name='saveas' value='1'>
);
    foreach $param ($query->param()) {
      if ($param =~ /^(MOZ_|--)/) {
        next if $query->param($param) eq '';
        print "<input type='hidden' name=$param value='"
          .$query->param($param)."'>\n";
      }
    }

  print qq(
    <!--
    <table cellpadding=0 cellspacing=1><tr><td>
	<input type='submit' value='Save the script'>
	</td></tr></table>
    -->

    <table cellspacing=2 cellpading=0 border=0><tr><td>
	<table bgcolor="#FF0000" cellspacing=0 cellpadding=2 border=0><tr valign=middle><td align=center>
	<table bgcolor="$chrome_color" cellspacing=0 cellpadding=2 border=0><tr valign=middle><td align=center>
    <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=10 width="600" border=0><tr><td>
    <pre>);

  &print_script;

  print qq(</pre>
	   </td></tr></table>
	   </td></tr></table>
	   </td></tr></table>
	   </td></tr></table>

    <table cellpadding=0 cellspacing=1><tr><td>
	<input type='submit' value='Save the script'>
	</td></tr></table>

<table cellspacing=0 cellpadding=0 border=0>
<tr><td colspan=3>
Save the script, then build the tree as follows,
</td></tr><tr><td>&nbsp;</td><td>
  1. </td><td>  <code>cvs co mozilla/client.mk</code>
</td></tr><tr><td></td><td>
  2. </td><td>  <code>cd mozilla</code>
</td></tr><tr><td></td><td>
  3. </td><td>  <code>gmake -f client.mk</code><br>
</td></tr><tr><td></td><td>
</td><td>      (default targets: <code>checkout depend build</code>)
</td></tr>
</td></tr><tr><td colspan=3>&nbsp;</td></tr><tr><td colspan=3>
</td></tr></table>
If you do not want to use <code><b>client.mk</b></code>, 
then use your usual build steps.<br>
<code><b>configure</b></code> will
pick up the options in your <code><b>.mozconfig</b></code> script.
<P>
Check out the <A HREF="http://www.mozilla.org/build/configurator-faq.html">Build Configuator FAQ</A>
for more information.
<p>
<hr align=left width=600>
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

  foreach $param ($query->param()) {
    if ($param =~ /^MOZ_/) {
      my $value = $query->param($param);
      $value =~ s/\s+$//;
      $value =~ s/^\s+//;
      next if $value eq '';
      next if $param eq 'MOZ_CO_MODULE' and $value eq 'SeaMonkeyEditor';
      next if $param eq 'MOZ_CO_BRANCH' and $value eq 'HEAD';
      print "# Options for client.mk.\n" if not $have_client_mk_options;
      print "mk_add_options $param=".$value."\n";
      $have_client_mk_options = 1;
    }
  }
  print "\n" if $have_client_mk_options;
  foreach $param ($query->param()) {
    if ($param =~ /^--/) {
      next if $query->param($param) eq '';
      print "# Options for 'configure' (same as command-line options).\n"
        if not $have_configure_options;
      print "ac_add_options $param";
      print "=".$query->param($param) if $query->param($param) ne "yes";
      print "\n";
      $have_configure_options = 1;
    }
  }
  if (not $have_client_mk_options and not $have_configure_options) {
    print "\n# No script needed. Only default values were selected\n";
  }
}

sub print_configure_form {
  mkdir 'configure-mirror', 0777 if not -d 'configure-mirror';
  system 'echo :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot Ay=0=a%0bZ | cat > configure-mirror/.cvspass' if not -f 'configure-mirror/.cvspass';
  link 'config.cgi', '.mozconfig' if not -f '.mozconfig';
  # Set the HOME variable to pick up '.cvspass' for cvs login
  system "cd configure-mirror && HOME=. cvs -d $CVSROOT co mozilla/configure.in > /dev/null 2>&1";

  print qq(
    <HTML>
    <HEAD>
      <TITLE>Mozilla Unix Build Configurator</TITLE>
    </HEAD>
    <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
 
    <FORM action='config.cgi' method='POST' name='ff'>
    <INPUT Type='hidden' name='preview' value='1'>

    <TABLE BGCOLOR="#000000" WIDTH="100%" BORDER=0 CELLPADDING=0 CELLSPACING=0>
    <TR><TD>
      <A HREF="http://www.mozilla.org/">
      <IMG SRC="http://www.mozilla.org/images/mozilla-banner.gif" ALT="" BORDER=0 WIDTH=600 HEIGHT=58>
      </A>
    </TD></TR></TABLE>

    <table cellpadding=0 cellspacing=4 border=0 width="500"><tr><td>
    <font size='+1' face='Helvetica,Arial'><b>
    Unix Build Configurator
    </b></font>
    </td></tr><tr><td>
    The mozilla Unix build system is designed to build without any user set
    options. However, should you need to tweak an option, the
    Configurator is here to help.
    This form produces a script that you can use to configure your build. 
    The script saves you the trouble of setting environment 
    variables for <code><b>client.mk</b></code>
    or typing command-line options for <code><b>configure</b></code>.
    </td></tr></table>

    <table cellpadding=0 cellspacing=0 border=0><tr><td>
	<input type="Submit" value="Preview Build Script">
    </td></tr></table>

    <font size=+1 face="Helvetica,Arial"><b>
    Options for "<code>client.mk</code>":
    </b></font><br>

    <table bgcolor="$chrome_color" cellspacing=0 cellpadding=0 border=0><tr><td>
    <table bgcolor="#FFFFFF" cellspacing=2 cellpadding=0 border=0><tr><td>
    <table cellspacing=0 cellpadding=4 border=0>

    <!-- Check out options -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>Check out options:</b></font><br>
    </td></tr><tr><td>
    <table cellpadding=0 cellspacing=0><tr><td>
    Check out module
    </td><td>
    <input type="text" name="MOZ_CO_MODULE" value="SeaMonkeyEditor">
    </td></tr><tr><td>
    Check out branch
    </td><td>
    <input type="text" name="MOZ_CO_BRANCH" value="HEAD">
    </td></tr></table>
    </td></tr>

    <!-- Object Directory -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>
    Object Directory:</b></font><br>
    </td></tr><tr><td><table><tr><td>
    <input type="radio" name="MOZ_OBJDIR" value="\@TOPSRCDIR\@" checked>
    <code>mozilla</code></td><td> Build in the source tree. (default)<br></td></tr><tr><td>
    <input type="radio" name="MOZ_OBJDIR" value="\@TOPSRCDIR\@/obj-\@CONFIG_GUESS\@">
    <code>mozilla/obj-`config.guess`</code> </td><td>(e.g. <code>mozilla/obj-i686-pc-linux-gnu)</code><br>
    </td></tr></table>
    </td></tr>


    </table>
    </td></tr></table>
    </td></tr></table>

    <br>
    <font size=+1 face="Helvetica,Arial"><b>
    Options for "<code>configure</code>" script</b> (updated on the fly)<b>:</b></font><br>

    <table bgcolor="$chrome_color" cellspacing=0 cellpadding=0 border=0><tr><td>
    <table bgcolor="#FFFFFF" cellspacing=2 cellpadding=0 border=0><tr><td>
    <table cellspacing=0 cellpadding=4 border=0>
  );

  my @unhandled_options = ();

  open(OPTIONS, "m4 webify-configure.m4 $configure_in|")
    or die "Error parsing configure.in\n";

  foreach (<OPTIONS>) {
    chomp;
    ($type, $prename, $name, $comment) = split /$field_separator/;
    ($dummy,$dummy2,$help) = split /\s+/, $comment, 3;

    if ($type eq 'header') {
      &header_option($comment);
    } elsif ($type eq 'unhandled') {
      push @unhandled_options, $comment;
    } else {
      eval "&${type}_option(\"--$prename-$name\",\"$help\");";
    }
  }
  header_option("Options not handled by Configurator"
               ." (Add them to the script by hand)");
  foreach $comment (@unhandled_options) {
    $comment =~ s/\\\$/\$/g;
    my ($dummy,$option,$help) = split /\s+/, $comment, 3;
    print "<tr><td>&nbsp;$option</td><td>&nbsp;&nbsp;&nbsp;$help</td></tr>\n";
  }

  print "</td></tr></table></td></tr>\n";

  print qq(
	   </table>
	   </td></tr></table>
	   </td></tr></table>
       <table><tr><td>
	   <input type="Submit" value="Preview Build Script">
       </td></tr></table>
	   </form>
       <p>
       <hr align=left width=600>
           Send questions or comments to 
           &lt;<a href="mailto:slamm\@netscape.com?subject=About the Build Configurator">slamm\@netcape.com</a>&gt;.
	  );

  print "\n</body>\n</html>\n";
}

sub bool_option {
  my ($name, $help) = @_;

  print "<tr><td>";
  print "<INPUT type='checkbox' name='$name' value='yes'";
  print " CHECKED" if $query->param($name) eq 'yes';
  print ">";
  print "$name";
  print "</td><td>&nbsp;&nbsp;&nbsp;$help</td></tr>";
}

sub string_option {
  my ($name, $help) = @_;

  print "<tr><td colspan=2>$name=";
  print "<INPUT type='text' name='$name'";
  print " value='".$query->param($name)."'" if defined $query->param($name);
  print ">";
  print " $help</td></tr>\n";
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
  print "</td></tr></table></td></tr>\n" if $inTable == 1;
  print qq(<tr bgcolor=$chrome_color><td colspan=3>
           <b><font face="Arial,Helvetica">
           $header
	   </font></b></td></tr>
	  );
  print "<tr><td><table cellspacing=0 cellpadding=0 border=0><tr><td>\n";
  $inTable = 1;
}
