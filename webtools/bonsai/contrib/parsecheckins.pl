#!/usr/bin/perl -w
#
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is parsecheckins.pl, a Bonsai-output -> HTML parser.
#
# The Initial Developer of the Original Code is
#        J. Paul Reed (preed@sigkill.com).
#
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):    Dave Miller <justdave@netscape.com>
#

use strict;

## Suggested usage: 
##
## cat saved_bonsai_query_output | perl parsecheckins.pl > output 2> logfile &
## tail -f logfile
##
## Script progress gets dumped on stderr

## hostname of your Bugzilla installation
my $bzHost = 'your.bugzilla.host';

## Set to 0 if you don't want to query your bugzilla host for the titles of
## the bugs associated with these checkins
my $getTitles = 1;

## Set to the lowest valid bug number in your bz install; for non-bmo
## instals, should probably be 0
my $lowBugnum = 1000;

## SCRIPT STARTS HERE ##
my $input;

while (<>) {
   $input .= $_;
}

## remove the header
$input =~ s/.*?<TH WIDTH=\d+\%>Description\n.*?\n//is;

## remove the footer
$input =~ s/<\/TABLE><br>.*//is;

## remove lines that contain nothing but starting a new table
$input =~ s/<\/TABLE><TABLE.*?\n//igs;

## remap the linefeeds so they only occur between <tr> blocks
$input =~ s/\n/ /gs;
$input =~ s/<\/tr>\s*<tr>/<\/tr>\n<tr>/igs;

## ok, at this point, each <tr> is on a line by itself.
## let's load this into an array of lines.
my @lines = split(/\n/,$input);

## strip all lines that don't contain a description
@lines = grep { $_ =~ /<TD WIDTH=4\d% VALIGN=TOP/ } @lines;

## strip all lines that are checkins that don't reference a bugzilla bug
## comment this line out if you want bugs that don't reference a bz bug
@lines = grep { $_ =~ /show_bug.cgi/ } @lines;

## now we do a bunch of transformations on each line:
@lines = map {
   ## remove all rowspan markers (simplifies some of the parsing below)
   s/rowspan=[^ >]+//ig;

   ## Strip off the leading and ending <tr>'s 
   s/^<tr>//i;
   s/<\/tr>$//i;

   ## Strip out extra <br>s
   s/<br>/ /ig;

   ## remove the date column because we don't need it
   s/<TD width=2\%.*?<td/<td/i;

   ## remove the email address link; if you want to keep the email address,
   ## comment out the 2nd regex and use the first (commented out) one
   # s/<a[^>]+>(.*?)<\/a>/$1/i;
   s/<td width=\d+\%><a[^>]+>(.*?)<\/a>\s*<td/<td/i;

   ## remove the filename column because we don't need it
   s/<td width=45\%.*?<td/<td/i; 

   ## remove the version number, branch tag, and linecount columns
   s/<td width=2\%>(?:<font|\&nbsp).*?<td/<td/i;
   s/<td width=2\%><tt>.*?<td/<td/i;
   s/<td width=2\%>(?:<font|\&nbsp).*?<td/<td/i;

   ## Strip the last <td> preceding the commit message 
   s/<td width=\d+%\s+VALIGN=TOP\s*>//i;

   ## Strip off parts of the comment we don't care about
   s/Patch by.+//; 
   s/Fix by.+//;
   s/r=.+//;
   s/a=.+//;
   s/r,a=.+//;
   s/r\/a.+//;

   ## Get the titles for bugs
   
   if (/show_bug\.cgi\?id=(\d+)/) {
      my $bugnum = $1;
   
      if ($getTitles && $bugnum > $lowBugnum) {
         ## We use lynx so libwww doesn't have to get installed; 
         ## yay for quick and dirty!
         my $bugreport = `lynx -source http://$bzHost/show_bug.cgi?id=$bugnum`;

         if ($bugreport =~ /<TITLE>Bug \d+ - ([^<]+)<\/TITLE>/i) {
            my $title = $1;
            print STDERR "$bugnum: $title\n";
   
            s/(HREF="[^"]+")/$1 title="$title"/;
         }
         else {
            print STDERR "BUG REPORT LACKS TITLE (not authorized to view?): "
             . "$bugnum\n";
         }
      }
   }
   else {
      print STDERR "NO BUG LINK FOUND: $_\n";
   }

   ## Clean up extra spaces
   s/^\s+//;
   s/\s+$//;

   ## Standardize the separator format:
   s/(\d+<\/A>)\s*.\s+/$1 \- /;

   ## return the final result; should be the comment message, titles and all.
   $_;

} @lines;

## Uncomment this if you want the bugs listed oldest first 
#@lines = reverse @lines;

## Play with the output format here; now, it gives an unenumerated html list

print "<ul>\n";

foreach my $line (@lines) {
   print "<li>$line</li>\n";
}

print "</ul>\n";
