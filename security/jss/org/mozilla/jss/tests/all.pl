#!/usr/bin/perl
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
# The Original Code is Netscape Security Services for Java.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

use Socket;

my $java;

# dist <dist_dir>
# release <java release dir> <nss release dir> <nspr release dir>
# auto   (test the current build directory)

sub usage {
    print "Usage:\n";
    print "$0 dist <dist_dir>\n";
    print "$0 release <jss release dir> <nss release dir> "
        . "<nspr release dir>\n";
    print "$0 auto\n";
    exit(1);
}

# Force Perl to do unbuffered output
# to avoid having Java and Perl output out of sync.
$| = 1;

# Global variables
my $testdir;
my $testrun = 0;
my $testpass = 0;
my $nss_lib_dir;
my $dist_dir;
my $pathsep        = ":";
my $scriptext      = "sh";
my $exe_suffix     = "";
my $lib_suffix     = ".so";
my $lib_jss        = "libjss";
my $jss_rel_dir    = "";
my $jss_classpath  = "";
my $portJSSEServer = 2876;
my $portJSSServer  = 2897;
my $hostname       = localhost;

# checkPort will return a free Port number
# otherwise it will die after trying 10 times. 
sub checkPort {
   my ($p) = @_; 
   my $localhost = inet_aton("localhost");
   my $max = $p + 10; # try to find a port 10 times
   my $port = sockaddr_in($p, $localhost);

   #create a socket 
   socket(SOCKET, PF_INET, SOCK_STREAM, getprotobyname('tcp')) 
   || die "Unable to create socket: $!\n";

   #loop until you find a free port
   while (connect(SOCKET, $port) && $p < $max) {
         print "$p is in use trying to find another port.\n";
         $p = $p + 1;
         $port = sockaddr_in($p, $localhost);
   }
   close SOCKET || die "Unable to close socket: $!\n";
   if ($p == $max) { 
      die "Unable to find a free port..\n";
   }

   return $p;
}


