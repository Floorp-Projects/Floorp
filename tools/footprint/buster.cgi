#!/usr/bin/perl
#
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
# The Original Code is buster.cgi, released
# November 13, 2000.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Waterson <waterson@netscape.com>
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

# This is a modified version of Chris Hofmann's <chofmann@netscape.com>
# infamous "browser buster" test harness. It's a bit simpler (CGI
# instead of using cookies; IFRAME instead of FRAMESET), and has some
# extra parameters that make it a bit easier to test with, but it's
# pretty faithful otherwise.
#
# It accepts a couple of parameters, including
#
#   file=<filename> Set this to the name of the file containing
#     the URLs that you want the buster to cycle through. This
#     might be a security hole, so don't run this script on a
#     server with s3kret stuff on it, mmkay?
#
#   page=<number> This is used to maintain state, and is the line
#     number in the file that the buster will pull up in the
#     IFRAME. Set if by hand if you need to for some reason.
#
#   last=<number> The buster will run until it's exhausted all
#     the URLs in the file, or until it reaches this line in the
#     file; e.g., setting it to "5" will load five URLs.
#
#   refresh=<number> The timeout (in seconds) to wait before doing
#     a page refresh, and thus loading the next URL. Defaults to
#     thirty.

use CGI;

# Find the page'th URL in the file with the specified name
sub FindURL($$)
{
    my ($file, $page) = @_;

    open URLS, $file
        || die("can't open $::File");

    LINE: while (<URLS>) {
        next LINE if /^#/;
        last LINE unless --$page;
    }

    close URLS;

    chomp;
    return $_;
}

# Scrape parameters
$::Query = new CGI;

$::File = $::Query->param("file");
$::File = "top100.txt" unless $::File;

$::Page = $::Query->param("page");
$::Page = 0 unless $::Page;
$::URL = FindURL($::File, ++$::Page);

$::Last = $::Query->param("last");
$::Last = -1 unless $::Last;

$::Refresh = $::Query->param("refresh");
$::Refresh = 30 unless $::Refresh;

# Header
print qq{Content-type: text/html

<html>
<head>
};

# Meat
if ($::URL && ($::Page <= $::Last || $::Last == -1)) {
    # Make a web page that'll load $::URL in an IFRAME, with
    # a meta-refresh that'll reload us again in short order.
    print qq{<meta http-equiv="Pragma" content="no-cache">
<meta http-equiv="refresh" content="$::Refresh;url=buster.cgi?file=$::File&page=$::Page&last=$::Last&refresh=$::Refresh">
<title>BrowserBuster II: $::URL</title>
<style type="text/css">
body {
  overflow: hidden;
  border: 0;
  margin: 0;
}
</style>
</head>
<script>
dump("+++ loading $::URL\\n");
</script>
<body>
};
    print "$::File: $::URL";
    if ($::Last != -1) {
        print " ($::Page of $::Last)<br>";
    }
    print qq{
<iframe width="100%" height="100%" src="$::URL">
};
}
else {
    # Make a web page that'll close the current browser
    # window, terminating the test app.
    print qq{<head>
<title>BrowserBuster II: Done!</title>
<body onload="window.close();">
All done!
};
}

# Footer
print qq{</body>
</html>
};
