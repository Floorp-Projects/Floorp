#!/usr/bin/perl
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
# The Original Code is the Netscape Mailstone utility, 
# released March 17, 2000.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1997-2000 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s):	Dan Christian <robodan@netscape.com>
#			Marcel DePaolis <marcel@netcape.com>
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#####################################################

# script to create test user accounts for Netscape Messaging Server 3, 4
#
# Given a set of parameters, this script will create an LDIF file
# for a number of email users of the form:
#	test1, test2, test3, ...	
#
# usage: perl create_accounts_ldif [users] [broadcast] [postmaster] [ options ]
#   [ -a allUsersAlias ]
#   [ -b basedn ]
#   [ -d maildomain ]
#   [ -f firstaccount ]
#   [ -k ]
#   [ -m mailhost ]
#   [ -n numaccounts ]
#   [ -o outputfile ]
#   [ -p password ]
#   [ -s storebase ]
#   [ -u username ]
#   [ -v ]
#   [ -w workload ]
#   [ -x maxstores ]
#   [ -3 ]
#
#perl -Ibin -- bin/makeusers.pl -d mailhost.example.com -m mailhost.example.com -b 'o=example.com' -u mailhost-test -n 100 -4 -o mailhost100.ldif

# Create the ldif for the user accounts and/or broadcast, postmaster account.
#
# The ldif then must be added to 
# the directory by hand (ldapadd, or through the dir admin server's
# Database Mgmt->Add entries from an ldif file).  

# A faster way
# is to export the existing directory, append the results of
# this script, and re-import the combined file.  This can be
# done using the following Netscape Directory Server commands:
#   stop-slapd
#   db2ldif outputfile
#   [ merge files ]
#   ldif2db inputfile  # for DS4 you would typically use -noconfig
#   start-sladp
#

print "Netscape Mailstone.\nCopyright (c) 1998,1999 Netscape Communications Corp.\n";

# server to be used in the internet mail address of the users
$domain = "newdomain.example.net";

# machine that will act as the user's mailhost
$mailhost = "mailhost.example.net";

# base dn for the user entries, e.g. o=Ace Industry,c=US 
$basedn = "o=Benchmark Lab, c=US";

# name of broadcast account
$bcastacct = "allusers";

# name of broadcast account
$postmasteraddr = "root\@localhost";

# base name to build user names, will construct test0, test1, ...
$username = "test%ld";

# user passwds, in SHA format, the passwd below is 'netscape'
#$userpassword =  "{SHA}aluWfd0LYY9ImsJb3h4afrI4AXk=";
# these can also be imported as cleartext
$userpassword = "netscape";

# 0: no numbered passwords, 1: number with userID
$maxpass = 0;

# first account to use
$firstaccount = 0;

# number of user accounts to create ($first - $first+$num-1)
$numaccounts = 1_000;

# For larger systems, spreading the users over multiple partitions
# is usually a good idea.  This example assumes you have
# created partitions named p0, p1, etc.

# store partition base name
$storebase = "p%ld";

# max store number (0 - maxstores-1), skip if 0
$maxstores = 0;

#default to msg 4 schemas
$usemsg4schema = 1;

#default to writing to stdout
$outfile = STDOUT;

# Initial UID for genpasswd
$firstuid = 1000;

sub usage {
    print "Usage: perl -Ibin -- makeusers [users] [broadcast] [postmaster]\n";
    print "\t[ -w workload ] [ -o outputFile ]\n";
    print "\t[ -d mailDomain ] [ -m mailHost ] [ -b baseDN ]\n";
    print "\t[ -u username ] [ -f firstAccount ] [ -n numAccounts ]\n";
    print "\t[ -p password ] [ -k ]\n";
    print "\t[ -s storeBase ] [ -x numStores ]\n";
    print "\t[ -a allUsersAlias ] [ -t postmasterAddress ]\n";
    print "\t[ -3 ]|[ -4 ]\n";
}