sub setup_vars {
    my $argv = shift;

    my $osname = `uname -s`;

    my $truncate_lib_path = 1;
    if( $osname =~ /HP/ ) {
        $ld_lib_path = "SHLIB_PATH";
        $scriptext = "sh";
        $lib_suffix = ".sl";
    } elsif( $osname =~ /win/i ) {
        $ld_lib_path = "PATH";
        $truncate_lib_path = 0;
        $pathsep = ";";
        $exe_suffix = ".exe";
        $lib_suffix = ".dll";
        $lib_jss    = "jss";
    } else {
        $ld_lib_path = "LD_LIBRARY_PATH";
        $scriptext = "sh";
    }

    my $jar_dbg_suffix = "_dbg";
    my $dbg_suffix     = "_DBG";
    $ENV{BUILD_OPT} and $dbg_suffix = "";
    $ENV{BUILD_OPT} and $jar_dbg_suffix = "";

    $ENV{CLASSPATH}  = "";
    $ENV{$ld_lib_path} = "" if $truncate_lib_path;


    if( $$argv[0] eq "dist" ) {
        shift @$argv;
        $dist_dir = shift @$argv or usage("did not provide dist_dir");

        $ENV{CLASSPATH} .= "$dist_dir/../xpclass$jar_dbg_suffix.jar";
        ( -f $ENV{CLASSPATH} ) or die "$ENV{CLASSPATH} does not exist";
        $ENV{$ld_lib_path} = $ENV{$ld_lib_path} . $pathsep . "$dist_dir/lib";
        $nss_lib_dir   = "$dist_dir/lib";
        $jss_rel_dir   = "$dist_dir/../classes$dbg_suffix/org";
        $jss_classpath = "$dist_dir/../xpclass$jar_dbg_suffix.jar";

        # Test directory = $DIST_DIR
        # make it absolute path
    } elsif( $$argv[0] eq "auto" ) {
        my $dist_dir = `make dist_dir`;
        my $obj_dir = `make obj_dir`;
        chomp($dist_dir);
        chomp($obj_dir);
        chomp( $dist_dir = `(cd $dist_dir ; pwd)`);
        chomp( $obj_dir = `(cd $obj_dir ; pwd)`);

        $nss_lib_dir   = "$obj_dir/lib";
        $jss_rel_dir   = "$dist_dir/classes$dbg_suffix/org";
        $jss_classpath = "$dist_dir/xpclass$jar_dbg_suffix.jar";

        $ENV{CLASSPATH} .= "$dist_dir/xpclass$jar_dbg_suffix.jar";
        ( -f $ENV{CLASSPATH} ) or die "$ENV{CLASSPATH} does not exist";
        #$ENV{$ld_lib_path} = $ENV{$ld_lib_path} . $pathsep . "$obj_dir/lib";
        $ENV{$ld_lib_path} = "$obj_dir/lib";
    } elsif( $$argv[0] eq "release" ) {
        shift @$argv;

        $jss_rel_dir     = shift @$argv or usage();
        my $nss_rel_dir  = shift @$argv or usage();
        my $nspr_rel_dir = shift @$argv or usage();

        $ENV{CLASSPATH} .= "$jss_rel_dir/../xpclass$jar_dbg_suffix.jar";
        $ENV{$ld_lib_path} =
            "$jss_rel_dir/lib$pathsep$nss_rel_dir/lib$pathsep$nspr_rel_dir/lib"
            . $pathsep . $ENV{$ld_lib_path};
        $nss_lib_dir = "$nss_rel_dir/lib";
        $jss_classpath = "$jss_rel_dir/../xpclass$jar_dbg_suffix.jar";
    } else {
        usage();
    }

    if ($ENV{PORT_JSSE_SERVER}) {
       $portJSSEServer = $ENV{PORT_JSSE_SERVER};
    }

    if ($ENV{PORT_JSS_SERVER}) { 
       $portJSSServer = $ENV{PORT_JSS_SERVER};
    }

    unless( $ENV{JAVA_HOME} ) {
        print "Must set JAVA_HOME environment variable\n";
        exit(1);
    }

    #
    # Use 64-bit Java on AMD64.
    #

    $java = "$ENV{JAVA_HOME}/jre/bin/java$exe_suffix";
    my $java_64bit = 0;
    if ($osname eq "SunOS") {
        if ($ENV{USE_64}) {
            my $cpu = `/usr/bin/isainfo -n`;
            if ($cpu == "amd64") {
                $java = "$ENV{JAVA_HOME}/jre/bin/amd64/java$exe_suffix";
                $java_64bit = 1;
        }
    }
    }
    (-f $java) or die "'$java' does not exist\n";
    $java = $java . $ENV{NATIVE_FLAG};

    if ($ENV{USE_64} && !$java_64bit) {
    $java = $java . " -d64";
    }

    $pwfile = "passwords";

    # testdir = /<ws>/mozilla/tests_results/jss/<hostname>.<version>
    # $all_dir = Directory where all.pl is
    my $all_dir = `dirname $0`;
    chomp $all_dir;
    # Find where mozilla directory is
    my $base_mozilla = $all_dir . "/../../../../../..";
    my $abs_base_mozilla = `cd $base_mozilla; pwd`;
    chomp $abs_base_mozilla;
    # $result_dir = Directory where the results are (mozilla/tests_results/jss)
    my $result_dir =  $abs_base_mozilla . "/tests_results";
    if (! -d $result_dir) {
       mkdir( $result_dir, 0755 ) or die;
    }
    my $result_dir =  $abs_base_mozilla . "/tests_results/jss";
    if( ! -d $result_dir ) {
      mkdir( $result_dir, 0755 ) or die;
    }
    # $host = hostname
    my $host = `uname -n`;
    $host =~ s/\..*//g;
    chomp $host;
    # $version = test run number (first = 1). Stored in $result_dir/$host
    my $version_file = $result_dir ."/" . $host;
    if ( -f $version_file) {
      open (VERSION, "< $version_file") || die "couldn't open " . $version_file . " for read";
      $version = <VERSION>;
      close (VERSION);
      chomp $version;
      $version = $version + 1;
    } else {
      $version = 1;
    }
    # write the version in the file
    open (VERSION, "> $version_file")  || die "couldn't open " . $version_file . " for write";
    print VERSION $version . "\n";
    close (VERSION);
    # Finally, set $testdir
    $testdir = $result_dir . "/" . $host . "." . $version;

    #in case multiple tests are being run on the same machine increase  
    #the port numbers with version number + 20

    $portJSSEServer = $portJSSEServer + $version + 20;
    $portJSSServer = $portJSSServer + $version + 20;

    print "*****ENVIRONMENT*****\n";
    print "java=$java\n";
    print "NATIVE_FLAG=$ENV{NATIVE_FLAG}\n";
    print "$ld_lib_path=$ENV{$ld_lib_path}\n";
    print "CLASSPATH=$ENV{CLASSPATH}\n";
    print "BUILD_OPT=$ENV{BUILD_OPT}\n";
    print "USE_64=$ENV{USE_64}\n";
    print "testdir=$testdir\n";
    print "portJSSEServer=$portJSSEServer\n";
    print "portJSSServer=$portJSSServer\n";
}

