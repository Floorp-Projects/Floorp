#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is this file as it was released upon February 18, 1999.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

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

%default = (
  'MOZ_CO_MODULE',  'SeaMonkeyAll',
  'MOZ_CO_BRANCH',  'HEAD',
  'MOZ_OBJDIR',     '@TOPSRCDIR@',
  'MOZ_CVS_FLAGS',  '-q -z 3',
  'MOZ_CO_FLAGS',   '-P',
);

# Set up pull by date
#
use POSIX qw(strftime);
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
$default{MOZ_CO_DATE} = strftime("%d %b %Y %H:%M %Z",
                                 $sec,$min,$hour,$mday,$mon,$year);

%fillin = %default;
$default_objdir_fillin='@TOPSRCDIR@/obj-@CONFIG_GUESS@';
$fillin{MOZ_OBJDIR} = $default_objdir_fillin;
$fillin{MOZ_MAKE_FLAGS}='-j4';

if ($query->param()) {
  &parse_params;
  if ($query->param(preview) eq "1") {
    print "Content-type: text/html\n\n";
    &print_script_preview;
    exit 0;
  } elsif ($query->param(saveas) eq "1") {
    print "Content-type: text/saveas\n\n";
    print print_script();
    exit 0;
  }
}

print "Content-type: text/html\n\n";
&print_configure_form;

## End main program
#########################################################

sub parse_params {
  if ($query->param('pull_by_date') eq 'on') {
    my $pull_date = $query->param('pull_date');
    $query->param(-name=>'MOZ_CO_DATE', -values=>[ $pull_date ]);
  }
  if ($query->param('parallel_build') eq 'on') {
    my $gmake_flags = $query->param('gmake_flags');
    $query->param(-name=>'MOZ_MAKE_FLAGS', -values=>[ $gmake_flags ]);
  }
  if ($query->param('MOZ_OBJDIR') eq 'fillin') {
    my $objdir = $query->param('objdir_fillin');
    $query->param(-name=>'MOZ_OBJDIR', -values=>[ $objdir ]);
  }
  foreach $param ($query->param()) {
    $fillin{$param} = $query->param($param) if defined($fillin{$param});
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
    Then, save this script as <code><b>$HOME/.mozconfig</b></code>.
    </td></tr></table>

    <form action='.mozconfig' method='get'>
    <input type='hidden' name='saveas' value='1'>
);
    foreach $param ($query->param()) {
      if ($param =~ /^(MOZ_|--)/) {
        my $value = $query->param($param);
        $value =~ s/\s+$//;
        $value =~ s/^\s+//;
        next if $param =~ /^--/   and $value eq '';
        next if $param =~ /^MOZ_/ and $value eq $default{$param};

        print "<input type='hidden' name=$param value='"
          .$query->param($param)."'>\n";
      }
    }

  my $script = &print_script;

  if ($script eq '') {
    print "<font size='+1'><b>";
    print "No script needed. Only default values were selected.<p>\n";
    print "</b></font>";
  } else {
      print qq(
      <!--
      <table cellpadding=0 cellspacing=1><tr><td>
  	<input type='submit' value='Save the script'>
  	</td></tr></table>
      -->
  
      <table cellspacing=2 cellpading=0 border=0>
        <tr><td>
  	<table bgcolor="#FF0000" cellspacing=0 cellpadding=2 border=0>
          <tr valign=middle><td align=center>
  	  <table bgcolor="$chrome_color" cellspacing=0 cellpadding=2 border=0>
            <tr valign=middle><td align=center>
            <table bgcolor="#FFFFFF" cellspacing=0 cellpadding=10 width="600" border=0>
              <tr><td>
                <pre>$script</pre>
  	      </td></tr></table>
  	    </td></tr></table>
  	  </td></tr></table>
        </td></tr></table>
  
      <table cellpadding=0 cellspacing=1><tr><td>
  	<input type='submit' value='Save the script'>
  	</td></tr></table>
    );
  }

  print qq(
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
then use your favorite build steps, and
<code><b>configure</b></code> will
pick up the options in your <code><b>.mozconfig</b></code> script.
<p>
       Questions? See the 
       <a href="http://www.mozilla.org/build/configurator-faq.html">
       Configurator FAQ</a>.<br>
       <a href="http://www.mozilla.org/build/unix.html">
       Back to the Unix Build Instructions</a>
       <p>
<hr align=left width=600>
           Send questions or comments to 
           &lt;<a href="mailto:slamm\@netscape.com?subject=About the Build Configurator">slamm\@netcape.com</a>&gt;.
	</form>

	  );
}

