#!perl
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#     Samir Gehani <sgehani@netscape.com>
#
#==============================================================================
# usage: perl deliver.pl version URLPath stubName blobName buildWizard
# e.g.   perl deliver.pl 5.0.0.1 ftp://foo/ mozilla-installer mozilla-installer
#
# Delivers the stub and blob installers to mozilla/installer/stub 
# and mozilla/installer/sea, respectively.  Also, delivers the .xpis
# to mozilla/installer/raw/xpi.
#
# NOTE:
# -----
#   * all args are optional 
#   * version is used by xpinstall and needs to bumped every build
#   * URLPath must have a trailing slash
#   * if you are not building a release version no need to pass any args
#   * pass in "buildwizard" as the last arg to build the wizard too
#   * you must be in deliver.pl's dir when calling it since it assumes DEPTH
#==============================================================================

use Cwd;
use Getopt::Std;
use File::Path;
getopts('o:s:');

#// constants
$SUBDIR = "mozilla-installer";
$_DEPTH  = "../../..";

# Determine topsrcdir
$_orig = cwd();
if (defined($opt_s)) {
    chdir($opt_s) || die "chdir($opt_s): $!\n";
} else {
    chdir($_DEPTH); # resolve absolute path
}
$topsrcdir = cwd();    
chdir($_orig);

# Determine topobjdir
if (defined($opt_o)) {
    chdir($opt_o) || die "chdir($opt_o): $!\n";
} else {
    chdir($_DEPTH); # resolve absolute path
}
$topobjdir = cwd();    
chdir($_orig);

$WIZSRC = $topsrcdir."/xpinstall/wizard/unix/src2";
$WIZARD = $topobjdir."/xpinstall/wizard/unix/src2";
$ROOT   = $topobjdir."/installer";
$STAGE  = $ROOT."/stage";
$RAW    = $ROOT."/raw";
$XPI    = $RAW."/xpi";
$BLOB   = $ROOT."/sea";
$STUB   = $ROOT."/stub";

#// default args
$aVersion = "5.0.0.0";
$aURLPath = "ftp://ftp.mozilla.org/";
$aStubName = "mozilla-installer";
$aBlobName = "mozilla-installer";
$aBuildWizard = "NO";

#// parse args
# all optional args: version, URLPath, stubName, blobName
if ($#ARGV >= 4) { $aBuildWizard = $ARGV[4]; }
if ($#ARGV >= 3) { $aBlobName    = $ARGV[3]; }
if ($#ARGV >= 2) { $aStubName    = $ARGV[2]; }
if ($#ARGV >= 1) { $aURLPath     = $ARGV[1]; }
if ($#ARGV >= 0) { $aVersion     = $ARGV[0]; }

#// create dist structure (mozilla/installer/{stage,raw,stub,sea})
if (-e $ROOT)
{
    if (-w $ROOT) 
        { system("rm -rf $ROOT"); }
    else 
        { die "--- deliver.pl: check perms on mozilla/installer: $!"; }
}

mkdir($ROOT, 0777)  || die "--- deliver.pl: couldn't mkdir root: $!";
mkdir($STAGE, 0777) || die "--- deliver.pl: couldn't mkdir stage: $!";
mkdir($RAW, 0777)   || die "--- deliver.pl: couldn't mkdir raw: $!";
mkdir($XPI, 0777)   || die "--- deliver.pl: couldn't mkdir xpi: $!";
mkdir($BLOB, 0777)  || die "--- deliver.pl: couldn't mkdir sea: $!";
mkdir($STUB, 0777)  || die "--- deliver.pl: couldn't mkdir stub: $!";


#-------------------------------------------------------------------------
#   Deliver wizard
#-------------------------------------------------------------------------
#// build the wizard 
if ($aBuildWizard eq "buildwizard")
{
    chdir($WIZARD);
    system($topsrcdir."/build/autoconf/update-makefile.sh");

    #// make unix wizard
    system("make");
    chdir($_orig);
}

#// deliver wizard to staging area (mozilla/installer/stage)
copy("$WIZSRC/mozilla-installer", $RAW);
copy("$WIZARD/mozilla-installer-bin", $RAW);
copy("$WIZSRC/installer.ini", $RAW);
copy("$WIZSRC/README", $RAW);
copy("$WIZSRC/MPL-1.1.txt", $RAW);
chmod(0755, "$RAW/mozilla-installer"); #// ensure shell script is executable

spew("Completed delivering wizard");


#-------------------------------------------------------------------------
#   Make .xpis
#-------------------------------------------------------------------------
#// call pkgcp.pl
chdir("$topsrcdir/xpinstall/packager");
system("perl pkgcp.pl -o unix -s $topobjdir/dist -d $STAGE -f $topsrcdir/xpinstall/packager/packages-unix -v");
spew("Completed copying build files");

#// call xptlink.pl to make big .xpt files/component
system("perl xptlink.pl -o unix -s $topobjdir/dist -d $STAGE -v");
spew("Completed xptlinking"); 

#// change the permissions in the staging area, stopgap for creating 
#// group/world writeable files in /usr for standard installs
system("chmod og-w -R $STAGE");

#// call makeall.pl tunneling args (delivers .xpis to $topobjdir/installer/stage)
chdir("$topsrcdir/xpinstall/packager/unix");
system("perl makeall.pl $aVersion $aURLPath $STAGE $XPI");
system("mv $topsrcdir/xpinstall/packager/unix/config.ini $RAW");
spew("Completed making .xpis");

#-------------------------------------------------------------------------
#   Package stub and sea
#-------------------------------------------------------------------------
#// tar and gzip mozilla-installer, mozilla-installer-bin, README, license, 
#// config.ini, installer.ini into stub
spew("Creating stub installer tarball...");
chdir("$RAW/..");
system("mv $RAW $ROOT/$SUBDIR");
system("tar cvf $STUB/$aStubName.tar ./$SUBDIR/mozilla-installer ./$SUBDIR/mozilla-installer-bin ./$SUBDIR/installer.ini ./$SUBDIR/README ./$SUBDIR/config.ini ./$SUBDIR/MPL-1.1.txt"); 
system("mv $ROOT/$SUBDIR $RAW");
system("gzip $STUB/$aStubName.tar");
spew("Completed creating stub installer tarball");

#// tar and gzip mozilla-installer, mozilla-installer-bin, README, license, 
#// config.ini, installer.ini and .xpis into sea
spew("Creating blob (aka full or sea) installer tarball...");
system("mv $RAW $ROOT/$SUBDIR");
system("tar cvf $BLOB/$aBlobName.tar ./$SUBDIR/"); 
system("mv $ROOT/$SUBDIR $RAW");
system("gzip $BLOB/$aBlobName.tar");
spew("Completed creating blob (aka full or sea) installer tarball");
chdir($_orig);

spew("Completed packaging stub and sea");
spew("Installers built (see mozilla/installer/{stub,sea})");


#-------------------------------------------------------------------------
#   Utilities
#-------------------------------------------------------------------------
sub spew 
{
    print "+++ deliver.pl: ".$_[0]."\n";
}

sub copy
{
    if (! -e $_[0])
    {
        die "--- deliver.pl: couldn't cp cause ".$_[0]." doesn't exist: $!";
    }
    system ("cp ".$_[0]." ".$_[1]);
}