sub print_case_result {
    my $result = shift;
    my $testname = shift;

    $testrun++;
    if ($result == 0) {
        $testpass++;
        print "JSSTEST_CASE $testrun ($testname): PASS\n";
    } else {
        print "JSSTEST_CASE $testrun ($testname): FAIL\n";
    }

}

setup_vars(\@ARGV);

my $signingToken = "Internal Key Storage Token";


print "*********************\n";

#
# Make the test database directory
#
if( ! -d $testdir ) {
    mkdir( $testdir, 0755 ) or die;
}
{
    my @dbfiles = 
        ("$testdir/cert8.db", "$testdir/key3.db", "$testdir/secmod.db", "$testdir/keystore.pfx");
    (grep{ -f } @dbfiles)  and die "There is already an old database in $testdir";
    my $result = system("cp $nss_lib_dir/*nssckbi* $testdir"); $result >>= 8;
    $result and die "Failed to copy builtins library";
}
my $result;

print "============= Setup DB\n";
$result = system("echo $java org.mozilla.jss.tests.SetupDBs $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.SetupDBs $testdir $pwfile");
$result >>=8;
$result and print "SetupDBs returned $result\n";
print_case_result ($result,"Setup DB");

#
# List CA certs
#
print "============= List CA certs\n";
$result = system("echo $java org.mozilla.jss.tests.ListCACerts $testdir");
$result = system("$java org.mozilla.jss.tests.ListCACerts $testdir");
$result >>=8;
$result and print "ListCACerts returned $result\n";
print_case_result ($result,"List CA certs");

#
# test sockets
#
print "============= test sockets\n";
$portJSSServer = checkPort($portJSSServer);
$result = system("echo $java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile $portJSSServer");
$result = system("$java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile $portJSSServer");
$result >>=8;
$result and print "SSLClientAuth returned $result\n";
print_case_result ($result,"Sockets");

$portJSSServer=$portJSSServer+1;

#
# test sockets in bypass mode
#
print "============= test sockets using bypass \n";
$portJSSServer = checkPort($portJSSServer);
$result = system("echo $java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile $portJSSServer bypass");
$result = system("$java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile $portJSSServer bypass");
$result >>=8;
$result and print "SSLClientAuth using bypass mode returned $result\n";
print_case_result ($result,"SSLClientAuth using bypass");

# test key gen
#
print "============= test key gen\n";
$result = system("echo $java org.mozilla.jss.tests.TestKeyGen $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.TestKeyGen $testdir $pwfile");
$result >>=8;
$result and print "TestKeyGen returned $result\n";
print_case_result ($result,"Key generation");

# test KeyFactory 
#
print "============= test KeyFactory\n";
$result = system("echo $java org.mozilla.jss.tests.KeyFactoryTest $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.KeyFactoryTest $testdir $pwfile");
$result >>=8;
$result and print "KeyFactoryTest returned $result\n";
print_case_result ($result,"KeyFactoryTest");

