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

# Send comments, improvements, bugs to Benjamin Smedberg <benjamin@smedbergs.us>
use CGI;

$query = new CGI;
$field_separator = '<<fs>>';
$configure_in    = 'configure-mirror/mozilla/configure.in';
$CVSROOT         = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';
$ENV{PATH}       = "$ENV{PATH}:/opt/cvs-tools/bin:/usr/local/bin"; # for cvs & m4

%default = (
  'MOZ_CO_MODULE',  '',
  'MOZ_CVS_FLAGS',  '',
  'MOZ_CO_FLAGS',   '',
  'MOZ_OBJDIR', '@TOPSRCDIR@/obj-@CONFIG_GUESS@'
);

# Set up pull by date

use POSIX qw(strftime);
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
$default{MOZ_CO_DATE} = strftime("%d %b %Y %H:%M %Z",
                                 $sec,$min,$hour,$mday,$mon,$year);

%fillin = %default;
$fillin{MOZ_MAKE_FLAGS}='-j4';


if ($query->param()) {
  &parse_params;
  if ($query->param("MOZ_CO_PROJECT").$query->param("MOZ_CO_MODULE") eq "") {
      show_error("You must specify at least one of MOZ_CO_PROJECT or MOZ_CO_MODULE.");
      exit 0;
  }
  if ($query->param("--enable-application") eq "") {
      show_error("You must specify an application for --enable-application");
      exit 0;
  }
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
      <link rel="home" title="Home" href="http://www.mozilla.org/">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/print.css"  media="print">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/base/content.css"  media="all">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/cavendish/content.css" title="Cavendish" media="screen">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/base/template.css"  media="screen">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/cavendish/template.css" title="Cavendish" media="screen">
      <link rel="icon" href="http://www.mozilla.org/images/mozilla-16.png" type="image/png">
    </HEAD>
    <body>

    <h1>Configurator Script Preview</h1>

    <p>Check the script to make sure the options are correct.
    Then, save this script as <code class="filename">$<var>HOME</var>/.mozconfig</code>.

    <form action='.mozconfig' method='get'>
    <input type='hidden' name='saveas' value='1'>
);
    foreach $param ($query->param()) {
      if ($param =~ /^(MOZ_|--)/) {
        my $value = $query->param($param);
        $value =~ s/\s+$//;
        $value =~ s/^\s+//;
        next if $value eq '';

        print "<input type='hidden' name=$param value='"
          .$query->param($param)."'>\n";
      }
    }

  my $script = &print_script;

      print qq(
      <pre class="code">$script</pre>
  
      <p><input type='submit' value='Save the script'>
	</form>

   <p>Save the script, then build the tree as follows,
   <ol>
     <li><code class="command">cvs co mozilla/client.mk</code>
     <li><code class="command">cd mozilla</code>
     <li><code class="command">gmake -f client.mk checkout</code>
     <li><code class="command">gmake -f client.mk build</code>
   </ol>

   <p>
       Questions? See the 
       <a href="http://www.mozilla.org/build/configurator-faq.html">
       Configurator FAQ</a> and the 
       <a href="http://www.mozilla.org/build/">the build instructions</a>.

       <address>Maintained by
           <a href="mailto:benjamin&#x40;smedbergs&#x2e;us?subject=Build Configurator">Benjamin Smedberg &lt;benjamin&#x40;smedberg&#x2e;us&gt;</a></address>
	  );
}

