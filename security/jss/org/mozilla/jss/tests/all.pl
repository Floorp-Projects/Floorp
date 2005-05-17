#
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

my $java;

# dist <dist_dir>
# release <java release dir> <nss release dir> <nspr release dir>

sub usage {
    print STDERR "Usage:\n";
    print STDERR "$0 dist <dist_dir>\n";
    print STDERR "$0 release <jss release dir> <nss release dir> "
        . "<nspr release dir>\n";
    exit(1);
}

my $nss_lib_dir;
my $dist_dir;
my $pathsep       = ":";
my $scriptext     = "sh";
my $exe_suffix    = "";
my $lib_suffix    = ".so";
my $lib_jss       = "libjss";
my $jss_rel_dir   = "";
my $jss_classpath = "";

sub setup_vars {
    my $argv = shift;

    my $osname = `uname -s`;

    my $truncate_lib_path = 1;
    if( $osname =~ /HP/ ) {
        $ld_lib_path = "SHLIB_PATH";
	$scriptext = "sh";
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

    unless( $ENV{JAVA_HOME} ) {
        print STDERR "Must set JAVA_HOME environment variable\n";
        exit(1);
    }

    $java = "$ENV{JAVA_HOME}/jre/bin/java$exe_suffix";
    (-f $java) or die "'$java' does not exist\n";
    $java = $java . $ENV{NATIVE_FLAG};

    if ($ENV{USE_64}) {
        $java = $java . " -d64";
    }

    $pwfile = "passwords";

    print STDERR "*****ENVIRONMENT*****\n";
    print STDERR "java=$java\n";
    print STDERR "NATIVE_FLAG=$ENV{NATIVE_FLAG}\n";
    print STDERR "$ld_lib_path=$ENV{$ld_lib_path}\n";
    print STDERR "CLASSPATH=$ENV{CLASSPATH}\n";
    print STDERR "USE_64=$ENV{USE_64}\n";
}

setup_vars(\@ARGV);

my $signingToken = "Internal Key Storage Token";


print STDERR "*********************\n";

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
        ("./cert8.db", "./key3.db", "./secmod.db");
    unlink @dbfiles;
    (grep{ -f } @dbfiles)  and die "Unable to delete old database files";
# if dbdir exists delete it
    my $dbdir = "./cert8.dir";
    if (-d $dbdir ) {
      unlink(< $dbdir/* >);     # delete all files in cert8.dir
      rmdir($dbdir) or die "Unable to delete cert8.dir";
    }    
    chdir ".." or die;
    my $result = system("cp $nss_lib_dir/*nssckbi* testdir"); $result >>= 8;
    $result and die "Failed to copy builtins library";
}
my $result;

print STDERR "============= Setup DB\n";
$result = system("$java org.mozilla.jss.tests.SetupDBs testdir $pwfile");
$result >>=8;
$result and die "SetupDBs returned $result";

#
# List CA certs
#
print STDERR "============= List CA certs\n";
$result = system("$java org.mozilla.jss.tests.ListCACerts $testdir");
$result >>=8;
$result and die "ListCACerts returned $result";

#
# test sockets
#
print STDERR "============= test sockets\n";
$result = system("$java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile");
$result >>=8;
$result and die "SSLClientAuth returned $result";

# test key gen
#
print STDERR "============= test key gen\n";
$result = system("$java org.mozilla.jss.tests.TestKeyGen $testdir $pwfile");
$result >>=8;
$result and die "TestKeyGen returned $result";

# test digesting
#
print STDERR "============= test digesting\n";
$result = system("$java org.mozilla.jss.tests.DigestTest $testdir $pwfile");
$result >>=8;
$result and die "DigestTest returned $result";

# test signing
#
print STDERR "============= test signing\n";
$result = system("$java org.mozilla.jss.tests.SigTest $testdir " .
            "\"$signingToken\" $pwfile"); $result >>=8;
$result and die "SigTest returned $result";

# test JCA Sig Test
#
print STDERR "============= test Mozilla-JSS SigatureSPI JCASitTest\n";
$result = system("$java org.mozilla.jss.tests.JCASigTest $testdir $pwfile");
$result >>=8;
$result and die "TestJCASigTest returned $result";

# test Secret Decoder Ring
#
print STDERR "============= test Secret Decoder Ring\n";
$result = system("$java org.mozilla.jss.tests.TestSDR $testdir $pwfile");
$result >>=8;
$result and die "TestSDR returned $result";

#
# Generate a known cert pair that can be used for testing
#
print STDERR "============= Generate known cert pair for testing\n";
$result=system("$java org.mozilla.jss.tests.GenerateTestCert $testdir $pwfile");
$result >>=8;
$result and die "Generate known cert pair for testing returned $result";

#
# Create keystore.pfx from generated cert db
# for "JSSCATestCert"
print STDERR "============= convert PKCS11 cert to PKCS12 format\n";
$result = system("$nss_lib_dir/../bin/pk12util$exe_suffix -o keystore.pfx -n JSSCATestCert -d ./$testdir -K netscape -W netscape");
$result >>=8;
$result and die "Convert PKCS11 to PKCS12 returned $result";

#
# Start both JSS and JSSE servers
#
print STDERR "============= Start JSSE server tests\n";
$result=system("./startJsseServ.$scriptext $jss_classpath $testdir $java");
$result >>=8;
$result and die "JSSE servers returned $result";

#
# Test JSS client communication
#
print STDERR "============= Start JSS client tests\n";
$result = system("cp $testdir/*.db .");
$result = system("$java org.mozilla.jss.tests.JSS_SSLClient");
$result >>=8;
$result and die "JSS client returned $result";

#
# Start both JSS and JSSE servers
#
print STDERR "============= Start JSS server tests\n";
$result=system("./startJssServ.$scriptext $jss_classpath $testdir $java");
$result >>=8;
$result and die "JSS servers returned $result";

#
# Test JSSE client communication
#
print STDERR "============= Start JSSE client tests\n";
$result = system("$java org.mozilla.jss.tests.JSSE_SSLClient");
$result >>=8;
$result and die "JSSE client returned $result";

#
# Test for JSS jar and library revision
#
print STDERR "============= Check JSS jar version\n";
$result = system("$java org.mozilla.jss.tests.JSSPackageTest");
$result >>=8;
my $LIB = "$lib_jss"."4"."$lib_suffix";
my $strings_exist = `which strings`;
chop($strings_exist);
if ($strings_exist ne "") {
    my $jsslibver = `strings $nss_lib_dir/$LIB | grep Header`;
    chop($jsslibver);
    print "$LIB = $jsslibver\n";
} else {
    print "Could not fetch Header information from $LIB\n";
}
$result and die "JSS jar package information test $result";

