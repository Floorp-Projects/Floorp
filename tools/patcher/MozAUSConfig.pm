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

package MozAUSConfig;

use Cwd;
use Config::General;
use Data::Dumper;

require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(new DEFAULT_MAR_NAME);

use MozAUSLib qw(SubstitutePath);

use strict;

##
## CONSTANTS
##

use vars qw( @RUN_MODES
             $DEFAULT_APP $DEFAULT_MODE $DEFAULT_CONFIG_FILE
             $DEFAULT_DOWNLOAD_DIR $DEFAULT_DELIVERABLE_DIR
             $DEFAULT_MAR_NAME
           );

@RUN_MODES = qw( build-tools
                 download 
                 create-patches create-patchinfo );

$DEFAULT_CONFIG_FILE = 'patcher2.cfg';
$DEFAULT_APP = 'MyApp';
$DEFAULT_DOWNLOAD_DIR = 'downloads';
$DEFAULT_DELIVERABLE_DIR = 'temp';
$DEFAULT_MAR_NAME = '%app%-%version%.%locale%.%platform%.complete.mar';

sub new
{
    my $class = shift;
    my $this = {};
    bless($this, $class);
    return $this->Initialize() ? $this : undef;
}

sub Initialize
{
    my $this = shift;
    $this->{'mStartingDir'} = getcwd();
    return ($this->ProcessCommandLineArgs() and
            $this->ReadConfig() and 
            $this->ExpandConfig() and
            $this->ReadPastUpdates() and
            $this->CreateUpdateGraph() and
            $this->TransferChannels());
}

sub GetStartingDir
{
    my $this = shift;
    die "ASSERT: mStartingDir must be a full path: $this->{'mStartingDir'}\n"
     if ($this->{'mStartingDir'} !~ m:^/:);
    return $this->{'mStartingDir'};
}

sub ProcessCommandLineArgs
{
    my $this = shift;
    my (%args);

    Getopt::Long::Configure('bundling_override', 'ignore_case_always', 
     'pass_through');
    Getopt::Long::GetOptions(\%args,
     'help|h|?', 'man', 'version', 'app=s', 'config=s', 'verbose',
     'dry-run', 'tools-dir=s', 'download-dir=s', 'deliverable-dir=s',
     @RUN_MODES)
     or return 0;

    $this->{'mConfigFilename'} = defined($args{'config'}) ? $args{'config'} :
     $DEFAULT_CONFIG_FILE;

    $this->{'mApp'} = defined($args{'app'}) ? $args{'app'} : $DEFAULT_APP;

    $this->{'mVerbose'} = defined($args{'verbose'}) ? $args{'verbose'} : 0;
    
    $this->{'mDownloadDir'} = defined($args{'mDownloadDir'}) ? 
     $args{'mDownloadDir'} : $DEFAULT_DOWNLOAD_DIR;
    $this->{'mDeliverableDir'} = defined($args{'mDeliverableDir'}) ? 
     $args{'mDeliverableDir'} : $DEFAULT_DELIVERABLE_DIR;

    # Is this a dry run, and we'll just print what we *would* do?
    $this->{'dryRun'} = defined($args{'dryRun'}) ? 1 : 0;

    ## Expects to be the dir that $mToolsDir/mozilla/[all the tools] will be in.
    $this->{'mToolsDir'} = defined($args{'mToolsDir'}) ? $args{'mToolsDir'} : getcwd();

    # A bunch of paths need to be full pathed; they're all part of $this, so all
    # we really need is the key values; check them all here.
    foreach my $pathKey (qw(mDownloadDir mDeliverableDir mToolsDir)) {
        if ($this->{$pathKey} !~ m:^/:) {
            $this->{$pathKey} = $this->GetStartingDir() . '/' .
             $this->{$pathKey};
        }
    }

    $this->{'run'} = [];

    foreach my $mode (@RUN_MODES) {
        push(@{$this->{'run'}}, $mode) if defined $args{$mode};
    }

    return 0 if (not scalar(@{$this->{'run'}}));

    return 1;
}

sub ReadConfig
{
    my $this = shift;

    my $configFile = $this->{'mConfigFilename'};
    if (not -r $configFile) {
        die "Config file '$configFile' isn't readable";   
    }
    my $configObj = new Config::General(-ConfigFile => $configFile);

    my %rawConfig = $configObj->getall();

    $this->{'mRawConfig'} = \%rawConfig;
    return $this->CookConfig();
}

