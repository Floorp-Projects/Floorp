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
# The Original Code is Netscape Security Services for Java.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2001 Netscape Communications Corporation.  All
# Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable
# instead of those above.  If you wish to allow use of your
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
use strict;

my $passwd = "netscape";
my $passwdfile = "password";
my $dist = "$ARGV[0]";
my $java = "$ARGV[1]";
my $ssltesthost = "trading.etrade.com";
my $signingToken = "Internal Key Storage Token";
(-d $dist) or die "Directory '$dist' does not exist\n";
(-f $java) or die "'$java' does not exist\n";

my @env_vars = (
    "LD_LIBRARY_PATH",
    "CLASSPATH"
);
my $modutil = "$dist/bin/modutil";

print "*****ENVIRONMENT*****\n";
print "\$(DIST)=$dist\n";
print "\$(JAVA)=$java\n";
foreach my $var (@env_vars) {
    print "$var=$ENV{$var}\n";
}
print "modutil is $modutil\n";
print "password is $passwd\n";
print "*********************\n";

#
# Make the test database directory
#
my $testdir = "testdir";
if( ! -d $testdir ) {
    mkdir( $testdir, 0755 ) or die;
}
{
    chdir "testdir" or die;
    my @dbfiles = 
        ("./cert7.db", "./key3.db", "./secmod.db", "./secmodule.db");
    unlink @dbfiles;
    (grep{ -f } @dbfiles)  and die "Unable to delete old database files";
    chdir ".." or die;
    my $result = system("cp $dist/lib/libnssckbi.so testdir"); $result >>= 8;
    $result and die "Failed to copy builtins library";
}
my $result;
#$result = system("$modutil -dbdir $testdir -create -force"); $result >>= 8;
#$result and die "modutil returned $result";
#system("echo $passwd > $testdir/$passwdfile");
#$result = system("$modutil -dbdir $testdir -force -changepw ".
#    "\"NSS Certificate DB\" -pwfile $testdir/$passwdfile ".
#    "-newpwfile $testdir/$passwdfile"); $result >>= 8;
#$result and die "modutil returned $result";
#$result = system("$modutil -force -dbdir $testdir -add builtins -libfile /u/nicolson/local/jss/mozilla/dist/SunOS5.8_DBG.OBJ/lib/libnssckbi.so");
#$result and die "modutil returned $result";
$result = system("$java org.mozilla.jss.tests.SetupDBs testdir"); $result >>=8;
$result and die "SetupDBs returned $result";

#
# test sockets
#
$result = system("$java socketTest $testdir $ssltesthost"); $result >>=8;
$result and die "socketTest returned $result";

# test key gen
#
$result = system("$java org.mozilla.jss.tests.TestKeyGen $testdir");$result >>=8;
$result and die "TestKeyGen returned $result";

# test signing
#
$result = system("$java org.mozilla.jss.tests.SigTest $testdir " .
            "\"$signingToken\""); $result >>=8;
$result and die "SigTest returned $result";
