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

use strict;

use Getopt::Long;
use Data::Dumper;
use Cwd;
use English;
use IO::Handle;

use File::Path;
use File::Copy qw(move copy);
use File::Spec::Functions;

use MozAUSConfig;
use MozAUSLib qw(CreatePartialMarFile
                 GetAUS2PlatformStrings
                 EnsureDeliverablesDir
                 ValidateToolsDirectory SubstitutePath);

use MozBuild::Util qw(MkdirWithPath RunShellCommand DownloadFile HashFile);

$Data::Dumper::Indent = 1;

autoflush STDOUT 1;
autoflush STDERR 1;

##
## CONSTANTS
##

use vars qw($PID_FILE
            $DEFAULT_HASH_TYPE
            $DEFAULT_CVSROOT
            $DEFAULT_SCHEMA_VERSION $CURRENT_SCHEMA_VERSION
            $SNIPPET_DIR $SNIPPET_TEST_DIR
            $ST_SIZE );

$PID_FILE = 'patcher2.pid';
$DEFAULT_HASH_TYPE = 'SHA1';
$DEFAULT_CVSROOT = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';

$DEFAULT_SCHEMA_VERSION = 0;
$CURRENT_SCHEMA_VERSION = 1;

$SNIPPET_DIR = 'aus2';
$SNIPPET_TEST_DIR = $SNIPPET_DIR . '.test';

$ST_SIZE = 7;

sub main {
    Startup();

    my (%args, %move_args);
    my $config = new MozAUSConfig();

    PrintUsage(exitCode => 1) if ($config eq undef);

    if (not $config->RequestedStep('build-tools') and
        not ValidateToolsDirectory(toolsDir => $config->GetToolsDir())) {
        my $badDir = $config->GetToolsDir();
        print STDERR <<__END_TOOLS_ERROR__;
ERROR: $badDir doesn't contain 
the required build tools and --build-tools wasn't requested; bailing...
__END_TOOLS_ERROR__
        PrintUsage(exitCode => 1);
    }

    my $startdir = getcwd();
    my $deliverableDir = EnsureDeliverablesDir(config => $config);

    #printf("PRE-REMOVE-BROKEN-UPDATES:\n\n%s", Data::Dumper::Dumper($config));
    $config->RemoveBrokenUpdates();
    #printf("POST-REMOVE-BROKEN:\n\n%s", Data::Dumper::Dumper($config));

    BuildTools(config => $config) if $config->RequestedStep('build-tools');

    run_download_complete_patches(config => $config) if $config->RequestedStep('download');

    if ($config->RequestedStep('create-patches')) {
        CreatePartialPatches(config => $config);
        CreateCompletePatches(config => $config);
    }

    if ($config->RequestedStep('(create-patches|create-patchinfo)')) {
        CreatePartialPatchinfo(config => $config);
        CreateCompletePatchinfo(config => $config);
        CreatePastReleasePatchinfo(config => $config);
    }

    Shutdown();
}

#################

sub PrintUsage {
    my %args = @_;
    my $exitCode = $args{'exitCode'};
    print STDERR <<__END_USAGE__;
You screwed up this command usage; oh well; no docs yet.
__END_USAGE__
   exit($exitCode) if (defined($exitCode));
}

sub BuildTools {
    my %args = @_;
    my $config = $args{'config'};
    my $codir = $config->GetToolsDir();

    my $startdir = getcwd();

    # Handle the cases where we shouldn't/can't proceed.
    if ( -e $codir and ! -d $codir ) {
        die "ERROR: $codir exists and isn't a directory";
    }

    # Make the parent path.
    MkdirWithPath(dir => $codir, mask => 0751) or
     die "ERROR: MkdirWithPath($codir) FAILED";
    chdir($codir);

    # Handle the cases where we shouldn't/can't proceed.
    if (-e "$codir/mozilla") {
        die "ERROR: $codir/mozilla exists.  Please move it away before continuing!";
    }

    { # Create and execute CVS checkout command.
        printf("Checking out source code... \n");
        my $cvsroot = $ENV{'CVSROOT'} || $DEFAULT_CVSROOT;
        my $checkoutArgs = ["-d$cvsroot",
                            'co', 'mozilla/client.mk' ];
        my $rv = RunShellCommand(command => 'cvs',
                                 args => $checkoutArgs, 
                                 output => 1);
        if ($rv->{'exitValue'} != 0) {
            print "checkout FAILED: $rv->{'output'}\n";
        }

        # TODO - fix this to refer to the update-tools pull-target when
        # bug 329686 gets fixed.

        $ENV{'MOZ_CO_PROJECT'} = 'all';
        my $makeArgs = ['-f', 'mozilla/client.mk', 'checkout'];
        $rv = RunShellCommand(command => 'make',
                              args => $makeArgs,
                              output => 1);

        if ($rv->{'exitValue'} != 0) {
            print "make " . join(" ", $makeArgs) . " FAILED: $rv->{'output'}\n";
        }

        printf("\n\nCheckout complete.\n");
    } # Create and execute CVS checkout command.

    my $mozDir = catfile($codir, 'mozilla');
    # The checkout directory should exist but doesn't.
    if (not -e $mozDir) { 
        die "ERROR: Couldn't create checkout directory $mozDir";
    }

    { # Build mozilla dependencies and tools.
        my $mozconfig;

        $mozconfig = "mk_add_options MOZ_CO_PROJECT=tools/update-packaging\n";
        $mozconfig .= "ac_add_options --enable-application=tools/update-packaging\n";
        # these aren't required and introduce more dependencies
        $mozconfig .= "ac_add_options --disable-dbus\n";
        $mozconfig .= "ac_add_options --disable-svg\n";
        # This is necessary because PANGO'S NOW A DEPENDANCY! WHEEEEEE.
        # (but update packaging doesn't need it)
        $mozconfig .= "ac_add_options --enable-default-toolkit=gtk2\n";
        open(MOZCFG, '>' . catfile($mozDir, '.mozconfig')) or die "ERROR: Opening .mozconfig for writing failed: $!";
        print MOZCFG $mozconfig;
        close(MOZCFG);

        my $rv = RunShellCommand(command => './configure',
                                 dir => $mozDir,
                                 output => 1);

        if ($rv->{'exitValue'} != 0) {
            print "./configure FAILED: $rv->{'output'}\n";
        }

        $rv = RunShellCommand(command => 'make',
                              dir => $mozDir,
                              output => 1);

        if ($rv->{'exitValue'} != 0) {
            print "make FAILED: $rv->{'output'}\n";
        }
    } # Build mozilla dependencies and tools.

    if (not ValidateToolsDirectory(toolsDir => $codir)) {
        die "BuildTools(): Couldn't find the tools after a BuildTools() run; something's wrong... bailing...\n";
    }

    # Change directory to the starting directory.
    chdir($startdir);

    return 1;
}