sub print_script {
  my $out = '';

  $out .= "# Build configuration script\n";
  $out = "#\n";
  $out .= "# See http://www.mozilla.org/build/ for build instructions.\n";
  $out .= "#\n";
  $out .= "\n";

  foreach $param ($query->param()) {
    if ($param =~ /^MOZ_/) {
      my @values = $query->param($param);
      my $value;
      if (scalar(@values) > 1) {
        $value = join(",", @values);
      }
      else {
        $value = $values[0];
      }
      $value =~ s/\s+$//;
      $value =~ s/^\s+//;
      next if $value eq '';
      $value = "\"$value\"" if $value =~ /\s/;
      $out .= "# Options for client.mk.\n" unless $have_client_mk_options;
      $out .= "mk_add_options $param=".$value."\n";
      $have_client_mk_options = 1;
    }
  }
  $out .= "\n" if $have_client_mk_options;
  foreach $param ($query->param()) {
    if ($param =~ /^--/) {
      my $value = $query->param($param);
      next if $value eq '';

      # Wrap in double quotes if $value contains a space.
      $value = "\"$value\"" if $value =~ /\s/;

      $out .= "# Options for 'configure' (same as command-line options).\n"
        if not $have_configure_options;
      $out .= "ac_add_options $param";
      $out .= "=".$value if $value ne "yes";
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
      <TITLE>Mozilla Build Configurator</TITLE>
      <link rel="home" title="Home" href="http://www.mozilla.org/">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/print.css"  media="print">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/base/content.css"  media="all">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/cavendish/content.css" title="Cavendish" media="screen">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/base/template.css"  media="screen">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/cavendish/template.css" title="Cavendish" media="screen">
      <link rel="icon" href="http://www.mozilla.org/images/mozilla-16.png" type="image/png">
    </HEAD>
    <body>
 
    <FORM action='config.cgi' method='POST' name='ff'>
    <INPUT Type='hidden' name='preview' value='1'>

    <h1>Unix Build Configurator</h1>

    <p>The mozilla Unix Build System has a few required build parameters, and
    additional optional parameters. This build configurator may help you to
    tweak the build options. You should read
    <a href="http://www.mozilla.org/build/configure-build.html">the build configuration overview</a>
    before using this tool.

    <p><input type="Submit" value="Preview Build Script">

    <h2>Options for <code class="filename">client.mk</code></h2>

    <div class="section">
    <h3>Check out options:</h3>

    <p>MOZ_CO_PROJECT: (select one or more)<br>

    <select name="MOZ_CO_PROJECT" multiple="true">
      <option value="suite">suite (Mozilla Seamonkey)
      <option value="browser">browser (Firefox)
      <option value="mail">mail (Thunderbird)
      <option value="composer">composer (standalone composer)
      <option value="calendar">calendar (standalone calendar == Sunbird)
      <option value="xulrunner">xulrunner
      <option value="macbrowser">macbrowser (Camino)
    </select>

    <p>MOZ_CO_MODULE: (extra modules)<br>
    <input type="text" name="MOZ_CO_MODULE" value="$fillin{MOZ_CO_MODULE}">

    <p>CVS flags:<br>
    <code>cvs</code>&nbsp;<input type="text" name="MOZ_CVS_FLAGS"
     value="$fillin{MOZ_CVS_FLAGS}" size="16">&nbsp;<code>co</code>&nbsp;<input
     type="text" name="MOZ_CO_FLAGS" value="$fillin{MOZ_CO_FLAGS}" size="16">

    <p><input type="checkbox" name="pull_by_date");
  #print 'checked' if $fillin{MOZ_CO_DATE} ne $default{MOZ_CO_DATE};
  print qq(>&nbsp;
    Pull by date&nbsp;<input type='text' name='pull_date' value='$fillin{MOZ_CO_DATE}' size='25'>
    </div>

    <div class="section">
    <!-- Object Directory -->
    <h3>Object Directory:</h2>

    <p><input type="radio" name="MOZ_OBJDIR" value="fillin" checked>&nbsp;<input
        type="text" name="objdir_fillin" value="$fillin{MOZ_OBJDIR}" size='50'> (e.g. <code class="filename">mozilla/obj-i686-pc-linux-gnu</code>)<br>

       <input type="radio" name="MOZ_OBJDIR" value="">&nbsp;<code>mozilla</code>&nbsp;Build in the source tree.
    </div>

    <div class="section">
    <h3>Make options:</h2>

    <input type="checkbox" name="parallel_build">&nbsp;Parallel build using
    <input type='text' name='gmake_flags' value='$fillin{MOZ_MAKE_FLAGS}' 
      size='10'>
    </div>

    <h2>Options for <code class="filename">configure</code> script</h2>
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

    print "<table>";

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
    print "<tr><td>&nbsp;$option  <td>&nbsp;&nbsp;&nbsp;$help\n";
  }

  print "</table>\n";

  print qq(
	   <p><input type="Submit" value="Preview Build Script">
	   </form>

     <p>Questions? See the 
       <a href="http://www.mozilla.org/build/configurator-faq.html">
       Configurator FAQ</a>.<br>
       <a href="http://www.mozilla.org/build/">Back to the Build Instructions</a>

       <address>Maintained by
           <a href="mailto:benjamin&#x40;smedbergs&#x2e;us?subject=Build Configurator">Benjamin Smedberg &lt;benjamin&#x40;smedbergs&#x2e;us&gt;</a></address>
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
  print "<td>&nbsp;&nbsp;&nbsp;$help";
}

sub string_option {
  my ($name, $help) = @_;

  print "<tr><td colspan=2>$name=";
  print "<INPUT type='text' name='$name'";
  print " value='".$query->param($name)."'" if defined $query->param($name);
  print ">";
  print " $help\n";
}

sub bool_or_string_option {
  my ($name, $help) = @_;

  print "<tr><td align=right>";
  print "<INPUT type='checkbox' name='$name'>";
  print "<td>$name";
  print "<td>$help\n";
  print "<tr><td align=right>$name=<td align=left>";
  print "<INPUT type='text' name='$name'>";
  print "<td>$help\n";
}

sub header_option {
  my ($header) = @_;

  print "</table></div>" if $inTable;

  print qq(
  <div class="section">
  <h3>$header</h3>
  <table>
	  );
  $inTable = 1;
}

sub show_error {
  my ($msg) = @_;

  print "Content-type: text/html\n\n";
  print qq(
    <HTML>
    <HEAD>
      <TITLE>Mozilla Build Configurator - Error</TITLE>
      <link rel="home" title="Home" href="http://www.mozilla.org/">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/print.css"  media="print">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/base/content.css"  media="all">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/cavendish/content.css" title="Cavendish" media="screen">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/base/template.css"  media="screen">
      <link rel="stylesheet" type="text/css" href="http://www.mozilla.org/css/cavendish/template.css" title="Cavendish" media="screen">
      <link rel="icon" href="http://www.mozilla.org/images/mozilla-16.png" type="image/png">
    </HEAD>
    <body>
    <h1>Configuration Error</h1>
    <p class="important">$msg

    <p><a href="javascript:history.go(-1)">Go back</a>
  );
}
