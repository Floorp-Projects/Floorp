#!/usr/bonsaitools/bin/perl -w
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
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

require 'lloydcgi.pl';
require 'utils.pl';

# use strict;

my $db = ConnectToDatabase();

$| = 1;

if (!defined $form{'repositoryid'}) {
    print "Content-type: text/html

This will create a cryptic report of all the people who have ever
touched each file within a directory heirarchy.

<P>

<form>
Repository: <select name='repositoryid'>
";

    my $query = $db->Query("select id, repository from repositories order by id");
    my @row;
    while (@row = $query->fetchrow()) {
        my ($id, $name) = (@row);
        print "<option value=$id>$name\n";
    }
    print "</select><br>
Directory: <input size=60 name=dir>
<input type=submit value='Submit'>
</form>
";
    exit;
}


print "Content-type: text/plain\n\n";

my $qstr = "select distinct who, dir, file from checkins, people, dirs, files where repositoryid = $form{'repositoryid'} and dirid=dirs.id and dir like '$form{'dir'}%' and fileid=files.id and whoid=people.id order by dir, file";
my $query = $db->Query($qstr);

if (!$query) {
    die "Bad query: $qstr \n\n $::db_errstr";
}


my @row;
my $last = "";
while (@row = $query->fetchrow()) {
    my ($who, $dir, $file) = (@row);
    my $cur = "$dir/$file";
    if ($cur ne $last) {
        print "\n$cur\n";
        $last = $cur;
    }
    print "$who\n";
}