sub readWorkConfig {		# read the workload in, parse our params
    my $workloadfile = shift || die "Workload file name expected\n";

    do 'args.pl'|| die $@;
    readWorkloadFile ($workloadfile, \@workload)
	|| die "Error reading workload: $@\n";
				# assign all the parameters from the config
    $mailhost = $defaultSection->{SERVER}
	if ($defaultSection->{SERVER});
    if ($defaultSection->{ADDRESSFORMAT}) {
	my $addr = $defaultSection->{ADDRESSFORMAT};
	$addr =~ s/^.*@//;
	$domain = $addr;
    }
    if ($defaultSection->{LOGINFORMAT}) {
	my $user = $defaultSection->{LOGINFORMAT};
	#$user =~ s/%ld$//;
	$username = $user;
    }
    $numaccounts = $defaultSection->{NUMLOGINS}
        if ($defaultSection->{NUMLOGINS});
    $firstaccount = $defaultSection->{FIRSTLOGINS}
        if ($defaultSection->{FIRSTLOGINS});

    $userpassword = $defaultSection->{PASSWDFORMAT}
    if ($defaultSection->{SERVER});

    if ($userpassword =~ m/%ld/) { # see if numbered passwords
	$maxpass++;
	#$userpassword =~ s/%ld//g;
    }

    # what isnt set: basedn, storebase, maxstores, usemsg4schema
}

while (@ARGV) {
    $arg = shift(@ARGV);

    if ($arg =~ /^-a$/i) {	# allusers (broadcast) user name
	$bcastacct = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-b$/i) {	# LDAP base DN
	$basedn = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-d$/i) {	# mail domain
	$domain = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-f$/i) {	# initial account
	$firstaccount = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-k$/i) {	# use numbered passwords
	$maxpass++;
	next;
    }
    if ($arg =~ /^-h$/i) {	# help
	usage();
	exit 0;
    }
    if ($arg =~ /^-m$/i) {	# mail server name
	$mailhost = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-n$/i) {	# number of accounts
	$numaccounts = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-o$/i) {	# name output file
	my $fname = shift || die "File name expected\n";
	open OUTFILE, ">$fname" || die "Error opening file $@\n";
	$outfile = OUTFILE;
	next;			# use msg4 user admin schema
    }
    if ($arg =~ /^-p$/i) {	# password
	$userpassword = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-s$/i) {	# base name for above
	$storebase = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-t$/i) {	# postmaster address
	$postmasteraddress = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-u$/i) {	# user name base
	$username = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-v$/i) {	# be verbose
	$verbose++;
	next;
    }
    # do this when read, so that later switches can override
    if ($arg =~ /^-w$/i) {	# get a workload file
	readWorkConfig (shift(@ARGV));
	next;
    }
    if ($arg =~ /^-x$/i) {	# number of partitions (0 to skip)
	$maxstores = shift(@ARGV);
	next;
    }
    if ($arg =~ /^-3$/) {	# no msg4 schema
	$usemsg4schema = 0;
	next;
    }
    if ($arg =~ /^-4$/) {	# use msg4 user admin schema
	$usemsg4schema = 1;
	next;
    }
    if ($arg =~ /^users$/i) {
	$genusers++;
	next;
    }
    if ($arg =~ /^broadcast$/i) {
	$genbroadcast++;
	next;
    }
    if ($arg =~ /^passwd$/i) {
	$genpasswd++;
	next;
    }
    if ($arg =~ /^postmaster$/i) {
	$genpostmaster++;
	next;
    }

    print STDERR "Unknown argument $arg.  Use -h for help.\n";
    exit 1;
}

unless (($genusers) || ($genbroadcast) || ($genpasswd) || ($genpostmaster)) {
    print STDERR "Must specify mode [users] [broadcast] [postmaster] ...\n";
    usage();
    exit 0;
}

# specify number fields, if needed
unless ($username =~ /%ld/) {
    $username .= '%ld';
}
if (($maxpass) && !($userpassword =~ /%ld/)) {
    $userpassword .= '%ld';
}
if (($maxstores) && !($storename =~ /%ld/)) {
    $storename .= '%ld';
}