sub run_download_complete_patches {
    my %args = @_;
    my $config = $args{'config'};
    my $total = download_complete_patches(config => $config);
    download_complete_patches(total => $total, config => $config);
}

sub download_complete_patches {
    my %args = @_;
    my $config = $args{'config'};

    my $i = 0;
    my $total = $args{'total'};
    my $calculate_total = 0;
    if (defined($total)) {
        printf("Downloading complete patches - $total to download\n");
    } else {
        $calculate_total = 1;
    }

    my $startdir = getcwd();
    my $deliverableDir = EnsureDeliverablesDir(config => $config);
    chdir($deliverableDir);

    my $fromReleaseVersion = $config->GetCurrentUpdate()->{'from'};
    my $toReleaseVersion = $config->GetCurrentUpdate()->{'to'};

    my $r_config = $config->{'mAppConfig'}->{'release'};
    my @releases = ($fromReleaseVersion, $toReleaseVersion);

    for my $r (@releases) {
        my $rl_config = $r_config->{$r};
        my $rlp_config = $rl_config->{'platforms'};
        my @platforms = sort(keys(%{$rlp_config}));
        for my $p (@platforms) {
            my $platform_locales = $rlp_config->{$p}->{'locales'};

            for my $l (@$platform_locales) {
                chdir($deliverableDir);
                my $relPath = catfile($r, 'ftp');
                MkdirWithPath(dir => $relPath, mask => 0751) or 
                 die "MkdirWithPath($relPath) FAILED\n";
                chdir($relPath);

                my $download_url = $rl_config->{'completemarurl'};
                $download_url = SubstitutePath(path => $download_url,
                                               platform => $p,
                                               locale => $l);

                my $output_filename = SubstitutePath(
                 path => $MozAUSConfig::DEFAULT_MAR_NAME,
                 platform => $p,
                 locale => $l,
                 version => $r,
                 app => lc($config->GetApp()));

                next if -e $output_filename;
                $i++;
                next if $calculate_total;

                my $path = ".";
                if ( $output_filename =~ m/^(.*)\/([^\/]*)$/ ) {
                    $path = $1;
                }
                MkdirWithPath(dir => $path, mask => 0751) or 
                 die "Failed to mkpath($path)";
                chdir($path);

                my $download_url_s = $download_url;
                my $output_filename_s = $output_filename;

                #next if -e $output_filename;

                $download_url_s =~ s/^.*(.{57})$/...$1/ if (length($download_url_s) > 60);
                $output_filename_s =~ s/^.*(.{57})$/...$1/ if (length($output_filename_s) > 60);

                my $start_time = time();

                PrintProgress(total => $total, current => $i,
                 string => $output_filename_s);
                
                if (exists($rl_config->{'completemaruser'}) and
                 exists($rl_config->{'completemarpasswd'})) {
                    DownloadFile(url => $download_url,
                     dest => $output_filename,
                     user => $rl_config->{'completemaruser'}, 
                     password => $rl_config->{'completemarpasswd'} );
                } else {
                    DownloadFile(url => $download_url,
                     dest => $output_filename );
                }

                chdir(catfile($deliverableDir, $r, 'ftp'));

                my $end_time = time();
                my $total_time = $end_time - $start_time;

                if ( -f $output_filename ) {
                    printf("done (" . $total_time . "s)\n");
                } else {
                    printf("failed (" . $total_time . "s)\n");
                }

                select(undef, undef, undef, 0.5);
            }
        }
    }

    chdir($startdir);

    if (defined($total)) {
        printf("Finished downloading complete patches.\n");
    }

    return $i;
} # download_complete_patches

sub PrintProgress
{
    my %args = @_;
    my $currentStep = $args{'current'};
    my $totalSteps = $args{'total'};
    my $stepString = $args{'string'};

    my $length = length($totalSteps);
    my $format = "[%${length}s/%${length}s]";

    my $progressStr = sprintf($format, $currentStep, $totalSteps);

    print "\t$progressStr $stepString... ";
}

