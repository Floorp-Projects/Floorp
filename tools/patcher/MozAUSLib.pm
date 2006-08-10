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

require Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw(CreatePartialMarFile CreatePartialMarPatchInfo
                GetAUS2PlatformStrings GetBouncerPlatformStrings
                ValidateToolsDirectory
                MkdirWithPath RunShellCommand
                SubstitutePath
               );

use strict;

##
## CONSTANTS 
##

use vars qw($MAR_BIN $MBSDIFF_BIN $MAKE_BIN
            $INCREMENTAL_UPDATE_BIN $UNWRAP_FULL_UPDATE_BIN
            $TMPDIR_PREFIX 
            $ST_SIZE
            %BOUNCER_PLATFORMS %AUS2_PLATFORMS
            $DEFAULT_PARTIAL_MAR_OUTPUT_FILE
            $EXEC_TIMEOUT );

$ST_SIZE = 7;

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

$EXEC_TIMEOUT = 600;

## This is a wrapper function to get easy true/false return values from a
## mkpath()-like function. mkpath() *actually* returns the list of directories
## it created in the pursuit of your request, and keeps its actual success 
## status in $@. 

sub MkdirWithPath
{
    my $dirToCreate = shift;
    my $printProgress = shift;
    my $dirMask = shift;

    die "ASSERT: MkdirWithPath() needs an arg\n" if not defined($dirToCreate);

    ## Defaults based on what mkpath does...
    $printProgress = defined($printProgress) ? $printProgress : 0;
    $dirMask = defined($dirMask) ? $dirMask : 0777;

    eval { mkpath($dirToCreate, $printProgress, $dirMask) };
    return defined($@);
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
        if (not MkdirWithPath($dir)) {
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
    my $extractCommand = "MAR=$mar $mozdir/$UNWRAP_FULL_UPDATE_BIN $startingWd/$fromCompleteMar";
    printf("Decompressing $fromCompleteMar with $extractCommand... \n");
    chdir($fromDir) or die "chdir() $fromDir failed: $ERRNO";

    my $rv = RunShellCommand(command => $extractCommand);
    if ($rv->{'exit_value'} != 0) {
        die "FAILED: $extractCommand: $rv->{'exit_value'}, output: $rv->{'output'}\n";
    }

    printf("done\n");

    # Extract the destination MAR file.
    $extractCommand = "MAR=$mar $mozdir/$UNWRAP_FULL_UPDATE_BIN $startingWd/$toCompleteMar";
    printf("Decompressing $toCompleteMar with $extractCommand... \n");
    chdir($toDir) or die "chdir() $toDir failed: $ERRNO";;

    my $rv = RunShellCommand(command => $extractCommand);
    if ($rv->{'exit_value'} != 0) {
        die "FAILED: $extractCommand: $rv->{'exit_value'}, output: $rv->{'output'}\n";
    }

    printf("done\n");

    # Build the partial patch.
    chdir($mozdir);

    my $forceArgument = (defined($forceList) && scalar(@{$forceList}) > 0) ?
     ('-f ' . join(' -f ', @{$forceList})) : '';

    my $outputMar = "$mozdir/$DEFAULT_PARTIAL_MAR_OUTPUT_FILE";
    my $cmd = "MAR=$mar MBSDIFF=$mbsdiff"
         . " $makeIncrementalUpdate $forceArgument "
         . " $outputMar "
         . " $fromDir $toDir";

    printf("Building partial update with: $cmd...\n");
    my $rv = RunShellCommand(command => $cmd, output => 1);
    if ($rv->{'exit_value'} != 0) {
        die "FAILED: $cmd: $rv->{'exit_value'}, output: $rv->{'output'}\n";
    }

    printf("done\n");

    if (-f $outputMar) {
        printf("Found $outputMar.\n");
    } else {
        print STDERR "Couldn't find partial output mar: $outputMar\n";
    }

    printf("Moving $outputMar to $outputDir/$outputFile... \n");
    move($outputMar, "$outputDir/$outputFile") or 
     die "move($outputMar, $outputDir/$outputFile) failed: $ERRNO";
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

# Stolen from the code for bug 336463

sub RunShellCommand 
{
    my %args = @_;
    my $shellCommand = $args{'command'};

    # optional
    my $timeout = exists($args{'timeout'}) ? $args{'timeout'} : $EXEC_TIMEOUT;
    my $redirectStderr = exists($args{'redirectStderr'}) ? $args{'redirectStderr'} : 1;
    my $printOutputImmediately = exists($args{'output'}) ? $args{'output'} : 0;

    my $now = localtime();
    local $_;
 
    chomp($shellCommand);

    my $exit_value = 1;
    my $signal_num;
    my $sig_name;
    my $dumped_core;
    my $timed_out;
    my $output;

    eval {
        local $SIG{'ALRM'} = sub { die "alarm\n" };
        alarm $timeout;
 
        if (! $redirectStderr || $shellCommand =~ "2>&1") {
            open CMD, "$shellCommand |" or die "Could not run command: $!";
        } else {
            open CMD, "$shellCommand 2>&1 |" or die "Could not run command: $!";
        }

        while (<CMD>) {
            $output .= $_;
            print $_ if ($printOutputImmediately);
        }

        close CMD or die "Could not close command: $!";
        $exit_value = $? >> 8;
        $signal_num = $? >> 127;
        $dumped_core = $? & 128;
        $timed_out = 0;
        alarm 0;
    };

    if ($@){
        if ($@ eq "alarm\n") {
            $timed_out = 1;
        } else {
            warn "Error running $shellCommand: $@\n";
            $output = $@;
        }
    }

    if ($exit_value || $timed_out || $dumped_core || $signal_num) {
        if ($timed_out) {
            # callers expect exit_value to be non-zero if request timed out
            $exit_value = 1;
        }
    }

    return { timed_out => $timed_out,
             exit_value => $exit_value,
             sig_name => $sig_name,
             output => $output,
             dumped_core => $dumped_core };
}

sub SubstitutePath
{
    my %args = @_;

    my $string = $args{'path'};
    my $platform = $args{'platform'};
    my $locale = $args{'locale'};
    my $version = $args{'version'};
    my $app = $args{'app'};

    my %bouncer_platforms = GetBouncerPlatformStrings();
    my $bouncer_platform = $bouncer_platforms{$platform};

    $string =~ s/%platform%/$platform/g;
    $string =~ s/%locale%/$locale/g;
    $string =~ s/%bouncer-platform%/$bouncer_platform/g;
    $string =~ s/%version%/$version/g;
    $string =~ s/%app%/$app/g;

    return $string;
}

sub DownloadFile
{
    my %args = @_;

    my $sourceUrl = $args{'url'};

    die "ASSERT: Invalid Source URL: $sourceUrl\n" 
     if ($sourceUrl !~ m|^http://|);

    my $destArgument = defined($args{'dest'}) ? "-O $args{'dest'}" : '';

    my $httpUser = defined($args{'user'}) ?
     "--http-user=\"$args{'user'}\"" : '';
    my $httpPassword = defined($args{'password'}) ? 
     "--http-password=\"$args{'password'}\"" : '';
    my $httpAuthArgs = "$httpUser $httpPassword";

    my $wgetCommand = "wget $destArgument $httpAuthArgs --progress=dot:mega " .
     $sourceUrl;
    my $rv = RunShellCommand(command => $wgetCommand);
    if ($rv->{'exit_value'} != 0) {
        die "$wgetCommand FAILED: $rv->{'exit_value'}, output: $rv->{'output'}\n";
    }
}

1;
