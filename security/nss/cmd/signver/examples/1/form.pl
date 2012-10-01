#! /usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


print "Content-type: text/html\n\n";
print "<B>Server Name:</B> ", $ENV{'SERVER_NAME'}, "<BR>", "\n";
print "<B>Server Port:</B> ", $ENV{'SERVER_PORT'}, "<BR>", "\n";
print "<B>Server Software:</B> ", $ENV{'SERVER_SOFTWARE'}, "<BR>", "\n";
print "<B>Server Protocol:</B> ", $ENV{'SERVER_PROTOCOL'}, "<BR>", "\n";
print "<B>CGI Revision:</B> ", $ENV{'GATEWAY_INTERFACE'}, "<BR>", "\n";
print "<B>Browser:</B> ", $ENV{'HTTP_USER_AGENT'}, "<BR>", "\n";
print "<B>Remote Address:</B> ", $ENV{'REMOTE_ADDR'}, "<BR>", "\n";
print "<B>Remote Host:</B> ", $ENV{'REMOTE_HOST'}, "<BR>", "\n";
print "<B>Remote User:</B> ", $ENV{'REMOTE_USER'}, "<BR>", "\n";
print "You typed:\n";

while( $_ = <STDIN>) {
  print "$_";
}