if ($verbose) {
    print STDERR "Here is the configuration:\n";
    print STDERR "baseDN='$basedn' \t";
    print STDERR (($usemsg4schema) ? "-4\n" : "-3\n");
    print STDERR "mailHost='$mailhost' \tdomain='$domain'\n";
    print STDERR "userName='$username' \tnumAccounts=$numaccounts \tfirstAccount=$firstaccount\n";
    print STDERR "userPassword='$userpassword'\n";
    print STDERR "allUsersAccount='$bcastacct'\n" if ($genbroadcast);
    print STDERR "postmasterAddress='$postmasterAddress'\n" if ($genpostmaster);
}


if ($genusers) {		# Create the user accounts
    $storenum=0;
    for ($i = $firstaccount; $i < $firstaccount+$numaccounts; $i++) {
	# build user account name
	my $acctname = $username;
	$acctname =~ s/%ld/$i/;	# insert user number
	my $password = $userpassword;
	$password =~ s/%ld/$i/;	# insert user number



	# MAKE SURE THERE ARE NO TRAILING SPACES IN THE LDIF
	my $extradata = "";

	if ($maxstores > 0) {	# assign them to a store
	    my $storename = $storebase;
	    $storename =~ s/%ld/$storenum/;
	    $extradata .= "mailmessagestore: $storename\n";
	    $storenum++;
	    $storenum=0 if ($storenum >= $maxstores);
	}

	$extradata .= "objectclass: nsMessagingServerUser\n"
	    if ($usemsg4schema);

print $outfile <<END;
dn: uid=$acctname, $basedn
userpassword: $password
givenname: $acctname
sn: $acctname
cn: $acctname
uid: $acctname
mail: $acctname\@$domain
mailhost: $mailhost
maildeliveryoption: mailbox
objectclass: top
objectclass: person
objectclass: organizationalPerson
objectclass: inetOrgPerson
objectclass: mailRecipient
$extradata
END
    }
}

if ($genbroadcast) {		# Create the broadcast account
    # MAKE SURE THERE ARE NO TRAILING SPACES IN THE LDIF
    my $password = $userpassword;
    $password =~ s/%ld//;	# strip user number
    # initial part
    print $outfile <<END;
dn: uid=$bcastacct, $basedn
userpassword: $password
givenname: $bcastacct
sn: $bcastacct
cn: $bcastacct
uid: $bcastacct
mail: $bcastacct\@$domain
mailhost: $mailhost
maildeliveryoption: forward
END

    # now put in each address
    for ($i = $firstaccount; $i < $firstaccount+$numaccounts; $i++) {
	# build user account name
	my $acctname = $username;
	$acctname =~ s/%ld/$i/;	# insert user number

	print $outfile "mailforwardingaddress: $acctname\@$domain\n";
    }

    # final part
    print $outfile <<END;
objectclass: top
objectclass: person
objectclass: organizationalPerson
objectclass: inetOrgPerson
objectclass: mailRecipient

END
}

if ($genpostmaster) {		# Create the postmaster account
    # MAKE SURE THERE ARE NO TRAILING SPACES IN THE LDIF
    print $outfile <<END;
dn: cn=postmaster, $basedn
cn: postmaster
mail: postmaster\@$domain
mailalternateaddress: postmaster\@$mailhost
mgrprfc822mailmember: $postmasterAddress
objectclass: top
objectclass: mailGroup
objectclass: groupOfUniqueNames

END
}

# mixing passwd output with the ldif output above would be quite silly
if ($genpasswd) {		# Create passwd entries for makeusers
    for ($i = $firstaccount; $i < $firstaccount+$numaccounts; $i++) {
	# build user account name
	my $acctname = $username;
	$acctname =~ s/%ld/$i/;	# insert user number
	my $password = $userpassword;
	$password =~ s/%ld/$i/;	# insert user number
	my $uid = $firstuid + $i;
	print $outfile "$acctname:$password:$uid:$uid:Mail user $acctname:/home/$acctname:/bin/sh\n";
    }
}

exit 0;
