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
  if ($query->param('nspr_option') eq 'userdefined') {
    my $nspr_dir = $query->param('nspr_dir');
    $nspr_dir =~ s@/$@@;
    $nspr_dir =~ s@/lib$@@;
    $query->param(-name=>'--with-nspr',
		  -values=>[$nspr_dir]);
  }
  #if ($query->param('nspr_option') eq 'rpm') {
  #  $query->param(-name=>'--with-nspr',
  #		  -values=>['/usr']);
  #}
  if ($query->param('nspr_option') eq 'tip') {
    $query->param(-name=>'--with-nspr',
		  -values=>['@OBJDIR@/nspr']);
  }
  if ($query->param('debug_option') eq 'userdefined') {
    $query->param(-name=>'--enable-debug',
		  -values=>[$query->param('debug_dirs')]);
  }
  if ($query->param('pthreads_'.$query->param('nspr_option')) eq 'yes') {
      $query->param(-name=>'--with-pthreads',
		    -values=>['yes']);
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
  1.</td><td>  <code>cvs co mozilla/client.mk</code>
</td></tr><tr><td></td><td>
  2.</td><td>  <code>cd mozilla</code>
</td></tr><tr><td></td><td>
  3.</td><td>  <code>gmake -f client.mk</code><br>
</td></tr><tr><td></td><td>
</td><td>      (default targets = <code>checkout build</code>)
</td></tr>
</td></tr><tr><td colspan=3>&nbsp;</td></tr><tr><td colspan=3>
Here is a shortcut you can use to run <code>viewer</code>
or <code>apprunner</code> when the tree is built,
</td></tr><tr><td></td><td>
  1.</td><td>   <code>cd &lt;objdir&gt;</code>
</td></tr><tr><td></td><td>
  2a.</td><td>  <code>gmake run_viewer
</td></tr><tr><td></td><td>
  2b.</td><td>  <code>gmake run_apprunner
</td></tr></table>
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

  print "# Options for client.mk.\n";
  print "# Note: client.mk also uses the following 'ac_add_options',\n";
  print "#          --with-nspr=blah\n";
  print "#          --with-pthreads\n";
  foreach $param ($query->param()) {
    if ($param =~ /^MOZ_/) {
      my $value = $query->param($param);
      $value =~ s/\s+$//;
      $value =~ s/^\s+//;
      next if $value eq '';
      next if $param eq 'MOZ_CO_MODULE' and $value eq 'SeaMonkeyEditor';
      next if $param eq 'MOZ_CO_BRANCH' and $value eq 'HEAD';
      print "mk_add_options $param=".$value."\n";
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
    This form produces a script that you can save and use to configure your
    mozilla build. If this form does not have some options you want, you can
	add them to the script later.
    </td></tr></table>

    <table cellpadding=0 cellspacing=0 border=0><tr><td>
	<input type="Submit" value="Preview Build Script">
    </td></tr></table>

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
    <code>mozilla</code></td><td> (build in the source tree)<br></td></tr><tr><td>
    <input type="radio" name="MOZ_OBJDIR" value="\@TOPSRCDIR\@/obj-\@CONFIG_GUESS\@">
    <code>mozilla/obj-\@CONFIG_GUESS\@</code> </td><td>(use directory like&nbsp; <code>mozilla/obj-i686-pc-linux-gnu</code>)<br>
    <!-- Take this option out for now...
    <input type="radio" name="MOZ_OBJDIR" value="\@TOPSRCDIR\@/../obj-\@CONFIG_GUESS\@">
    mozilla/../obj-\@CONFIG_GUESS\@(e.g. <code>mozilla/../obj-i686-pc-linux-gnu</code>)<br>
    -->
    </td></tr></table>
    </td></tr>

    <!-- NSPR and Pthreads -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>NSPR and Pthreads:</b></font><br>
    </td></tr><tr><td>

    NSPR and mozilla can be build either with or without
    pthreads (POSIX threads). They should both be build the same way. Selecting Pthreads in the right column will cause mozilla to be built with be threads.
    For more information on NSPR and pthreads, check 
    <a href="http://www.mozilla.org/docs/refList/refNSPR/platforms.html">
    the NSPR supported platforms</a>.

    <table cellpadding=0 cellspacing=5><tr><td>
    <table cellpadding=0 cellspacing=0><tr><td>
    <input type="radio" name="nspr_option" value="tip" checked>
    Build NSPR from the tip (installs in <code>\@OBJDIR\@/nspr</code>)
    </td><td>&nbsp;
    </td><td>
    <input type="checkbox" name="pthreads_tip" value="yes">
    Use Pthreads for NSPR and mozilla
    </td></tr><tr><td>
    <input type="radio" name="nspr_option" value="userdefined" onclick="document.ff.nspr_dir.focus();">
    NSPR is installed in
    <input type="text" name="nspr_dir" onfocus="document.ff.nspr_option[1].checked=true;">
    </td><td>&nbsp;
    </td><td>
    <input type="checkbox" name="pthreads_userdefined" value="yes">
    NSPR was Built with Pthreads, so build mozilla with Pthreads
    </td></tr>
    <!-- Reduce clutter. This option was only for nscp folk
    <tr><td>
    <input type="radio" name="nspr_option" value="rpm">
    NSPR is installed in /usr/lib (NSPR RPM installation)
    </td><td>&nbsp;
    </td><td>
    <input type="checkbox" name="pthreads_rpm" value="yes">
    NSPR was Built with Pthreads, so build mozilla with Pthreads
    </td></tr>
    -->
    </table>
    </td></tr></table>
    </td></tr>

    <!-- Threads
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>Threads:</b></font><br>
    </td></tr><tr><td>
    NSPR and mozilla can both be built with or without
    pthreads (POSIX threads).
    Check
    <a href="http://www.mozilla.org/docs/refList/refNSPR/platforms.html">
    the NSPR supported platforms</a>
    to see if you can choose this option.<p>
    <input type="checkbox" name="--with-pthreads" value="yes">
    The selected NSPR was built with pthreads, 
    or if building NSPR from the tip 
    and you would like to build it with pthreads.<br>
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
    (Sets <code>USE_PTHREADS=1</code> for NSPR, 
    and <code>--with-pthreads</code> for mozilla client.)
    </td></tr>
    -->

    <!-- Debug -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>Debug option:</b></font><br>
    </td></tr><tr><td>
    <input type="radio" name="debug_option" value="yes" checked>
    Enable debug symbols<br>
    <input type="radio" name="debug_option" value="no">
    Disable debug symbols<br>
    <input type="radio" name="debug_option" value="userdefined" onclick="document.ff.debug_dirs.focus();">
    Enable debug symbols, but only for the following directories: <br>
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
    <input type="text" name="debug_dirs" size=50 onfocus="document.ff.debug_option[2].checked=true;"> (comma separated, no spaces)<br>
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

  open(OPTIONS, "m4 webify-configure.m4 $configure_in|")
    or die "Error parsing configure.in\n";

  foreach (<OPTIONS>) {
    chomp;
    ($type, $prename, $name, $comment) = split /$field_separator/;
    ($dummy,$dummy2,$help) = split /\s+/, $comment, 3;
    #$help =~ s/\\\$/\$/g;

    next if $name eq 'debug';

    if ($type eq "header") {
      print "</td></tr></table></td></tr>\n" if $inTable == 1;
      &header_option($comment);
      print "<tr><td><table cellspacing=0 cellpadding=0 border=0><tr><td>\n";
      $inTable = 1;
    } else {
      eval "&${type}_option(\"--$prename-$name\",\"$help\");";
    }
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
  print qq(<tr bgcolor=$chrome_color><td colspan=3>
           <b><font face="Arial,Helvetica">
           $header
	   </font></b></td></tr>
	  );
}
