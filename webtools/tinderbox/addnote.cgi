#!/usr/bin/perl --
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
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

use strict;
use Fcntl qw(:flock);

require 'tbglobals.pl';
require 'showbuilds.pl';

# Process the form arguments
my %form = &split_cgi_args();
my %cookie_jar = &split_cookie_args();

my ($args, $tree, $logfile, $errorparser, $buildname, $buildtime);

if (defined($args = $form{log})) {
  # Use simplified arguments that uses the logfile as a key.
  ($tree, $logfile) = split /\//, $args;
  $tree = &require_only_one_tree($tree);

  my $br = tb_find_build_record($tree, $logfile);
  $errorparser = $br->{errorparser};
  $buildname   = $br->{buildname};
  $buildtime   = $br->{buildtime};
} else {
  # Use old style arguments;
  $tree        = &require_only_one_tree($form{tree});
  $logfile     = $form{logfile};
  $errorparser = $form{errorparser};
  $buildname   = $form{buildname};
  $buildtime   = $form{buildtime};
}

print "Content-Type:text/html\n";

# Remember email address in cookie
#
if (defined $ENV{REQUEST_METHOD} and $ENV{REQUEST_METHOD} eq 'POST'
    and defined $form{note}) {
    # Expire the cookie 5 months from now
    print "Set-Cookie: email=$form{who}; expires="
        . toGMTString(time + 86400 * 152) . "; path=/\n";
}     
print "\n<HTML>\n";

if ($form{note}) {
  # Form Submission.
  # Add the comment.

  my $note = $form{note};
  my $who  = $form{who};

  $note =~ s/\&/&amp;/gi;
  $note =~ s/\</&lt;/gi;
  $note =~ s/\>/&gt;/gi;
  my $enc_note = url_encode($note);

  my $now = time;

  # Save comment to the notes.txt file.
  #
  my $err = 0;
  open NOTES, ">>", "$tree/notes.txt" or $err++;
  if ($err) {
      print "<title>Error</title>\n";
      print "<h1>Could not open $tree/notes.txt for writing.</h1>\n";
      print "<h2>Please contact the webmaster.</h2>\n";
      die "Could not open $tree/notes.txt for writing.\n";
  }
  flock NOTES, LOCK_EX;
  print NOTES "$buildtime|$buildname|$who|$now|$enc_note\n"; 

  # Find the latest times for the "other" trees
  my (%quickdata);
  tb_loadquickparseinfo($tree, $form{maxdate}, \%quickdata, 1);

  foreach my $element (keys %form) {
    # The checkboxes for the builds have "NAME" set to the build name
    if (defined $quickdata{$element}->{buildtime}) {
      print NOTES "$quickdata{$element}->{buildtime}|$element|$who|$now|$enc_note\n"; 
    }
  }
  close NOTES;

  # Give a confirmation
  #
  print "<H1>The following comment has been added to the log</h1>\n";
  print "<pre>\n[<b>$who - ".print_time($now)."</b>]\n$note\n</pre>";

  my $enc_buildname = url_encode($buildname);
  print "<p><a href='showlog.cgi?tree=$tree&buildname=$enc_buildname"
    ."&buildtime=$buildtime&logfile=$logfile&errorparser=$errorparser'>"
    ."Go back to the Error Log</a><br><a href='showbuilds.cgi?tree=$tree'>"
    ."Go back to the build Page</a>";

  # Rebuild the static tinderbox pages
  my %new_form = ();
  $new_form{tree} = $tree;
  &tb_build_static(\%new_form);

} else {
  # Print the form to submit a comment

  if ($buildname eq '' or $buildtime == 0) {
    print "<h1>Invalid parameters</h1>\n";
    die "\n";
  }

  # Retrieve the email address from the cookie jar.
  #
  my $emailvalue = '';
  $emailvalue = " value='$cookie_jar{email}'" if defined $cookie_jar{email};

  print <<__END_FORM;
<head><title>Add a Comment to $buildname log</title></head>
<body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">

<table><tr><td>
  <b><font size="+2">
    Add a Log Comment
  </font></b>
</td></tr><tr><td>
  <b><code>
    $buildname
  </code></b>
</td></tr></table>

<form action='addnote.cgi' METHOD='post'>

<INPUT Type='hidden' name='buildname' value='${buildname}'>
<INPUT Type='hidden' name='buildtime' value='${buildtime}'>
<INPUT Type='hidden' name='errorparser' value='$errorparser'>
<INPUT Type='hidden' name='logfile' value='$logfile'>
<INPUT Type='hidden' name='tree' value='$tree'>

<table border=0 cellpadding=4 cellspacing=1>
<tr valign=top>
  <td align=right>
     <NOWRAP>Email address:</NOWRAP>
  </td><td>
     <INPUT Type='input' name='who' size=32$emailvalue><BR>
  </td>
</tr><tr valign=top>
  <td align=right>
     Comment:
  </td><td>
     <TEXTAREA NAME=note ROWS=10 COLS=30 WRAP=HARD></textarea>
  </td>
</tr>
</table>
<br><b><font size="+2">Additional Builds</font></b><br>
(Comment will be added to the most recent cycle.)<br>
__END_FORM

  # Find the names of the "other" trees
  my (%quickdata);
  tb_loadquickparseinfo($tree, $form{maxdate}, \%quickdata);
  my $ignore_builds = &tb_load_ignorebuilds($tree);

  # Add a checkbox for the each of the other builds
  for my $other_build_name (sort keys %quickdata) {
    if ($other_build_name ne '' and $other_build_name ne $buildname
        and not $ignore_builds->{$other_build_name}) {
      print "<INPUT TYPE='checkbox' NAME='$other_build_name'>";
      print "$other_build_name<BR>\n";
    }
  }

  print "<INPUT Type='submit' name='submit' value='Add Comment'><BR>";
  print "</form>\n</body>\n</html>";
}