sub CreateCompletePatches {
    my %args = @_;
    my $config = $args{'config'};

    my $update = $config->GetCurrentUpdate();

    my $i = 0;
    my $total = 0;
    foreach my $plat (keys(%{$update->{'platforms'}})) {
        $total += scalar(keys(%{$update->{'platforms'}->{$plat}->{'locales'}}));
    }
    printf("Complete patches - $total to create\n");

    my $startdir = getcwd();
    my $deliverableDir = EnsureDeliverablesDir(config => $config);
    chdir($deliverableDir);

    my $u_config = $config->{'mAppConfig'}->{'update_data'};
    my @updates = sort keys %$u_config;

    #printf("%s", Data::Dumper::Dumper($config->{'app_config'}->{'update_data'}));

    for my $u (@updates) {
        my $complete = $u_config->{$u}->{'complete'};
        my $complete_path = $complete->{'path'};
        my $complete_url = $complete->{'url'};

        my @platforms = sort keys %{$u_config->{$u}->{'platforms'}};
        for my $p (@platforms) {
            my $ul_config = $u_config->{$u}->{'platforms'}->{$p}->{'locales'};
            my @locales = sort keys %$ul_config;
            for my $l (@locales) {
                my $from = $ul_config->{$l}->{'from'};
                my $to = $ul_config->{$l}->{'to'};

                my $from_path = $from->{'path'};
                my $to_path = $to->{'path'};

                my $to_name = $u_config->{$u}->{'to'};

                my $gen_complete_path = $complete_path;
                $gen_complete_path = SubstitutePath(path => $complete_path,
                                                    platform => $p,
                                                    locale => $l);

                my $gen_complete_url = $complete_url;
                $gen_complete_url = SubstitutePath(path => $complete_url,
                                                   platform => $p,
                                                   locale => $l);

                #printf("%s", Data::Dumper::Dumper($to));

                my $complete_pathname = catfile($u, 'ftp', $gen_complete_path);

                # Go to next iteration if this partial patch already exists.
                next if -e $complete_pathname;

                if ( -f $to_path and
                     ! -e $complete_pathname ) {
                    my $start_time = time();

                    # copy complete to the expected result
                    PrintProgress(total => $total, current => ++$i,
                     string => "$u/$p/$l");
                    $complete_pathname =~ m/^(.*)\/[^\/]*/;
                    my $parentdir = $1;
                    MkdirWithPath(dir => $parentdir, mask => 0751) or 
                     die "Failed to mkpath($parentdir)";
                    system("rsync -a $to_path $complete_pathname");

                    my $end_time = time();
                    my $total_time = $end_time - $start_time;

                    printf("done (" . $total_time . "s)\n");
                }

                #last if $i > 2;
                #$i++;
                select(undef, undef, undef, 0.5);
            }
            #last;
        }
        #last;
    }

    #printf("%s", Data::Dumper::Dumper($u_config));

    chdir($startdir);

    if (defined($total)) {
        printf("\n");
    }

    return $i;
} # create_complete_patches


sub CreatePartialPatches {
    my %args = @_;
    my $config = $args{'config'};

    my $update = $config->GetCurrentUpdate();

    my $total = 0;
    my $i = 0;
    foreach my $plat (keys(%{$update->{'platforms'}})) {
        $total += scalar(keys(%{$update->{'platforms'}->{$plat}->{'locales'}}));
    }
    printf("Partial patches - $total to create\n");

    my $startdir = getcwd();
    my $deliverableDir = EnsureDeliverablesDir(config => $config);
    chdir($deliverableDir);

    my $u_config = $config->{'mAppConfig'}->{'update_data'};
    my @updates = sort keys %$u_config;

    #printf("%s", Data::Dumper::Dumper($config->{'app_config'}->{'update_data'}));

    for my $u (@updates) {
        my $partial = $u_config->{$u}->{'partial'};
        my $partial_path = $partial->{'path'};
        my $partial_url = $partial->{'url'};
        my $forcedUpdateList = $u_config->{$u}->{'force'};

        my @platforms = sort keys %{$u_config->{$u}->{'platforms'}};
        for my $p (@platforms) {
            my $ul_config = $u_config->{$u}->{'platforms'}->{$p}->{'locales'};
            my @locales = sort keys %$ul_config;
            for my $l (@locales) {
                my $from = $ul_config->{$l}->{'from'};
                my $to = $ul_config->{$l}->{'to'};

                my $from_path = $from->{'path'};
                my $to_path = $to->{'path'};

                my $to_name = $u_config->{$u}->{'to'};

                my $gen_partial_path = $partial_path;
                $gen_partial_path = SubstitutePath(path => $partial_path,
                                                   platform => $p,
                                                   locale => $l );

                my $gen_partial_url = $partial_url;
                $gen_partial_url = SubstitutePath(path => $partial_url,
                                                  platform => $p,
                                                  locale => $l );

                #printf("%s", Data::Dumper::Dumper($to));

                my $partial_pathname = catfile($u, 'ftp', $gen_partial_path);

                # Go to next iteration if this partial patch already exists.
                next if -e $partial_pathname;
                $i++;

                if ( -f $from_path and
                     -f $to_path and
                     ! -e $partial_pathname ) {
                    my $start_time = time();

                    PrintProgress(total => $total, current => $i,
                     string => "$u/$p/$l");

                    my $rv = CreatePartialMarFile(from => $from_path,
                                                  to => $to_path,
                                                  mozdir => $config->GetToolsDir(),
                                                  outputDir => getcwd(),
                                                  outputFile => 'partial.mar',
                                                  force => $forcedUpdateList);

                    if ($rv <= 0) {
                        die 'Partial mar creation failed (see error above?); ' .
                         'aborting.'; 
                    }

                    # rename partial.mar to the expected result
                    $partial_pathname =~ m/^(.*)\/[^\/]*$/g;
                    my $partial_pathname_parent = $1;
                    MkdirWithPath(dir => $partial_pathname_parent) or 
                     die "ASSERT: MkdirWithPath($partial_pathname_parent) FAILED\n";
                    move('partial.mar', $partial_pathname) or 
                     die "ASSERT: move(partial.mar, $partial_pathname) FAILED\n";

                    my $end_time = time();
                    my $total_time = $end_time - $start_time;

                    printf("done (" . $total_time . "s)\n");
                }

                #last if $i > 2;
                #$i++;
                select(undef, undef, undef, 0.5);
            }
            #last;
        }
        #last;
    }

    #printf("%s", Data::Dumper::Dumper($u_config));

    chdir($startdir);

    if (defined($total)) {
        printf("\n");
    }

    return $i;
} # create_partial_patches

