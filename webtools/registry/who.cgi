#!/usr/bonsaitools/bin/perl
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
# The Original Code is the Application Registry.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

require 'lloydcgi.pl';
$|=1;

$rawname= $form{"email"};
$email = $rawname;
$enc_email = url_encode($email);

$email =~ s/%/@/ if $email !~ '@';
$username = $email;
$username =~ s/@.*$//;
$hostname = $email;
$hostname =~ s/^.*@//;
$hostname = "netscape.com" if $hostname eq $email;

$full_name = $email;
$enc_full_name = $email;    #this should be url encoded

@extra_url = ();
@extra_text = ();

print "Content-type: text/html\n\n<HTML>\n";
print "<base target='_top'>\n";

print "<table border=1 cellspacing=1 cellpadding=3><tr><td>\n";
print "$email\n";
print "<SPACER TYPE=VERTICAL SIZE=5>\n";

&load_extra_data;

for ($ii=0; $ii < @extra_text; $ii++) {
  $text = $extra_text[$ii];    
  if ($url = $extra_url[$ii]) {
    print "<dt><a href=$url>$text</a>\n";
  }
  else {
    print "<dt>$text\n";
  }
}

#print "<hr>\n" if @extra_text;

print qq(
<dt><A HREF='../bonsai/cvsquery.cgi?module=allrepositories&branch=&dir=&file=&who=$enc_email&sortby=Date&hours=2&date=week'>
    Check-ins within 7 days</A>
<dt><A HREF='mailto:$username\@$hostname'>
    Send Mail</A>
</table>
);

sub load_extra_data {
  my $ii, $url, $text;

  $ii = 0;
  for ($ii=0;
       ($url = $form{"u$ii"}) ne '' || $form{"t$ii"} ne '';
       $ii++) {
    $text = $form{"t$ii"};
    $text = $url if $url_text eq '';
    $extra_url[$ii] = $url;
    $extra_text[$ii] = $text;
  }

  # Adding the following for tinderbox
  if (($last_checkin_data = $form{d}) ne '') {
    my ($module,$branch,$root,$mindate,$maxdate) = split /\|/, $last_checkin_data;

    $extra_url[++$ii] = "../bonsai/cvsquery.cgi?module=$module"
                      . "&branch=$branch&cvsroot=$root&date=explicit"
                      . "&mindate=$mindate&maxdate=$maxdate&who=$enc_email";
    $extra_text[$ii] = "Last check-in";

    $extra_url[++$ii] = "../bonsai/cvsquery.cgi?module=$module"
                      . "&branch=$branch&cvsroot=$root&date=day&who=$enc_email";
    $extra_text[$ii] = "Check-ins within 24 hours";
  }
}