sub CookConfig
{
    my $this = shift;
    $this->{'mAppConfig'} = $this->{'mRawConfig'}->{'app'}->{$this->GetApp()};
    return 1;
}

sub GetAppConfig
{
    my $this = shift;
    return $this->{'mAppConfig'};
}

sub GetAllAppReleases
{
    my $this = shift;
    return $this->GetAppConfig()->{'release'};
}

sub GetAppRelease
{
    my $this = shift;
    my $release = shift;
    return $this->GetAppConfig()->{'release'}->{$release};
}

sub GetPastUpdates
{
    my $this = shift;
    return $this->GetAppConfig()->{'mPastUpdates'};
}

sub  GetCurrentUpdate
{
    my $this = shift;
    my $updateData = $this->GetAppConfig()->{'update_data'};
    my @updateKeys = keys(%{$updateData});
    die "ASSERT: MozAUSConfig::GetCurrentUpdate() must return 1 update\n"
     if (scalar(@updateKeys) != 1);
    ## Stupid...
    return $this->GetAppConfig()->{'update_data'}->{$updateKeys[0]};
}

##
## Creates a platform entry in each release hash that maps platform 
## -> buildid/the locales the platform needs updates for; takes into account
## the exceptions list, and prunes those.
##

sub ExpandConfig
{
    my $this = shift;

    my $prefix_dir = $this->{'prefix'};
    my $buildinfo_dir = "$prefix_dir/build";

    # Expand basic release config into information about locales, pruning as needed.

    my $r_config = $this->GetAppConfig()->{'release'};
    for my $r (keys(%{$this->GetAllAppReleases()})) {
        my $rl_config = $r_config->{$r};
        my $locale_list = $rl_config->{'locales'};

        my @locales = split(/\s+/, $locale_list);

        my $rlp_config = $rl_config->{'platforms'};
        my @platforms = keys(%{$rlp_config});
        for my $p (@platforms) {
            my $build_id = $rlp_config->{$p};
            delete($rlp_config->{$p});
            if (!exists($rlp_config->{$p}->{'locales'})
                or !defined($rlp_config->{$p}->{'locales'})) {
                $rlp_config->{$p}->{'locales'} = [];
            }
            $rlp_config->{$p}->{'build_id'} = $build_id;

            my $platform_locales = $rlp_config->{$p}->{'locales'};

            for my $l (@locales) {
                if (exists($rl_config->{'exceptions'}->{$l})
                    and defined($rl_config->{'exceptions'}->{$l})) {

                    my @e = split(/\s*,\s*/, $rl_config->{'exceptions'}->{$l});

                    push(@$platform_locales, $l) if grep(/^$p$/, @e);
                    next;
                }

                push(@$platform_locales, $l);
            }

            my @sorted_locales = sort(@{$platform_locales});
            $rlp_config->{$p}->{'locales'} = \@sorted_locales;
        }
    }

    return 1;
}

#
# Parses all the past-update lines, and adds the past update channels to the
# update-TO release. Not that useful?
#

sub ReadPastUpdates
{
    my $this = shift;

    my(@update_config, @updates);
    my $update = $this->{'mAppConfig'}->{'past-update'};
    # An artifact of Config::General; if multiple |past-update|s are 
    # defined, you get them in an array ref; otherwise, it's just a string.
    if (ref($update) eq 'ARRAY') {
        @update_config = @{$update};
    } else {
        push(@update_config, $update);
    }

    for my $u (@update_config) {
        # Parse apart the update definition into from/to/update channels and 
        # stuff the info into @updates.
        if ( $u =~ /^\s*(\S+)\s+(\S+)\s+([\w\s\-]+)$/ ) {
            my $update_node = {};
            $update_node->{'from'} = $1;
            $update_node->{'to'} = $2;
            my @update_channels = split(/\s+/, $3);
            $update_node->{'channels'} = \@update_channels;

            push(@updates, $update_node);
        }
        else {
            print STDERR "read_past_updates(): Invalid past-update specification: $u\n";
        }
    }

    my $appConfig = $this->GetAppConfig();
    $appConfig->{'mPastUpdates'} = [];

    for my $u (@updates) {
        my $u_from = $u->{'from'};
        my $u_to = $u->{'to'};
        my @u_channels = @{$u->{'channels'}};

        # If the release that this update points to isn't defined, ...
        if (!exists($this->{'mAppConfig'}->{'release'}->{$u_to}) or
            !defined($this->{'mAppConfig'}->{'release'}->{$u_to})) {
            # Skip to the next update.
            print STDERR "Warning: a past-update mentions release $u_to which doesn't exist.\n";
            next;
        }

        my $pastUpdateNode = { 'from' => $u_from,
                               'to' => $u_to,
                               'channels' => \@u_channels };

        push(@{$appConfig->{'mPastUpdates'}}, $pastUpdateNode);

        my $ur_config = $this->{'mAppConfig'}->{'release'}->{$u_to};
        foreach my $channel (@u_channels) {
            $this->AddChannelToRelease(release => $ur_config,
                                       channel => $channel);
        }
    }

    return 1;
}