sub get_aus_platform_string {
    my $short_platform = shift;
    my %aus_platform_strings = GetAUS2PlatformStrings();
    my $aus_platform = $aus_platform_strings{$short_platform};

    if (not defined($aus_platform)) {
       die "get_aus_platform_string(): Unknown short platform: $short_platform";
    }

    return $aus_platform;
}

sub CreateCompletePatchinfo {
    my %args = @_;
    my $config = $args{'config'};

    my $i = 0;
    my $total = 0;
    my $update = $config->GetCurrentUpdate();

    my $startdir = getcwd();
    
    my $deliverableDir = EnsureDeliverablesDir(config => $config);
    chdir($deliverableDir);

    my $u_config = $config->GetAppConfig()->{'update_data'};
    my @updates = sort keys %$u_config;

    foreach my $u (@updates) {
        my @channels = @{$u_config->{$u}->{'all_channels'}};
        my @platforms = sort keys %{$u_config->{$u}->{'platforms'}};
        foreach my $p (@platforms) {
            my $ul_config = $u_config->{$u}->{'platforms'}->{$p}->{'locales'};
            my @locales = sort keys %$ul_config;
            foreach my $l (@locales) {
                foreach my $c (@channels) {
                    $total++;
                }
            }
        }
    }

    printf("Complete patch info - $total to create\n");

    #printf("%s", Data::Dumper::Dumper($config->{'app_config'}->{'update_data'}));

    for my $u (@updates) {
        my $complete = $u_config->{$u}->{'complete'};
        my $complete_path = $complete->{'path'};
        my $complete_url = $complete->{'url'};

        my @channels = @{$u_config->{$u}->{'all_channels'}};
        my $channel = $u_config->{$u}->{'channel'};
        my @platforms = sort keys %{$u_config->{$u}->{'platforms'}};
        for my $p (@platforms) {
            my $ul_config = $u_config->{$u}->{'platforms'}->{$p}->{'locales'};
            my @locales = sort keys %$ul_config;
            for my $l (@locales) {
                my $from = $ul_config->{$l}->{'from'};
                my $to = $ul_config->{$l}->{'to'};

                my $from_path = $from->{'path'};
                my $to_path = $to->{'path'};

                my $to_name = $u_config->{$u}->{'to'};

                # Build patch info
                my $from_aus_app = ucfirst($config->GetApp());
                my $from_aus_version = $from->{'appv'};
                my $from_aus_platform = get_aus_platform_string($p);
                my $from_aus_buildid = $from->{'build_id'};

                my $gen_complete_path = $complete_path;
                $gen_complete_path = SubstitutePath(path => $complete_path,
                                                    platform => $p,
                                                    locale => $l );
                my $complete_pathname = "$u/ftp/$gen_complete_path";

                my $gen_complete_url = SubstitutePath(path => $complete_url,
                                                   platform => $p,
                                                   locale => $l );

                my $detailsUrl = SubstitutePath(
                 path => $u_config->{$u}->{'details'},
                 locale => $l,
                 version => $to->{'appv'});

                my $licenseUrl = undef;
                if (defined($u_config->{$u}->{'license'})) {
                    $licenseUrl = SubstitutePath(
                     path => $u_config->{$u}->{'license'},
                     locale => $l,
                     version => $to->{'appv'});
                }

                my $updateType = $config->GetCurrentUpdate()->{'updateType'};

                for my $c (@channels) {
                    my $aus_prefix = catfile($u, $SNIPPET_DIR, 
                                             $from_aus_app,
                                             $from_aus_version,
                                             $from_aus_platform,
                                             $from_aus_buildid,
                                             $l, $c);

                    my $complete_patch = $ul_config->{$l}->{'complete_patch'};
                    $complete_patch->{'info_path'} = catfile($aus_prefix,
                     'complete.txt');

                    # Go to next iteration if this partial patch already exists.
                    next if ( -e $complete_patch->{'info_path'} or ! -e $complete_pathname );
                    $i++;

                    #printf("partial = %s", Data::Dumper::Dumper($partial_patch));
                    #printf("complete = %s", Data::Dumper::Dumper($complete_patch));

                    PrintProgress(total => $total, current => $i,
                     string => "$u/$p/$l/$c");

                    $complete_patch->{'patch_path'} = $to_path;
                    $complete_patch->{'type'} = 'complete';

                    my $hash_type = $DEFAULT_HASH_TYPE;
                    $complete_patch->{'hash_type'} = $hash_type;
                    $complete_patch->{'hash_value'} = HashFile(file => $to_path,
                                                       type => $hash_type);

                    $complete_patch->{'hash_value'} =~ s/^(\S+)\s+.*$/$1/g;

                    $complete_patch->{'build_id'} = $to->{'build_id'};
                    $complete_patch->{'appv'} = $to->{'appv'};
                    $complete_patch->{'extv'} = $to->{'extv'};
                    $complete_patch->{'size'} = (stat($to_path))[$ST_SIZE];

                    my $channelSpecificUrlKey = $c . '-url';

                    if (exists($complete->{$channelSpecificUrlKey})) {
                        $complete_patch->{'url'} = SubstitutePath(
                          path => $complete->{$channelSpecificUrlKey}, 
                          platform => $p,
                          locale => $l);
                    } else {
                        $complete_patch->{'url'} = $gen_complete_url;
                    }

                    $complete_patch->{'details'} = $detailsUrl;
                    $complete_patch->{'license'} = $licenseUrl;
                    $complete_patch->{'updateType'} = $updateType;

                    write_patch_info(patch => $complete_patch,
                                     schemaVer => $to->{'schema'});

                    if (defined($u_config->{$u}->{'testchannel'})) {
                        # Deep copy this data structure, since it's a copy of 
                        # $ul_config->{$l}->{'complete_patch'};
                        my $testPatch = {};
                        foreach my $key (keys(%{$complete_patch})) {
                            $testPatch->{$key} = $complete_patch->{$key};
                        }

                        foreach my $testChan (split(/[\s,]+/, 
                         $u_config->{$u}->{'testchannel'})) {

                            $testPatch->{'info_path'} = catfile($u,
                             'aus2.test', $from_aus_app, 
                             $from_aus_version, $from_aus_platform, 
                             $from_aus_buildid, $l, $testChan, 'complete.txt');

                            my $testUrlKey = $testChan . '-url';

                            if (exists($complete->{$testUrlKey})) {
                                $testPatch->{'url'} = SubstitutePath(path => 
                                  $complete->{$testUrlKey},
                                  platform => $p,
                                  locale => $l );
                            } else {
                                $testPatch->{'url'} = $gen_complete_url;
                            }

                            write_patch_info(patch => $testPatch,
                                             schemaVer => $to->{'schema'});
                        }
                    }

                    print("done\n");
                }
            }
        }
    }

    chdir($startdir);

    printf("\n");

    return $i;
} # create_complete_patch_info

