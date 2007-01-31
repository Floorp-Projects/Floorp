#!/usr/bin/perl
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
# The Original Code is Patcher 2, a patch generator for the AUS2 system.
#
# The Initial Developer of the Original Code is
#   Mozilla Corporation
#
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chase Phillips (chase@mozilla.org)
#   J. Paul Reed (preed@mozilla.com)
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
#

package MozAUSLib;

use Cwd;
use File::Path;
use File::Copy qw(move copy);
use English;

use File::Spec::Functions;

use MozBuild::Util qw(RunShellCommand MkdirWithPath);

require Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw(CreatePartialMarFile CreatePartialMarPatchInfo
                GetAUS2PlatformStrings GetBouncerPlatformStrings
                ValidateToolsDirectory
                EnsureDeliverablesDir
                SubstitutePath
               );

use strict;

##
## CONSTANTS 
##

use vars qw($MAR_BIN $MBSDIFF_BIN $MAKE_BIN
            $INCREMENTAL_UPDATE_BIN $UNWRAP_FULL_UPDATE_BIN
            $TMPDIR_PREFIX 
            %BOUNCER_PLATFORMS %AUS2_PLATFORMS
            $DEFAULT_PARTIAL_MAR_OUTPUT_FILE);

$MAR_BIN = 'dist/host/bin/mar';
$MBSDIFF_BIN = 'dist/host/bin/mbsdiff';

$INCREMENTAL_UPDATE_BIN = 'tools/update-packaging/make_incremental_update.sh';
$UNWRAP_FULL_UPDATE_BIN = 'tools/update-packaging/unwrap_full_update.pl';

$MAKE_BIN = '/usr/bin/make';
$TMPDIR_PREFIX = '/dev/shm/tmp/MozAUSLib';

%BOUNCER_PLATFORMS = ( 'win32' => 'win',
                       'linux-i686' => 'linux',
                       'mac' => 'osx',
                       'unimac' => 'osx',
                     );

%AUS2_PLATFORMS = ( 'macppc' => 'Darwin_ppc-gcc3',
                    'mac' => 'Darwin_Universal-gcc3',
                    'linux-i686' => 'Linux_x86-gcc3',
                    'win32' => 'WINNT_x86-msvc' );

$DEFAULT_PARTIAL_MAR_OUTPUT_FILE = 'partial.mar';

sub EnsureDeliverablesDir
{
   my %args = @_;

   die "ASSERT: null config spec\n" if (not defined($args{'config'}));
   my $configSpec = $args{'config'};

   my $fullDeliverableDirPath = catfile($configSpec->GetDeliverableDir(),
                                        lc($configSpec->GetApp()));

   MkdirWithPath(dir => $fullDeliverableDirPath, mask => 0751) or
    die "ASSERT: EnsureDeliverablesDir(): " .
    "MkdirWithPath($fullDeliverableDirPath) failed\n";
   return $fullDeliverableDirPath;
}

sub ValidateToolsDirectory
{
    my %args = @_;
    my $toolsDir= $args{'toolsDir'};

    if ($toolsDir !~ /^\//) {
       die "ASSERT: ValidateToolsDirectory() requires a full path: $toolsDir\n";
    }

    my $binPrefix = "$toolsDir/mozilla";
    return (-d $binPrefix and
            -x "$binPrefix/$MAR_BIN" and
            -x "$binPrefix/$MBSDIFF_BIN" and
            -x "$binPrefix/$INCREMENTAL_UPDATE_BIN" and
            -x "$binPrefix/$UNWRAP_FULL_UPDATE_BIN");
}

sub GetAUS2PlatformStrings
{
    my %retHash = %AUS2_PLATFORMS;
    return %retHash;
}

sub GetBouncerPlatformStrings
{
    my %retHash = %BOUNCER_PLATFORMS;
    return %retHash;
}