sub print_script {
  my $out = '';

  $out =  "# sh\n";
  $out .= "# Build configuration script\n";
  $out .= "#\n";
  $out .= "# See http://www.mozilla.org/build/unix.html for build instructions.\n";
  $out .= "#\n";
  $out .= "\n";

  foreach $param ($query->param()) {
    if ($param =~ /^MOZ_/) {
      my $value = $query->param($param);
      $value =~ s/\s+$//;
      $value =~ s/^\s+//;
      next if $value eq $default{$param};
      $value = "\"$value\"" if $value =~ /\s/;
      $out .= "# Options for client.mk.\n" unless $have_client_mk_options;
      $out .= "mk_add_options $param=".$value."\n";
      $have_client_mk_options = 1;
    }
  }
  $out .= "\n" if $have_client_mk_options;
  foreach $param ($query->param()) {
    if ($param =~ /^--/) {
      next if $query->param($param) eq '';
      $out .= "# Options for 'configure' (same as command-line options).\n"
        if not $have_configure_options;
      $out .= "ac_add_options $param";
      $out .= "=".$query->param($param) if $query->param($param) ne "yes";
      $out .= "\n";
      $have_configure_options = 1;
    }
  }
  if ($have_client_mk_options or $have_configure_options) {
    return $out;
  } else {
    return '';
  }
}

sub print_configure_form {
  mkdir 'configure-mirror', 0777 if not -d 'configure-mirror';
  system 'echo :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot Ay=0=a%0bZ | cat > configure-mirror/.cvspass' if not -f 'configure-mirror/.cvspass';
  symlink 'config.cgi', '.mozconfig' if not -f '.mozconfig';
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
    The mozilla Unix Build System is designed to work free of any user set
    options. However, should you need to tweak an option, the
    Configurator is here to help.<p>
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
    <table cellpadding=0 cellspacing=0 width="100%"><tr><td>
    Check out module
    </td><td>
    <input type="text" name="MOZ_CO_MODULE" value="$fillin{MOZ_CO_MODULE}">
    </td></tr><tr><td>
    CVS flags
    </td><td>
    <code>cvs</code>&nbsp;
    <input type="text" name="MOZ_CVS_FLAGS" value="$fillin{MOZ_CVS_FLAGS}"
     size="16">
    &nbsp;<code>co</code>&nbsp;
    <input type="text" name="MOZ_CO_FLAGS" value="$fillin{MOZ_CO_FLAGS}"
     size="16">
    </td></tr><tr><td>
    <input type="checkbox" name="pull_by_date");
  #print 'checked' if $fillin{MOZ_CO_DATE} ne $default{MOZ_CO_DATE};
  print qq(>&nbsp;
    Pull by date
    </td><td>
    <input type='text' name='pull_date' value='$fillin{MOZ_CO_DATE}' size='25'>
    </td></tr></table>
    </td></tr>

    <!-- Object Directory -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>
    Object Directory:</b></font><br>
    </td></tr><tr><td><table><tr><td>
    <input type="radio" name="MOZ_OBJDIR" value="$default{MOZ_OBJDIR}");
  print 'checked' if $fillin{MOZ_OBJDIR} eq $default_objdir_fillin;
  print qq(>&nbsp;
    <code>mozilla</code></td><td> Build in the source tree. (default)<br></td></tr><tr><td>
    <input type="radio" name="MOZ_OBJDIR" value="fillin");
  print 'checked' if $fillin{MOZ_OBJDIR} ne $default_objdir_fillin;
  print qq(>&nbsp;
    <input type="text" name="objdir_fillin" value="$fillin{MOZ_OBJDIR}" size='30'>
    </td><td>);
  print '(e.g. <code>mozilla/obj-i686-pc-linux-gnu)</code>'
    if $fillin{MOZ_OBJDIR} eq $default{MOZ_OBJDIR};
  print qq(
    </td></tr></table>
    </td></tr>

    <!-- Make Options -->
    <tr bgcolor="$chrome_color"><td>
    <font face="Helvetica,Arial"><b>
    Make options:</b></font><br>
    </td></tr><tr><td><table><tr><td>
    <input type="checkbox" name="parallel_build");
  #print 'checked' if $fillin{MOZ_MAKE_FLAGS} ne $default{MOZ_MAKE_FLAGS};
  print qq(>&nbsp;
    Parallel build using
    <input type='text' name='gmake_flags' value='$fillin{MOZ_MAKE_FLAGS}' 
      size='10'>
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

    undef @options;

    while ($line = <OPTIONS>) {
        chomp $line;
        
        if ($line !~ m/^bool/ &&
            $line !~ m/^string/ &&
            $line !~ m/^header/ &&
            $line !~ m/^unhandled/) {
            # Must be continuation comment
            $line =~ s/^\s+/ /;
            $oldline .= $line;
        } else {
            push @options, $oldline;
            $oldline = $line;
        }
    }
    push @options, $oldline;

    foreach $line (@options) {
        ($type, $prename, $name, $comment) = split /$field_separator/, $line;
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
               ." (Add them to the .mozconfig script by hand)");
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

       Questions? See the 
       <a href="http://www.mozilla.org/build/configurator-faq.html">
       Configurator FAQ</a>.<br>
       <a href="http://www.mozilla.org/build/unix.html">
       Back to the Unix Build Instructions</a>
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