sub CreatePastReleasePatchinfo {
    my %args = @_;
    my $config = $args{'config'};

    my $patchInfoFilesCreated = 0;
    my $totalPastUpdates = 0;

    my $startDir = getcwd();
    my $deliverableDir = EnsureDeliverablesDir(config => $config);
    chdir($deliverableDir);

    my $update = $config->GetCurrentUpdate();
    my $prefixStr = "$update->{'from'}-$update->{'to'}";

    foreach my $pastUpd (@{$config->GetPastUpdates()}) {
        my $fromRelease = $config->GetAppRelease($pastUpd->{'from'});
        my @pastFromPlatforms = sort(keys(%{$fromRelease->{'platforms'}}));
        foreach my $fromPlatform (@pastFromPlatforms) {
            foreach my $locale (@{$fromRelease->{'platforms'}->{$fromPlatform}->{'locales'}}) {
                foreach my $channel (@{$pastUpd->{'channels'}}) {
                    $totalPastUpdates++;
                }
            }
        }
    }

    # Multiply by two for the partial and the complete...
    $totalPastUpdates *= 2;

    printf("Past release patch info - $totalPastUpdates to create\n");

    foreach my $pastUpd (@{$config->GetPastUpdates()}) {
        my $fromRelease = $config->GetAppRelease($pastUpd->{'from'});
        my $currentRelease = $config->GetAppRelease($config->GetCurrentUpdate()->{'to'});

        my @pastFromPlatforms = sort(keys(%{$fromRelease->{'platforms'}}));

        my $complete = $config->GetCurrentUpdate()->{'complete'};
        my $completePath = $complete->{'path'};
        my $completeUrl = $complete->{'url'};

        foreach my $fromPlatform (@pastFromPlatforms) {
            # XXX - This is a hack, solely to support the fact that "mac"
            # now means "universal binaries," but for 1.5.0.2 and 1.5.0.3, there
            # was "mac" which meant PPC and "unimac," which meant universal
            # binaries. Unfortunately, we can't just make > 1.5.0.4 use a
            # platform of "unimac," because all the filenames are foo.mac.dmg,
            # not foo.unimac.dmg. Le sigh.
            #
            # So, what this does is checks if $patchPlatformNode is null AND
            # our platform is macppc; we want all the old macppc builds to
            # update to the universal builds; so, we can get the proper locales
            # and build IDs by grabbing the proper patchPlatformNode using
            # the a key of 'mac' to generate the right strings.
            #
            # But, that's not all; we need to support this concept of "platform
            # transformations," so now we have a "fromPlatform" and a
            # "toPlatform" which, MOST of the time, will be the same, but
            # for this specific case, won't be.

            my $toPlatform = $fromPlatform;
            my $patchPlatformNode = $update->{'platforms'}->{$toPlatform};

            if ($patchPlatformNode eq undef && $fromPlatform eq 'macppc') {
                $toPlatform = 'mac';
                $patchPlatformNode = $update->{'platforms'}->{$toPlatform};
            }


            foreach my $locale (@{$fromRelease->{'platforms'}->{$fromPlatform}->{'locales'}}) {
                my $patchLocaleNode = $patchPlatformNode->{'locales'}->{$locale}->{'to'};
                if ($patchLocaleNode eq undef) {
                    print STDERR "No known patch for locale $locale, $fromRelease->{'version'} -> $currentRelease->{'version'}; skipping...\n";
                    next;
                }

                my $to_path = $patchLocaleNode->{'path'};

                # Build patch info
                my $fromAusApp = ucfirst($config->GetApp());
                my $fromAusVersion = $fromRelease->{'version'};
                my $fromAusPlatform = get_aus_platform_string($fromPlatform);
                my $fromAusBuildId = $fromRelease->{'platforms'}->{$fromPlatform}->{'build_id'};

                my $genCompletePath = SubstitutePath(path => $completePath,
                                                     platform => $toPlatform,
                                                     locale => $locale );

                my $completePathname = "$prefixStr/ftp/$genCompletePath";

                my $genCompleteUrl = SubstitutePath(path => $completeUrl,
                                                    platform => $toPlatform,
                                                    locale => $locale );

                my $detailsUrl = SubstitutePath(
                 path => $config->GetCurrentUpdate()->{'details'},
                 locale => $locale,
                 version => $patchLocaleNode->{'appv'});

                my $licenseUrl = undef;
                if (defined($config->GetCurrentUpdate()->{'license'})) {
                    $licenseUrl = SubstitutePath(
                     path => $config->GetCurrentUpdate()->{'license'},
                     locale => $locale,
                     version => $patchLocaleNode->{'appv'});
                }

                my $updateType = $config->GetCurrentUpdate()->{'updateType'};

                foreach my $channel (@{$pastUpd->{'channels'}}) {
                    my $ausDir = ($channel =~ /test(-\w+)?$/) 
                     ? $SNIPPET_TEST_DIR : $SNIPPET_DIR;

                     my $ausPrefix = catfile($prefixStr, $ausDir, $fromAusApp,
                                             $fromAusVersion, $fromAusPlatform,
                                             $fromAusBuildId, $locale,
                                             $channel);

                    my $completePatch = {};
                    $completePatch ->{'info_path'} = catfile($ausPrefix,
                                                             'complete.txt');


                    my $prettyPrefix = "$pastUpd->{'from'}-$update->{'to'}";
                    PrintProgress(total => $totalPastUpdates,
                     current => ++$patchInfoFilesCreated,
                     string => "$prettyPrefix/$fromAusPlatform/$locale/$channel/complete"); 

                    # Go to next iteration if this partial patch already exists.
                    #next if ( -e $complete_patch->{'info_path'} or ! -e $complete_pathname );

                    $completePatch->{'patch_path'} = $to_path;
                    $completePatch->{'type'} = 'complete';

                    my $hash_type = $DEFAULT_HASH_TYPE;
                    $completePatch->{'hash_type'} = $hash_type;
                    $completePatch->{'hash_value'} = HashFile(file => $to_path,
                                                              type => $hash_type);
                    $completePatch->{'build_id'} = $patchLocaleNode->{'build_id'};
                    $completePatch->{'appv'} = $patchLocaleNode->{'appv'};
                    $completePatch->{'extv'} = $patchLocaleNode->{'extv'};
                    $completePatch->{'size'} = (stat($to_path))[$MozAUSLib::ST_SIZE];

                    my $channelSpecificUrlKey = $channel . '-url';

                    if (exists($completePatch->{$channelSpecificUrlKey})) {
                        $completePatch->{'url'} = SubstitutePath(
                         path => $completePatch->{$channelSpecificUrlKey},
                         platform => $toPlatform,
                         locale => $locale);
                    } else {
                        $completePatch->{'url'} = $genCompleteUrl;
                    }

                    $completePatch->{'details'} = $detailsUrl;
                    $completePatch->{'license'} = $licenseUrl;
                    $completePatch->{'updateType'} = $updateType;

                    write_patch_info(patch => $completePatch,
                                     schemaVer => $patchLocaleNode->{'schema'});
                    print("done\n");

                    # Now, write the same information as a partial, since
                    # for now, we publish the "partial" and "complete" updates
                    # as pointers to the complete.
                    $completePatch->{'type'} = 'partial';
                    $completePatch->{'info_path'} = "$ausPrefix/partial.txt";
                    PrintProgress(total => $totalPastUpdates,
                     current => ++$patchInfoFilesCreated,
                     string => "$prettyPrefix/$fromAusPlatform/$locale/$channel/partial"); 
                    write_patch_info(patch => $completePatch,
                                     schemaVer => $patchLocaleNode->{'schema'});
                    print("done\n");
                }
            }
        }
    }

    chdir($startDir);
    printf("\n");
}