sub CreateUpdateGraph
{
    my $this = shift;
    my %args = @_;

    my $appConfig = $this->GetAppConfig();

    my $temp_prefix = lc($this->GetApp());

    my @updates;
    my $update = $appConfig->{'current-update'};
    if (ref($update) eq 'ARRAY') {
        @updates = @$update;
    } else {
        push(@updates, $update);
    }

    if (!defined($appConfig->{'update_data'})) {
        $appConfig->{'update_data'} = {};
    }

    my $u_config = $appConfig->{'update_data'};

    for my $u (@updates) {
        my $u_from = $u->{'from'};
        my $u_to = $u->{'to'};
        my $u_channel = $u->{'channel'};
        my $u_testchannel = $u->{'testchannel'};
        my $u_partial = $u->{'partial'};
        my $u_complete = $u->{'complete'};
        my $u_details = $u->{'details'};
        my $u_license = $u->{'license'};
        my $u_updateType = $u->{'updateType'};
        my $u_force = [];
      
        if (defined($u->{'force'})) {
            if (ref($u->{'force'}) eq 'ARRAY') {
                $u_force = $u->{'force'};
            } else {
                push(@{$u_force}, $u->{'force'}); 
            }
        }

        my $u_key = "$u_from-$u_to";

        # If the release that this update points to isn't defined, ...
        # TODO - check the from release
        if (not defined($this->GetAppRelease($u_to))) {
            # Skip to the next update.
            print STDERR "Update $u_key refers to a non-existant release endpoint: $u_to\n";
            next;
        }

        # Add the channel(s) to the update information.
        my $ur_config = $appConfig->{'release'}->{$u_from};
        my @channels = split(/\s+/, $u_channel);
        for my $c (@channels) {
            $this->AddChannelToRelease(release => $ur_config, channel => $c);
        }

        if (!defined($u_config->{$u_key})) {
            $u_config->{$u_key} = {};
        }

        $u_config->{$u_key}->{'from'} = $u_from;
        $u_config->{$u_key}->{'to'} = $u_to;
        $u_config->{$u_key}->{'channel'} = $u_channel;
        $u_config->{$u_key}->{'testchannel'} = $u_testchannel;
        $u_config->{$u_key}->{'partial'} = $u_partial;
        $u_config->{$u_key}->{'complete'} = $u_complete;
        $u_config->{$u_key}->{'details'} = $u_details;
        $u_config->{$u_key}->{'license'} = $u_license;
        $u_config->{$u_key}->{'updateType'} = $u_updateType;
        $u_config->{$u_key}->{'force'} = $u_force;
        $u_config->{$u_key}->{'platforms'} = {};

        my $r_config = $this->{'mAppConfig'}->{'release'};
        my @releases = keys %$r_config;

        # Find the set of locales that intersect for each platform by calculating how many times they appear.

        my $locale_intersection = {};

        for my $side ('from', 'to') {
            my $subu = $side eq 'from' ? $u_from : $u_to;

            if (!defined($r_config->{$subu})) {
                die "ERROR: trying to update from/to a build that we have no release info about!";
            }

            my $rl_config = $r_config->{$subu};
            my $rlp_config = $rl_config->{'platforms'};
            my @platforms = keys %$rlp_config;
            for my $p (@platforms) {
                my $platform_locales = $rlp_config->{$p}->{'locales'};

                for my $l (@$platform_locales) {
                    if (!defined($locale_intersection->{$p})) {
                        $locale_intersection->{$p} = {};
                    }

                    if (!defined($locale_intersection->{$p}->{$l})) {
                        $locale_intersection->{$p}->{$l} = 0;
                    }

                    $locale_intersection->{$p}->{$l}++;
                }
            }
        } # for my $side ("from", "to")

        # Store the set of locales that intersect in the $update_locales hash.

        my $update_locales = {};

        for my $platform (keys %$locale_intersection) {
            for my $locale (keys %{$locale_intersection->{$platform}}) {
                if ($locale_intersection->{$platform}->{$locale} > 1) {
                    if (!defined($update_locales->{$platform})) {
                        $update_locales->{$platform} = [];
                    }

                    push(@{$update_locales->{$platform}}, $locale);
                }
            }
        }



        # Now build an update based on each side of the update graph that 
        # only creates an update between the locale intersection.

        for my $side ('from', 'to') {
            my $subu = $side eq 'from' ? $u_from : $u_to;

            if (!defined($r_config->{$subu})) {
                die "ERROR: trying to update from/to a build that we have no release info about!";
            }

            my $rl_config = $r_config->{$subu};
            my $rlp_config = $rl_config->{'platforms'};
            my @platforms = keys %$rlp_config;
            for my $p (@platforms) {
                if (!defined($u_config->{$u_key}->{'platforms'}->{$p})) {
                    $u_config->{$u_key}->{'platforms'}->{$p} = {};
                }

                my $up_config = $u_config->{$u_key}->{'platforms'}->{$p};

                $up_config->{'build_id'} = $rlp_config->{$p}->{'build_id'};

                if (!defined($up_config->{'locales'})) {
                    $up_config->{'locales'} = {};
                }

                my $ul_config = $up_config->{'locales'};

                my $platform_locales = $update_locales->{$p};
                for my $l (@$platform_locales) {
                    if (!defined($ul_config->{$l})) {
                        $ul_config->{$l} = {};
                    }

                    $ul_config->{$l}->{$side} = $this->GatherCompleteData(
                     release => $subu,
                     completemarurl => $rl_config->{'completemarurl'},
                     platform => $p,
                     locale => $l,
                     build_id => $rlp_config->{$p}->{'build_id'},
                     version => $rl_config->{'version'},
                     extensionVersion => $rl_config->{'extension-version'},
                     schemaVersion => $rl_config->{'schema'} );
                }
            }
        } # for my $side ("from", "to")
    }
    return 1;
}

