#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Sam Ziegler <sam@ziegler.org>

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":
use vars %::COOKIE;


confirm_login();

print "Content-type: text/html\n\n";

if (Param("maintainer") ne $::COOKIE{'Bugzilla_login'}) {
    print "<H1>Sorry, you aren't the maintainer of this system.</H1>\n";
    print "And so, you aren't allowed to edit the parameters of it.\n";
    exit;
}


PutHeader("Saving new owners");

SendSQL("select program, value, initialowner from components order by program, value");

my @line;

foreach my $key (keys(%::FORM)) {
    $::FORM{url_decode($key)} = $::FORM{$key};
}

my @updates;
my $curIndex = 0;

while (@line = FetchSQLData()) {
    my $curItem = "$line[0]_$line[1]";
    if (exists $::FORM{$curItem}) {
        $::FORM{$curItem} =~ s/\r\n/\n/;
        if ($::FORM{$curItem} ne $line[2]) {
            print "$line[0] : $line[1] is now owned by $::FORM{$curItem}.<BR>\n";
            $updates[$curIndex++] = "update components set initialowner = '$::FORM{$curItem}' where program = '$line[0]' and value = '$line[1]'";
        }
    }
}

foreach my $update (@updates) {
    SendSQL($update);
}

print "OK, done.<p>\n";
print "<A HREF=\"editowners.cgi\">Edit the owners some more.</A>\n<P>\n";
print "<A HREF=\"query.cgi\">Go back to the query page.</A>\n";
