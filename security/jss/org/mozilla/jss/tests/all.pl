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
#use strict;
#use Config;

my $java;

# dist <dist_dir>
# release <java release dir> <nss release dir> <nspr release dir>

sub usage {
    print "Usage:\n";
    print "$0 dist <dist_dir>\n";
    print "$0 release <jss release dir> <nss release dir> "
        . "<nspr release dir>\n";
    exit(1);
}

my $nss_lib_dir;

sub setup_vars {
    my $argv = shift;

    my $osname = `uname -s`;

    my $truncate_lib_path = 1;
    my $pathsep = ":";
    my $exe_suffix = "";
    if( $osname =~ /HP/ ) {
        $ld_lib_path = "SHLIB_PATH";
    } elsif( $osname =~ /win/i ) {
        $ld_lib_path = "PATH";
        $truncate_lib_path = 0;
        $pathsep = ";";
        $exe_suffix = ".exe";
    } else {
        $ld_lib_path = "LD_LIBRARY_PATH";
    }

    my $dbg_suffix = "_DBG";
    $ENV{BUILD_OPT} and $dbg_suffix = "";

    $ENV{CLASSPATH}  = "";
    $ENV{$ld_lib_path} = "" if $truncate_lib_path;

    if( $$argv[0] eq "dist" ) {
        shift @$argv;
        my $dist_dir = shift @$argv or usage("did not provide dist_dir");

        $ENV{CLASSPATH} .= "$pathsep$dist_dir/../classes$dbg_suffix";
        $ENV{$ld_lib_path} .= "$pathsep$dist_dir/lib";
        $nss_lib_dir = "$dist_dir/lib"
    } elsif( $$argv[0] eq "release" ) {
        shift @$argv;

        my $jss_rel_dir = shift @$argv or usage();
        my $nss_rel_dir = shift @$argv or usage();
        my $nspr_rel_dir = shift @$argv or usage();

        $ENV{CLASSPATH} .= "$jss_rel_dir/../xpclass$dbg_suffix.jar";
        $ENV{$ld_lib_path} =
            "$jss_rel_dir/lib$pathsep$nss_rel_dir/lib$pathsep$nspr_rel_dir/lib"
            . $pathsep . $ENV{$ld_lib_path};
        $nss_lib_dir = "$nss_rel_dir/lib";
    } else {
        usage();
    }

    unless( $ENV{JAVA_HOME} ) {
        print "Must set JAVA_HOME environment variable\n";
        exit(1);
    }

    $java = "$ENV{JAVA_HOME}/jre/bin/java$exe_suffix";
    $pwfile = "passwords";

    print "*****ENVIRONMENT*****\n";
    print "java=$java\n";
    print "$ld_lib_path=$ENV{$ld_lib_path}\n";
    print "CLASSPATH=$ENV{CLASSPATH}\n";
}

setup_vars(\@ARGV);

my $signingToken = "Internal Key Storage Token";
(-f $java) or die "'$java' does not exist\n";

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
    my $result = system("cp $nss_lib_dir/*nssckbi* testdir"); $result >>= 8;
    $result and die "Failed to copy builtins library";
}
my $result;
$result = system("$java org.mozilla.jss.tests.SetupDBs testdir $pwfile");
$result >>=8;
$result and die "SetupDBs returned $result";

#
# List CA certs
#
$result = system("$java org.mozilla.jss.tests.ListCACerts $testdir");
$result >>=8;
$result and die "ListCACerts returned $result";

#
# test sockets
#
$result = system("$java org.mozilla.jss.tests.SSLClientAuth $testdir $pwfile");
$result >>=8;
$result and die "SSLClientAuth returned $result";

# test key gen
#
$result = system("$java org.mozilla.jss.tests.TestKeyGen $testdir $pwfile");
$result >>=8;
$result and die "TestKeyGen returned $result";

# test signing
#
$result = system("$java org.mozilla.jss.tests.SigTest $testdir " .
            "\"$signingToken\" $pwfile"); $result >>=8;
$result and die "SigTest returned $result";