sub GatherCompleteData
{
    my $this = shift;

    my %args = @_;

    my $completemarurl = $args{'completemarurl'};
    my $platform = $args{'platform'};
    my $locale = $args{'locale'};
    my $release = $args{'release'};
    my $build_id = $args{'build_id'};
    my $version = $args{'version'};
    my $extensionVersion = $args{'extensionVersion'};
    my $schemaVersion = $args{'schemaVersion'};

    my $config = $args{'config'};

    #my $startdir = getcwd();
    $completemarurl = SubstitutePath(path => $completemarurl,
                                     platform => $platform,
                                     locale => $locale,
                                     version => $version );

    my $filename = SubstitutePath(path => $DEFAULT_MAR_NAME,
                                  platform => $platform,
                                  locale => $locale,
                                  version => $release,
                                  app => lc($this->GetApp()));

    my $local_filename = "$release/ftp/$filename";

    my $node = {};
    $node->{'path'} = $local_filename;

    if ( -e $local_filename ) {
        #printf("found $local_filename\n");
        #my $md5 = `md5sum $local_filename`;
        #chomp($md5);
        #$md5 =~ s/^([^\s]*)\s+.*$/$1/g;

        #$node->{'md5'} = $md5;
        #$node->{'size'} = (stat($local_filename))[7];
    } else {
        #printf("did not find $local_filename\n");
    }

    $node->{'url'} = $completemarurl;
    $node->{'build_id'} = $build_id;
    # XXX - This is a huge hack and needs to be fixed. It's to support 
    # Y!/Google builds, wherein we specify the version as 
    # Firefox_version-google/yahoo. However, since this is used for appv 
    # and extv, the version needs to NOT have these -google/-yahoo identifiers.
    my $numericVersion = $version;
    $numericVersion =~ s/\-.*$//;
    $node->{'appv'} = $numericVersion;

    # Most of the time, the extv should be the same as the appv; sometimes,
    # however, this won't be the case. This adds support to allow you to specify
    # an extv that is different from the appv. 
    $node->{'extv'} = defined($extensionVersion) ? 
     $extensionVersion : $numericVersion;

    if (defined($schemaVersion)) {
        $node->{'schema'} = $schemaVersion;
    }

    #chdir($startdir);

    return $node;
}