sub CreatePartialMarFile
{
    my %args = @_;

    my $fromCompleteMar = $args{'from'};
    my $toCompleteMar = $args{'to'};
    my $outputDir = $args{'outputDir'} || getcwd();
    my $outputFile = $args{'outputFile'} || $DEFAULT_PARTIAL_MAR_OUTPUT_FILE;
    my $mozdir = $args{'mozdir'};
    my $forceList = $args{'force'};

    my $startingWd = getcwd();

    if (not defined($mozdir)) {
        die "ASSERT: CreatePartialMarFile(): mozdir undefined\n";
    }

    if (not ValidateToolsDirectory(toolsDir => $mozdir)) {
        print STDERR "Invalid Mozilla working dir: $mozdir\n";
        return -1;
    } else {
        # We actually want the CVS directory itself...
        $mozdir .= '/mozilla';
    }

    my $mar = "$mozdir/$MAR_BIN";
    my $mbsdiff = "$mozdir/$MBSDIFF_BIN";
    my $makeIncrementalUpdate = "$mozdir/$INCREMENTAL_UPDATE_BIN";

    # Seed PRNG.
    srand (time() ^ $PID ^ unpack "%L*", `ps axww|gzip`);
    my $tmpDir = "$TMPDIR_PREFIX.CreatePartialMarFile/$PID/" . 
     int(rand(1000000));

    my $fromDir = "$tmpDir/from"; 
    my $toDir = "$tmpDir/to";
    my $partialDir = "$tmpDir/partial";

    my @createdDirs = ($tmpDir, $fromDir, $toDir, $partialDir);

    foreach my $dir (@createdDirs) {
        if (not MkdirWithPath(dir => $dir)) {
            print STDERR "MkdirWithPath() failed on $dir: $EVAL_ERROR\n";
            return -1;
        }
    }

    if ( not -r $fromCompleteMar) {
        print STDERR "CreatePartialMarFile: $fromCompleteMardoesn't exist!";
        return -1;
    }

    if ( not -r $toCompleteMar ) {
        print STDERR "CreatePartialMarFile: $toCompleteMar doesn't exist!";
        return -1;
    }

    # XXX - Add check here to verify md5/sha1 checksum of file is as-expected.

    # Extract the source MAR file.
    $ENV{'MAR'} = $mar;

    my $extractCommand = catfile($mozdir, $UNWRAP_FULL_UPDATE_BIN);
    my $unwrapArgs = [catfile($startingWd, $fromCompleteMar)];

    printf("Decompressing $fromCompleteMar with $extractCommand " .
     join(" ", @{$unwrapArgs}) . "...\n");
    chdir($fromDir) or die "chdir() $fromDir failed: $ERRNO";

    my $rv = RunShellCommand(command => $extractCommand,
                             args => $unwrapArgs,
                             output => 1);
    
    if ($rv->{'exitValue'} != 0) {
        die "FAILED: $extractCommand: $rv->{'exitValue'}, output: $rv->{'output'}\n";
    }

    printf("done\n");

    # Extract the destination MAR file.
 
    $unwrapArgs = [catfile($startingWd, $toCompleteMar)];
    printf("Decompressing $toCompleteMar with $extractCommand " .
     join(" ", @{$unwrapArgs}) . "...\n");
    chdir($toDir) or die "chdir() $toDir failed: $ERRNO";;

    $rv = RunShellCommand(command => $extractCommand,
                          args => $unwrapArgs,
                          output => 1);

    if ($rv->{'exitValue'} != 0) {
        die "FAILED: $extractCommand: $rv->{'exitValue'}, output: $rv->{'output'}\n";
    }

    printf("done\n");

    # Build the partial patch.
    chdir($mozdir);

    my $outputMar = catfile($mozdir, $DEFAULT_PARTIAL_MAR_OUTPUT_FILE);
    $ENV{'MBSDIFF'} = $mbsdiff;

    my $incrUpdateArgs = [];
    
    # Tack any force arguments onto the beginning of the arg list
    if (defined($forceList) && scalar(@{$forceList}) > 0) {
        foreach my $file (@{$forceList}) {
            push(@{$incrUpdateArgs}, ('-f', $file));
        }
    }

    push(@{$incrUpdateArgs}, ($outputMar, $fromDir, $toDir));

    printf("Building partial update with: $makeIncrementalUpdate " . 
     join(" ", @{$incrUpdateArgs}) . "\n...");

    $rv = RunShellCommand(command => $makeIncrementalUpdate,
                          args => $incrUpdateArgs,
                          output => 1);

    if ($rv->{'exitValue'} != 0) {
        die "FAILED: $makeIncrementalUpdate: $rv->{'exitValue'}, " . 
         "output: $rv->{'output'}\n";
    }

    printf("done\n");

    if (-f $outputMar) {
        printf("Found $outputMar.\n");
    } else {
        print STDERR "Couldn't find partial output mar: $outputMar\n";
    }

    my $finalDeliverable = catfile($outputDir, $outputFile);
    printf("Moving $outputMar to $finalDeliverable... \n");
    move($outputMar, $finalDeliverable) or 
     die "move($outputMar, $finalDeliverable) failed: $ERRNO";
    printf("done\n");

    printf("Removing temporary directories...\n");
    foreach my $dir (@createdDirs ) {
        printf("\tRemoving $dir...\n");
        eval { rmtree($dir) };
        if ($EVAL_ERROR) {
            print STDERR "rmtree on $dir failed; $EVAL_ERROR";
            return 1;
        }
    }
    printf("done\n");

    chdir($startingWd) or die "Couldn't chdir() back to $startingWd: $ERRNO";
    return 1;
}

sub SubstitutePath
{
    my %args = @_;

    my $string = $args{'path'} || 
     die 'ASSERT: SubstitutePath() called with null path';
    my $platform = $args{'platform'} || 'UNDEFINED';
    my $locale = $args{'locale'} ||'UNDEFINED';
    my $version = $args{'version'} || 'UNDEFINED';
    my $app = $args{'app'} || 'UNDEFINED';

    my %bouncer_platforms = GetBouncerPlatformStrings();
    my $bouncer_platform = $bouncer_platforms{$platform};

    $string =~ s/%platform%/$platform/g;
    $string =~ s/%locale%/$locale/g;
    $string =~ s/%bouncer\-platform%/$bouncer_platform/g;
    $string =~ s/%version%/$version/g;
    $string =~ s/%app%/$app/g;

    return $string;
}

1;