# test digesting
#
print "============= test digesting\n";
$result = system("echo $java org.mozilla.jss.tests.DigestTest $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.DigestTest $testdir $pwfile");
$result >>=8;
$result and print "DigestTest returned $result\n";
print_case_result ($result,"Digesting");


# test HMAC 
#
print "============= test HMAC\n";
$result = system("echo $java org.mozilla.jss.tests.HMACTest $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.HMACTest $testdir $pwfile");
$result >>=8;
$result and print "HMACTest returned $result\n";
print_case_result ($result,"HMACTest");

# test JCA Sig Test
#
print "============= test Mozilla-JSS SigatureSPI JCASigTest\n";
$result = system("echo $java org.mozilla.jss.tests.JCASigTest $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.JCASigTest $testdir $pwfile");
$result >>=8;
$result and print "TestJCASigTest returned $result\n";
print_case_result ($result,"Mozilla-JSS SigatureSPI JCASigTest");

# test Secret Decoder Ring
#
print "============= test Secret Decoder Ring\n";
$result = system("echo $java org.mozilla.jss.tests.TestSDR $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.TestSDR $testdir $pwfile");
$result >>=8;
$result and print "TestSDR returned $result\n";
print_case_result ($result,"Secret Decoder Ring");

#
# Generate a known cert pair that can be used for testing
#
print "============= Generate known cert pair for testing\n";
$result=system("echo $java org.mozilla.jss.tests.GenerateTestCert $testdir $pwfile");
$result=system("$java org.mozilla.jss.tests.GenerateTestCert $testdir $pwfile");
$result >>=8;
$result and print "Generate known cert pair for testing returned $result\n";

#
# List cert by certnick
#
print "============= List cert by certnick\n";
$result = system("echo $java org.mozilla.jss.tests.ListCerts $testdir JSSCATestCert");
$result = system("$java org.mozilla.jss.tests.ListCerts $testdir JSSCATestCert");
$result >>=8;
$result and print "List cert by certnick returned $result\n";
print_case_result ($result,"List cert by certnick");

#
# Verify cert by certnick
#
print "============= Verify cert by certnick\n";
$result = system("echo $java org.mozilla.jss.tests.VerifyCert $testdir $pwfile JSSCATestCert");
$result = system("$java org.mozilla.jss.tests.VerifyCert $testdir $pwfile JSSCATestCert");
$result >>=8;
$result and print "Verify cert by certnick returned $result\n";
print_case_result ($result,"Verify cert by certnick");

#
# Create keystore.pfx from generated cert db
# for "JSSCATestCert"
print "============= convert PKCS11 cert to PKCS12 format\n";
$result = system("echo $nss_lib_dir/../bin/pk12util$exe_suffix -o $testdir/keystore.pfx -n JSSCATestCert -d $testdir -K netscape -W netscape");
$result = system("$nss_lib_dir/../bin/pk12util$exe_suffix -o $testdir/keystore.pfx -n JSSCATestCert -d $testdir -K netscape -W netscape");
$result >>=8;
$result and print "Convert PKCS11 to PKCS12 returned $result\n";

#
# TestSDR Test
#
print "============= TestSDR Test\n";
$result = system("echo $java org.mozilla.jss.tests.TestSDR $testdir $pwfile");
$result = system("$java org.mozilla.jss.tests.TestSDR $testdir $pwfile");
$result >>=8;
$result and print "TestSDR test returned $result\n";
print_case_result ($result,"TestSDR test");

#
# Start JSSE server
#
print "============= Start JSSE server tests\n";
$portJSSEServer = checkPort($portJSSEServer);
$result=system("echo ./startJsseServ.$scriptext $jss_classpath $testdir $portJSSEServer $java");
$result=system("./startJsseServ.$scriptext $jss_classpath $testdir $portJSSEServer $java");
$result >>=8;
$result and print "JSSE servers returned $result\n";

#
# Test JSS client communication
#
print "============= Start JSS client tests\n";
$result = system("echo $java org.mozilla.jss.tests.JSS_SSLClient $testdir $pwfile $hostname $portJSSEServer bypassOff");
$result = system("$java org.mozilla.jss.tests.JSS_SSLClient $testdir $pwfile $hostname $portJSSEServer bypassOff");
$result >>=8;
$result and print "JSS client returned $result\n";
print_case_result ($result,"JSSE server / JSS client");

