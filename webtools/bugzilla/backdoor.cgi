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
# Contributor(s): Terry Weissman <terry@mozilla.org>

# Provides a silly 'back-door' mechanism to let me automatically insert
# bugs from the netscape bugsystem.  Other installations of Bugzilla probably
# don't need to worry about this file any.

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars %::versions;


ConnectToDatabase();

print "Content-type: text/plain\n\n";

# while (my ($key,$value) = each %ENV) {
#     print "$key=$value\n";
# }

my $host = $ENV{'REMOTE_ADDR'};

SendSQL("select passwd from backdoor where host = '$host'");
my $passwd = FetchOneColumn();
if (!defined $passwd || !defined $::FORM{'passwd'} ||
    $passwd ne crypt($::FORM{'passwd'}, substr($passwd, 0, 2))) {
    print "Who are you?\n";
    print "Env:\n";
    while (my ($key,$value) = each %ENV) {
        print "$key=$value\n";
    }
    print "\nForm:\n";
    while (my ($key,$value) = each %::FORM) {
        print "$key=$value\n";
    }
    exit;
}



my $prod = $::FORM{'product'};
my $comp = $::FORM{'component'};
my $version = $::FORM{'version'};

GetVersionTable();


sub Punt {
    my ($label, $value) = (@_);
    my $maintainer = Param("maintainer");
    print "I don't know how to move into Bugzilla a bug with a $label of $value.
If you really do need to do this, speak to $maintainer and maybe he
can teach me.";
    exit;
}


# Do remapping of things from BugSplat world to Bugzilla.

if ($prod eq "Communicator") {
    $prod = "Mozilla";
    $version = "other";
}

    
# Validate fields, and whine about things that we apparently couldn't remap
# into something legal.

    
if (!defined $::components{$prod}) {
    Punt("product", $prod);
}
if (lsearch($::components{$prod}, $comp) < 0) {
    Punt("component", $comp);
}
if (lsearch($::versions{$prod}, $version) < 0) {
    Punt("version", $version);
}


$::FORM{'product'} = $prod;
$::FORM{'component'} = $comp;
$::FORM{'version'} = $version;


$::FORM{'long_desc'} =
    "(This bug imported from BugSplat, Netscape's internal bugsystem.  It
was known there as bug #$::FORM{'bug_id'}
http://scopus.netscape.com/bugsplat/show_bug.cgi?id=$::FORM{'bug_id'}
Imported into Bugzilla on " . time2str("%D %H:%M", time()) . ")

" . $::FORM{'long_desc'};
    

$::FORM{'reporter'} =
    DBNameToIdAndCheck("$::FORM{'reporter'}\@netscape.com", 1);
$::FORM{'assigned_to'} = 
    DBNameToIdAndCheck("$::FORM{'assigned_to'}\@netscape.com", 1);
    

my @list = ('reporter', 'assigned_to', 'product', 'version', 'rep_platform',
            'op_sys', 'bug_status', 'bug_severity', 'priority', 'component',
            'short_desc', 'long_desc', 'creation_ts', 'delta_ts');

my @vallist;
foreach my $i (@list) {
    push @vallist, SqlQuote($::FORM{$i});
}

my $query = "insert into bugs (" .
    join(',', @list) .
    ") values (" .
    join(',', @vallist) .
    ")";


SendSQL($query);

SendSQL("select LAST_INSERT_ID()");
my $zillaid = FetchOneColumn();

print "Created bugzilla bug $zillaid\n";
system("./processmail $zillaid < /dev/null > /dev/null 2> /dev/null &");
