#####
# TODO:
# test scripts should take passwd on command line & login manually.
# fix socket test: unknown issuer
# get rid of modutil, do it from java.
####
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