$portJSSServer=$portJSSServer+1;

#
# Start JSS server
#
print "============= Start JSS server tests\n";
$portJSSServer = checkPort($portJSSServer);
$result=system("echo ./startJssServ.$scriptext $jss_classpath $testdir $portJSSServer bypassOff  fipsOff $java");
$result=system("./startJssServ.$scriptext $jss_classpath $testdir $portJSSServer bypassOff  fipsOff $java");
$result >>=8;
$result and print "JSS servers returned $result\n";

#
# Test JSSE client communication
#
print "============= Start JSSE client tests\n";
$result = system("echo $java org.mozilla.jss.tests.JSSE_SSLClient $testdir $portJSSServer");
$result = system("$java org.mozilla.jss.tests.JSSE_SSLClient $testdir $portJSSServer");
$result >>=8;
$result and print "JSSE client returned $result\n";
print_case_result ($result,"JSS server / JSSE client");

$portJSSServer=$portJSSServer+1;

#
# Test Enable FIPSMODE 
#
print "============= Start enable FIPSMODE\n";
$result = system("echo $java org.mozilla.jss.tests.FipsTest $testdir enable");
$result = system("$java org.mozilla.jss.tests.FipsTest $testdir enable");
$result >>=8;
$result and print "Enable FIPSMODE returned $result\n";
print_case_result ($result,"FIPSMODE enabled");

#
# test sockets
#
print "============= test sockets\n";
$portJSSServer = checkPort($portJSSServer);
$result = system("echo $java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile $portJSSServer");
$result = system("$java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile $portJSSServer");
$result >>=8;
$result and print "SSLClientAuth returned $result\n";
print_case_result ($result,"Sockets");

$portJSSServer=$portJSSServer+1;

#
# Test chkfips FIPSMODE 
#
print "============= Start enable FIPSMODE\n";
$result = system("echo $java org.mozilla.jss.tests.FipsTest $testdir chkfips");
$result = system("$java org.mozilla.jss.tests.FipsTest $testdir chkfips");
$result >>=8;
$result and print "Enable FIPSMODE returned $result\n";
print_case_result ($result,"FIPSMODE enabled");

#
# Start JSS server
#
print "============= Start JSS server tests\n";
$portJSSServer = checkPort($portJSSServer);
$result=system("echo ./startJssServ.$scriptext $jss_classpath $testdir $portJSSServer bypassOff  fips $java");
$result=system("./startJssServ.$scriptext $jss_classpath $testdir $portJSSServer bypassOff  fips $java");
$result >>=8;
$result and print "JSS servers returned $result\n";

#
# Test JSSE client communication
#
print "============= Start JSSE client tests\n";
$result = system("echo $java org.mozilla.jss.tests.JSSE_SSLClient $testdir $portJSSServer");
$result = system("$java org.mozilla.jss.tests.JSSE_SSLClient $testdir $portJSSServer");
$result >>=8;
$result and print "JSSE client returned $result\n";
print_case_result ($result,"JSS server / JSSE client");

$portJSSServer=$portJSSServer+1;

#
# Test Disable FIPSMODE 
#
print "============= Start disable FIPSMODE\n";
$result = system("echo $java org.mozilla.jss.tests.FipsTest $testdir disable");
$result = system("$java org.mozilla.jss.tests.FipsTest $testdir disable");
$result >>=8;
$result and print "Disable FIPSMODE returned $result\n";
print_case_result ($result,"FIPSMODE disabled");

#
# Test SecretKeys
#
print "============= Start Secret Key Gen and Ciphers\n";
$result = system("echo $java org.mozilla.jss.tests.SymKeyGen $testdir");
$result = system("$java org.mozilla.jss.tests.SymKeyGen $testdir");
$result >>=8;
$result and print "SymKeyGen returned $result\n";
print_case_result ($result,"SymKeyGen successful");