sub CreatePartialPatchinfo {
    my %args = @_;
    my $config = $args{'config'};

    my $i = 0;
    my $total = 0;

    #printf("%s", Data::Dumper::Dumper($config->{'app_config'}->{'update_data'}));

    my $startdir = getcwd();
    my $deliverableDir = EnsureDeliverablesDir(config => $config);
    chdir($deliverableDir);

    my $u_config = $config->{'mAppConfig'}->{'update_data'};
    my @updates = sort keys %$u_config;

    # TODO - This could be cleaner.
    foreach my $u (@updates) {
        my @channels = @{$u_config->{$u}->{'all_channels'}};
        my @platforms = sort keys %{$u_config->{$u}->{'platforms'}};
        foreach my $p (@platforms) {
            my $ul_config = $u_config->{$u}->{'platforms'}->{$p}->{'locales'};
            my @locales = sort keys %$ul_config;
            foreach my $l (@locales) {
                foreach my $c (@channels) {
                    $total++;
                }
            }
        }
    }

    printf("Partial patch info - $total to create\n");

    #printf("%s", Data::Dumper::Dumper($config->{'app_config'}->{'update_data'}));

    for my $u (@updates) {
        my $partial = $u_config->{$u}->{'partial'};
        my $partial_path = $partial->{'path'};
        my $partial_url = $partial->{'url'};

        my @channels = @{$u_config->{$u}->{'all_channels'}};
        my $channel = $u_config->{$u}->{'channel'};
        my @platforms = sort keys %{$u_config->{$u}->{'platforms'}};
        for my $p (@platforms) {
            my $ul_config = $u_config->{$u}->{'platforms'}->{$p}->{'locales'};
            my @locales = sort keys %$ul_config;
            for my $l (@locales) {
                my $from = $ul_config->{$l}->{'from'};
                my $to = $ul_config->{$l}->{'to'};

                my $from_path = $from->{'path'};
                my $to_path = $to->{'path'};

                #my $to_name = $u_config->{$u}->{'to'};

                # Build patch info
                my $from_aus_app = ucfirst($config->GetApp());
                my $from_aus_version = $from->{'appv'};
                my $from_aus_platform = get_aus_platform_string($p);
                my $from_aus_buildid = $from->{'build_id'};

                my $gen_partial_path = $partial_path;
                $gen_partial_path = SubstitutePath(path => $partial_path,
                                                   platform => $p,
                                                   locale => $l );
                my $partial_pathname = "$u/ftp/$gen_partial_path";

                my $gen_partial_url = SubstitutePath(path => $partial_url,
                                                     platform => $p,
                                                     locale => $l );

                my $detailsUrl = SubstitutePath(
                 path => $u_config->{$u}->{'details'},
                 locale => $l,
                 version => $to->{'appv'});
            
                my $licenseUrl = undef;
                if (defined($u_config->{$u}->{'license'})) {
                    $licenseUrl = SubstitutePath(
                     path => $u_config->{$u}->{'license'},
                     locale => $l,
                     version => $to->{'appv'});
                }

                my $updateType = $u_config->{$u}->{'updateType'};

                for my $c (@channels) {
                    my $aus_prefix = catfile($u, $SNIPPET_DIR, 
                                             $from_aus_app,
                                             $from_aus_version,
                                             $from_aus_platform,
                                             $from_aus_buildid,
                                             $l,
                                             $c);

                    my $partial_patch = $ul_config->{$l}->{'partial_patch'};
                    $partial_patch->{'info_path'} = catfile($aus_prefix,
                                                            'partial.txt');

                    # Go to next iteration if this partial patch already exists.
                    next if ( -e $partial_patch->{'info_path'} or ! -e $partial_pathname );
                    $i++;

                    PrintProgress(total => $total, current => $i,
                     string => "$u/$p/$l/$c");

                    $partial_patch->{'patch_path'} = $partial_pathname;
                    $partial_patch->{'type'} = 'partial';

                    my $hash_type = $DEFAULT_HASH_TYPE;
                    $partial_patch->{'hash_type'} = $hash_type;
                    $partial_patch->{'hash_value'} = HashFile(
                     file => $partial_pathname,
                     type => $hash_type);

                    $partial_patch->{'hash_value'} =~ s/^(\S+)\s+.*$/$1/g;

                    $partial_patch->{'build_id'} = $to->{'build_id'};
                    $partial_patch->{'appv'} = $to->{'appv'};
                    $partial_patch->{'extv'} = $to->{'extv'};
                    $partial_patch->{'size'} = (stat($partial_pathname))[$ST_SIZE];

                    my $channelSpecificUrlKey = $c . '-url';

                    if (exists($partial->{$channelSpecificUrlKey})) {
                        $partial_patch->{'url'} = SubstitutePath(
                          path => $partial->{$channelSpecificUrlKey}, 
                          platform => $p,
                          locale => $l);
                    } else {
                        $partial_patch->{'url'} = $gen_partial_url;
                    }

                    $partial_patch->{'details'} = $detailsUrl;
                    $partial_patch->{'license'} = $licenseUrl;
                    $partial_patch->{'updateType'} = $updateType;

                    write_patch_info(patch => $partial_patch,
                                     schemaVer => $to->{'schema'});

                    if (defined($u_config->{$u}->{'testchannel'})) {
                        # Deep copy this data structure, since it's a copy of 
                        # $ul_config->{$l}->{'complete_patch'};
                        my $testPatch = {};
                        foreach my $key (keys(%{$partial_patch})) {
                            $testPatch->{$key} = $partial_patch->{$key};
                        }

                        foreach my $testChan (split(/[\s,]+/, 
                         $u_config->{$u}->{'testchannel'})) {

                            $testPatch->{'info_path'} = catfile($u,
                             'aus2.test', $from_aus_app,
                             $from_aus_version, $from_aus_platform,
                             $from_aus_buildid, $l, $testChan, 'partial.txt');

                            my $testChanKey = $testChan . '-url';

                            if (exists($partial->{$testChanKey})) {
                                $testPatch->{'url'} = SubstitutePath(path => 
                                 $partial->{$testChanKey},
                                 platform => $p,
                                 locale => $l );
                            } else {
                                $testPatch->{'url'} = $gen_partial_url;
                            }

                            write_patch_info(patch => $testPatch,
                                             schemaVer => $to->{'schema'});
                        }
                    }

                    printf("done\n");
                }
            }
        }
    }

    chdir($startdir);

    if (defined($total)) {
        printf("\n");
    }

    return $i;
} # create_partial_patch_info

