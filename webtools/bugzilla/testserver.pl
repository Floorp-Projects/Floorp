#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# Contributor(s): Joel Peshkin <bugreport@peshkin.net>
#                 Byron Jones <byron@glob.com.au>

# testserver.pl is involked with the baseurl of the Bugzilla installation
# as its only argument.  It attempts to troubleshoot as many installation
# issues as possible.

use Socket;
my $envpath = $ENV{'PATH'};
use lib ".";
use strict;
require "globals.pl";
eval "require LWP; require LWP::UserAgent;";
my $lwp = $@ ? 0 : 1;

$ENV{'PATH'}= $envpath;

if ((@ARGV != 1) || ($ARGV[0] !~ /^https?:/))
{
    print "Usage: $0 <URL to this Bugzilla installation>\n";
    print "e.g.:  $0 http://www.mycompany.com/bugzilla\n";
    exit(1);
}


# Try to determine the GID used by the webserver.
my @pscmds = ('ps -eo comm,gid', 'ps -acxo command,gid');
my $sgid = 0;
if ($^O !~ /MSWin32/i) {
    foreach my $pscmd (@pscmds) {
        open PH, "$pscmd 2>/dev/null |";
        while (my $line = <PH>) {
            if ($line =~ /^(?:\S*\/)?httpd\s+(\d+)$/) {
                $sgid = $1 if $1 > $sgid;
            }
        }
        close(PH);
    }
}

# Determine the numeric GID of $webservergroup
my $webgroupnum = 0;
if ($::webservergroup =~ /^(\d+)$/) {
    $webgroupnum = $1;
} else {
    eval { $webgroupnum = (getgrnam $::webservergroup) || 0; };
}

# Check $webservergroup against the server's GID
if ($sgid > 0) {
    if ($::webservergroup eq "") {
        print 
"WARNING \$webservergroup is set to an empty string.
That is a very insecure practice. Please refer to the
Bugzilla documentation.\n";
    } elsif ($webgroupnum == $sgid) {
        print "TEST-OK Webserver is running under group id in \$webservergroup.\n";
    } else {
        print 
"TEST-WARNING Webserver is running under group id not matching \$webservergroup.
This if the tests below fail, this is probably the problem.
Please refer to the webserver configuration section of the Bugzilla guide. 
If you are using virtual hosts or suexec, this warning may not apply.\n";
    }
} elsif ($^O !~ /MSWin32/i) {
   print "TEST-???? Could not identify group webserver is using.\n";
}


# Try to fetch a static file (ant.jpg)
$ARGV[0] =~ s/\/$//;
my $url = $ARGV[0] . "/ant.jpg";
if (fetch($url)) {
    print "TEST-OK Got ant picture.\n";
} else {
    print 
"TEST-FAILED Fetch of ant.jpg failed
Your webserver could not fetch $url.
Check your webserver configuration and try again.\n";
    exit(1);
}

# Try to execute a cgi script
my $response = fetch($ARGV[0] . "/testagent.cgi");
if ($response =~ /^OK/) {
    print "TEST-OK Webserver is executing CGIs.\n";
} elsif ($response =~ /^#!/) {
    print 
"TEST-FAILED Webserver is fetching rather than executing CGI files.
Check the AddHandler statement in your httpd.conf file.\n";
    exit(1);
} else {
    print "TEST-FAILED Webserver is not executing CGI files.\n"; 
}

# Make sure that webserver is honoring .htaccess files
$::localconfig =~ s~^\./~~;
$url = $ARGV[0] . "/$::localconfig";
$response = fetch($url);
if ($response) {
    print 
"TEST-FAILED Webserver is permitting fetch of $url.
This is a serious security problem.
Check your webserver configuration.\n";
    exit(1);
} else {
    print "TEST-OK Webserver is preventing fetch of $url.\n";
}

sub fetch {
    my $url = shift;
    my $rtn;
    if ($lwp) {
        my $req = HTTP::Request->new(GET => $url);
        my $ua = LWP::UserAgent->new;
        my $res = $ua->request($req);
        $rtn = ($res->is_success ? $res->content : undef);
    } elsif ($url =~ /^https:/i) {
        die("You need LWP installed to use https with testserver.pl");
    } else {
        my($host, $port, $file) = ('', 80, '');
        if ($url =~ m#^http://([^:]+):(\d+)(/.*)#i) {
            ($host, $port, $file) = ($1, $2, $3);
        } elsif ($url =~ m#^http://([^/]+)(/.*)#i) {
            ($host, $file) = ($1, $2);
        } else {
            die("Cannot parse url");
        }

        my $proto = getprotobyname('tcp');
        socket(SOCK, PF_INET, SOCK_STREAM, $proto);
        my $sin = sockaddr_in($port, inet_aton($host));
        if (connect(SOCK, $sin)) {
            binmode SOCK;
            select((select(SOCK), $| = 1)[0]);

            # get content
    
            print SOCK "GET $file HTTP/1.0\015\012host: $host:$port\015\012\015\012";
            my $header = '';
            while (defined(my $line = <SOCK>)) {
                last if $line eq "\015\012";
                $header .= $line;
            }
            my $content = '';
            while (defined(my $line = <SOCK>)) {
                $content .= $line;
            }

            my ($status) = $header =~ m#^HTTP/\d+\.\d+ (\d+)#;
            $rtn = (($status =~ /^2\d\d/) ? $content : undef);
        }
    }
    return($rtn);
}