#
# Test Mozilla-JSS SecretKeys
#
print "============= Start Mozilla-JSS Secret Key Gen and Ciphers\n";
$result = system("echo $java org.mozilla.jss.tests.JCASymKeyGen $testdir");
$result = system("$java org.mozilla.jss.tests.JCASymKeyGen $testdir");
$result >>=8;
$result and print "JCASymKeyGen returned $result\n";
print_case_result ($result,"JCASymKeyGen successful");

$portJSSEServer=$portJSSEServer+1;

#
# Start JSSE server to test JSS client in bypassPKCS11 mode
#
print "============= Start JSSE server tests to test the bypass\n";
$portJSSEServer = checkPort($portJSSEServer);
$result=system("echo ./startJsseServ.$scriptext $jss_classpath $testdir $portJSSEServer $java");
$result=system("./startJsseServ.$scriptext $jss_classpath $testdir $portJSSEServer $java");
$result >>=8;
$result and print "JSSE servers testing JSS client in bypassPKCS11 test returned $result\n";

#
# Test JSS in bypassPKCS11 mode client communication
#
print "============= Start JSS client tests in bypassPKCS11 mode\n";
$result = system("echo $java org.mozilla.jss.tests.JSS_SSLClient $testdir $pwfile $hostname $portJSSEServer bypass");
$result = system("$java org.mozilla.jss.tests.JSS_SSLClient $testdir $pwfile $hostname $portJSSEServer bypass");
$result >>=8;
$result and print "JSS client in bypassPKCS11 mode returned $result\n";
print_case_result ($result,"JSSE server / JSS client in bypassPKCS11 mode");

$portJSSServer=$portJSSServer+1;

#
# Start JSS server in bypassPKCS11 mode 
#
print "============= Start JSS server tests in bypassPKCS11 mode\n";
$portJSSEServer = checkPort($portJSSEServer);
$result=system("echo ./startJssServ.$scriptext $jss_classpath $testdir $portJSSServer bypass fipsOff $java");
$result=system("./startJssServ.$scriptext $jss_classpath $testdir $portJSSServer bypass fipsOff $java");
$result >>=8;
$result and print "JSS servers in bypassPKCS11 mode returned $result\n";

#
# Test JSSE client communication
#
print "============= Start JSSE client tests to test the JSS server in bypassPKCS11 mode\n";
$result = system("echo $java org.mozilla.jss.tests.JSSE_SSLClient $testdir $portJSSServer");
$result = system("$java org.mozilla.jss.tests.JSSE_SSLClient $testdir $portJSSServer");
$result >>=8;
$result and print "JSSE client talking to JSS Server in bypassPKCS11 mode returned $result\n";
print_case_result ($result,"JSS server in bypassPKCS11 mode / JSSE client");

#
# Test for JSS jar and library revision
#
print "============= Check JSS jar version\n";
$result = system("$java org.mozilla.jss.tests.JSSPackageTest $testdir");
$result >>=8;
my $LIB = "$lib_jss"."4"."$lib_suffix";
my $strings_exist = `which strings`;
chomp($strings_exist);
if ($strings_exist ne "") {
    (-f "$nss_lib_dir/$LIB") or die "$nss_lib_dir/$LIB does not exist\n";
    my $jsslibver = `strings $nss_lib_dir/$LIB | grep Header`;
    chomp($jsslibver);
    if ($jsslibver ne "") {
        print "$LIB = $jsslibver\n";
    } else {
        print "Could not fetch Header information from $nss_lib_dir/$LIB\n";
    }
} else {
    print "Could not fetch Header information from $nss_lib_dir/$LIB\n";
    $result=1;
}
$result and print "JSS jar package information test returned $result\n";
print_case_result ($result,"Check JSS jar version");
print "JSSTEST_SUITE: $testpass / $testrun\n";
my $rate = $testpass / $testrun * 100;
printf "JSSTEST_RATE: %.0f %\n",$rate;

if ($testpass ne $testrun) {
    printf "Test Status: FAILURE\n";
    system("false");
} else {
    printf "Test Status: SUCCESS\n";
    system("true");
}
