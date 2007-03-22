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
# The Original Code is Mozilla Communicator.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corp.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
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

# It is required to return(1) so that the parent script performing a "require()"
# on this file does not think that it failed.
return(1);

sub StageProduct
{
  my($aDirSrcDist, $aDirStage, $aProductName, $aInOs) = @_;
  my($dirDistPackagesProductName) = "$aDirSrcDist/packages/$aProductName";
  my($dirStageProductName)        = "$aDirStage/$aProductName";
  my($dirMozRoot)                 = StageUtils::GetAbsPath("moz_root");
  my($dirMozPackager)             = StageUtils::GetAbsPath("moz_packager");

  my($osPkg);
  my($osPkgFile);
  if($aInOs =~ /^win$/i)
  {
    $osPkg = "dos";
    $osPkgFile = "win";
  }
  else
  {
    $osPkg = $aInOs;
    $osPkgFile = $aInOs;
  }

  StageUtils::CleanupStage($aDirStage, $aProductName);
  StageUtils::CleanupDistPackages("$aDirSrcDist/packages", $aProductName);
  StageUtils::CopyAdditionalPackage("$dirMozPackager/xpcom-$osPkgFile.pkg",                     $dirDistPackagesProductName);
  StageUtils::CopyAdditionalPackage("$dirMozPackager/pkgs-mfcembed-$osPkgFile",                 $dirDistPackagesProductName);
  StageUtils::CopyAdditionalPackage("$dirMozPackager/pkgs-mfcembed-$osPkgFile-supp",            $dirDistPackagesProductName);
  StageUtils::CopyAdditionalPackage("$dirMozRoot/embedding/config/basebrowser-$osPkgFile-supp", $dirDistPackagesProductName);
  StageUtils::CopyAdditionalPackage("$dirMozRoot/embedding/config/gre-$osPkgFile-supp",         $dirDistPackagesProductName);

  mkdir("$aDirStage", 0775)                        if (!(-e "$aDirStage"));
  mkdir("$aDirStage/$aProductName", 0775)          if (!(-e "$aDirStage/$aProductName"));
  mkdir("$aDirStage/$aProductName/mfcembed", 0775) if (!(-e "$aDirStage/$aProductName/mfcembed"));

  # Call pkgcp.pl on each of the package list
  system("perl \"$dirMozPackager/pkgcp.pl\" -s \"$aDirSrcDist\"                 -d \"$dirStageProductName\"          -f \"$dirDistPackagesProductName/xpcom-$osPkgFile.pkg\" -o $osPkg -v");
  system("perl \"$dirMozPackager/pkgcp.pl\" -s \"$aDirSrcDist/bin\"             -d \"$dirStageProductName/mfcembed\" -f \"$dirDistPackagesProductName/pkgs-mfcembed-$osPkgFile\" -o $osPkg -v");
  system("perl \"$dirMozPackager/pkgcp.pl\" -s \"$aDirSrcDist/gre_app_support\" -d \"$dirStageProductName/mfcembed\" -f \"$dirDistPackagesProductName/pkgs-mfcembed-$osPkgFile-supp\" -o $osPkg -v");
  system("perl \"$dirMozPackager/pkgcp.pl\" -s \"$aDirSrcDist/bin\"             -d \"$dirStageProductName/mfcembed\" -f \"$dirDistPackagesProductName/basebrowser-$osPkgFile-supp\" -o $osPkg -v");
  system("perl \"$dirMozPackager/pkgcp.pl\" -s \"$aDirSrcDist/bin\"             -d \"$dirStageProductName/mfcembed\" -f \"$dirDistPackagesProductName/gre-$osPkgFile-supp\" -o $osPkg -v");
}