sub AddChannelToRelease
{
    my $this = shift;
    my %args = @_;
    my $rconfig = $args{'release'};
    my $newChannel = $args{'channel'};

    # Validate arguments...
    die "ASSERT: AddChannelToRelease(): Invalid release config" if (not defined($rconfig));

    die "ASSERT: AddChannelToRelease(): Invalid new channel" if (not defined($newChannel));

    if (!defined($rconfig->{'all_channels'})) {
        # Create the release's all_channels.
        $rconfig->{'all_channels'} = [];
    }

    # return if the release already has this channel in all_channels, ...
    return if (grep(/^$newChannel$/, @{$rconfig->{'all_channels'}}));

    # Add the new channel to all_channels.
    push(@{$rconfig->{'all_channels'}}, $newChannel);
    return 1;
}

##
## Copies the channels defined in a from-release to the to-releae in an update,
## presumably so users of the from-release will get the update in the
## to-release.
##

sub TransferChannels
{
    my $this = shift;

    my $u_config = $this->GetAppConfig()->{'update_data'};
    my @updates = keys %$u_config;

    for my $u (@updates) {
        # If the update config doesn't have all_channels defined, define it as a list.
        if (!defined($u_config->{$u}->{'all_channels'})) {
            $u_config->{$u}->{'all_channels'} = [];
        }

        # Select the lhs of the update config.
        my $u_from = $u_config->{$u}->{'from'};
        # Correlate the lhs of the update config with its release config.
        my $rconfig = $this->{'mAppConfig'}->{'release'}->{$u_from};

        # If the lhs side of the update's release configuration has all_channels defined, push
        # those entries onto the update's all_channels.
        if (defined($rconfig->{'all_channels'})) {
            my @all_channels = @{$rconfig->{'all_channels'}};
            push(@{$u_config->{$u}->{'all_channels'}}, @all_channels);
        }
    }
    return 1;
}


sub RemoveBrokenUpdates
{
    my $this = shift;

    my $temp_prefix = lc($this->GetApp());
    my $startdir = getcwd();
    chdir("temp/$temp_prefix") or die "ASSERT: chdir(temp/$temp_prefix) FAILED; cwd: " . getcwd();

    my $u_config = $this->GetAppConfig()->{'update_data'};
    my @updates = keys %$u_config;

    my $i = 0;
    for my $u (@updates) {
        my $partial = $u_config->{$u}->{'partial'};
        my $partial_path = $partial->{'path'};
        my $partial_url = $partial->{'url'};

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
                                                   locale => $l);

                my $gen_partial_url = $partial_url;
                $gen_partial_url = SubstitutePath(path => $partial_url,
                                                  platform => $p,
                                                  locale => $l);

                my $partial_pathname = "$u/ftp/$gen_partial_path";

                # Go to next iteration if this partial patch already exists.
                next if -e $partial_pathname;
                $i++;

                if ( ! -f $from_path or
                     ! -f $to_path ) {
                    # remove this update
                    delete($ul_config->{$l});
                }
            }
        }
    }

    #printf("%s", Data::Dumper::Dumper($u_config));

    chdir($startdir);
} # remove_broken_updates



sub RequestedStep
{
    my $this = shift;
    my $checkStep = shift;
    return grep(/^$checkStep$/, @{$this->{'run'}});
}

sub GetDeliverableDir
{
    my $this = shift;
    die "ASSERT: GetDownloadDir() must return a full path" if
     ($this->{'mDeliverableDir'} !~ m:^/:);
    return $this->{'mDeliverableDir'};
}

sub GetApp
{
    my $this = shift;
    return ucfirst($this->{'mApp'});
}

sub GetDownloadDir
{
    my $this = shift;
    die "ASSERT: GetDownloadDir() must return a full path" if
     ($this->{'mDownloadDir'} !~ m:^/:);
    return $this->{'mDownloadDir'};
}

sub GetToolsDir
{
    my $this = shift;
    die "ASSERT: GetToolsDir() must return a full path" if
     ($this->{'mToolsDir'} !~ m:^/:);
    return $this->{'mToolsDir'};
}

sub IsDryRun
{
    my $this = shift;
    return 1 == $this->{'dryRun'};
}

1;
