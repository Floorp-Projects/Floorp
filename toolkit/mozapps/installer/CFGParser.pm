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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Samir Gehani <sgehani@netscape.com>
#   Benjamin Smedberg <benjamin@smedbergs.us>
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

# Initialize global installer environment variables.  This information comes
# from the application's installer.cfg file which specifies such things as name
# and installer executable file names.

package CFGParser;

sub ParseInstallerCfg
{
    my ($cfgFile) = @_;

    $ENV{WIZ_distSubdir} = "bin";
    $ENV{WIZ_packagesFile} = "packages-static";

    open(fpInstallCfg, $cfgFile) || die"\ncould not open $cfgFile: $!\n";

    while ($line = <fpInstallCfg>)
    {
      if (substr($line, -2, 2) eq "\r\n") {
        $line = substr($line, 0, length($line) - 2) . "\n";
      }
      ($prop, $value) = ($line =~ m/(\w*)\s+=\s+(.*)\n/);

      if ($prop eq "VersionLanguage") {
        $ENV{WIZ_versionLanguage} = $value;
      }
      elsif ($prop eq "NameCompany") {
        $ENV{WIZ_nameCompany} = $value;
      }
      elsif ($prop eq "NameProduct") {
        $ENV{WIZ_nameProduct} = $value;
      }
      elsif ($prop eq "ShortNameProduct") {
        $ENV{WIZ_shortNameProduct} = $value;
      }
      elsif ($prop eq "NameProductInternal") {
        $ENV{WIZ_nameProductInternal} = $value;
      }
      elsif ($prop eq "VersionProduct") {
        $ENV{WIZ_versionProduct} = $value;
      }
      elsif ($prop eq "FileInstallerEXE") {
        $ENV{WIZ_fileInstallerExe} = $value;
      }
      elsif ($prop eq "FileMainEXE") {
        $ENV{WIZ_fileMainExe} = $value;
      }
      elsif ($prop eq "FileInstallerNETRoot") {
        $ENV{WIZ_fileNetStubRootName} = $value;
      }
      elsif ($prop eq "ComponentList") {
        $ENV{WIZ_componentList} = $value;
      }
      elsif ($prop eq "7ZipSFXModule") {
        $ENV{WIZ_sfxModule} = $value;
      }
      elsif ($prop eq "DistSubdir") {
        $ENV{WIZ_distSubdir} = $value;
      }
      elsif ($prop eq "packagesFile") {
        $ENV{WIZ_packagesFile} = $value;
      }
      elsif ($prop eq "GREVersion") {
        $ENV{WIZ_greVersion} = $value;
      }
      elsif ($prop eq "LicenseFile") {
        $ENV{WIZ_licenseFile} = $value;
      }
    }

    close(fpInstallCfg);

    # $gDefaultProductVersion has the form maj.min.release.bld where maj, min, release
    #   and bld are numerics representing version information.
    # Other variables need to use parts of the version info also so we'll
    #   split out the dot separated values into the array @versionParts
    #   such that:
    #
    #   $versionParts[0] = maj
    #   $versionParts[1] = min
    #   $versionParts[2] = release
    #   $versionParts[3] = bld
    @versionParts = split /\./, $ENV{WIZ_versionProduct};
    if($versionParts[2] eq "0" || $versionParts[2] == "") {
      $versionMain = "$versionParts[0].$versionParts[1]";
    }
    else {
      $versionMain = "$versionParts[0].$versionParts[1].$versionParts[2]";
    }
    $ENV{WIZ_userAgentShort}       = "$versionMain";
}

1;

