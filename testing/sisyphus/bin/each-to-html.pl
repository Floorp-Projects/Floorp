#!/usr/bin/perl
# -*- Mode: Perl; tab-width: 4; indent-tabs-mode: nil; -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
# The Original Code is Mozilla Automated Testing Code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bob@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
use URI::Escape;
use lib "$ENV{HOME}/projects/mozilla.com/test.mozilla.com/www/bin/";

my $hook       = shift @ARGV || usage("hook");
my $sitelist   = shift @ARGV || usage("sitelist");

open SITES, "<$sitelist" || die "unable to open $sitelist\n";

my @sites = <SITES>;

close SITES;

my $site;

my $chromeurl;
my $testUrl;

print "<html><body>\n";

foreach $site (@sites)
{

    chomp $site;

    my $spider = "chrome://spider/content/spider.xul?" .
        "depth=0&timeout=120&waittime=5&hooksignal=on&autostart=on&autoquit=on&javascripterrors=off&" .
        "javascriptwarnings=off&chromeerrors=on&xblerrors=on&csserrors=off&" .
        "scripturl=" . 
        uri_escape("http://" . 
                   $ENV{TEST_HTTP} . 
                   $hook) . 
                   "&" .
                   "url=" . uri_escape(uri_escape($site));

    print "<a href=\"$spider\">$site\</a><br>\n";
}
print "</html>\n";

sub usage
{
    my $arg = shift @_;
    
    print <<USAGE;

Error in $arg.

Usage: each-to-html.pl hook sitelist

Create an HTML page containing links to invoke Spider with the 
appropriate URL and parameters.

hook       - path to userhook script
sitelist   - path to text file containing list of sites, 
             one to a line
USAGE

exit 2;
}

1;