sub write_patch_info {
    my %args = @_;

    my $patch = $args{'patch'};
    my $schemaVersion = $args{'schemaVer'} || $DEFAULT_SCHEMA_VERSION;

    my $info_path = $patch->{'info_path'};
    $info_path =~ m/^(.*)\/[^\/]*$/;
    my $info_path_parent = $1;
    my $text;

    if ($DEFAULT_SCHEMA_VERSION == $schemaVersion) {
        $text  = "$patch->{'type'}\n";
        $text .= "$patch->{'url'}\n";
        $text .= "$patch->{'hash_type'}\n";

        $text .= "$patch->{'hash_value'}\n";

        $text .= "$patch->{'size'}\n";
        $text .= "$patch->{'build_id'}\n";
        $text .= "$patch->{'appv'}\n";
        $text .= "$patch->{'extv'}\n";

        if (defined($patch->{'details'})) {
            $text .= "$patch->{'details'}\n";
        }
        if (defined($patch->{'license'})) {
            $text .= "$patch->{'license'}\n";
        }
        if (defined($patch->{'updateType'})) {
            $text .= "$patch->{'updateType'}\n";
        }
    } elsif ($CURRENT_SCHEMA_VERSION == $schemaVersion) {
        $text = "version=1\n";
        $text .= "type=$patch->{'type'}\n";
        $text .= "url=$patch->{'url'}\n";

        $text .= "hashFunction=$patch->{'hash_type'}\n";
        $text .= "hashValue=$patch->{'hash_value'}\n";

        $text .= "size=$patch->{'size'}\n";
        $text .= "build=$patch->{'build_id'}\n";
        $text .= "appv=$patch->{'appv'}\n";
        $text .= "extv=$patch->{'extv'}\n";

        if (defined($patch->{'details'})) {
            $text .= "detailsUrl=$patch->{'details'}\n";
        }

        if (defined($patch->{'license'})) {
            $text .= "licenseUrl=$patch->{'license'}\n";
        }

        if (defined($patch->{'updateType'})) {
            $text .= "updateType=$patch->{'updateType'}\n";
        }
    } else {
        die "ASSERT: Invalid schema version: $schemaVersion\n";
    }

    MkdirWithPath(dir => $info_path_parent) or
     die "MkdirWithPath($info_path_parent) FAILED";
    open(PATCHINFO, ">$patch->{'info_path'}") or 
     die "ERROR: Couldn't open $patch->{'info_path'} for writing!";
    print PATCHINFO $text;
    close(PATCHINFO);
} # write_patch_info

