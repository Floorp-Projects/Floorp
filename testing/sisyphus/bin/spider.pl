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
# The Original Code is Mozilla Automated Testing Code
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

# make unbuffered
select STDERR; $| = 1;
select STDOUT; $| = 1;

my $TEST_BIN  = $ENV{TEST_BIN} || "/work/mozilla/mozilla.com/test.mozilla.com/www/bin/";

use File::Temp qw/ tempfile tempdir /;
use File::Basename;
use Getopt::Mixed "nextOption";
use URI::Escape;
use Time::HiRes qw(sleep);

my $DEBUG = 0;

my $product      = "";
my $executable   = "";
my $profile      = "";
my $url          = "";
my $domain       = "";
my $depth        = 0;
my $timeout      = 0;
my $wait         = 0;
my $hook         = "";
my $start        = "off";
my $quit         = "off";
my $robot        = "off";
my $debug        = "off";
my $jserrors     = "off";
my $jswarnings   = "off";
my $chromeerrors = "off";
my $xblerrors    = "off";
my $csserrors    = "off";

parse_options();

my $hookon       = ($hook ? "on" : "off");
my $runtime = dirname($executable);

chdir $runtime or 
    die "spider.pl: unable to change directory to runtime $runtime";


my $chromeurl;

my $testUrl;

my $spider = "chrome://spider/content/spider.xul?" .
    "domain=$domain&" .
    "depth=$depth&" . 
    "timeout=$timeout" .
    "&waittime=$wait&" .
    "autostart=$start&" .
    "autoquit=$quit&" .
    "javascripterrors=$jserrors&" .
    "javascriptwarnings=$jswarnings&" .
    "chromeerrors=$chromeerrors&" .
    "xblerrors=$xblerrors&" .
    "csserrors=$csserrors&" .
    "hooksignal=$hookon&" .
    "url=" . uri_escape(uri_escape($url));

if ($hook)
{
    $spider .= "&scripturl=" .  uri_escape($hook);
}

my @args;
my $rc;
my $signal;
my $dumped;

if ($product eq "thunderbird")
{
    @args = ($executable, "-P", $profile, $spider);
}
else
{
    @args = ($executable, "-P", $profile, "-chrome", $spider);
}

system @args;
$rc = $? >> 8;

if ($rc == 99)
{
    exit $rc;
}

sub usage
{
    my $arg   = shift @_;
    my $value = shift @_;

    print <<USAGE;

    Error in $arg: $value

Usage: spider.pl --product=product --executable=exe --profile=profile --url=url --domain=domain --depth=depth --timeout=timeout --wait=wait --hook=hook 
--start --quit --robot --debug --jserrors --jswarnings --chromeerrors --xblerrors --csserrors

Invoke Spider a set of urls.

product      - firefox|thunderbird
exe          - path to browser executable
profile      - profile name
url          - url to spider
domain       - domain
depth        - depth each url is to be spidered
timeout      - time in seconds before Spider times out loading a page
wait         - pause spider after each page
hook         - url to Spider userhook script
start        - auto start
quit         - auto quit
robot        - obey robots.txt
debug        - spider debug
jserrors     - javascript errors
jswarnings   - javascript warnings
chromeerrors - chrome errors
xblerrors    - xbl errors
csserrors    - css errors
USAGE
exit(2);
}

sub parse_options {
    my ($option_data, $option, $value, $lastop);

    $option_data = 'product=s executable=s profile=s url=s domain=i depth=i timeout=i wait=i hook=s wait=i start  quit robot debug jserrors jswarnings chromeerrors xblerrors csserrors';

    Getopt::Mixed::init ($option_data);
    $Getopt::Mixed::order = $Getopt::Mixed::RETURN_IN_ORDER;

    $valgrind = 0;

    while (($option, $value) = nextOption()) 
    {
        if ($option eq "product")
        {
            $product = $value;
        }
        elsif ($option eq "executable")
        {
            $executable = $value;
        }
        elsif ($option eq "profile")
        {
            $profile = $value;
        }
        elsif ($option eq "url")
        {
            $url = $value;
        }
        elsif ($option eq "domain")
        {
            $domain = $value;
        }
        elsif ($option eq "depth")
        {
            $depth = $value;
        }
        elsif ($option eq "timeout")
        {
            $timeout = $value;
        }
        elsif ($option eq "wait")
        {
            $wait = $value;
        }
        elsif ($option eq "hook")
        {
            $hook = $value;
        }
        elsif ($option eq "start")
        {
            $start = "on";
        }
        elsif ($option eq "quit")
        {
            $quit = "on";
        }
        elsif ($option eq "robot")
        {
            $robot = "on";
        }
        elsif ($option eq "debug")
        {
            $debug = "on";
        }
        elsif ($option eq "jserrors")
        {
            $jserrors = "on";
        }
        elsif ($option eq "jswarnings")
        {
            $jswarnings = "on";
        }
        elsif ($option eq "chromeerrors")
        {
            $chromeerrors = "on";
        }
        elsif ($option eq "xblerrors")
        {
            $xblerrors = "on";
        }
        elsif ($option eq "csserrors")
        {
            $csserrors = "on";
        }
        $lastopt = $option;
    }

    Getopt::Mixed::cleanup();

    if ($product ne "firefox" && $product ne "thunderbird")
    {
        usage("product", $product);
    }
}
1;
