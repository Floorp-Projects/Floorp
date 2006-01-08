#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Despot Account Administration System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>


# This goes through the database, and sends whiny mail to anyone who has not
# changed their password.

$sourcedir = $0;
$sourcedir =~ s:/[^/]*$::;      # Remove last word, and slash before it.
if ($sourcedir eq "") {
    $sourcedir = ".";
}

chdir $sourcedir || die "Couldn't chdir to $sourcedir";


use DBI;
require 'utils.pl';

use vars qw( $db_host $db_name $db_user $db_pass );
do "config.pl" || die "Couldn't load config file";
my $dsn = "DBI:mysql:host=$db_host;database=$db_name";
$::db = DBI->connect($dsn, $db_user, $db_pass)
    || die "Can't connect to database server";

$query = $::db->prepare("SELECT email, neednewpassword FROM users");
$query->execute();

$changedsomething = 0;

while (@row = $query->fetchrow_array()) {
    ($email,$neednew) = @row;
    if ($neednew eq "Yes") {
        print "$email\n";
	open(MAIL, "| /bin/mail $email");
	print MAIL "Subject: [despot] Your mozilla.org account needs to have its password changed.";
	print MAIL q{
(This message has been automatically generated.)

You have a mozilla.org account, which is used to access various things at
mozilla.org.  Currently, your account has been disabled, and will stay 
disabled until you change your password.

To change your password, please visit
http://despot.mozilla.org/despot.cgi.
If you can't get that to work, send mail to staff@mozilla.org.  If it's
an emergency, try paging terry at page-terry@netscape.com.

Unless you take some action, you will probably get this very same email
every day.
};

	close MAIL;
    }
}

if ($changedsomething) {
    system("./syncit.pl");
}