sub Startup
{
    # A bunch of assumptions are made that this is NOT Win32; assert that...
    die "ASSERT: Can not currently run on Win32.\n" if ($OSNAME eq 'MSWin32');
    expire_or_win();
}

sub Shutdown
{
    print STDERR "IN SHUTDOWN...\n";
    my $rv = unlink($PID_FILE);
    # We should probably die in this condition, but... since we're shutting 
    # down, who cares?
    print STDERR "Failed to remove $PID_FILE: $ERRNO\n" if (not $rv);
}

# Contest subroutine that will either die or, if it returns, we have authority
# over other instances of this script to proceed.

sub expire_or_win {
    # Create the pid file before continuing.
    system("touch $PID_FILE");

    # Open the pid file for update (modes read and write).
    open(FH, "+<$PID_FILE") or die "Cannot open $PID_FILE for update: $!\n";

    # Obtain a file lock on the handle.
    flock(FH, 2);

    # Gather existing data from the file.
    my $existing_data;
    {
        local($/) = undef;
        $existing_data = <FH>;
    }
    chomp($existing_data);

    # If the existing data is a process ID that is already alive, die.
    if ( length($existing_data) > 0 and process_is_alive($existing_data) ) {
        die("System already running with PID $existing_data!\n");
    }

    # Otherwise, we reset the file handle and truncate the pid file, then write
    # our PID into it.
    seek(FH, 0, 0);
    truncate(FH, 0);
    print FH "$PID\n";

    # All done with the pid file.
    close(FH);
}

sub process_is_alive {
    my ($pid) = @_;

    my $psout = `ps -A | grep $pid | grep -v grep | awk "{if (\\\$1 == $pid) print}"`;
    length($psout) > 0 ? 1 : 0;
}

##
## ENTRY POINT (Yes, aaaalll the way down here...)
##

$SIG{'__DIE__'} = \&Shutdown;
$SIG{'INT'} = \&Shutdown;

main();
