#!c:\perl\bin\perl
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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
#   Curt Patrick <curt@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
# This perl script builds the xpi, config.ini, and js files.
#

use Cwd;
require "pwd.pl";
#&initpid;

# Make sure MOZ_SRC is set.
if($ENV{MOZ_SRC} eq "")
{
  print "Error: MOZ_SRC not set!";
  exit(1);
}

# Make sure there are at least three arguments
if($#ARGV < 2)
{
  PrintUsage();
}

require "$ENV{MOZ_SRC}\\mozilla\\config\\zipcfunc.pl";

$inDefaultVersion     = $ARGV[0];
# $ARGV[0] has the form maj.min.release.bld where maj, min, release
#   and bld are numerics representing version information.
# Other variables need to use parts of the version info also so we'll
#   split out the dot separated values into the array @versionParts
#   such that:
#
#   $versionParts[0] = maj
#   $versionParts[1] = min
#   $versionParts[2] = release
#   $versionParts[3] = bld
@versionParts = split /\./, $inDefaultVersion;

# We allow non-numeric characters to be included as the last 
#   characters in fields of $ARG[0] for display purposes (mostly to
#   show that we have moved past a certain version by adding a '+'
#   character).  Non-numerics must be stripped out of $inDefaultVersion,
#   however, since this variable is used to identify the the product 
#   for comparison with other installations, so the values in each field 
#   must be numeric only:
$inDefaultVersion =~ s/[^0-9.][^.]*//g;
print "The raw version id is:  $inDefaultVersion\n";

print "Product config perl script: $ARGV[1]\n";
require "$ARGV[1]";
$DEPTH = $ARGV[2];
$ScriptsDir = cwd();
$ScriptsDir =~ s/\//\\/g;

# Make sure $DEPTH and $ScriptsDir are defined before calling SetCfgVars()
$bPrepareDistArea      = 0;
$bCreateFullInstaller = 1;
$bCreateStubInstaller = 1;
WizCfg::SetCfgVars();

ParseArgv(@ARGV);
if($inXpiURL eq "")
{
  # archive url not supplied, set it to default values
  $inXpiURL      = "ftp://not.supplied.invalid";
}
if($inRedirIniURL eq "")
{
  # redirect url not supplied, set it to default value.
  $inRedirIniURL = $inXpiURL;
}

if($versionParts[2] eq "0")
{
   $versionMain = "$versionParts[0]\.$versionParts[1]";
}
else
{
   $versionMain = "$versionParts[0]\.$versionParts[1]\.$versionParts[2]";
}
print "The display version is: $versionMain\n";

# The following variables are for displaying version info in the 
# the installer.
$ENV{WIZ_userAgent}            = "$versionMain ($versionLanguage)";
$ENV{WIZ_userAgentShort}       = "$versionMain";
$ENV{WIZ_xpinstallVersion}     = "$versionMain";

print "bPrepareDistArea              : $bPrepareDistArea\n";
print "bCreateFullInstaller          : $bCreateFullInstaller\n";
print "bCreateStubInstaller          : $bCreateStubInstaller\n";

print "ScriptsDir:                   : $ScriptsDir\n";
print "ProdDir                       : $ProdDir\n";
print "XPI_JST_Dir                   : $XPI_JST_Dir\n";
print "StubInstJstDir                : $StubInstJstDir\n"; 
print "inStagePath                   : $inStagePath\n";
print "inDistPath                    : $inDistPath\n";
print "inXpiURL                      : $inXpiURL\n";
print "inRedirIniURL                 : $inRedirIniURL\n";

print "seiFileNameGeneric            : $seiFileNameGeneric\n";
print "seiStubRootName               : $seiStubRootName\n";
print "seiFileNameSpecific           : $seiFileNameSpecific\n";
print "seiFileNameSpecificStub       : $seiFileNameSpecificStub\n";
print "seuFileNameSpecific           : $seuFileNameSpecific\n";
print "seuzFileNameSpecific          : $seuzFileNameSpecific\n";
print "versionLanguage               : $versionLanguage\n";

print "ENV{WIZ_nameCompany}          : $ENV{WIZ_nameCompany}\n";
print "ENV{WIZ_nameProduct}          : $ENV{WIZ_nameProduct}\n";
print "ENV{WIZ_nameProductNoVersion} : $ENV{WIZ_nameProductNoVersion}\n";
print "ENV{WIZ_fileMainExe}          : $ENV{WIZ_fileMainExe}\n";
print "ENV{WIZ_fileUninstall}        : $ENV{WIZ_fileUninstall}\n";
print "ENV{WIZ_fileUninstallZip}     : $ENV{WIZ_fileUninstallZip}\n";

# Set the location of the local tmp stage directory
$gLocalTmpStage = $inStagePath;

# Check for existence of staging path
if(!(-d "$inStagePath"))
{
  die "\n Invalid path: $inStagePath\n";
}

WizCfg::GetStarted();

# List of components for to create xpi files from.  One component for each .jst found
chdir($XPI_JST_Dir);
@dirlist = <*>;
$i = 0;
foreach $file (@dirlist)
{
  if($file =~ /(.*)\.jst$/)
  {
    $gComponentList[$i++] = "$1"; 
  }
}
chdir($ScriptsDir);
print "ScriptsDir:  $ScriptsDir\n";

$i=0;
print "\nComponent List:\n";
foreach $mComponent (@gComponentList)
{
  print "Component: $mComponent\n";
}

if(VerifyComponents()) # return value of 0 means no errors encountered
{
  exit(1);
}

# Make sure inDistPath exists
if(!(-d "$inDistPath"))
{
  mkdir ("$inDistPath",0775);
}

if(-d "$inDistPath\\xpi")
{
  unlink <$inDistPath\\xpi\\*>;
}
else
{
  mkdir ("$inDistPath\\xpi",0775);
}

if(-d "$inDistPath\\uninstall")
{
  unlink <$inDistPath\\uninstall\\*>;
}
else
{
  mkdir ("$inDistPath\\uninstall",0775);
}

if(-d "$inDistPath\\setup")
{
  unlink <$inDistPath\\setup\\*>;
}
else
{
  mkdir ("$inDistPath\\setup",0775);
}

# If the installer binaries were not delivered by make to this dis area 
#   you need to do this step to get them there.
if($bPrepareDistArea)
{
  PrepareDistArea();
}

if(MakeXpiFiles($XPI_JST_Dir))
{
  exit(1);
}

WizCfg::MakeExeZipFiles();

if(MakeConfigFile())
{
  exit(1);
}

if(MakeUninstall())
{
  exit(1);
}

if(CopyFilesToDist())
{
  exit(1);
}

if(BuildInstallerExe())
{
  exit(1);
}

if($bCreateStubInstaller && CreateStubInstaller())
{
  exit(1);
}

if(GroupFilesForCD())
{
  exit(1);
}

if($bCreateFullInstaller && WizCfg::CreateFullInstaller())
{
  exit(1);
}

if(WizCfg::FinishUp())
{
  exit(1);
}

# end of script
print "\nmakeall.pl: Done\n";
exit(0);


sub PrepareDistArea
{
  print "Getting mozilla installer files\n";

  # Grab xpcom from mozilla build
  if(system("copy $::inDistPath\\..\\..\\installer\\mozilla\\xpi\\xpcom.xpi $::inDistPath"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\installer\\mozilla\\xpi\\xpcom.xpi $::inDistPath\n";
  }
  if(system("copy $::inDistPath\\..\\..\\installer\\mozilla\\xpi\\xpcom.xpi $::inDistPath\\xpi"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\installer\\mozilla\\xpi\\xpcom.xpi $::inDistPath\\xpi\n";
  }

  #Get setup.exe from mozilla install
  if(system("copy $::inDistPath\\..\\..\\installer\\mozilla\\setup.exe $::inDistPath"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\installer\\mozilla\\setup.exe $::inDistPath\n";
  }
  # Get resource file from mozilla install
  if(system("copy $::inDistPath\\..\\..\\installer\\mozilla\\setuprsc.dll $::inDistPath"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\installer\\mozilla\\setuprsc.dll $::inDistPath\n";
  }
  # Get uninstaller from mozilla install
  if(system("copy $::inDistPath\\..\\..\\installer\\mozilla\\uninstall.exe $::inDistPath"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\installer\\mozilla\\uninstall.exe $::inDistPath\n";
  }

  # Get tools to create self-extracting exe
  if(system("copy $::inDistPath\\..\\..\\installer\\mozilla\\$::seiFileNameGeneric $::inDistPath"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\installer\\mozilla\\$::seiFileNameGeneric $::inDistPath\n";
  }
  if(system("copy $::inDistPath\\..\\..\\installer\\mozilla\\nsztool.exe $::inDistPath"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\installer\\mozilla\\nsztool.exe $::inDistPath\n";
  }
}

sub MakeExeZip
{
  my($aSrcDir, $aExeFile, $aZipFile) = @_;
  my($saveCwdir);

  $saveCwdir = cwd();
  chdir($aSrcDir);
  if(system("$ENV{MOZ_TOOLS}\\bin\\zip $inDistPath\\xpi\\$aZipFile $aExeFile"))
  {
    chdir($saveCwdir);
    die "\n Error: $ENV{MOZ_TOOLS}\\bin\\zip $inDistPath\\xpi\\$aZipFile $aExeFile";
  }
  chdir($saveCwdir);
}

sub PrintUsage
{
  die "usage: $0 <default version> <staging path> <dist install path> [options]

       default version   : y2k compliant based date version.
                           ie: 5.0.0.2000040413

       config file path  : full path configuration file for the installer

       depth             : full path to the dir of the sources

       options include:
           -aurl <archive url>      : either ftp:// or http:// url to where the
                                      archives (.xpi, .exe, .zip, etc...) reside

           -rurl <redirect.ini url> : either ftp:// or http:// url to where the
                                      redirec.ini resides.  If not supplied, it
                                      will be assumed to be the same as archive
                                      url.
       \n";
}

sub ParseArgv
{
  my(@myArgv) = @_;
  my($counter);

  # The first 3 arguments are required, so start on the 4th.
  for($counter = 3; $counter <= $#myArgv; $counter++)
  {
    if($myArgv[$counter] =~ /^[-,\/]h$/i)
    {
      PrintUsage();
    }
    elsif($myArgv[$counter] =~ /^[-,\/]aurl$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inXpiURL = $myArgv[$counter];
        $inRedirIniURL = $inXpiURL;
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]rurl$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inRedirIniURL = $myArgv[$counter];
      }
    }
    elsif($myArgv[$counter] =~ /^[-,\/]sig$/i)
    {
      if($#myArgv >= ($counter + 1))
      {
        ++$counter;
        $inJarSignature = $myArgv[$counter];
      }
    }
  }
}

sub MakeConfigFile
{
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $ProdDir $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl config.it $ProdDir $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL\n";
    return(1);
  }

  # Make install.ini file
  if(system("perl makecfgini.pl install.it $ProdDir $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl install.it $ProdDir $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL\n";
    return(1);
  }
  return(0);
}

sub MakeUninstall
{
  if(MakeUninstallIniFile())
  {
    return(1);
  }

  # Copy the uninstall files to the dist uninstall directory.
  if(system("copy $ProdDir\\uninstall.ini $inDistPath"))
  {
    print "\n Error: copy $ProdDir\\uninstall.ini $inDistPath\n";
    return(1);
  }
  if(system("copy $ProdDir\\uninstall.ini $inDistPath\\uninstall"))
  {
    print "\n Error: copy $ProdDir\\uninstall.ini $inDistPath\\uninstall\n";
    return(1);
  }
  if(system("copy $ProdDir\\defaults_info.ini $inDistPath"))
  {
    print "\n Error: copy $ProdDir\\defaults_info.ini $inDistPath\n";
    return(1);
  }
  if(system("copy $ProdDir\\defaults_info.ini $inDistPath\\uninstall"))
  {
    print "\n Error: copy $ProdDir\\defaults_info.ini $inDistPath\\uninstall\n";
    return(1);
  }
  if(system("copy $inDistPath\\uninstall.exe $inDistPath\\uninstall"))
  {
    print "\n Error: copy $inDistPath\\uninstall.exe $inDistPath\\uninstall\n";
    return(1);
  }

  # build the self-extracting .exe (uninstaller) file.
  print "\nbuilding self-extracting uninstaller ($seuFileNameSpecific)...\n";
  if(system("copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seuFileNameSpecific"))
  {
    print "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seuFileNameSpecific\n";
    return(1);
  }
  if(system("$inDistPath\\nsztool.exe $inDistPath\\$seuFileNameSpecific $inDistPath\\uninstall\\*.*"))
  {
    print "\n Error: $inDistPath\\nsztool.exe $inDistPath\\$seuFileNameSpecific $inDistPath\\uninstall\\*.*\n";
    return(1);
  }

  MakeExeZip($inDistPath, $seuFileNameSpecific, $seuzFileNameSpecific);
  unlink <$inDistPath\\$seuFileNameSpecific>;
  return(0);
}

sub MakeUninstallIniFile
{
  # Make config.ini file
  if(system("perl makeuninstallini.pl uninstall.it $ProdDir $inDefaultVersion"))
  {
    print "\n Error: perl makeuninstallini.pl uninstall.it $ProdDir $inDefaultVersion\n";
    return(1);
  }

  return(0);
}

sub MakeJsFile
{
  my($mComponent, $JstDir) = (@_);

  if(system("perl makejs.pl $mComponent $JstDir $inDefaultVersion $gLocalTmpStage\\$mComponent"))
  {
    print "\n Error: perl makejs.pl $mComponent.jst $JstDir $inDefaultVersion $gLocalTmpStage\\$mComponent\n";
    return(1);
  }
  return(0);
}

sub MakeXpiFiles
{
  my($JstDir) = (@_);
  my($mComponent);

  foreach $mComponent (@gComponentList)
  {
    print "\nCreating xpi for $mComponent\n";

    # Make .js files
    if(MakeJsFile($mComponent, $JstDir))
    {
      return(1);
    }

    # Make .xpi file
    if(system("perl makexpi.pl $mComponent $XPI_JST_Dir $gLocalTmpStage $inDistPath\\xpi"))
    {
      print "\n Error: perl makexpi.pl $mComponent $XPI_JST_Dir $gLocalTmpStage $inDistPath\\xpi\n";
      return(1);
    }
  }
  return(0);
}

sub RemoveLocalTmpStage()
{
  # Remove tmpstage area
  if(-d "$gLocalTmpStage")
  {
    system("perl rdir.pl $gLocalTmpStage");
  }
  return(0);
}

sub VerifyComponents()
{
  my($mComponent);
  my($mError) = 0;

  print "\n Verifying existence of required components...\n";
  foreach $mComponent (@gComponentList)
  {
    if($mComponent =~ /talkback/i)
    {
      print " place holder: $inStagePath\\$mComponent\n";
      mkdir("$inStagePath\\$mComponent", 775);
    }
    elsif(-d "$inStagePath\\$mComponent")
    {
      print "           ok: $inStagePath\\$mComponent\n";
    }
    else
    {
      print "        Error: $inStagePath\\$mComponent does not exist!\n";
      $mError = 1;
    }
  }
  return($mError);
}

sub CopyFilesToDist()
{
  # Copy the setup files to the dist setup directory.
  WizCfg::CopyExtraDistFiles();
  if(system("copy $ProdDir\\install.ini $inDistPath"))
  {
    die "\n Error: copy $ProdDir\\install.ini $inDistPath\n";
  }
  if(system("copy $ProdDir\\install.ini $inDistPath\\setup"))
  {
    die "\n Error: copy $ProdDir\\install.ini $inDistPath\\setup\n";
  }
  if(system("copy $ProdDir\\config.ini $inDistPath"))
  {
    die "\n Error: copy $ProdDir\\config.ini $inDistPath\n";
  }
  if(system("copy $ProdDir\\config.ini $inDistPath\\setup"))
  {
    die "\n Error: copy $ProdDir\\config.ini $inDistPath\\setup\n";
  }
  if(system("copy $inDistPath\\setup.exe $inDistPath\\setup"))
  {
    die "\n Error: copy $inDistPath\\setup.exe $inDistPath\\setup\n";
  }
  if(system("copy $inDistPath\\setuprsc.dll $inDistPath\\setup"))
  {
    die "\n Error: copy $inDistPath\\setuprsc.dll $inDistPath\\setup\n";
  }
}

sub BuildInstallerExe()
{
  # build the self-extracting .exe (installer) file.
  print "\nbuilding self-extracting stub installer ($seiFileNameSpecificStub)...\n";
  if(system("copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecificStub"))
  {
    die "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecificStub\n";
  }
  if(system("$inDistPath\\nsztool.exe $inDistPath\\$seiFileNameSpecificStub $inDistPath\\setup\\*.*"))
  {
    die "\n Error: inDistPath\\nsztool.exe $inDistPath\\$seiFileNameSpecificStub $inDistPath\\setup\\*.*\n";
  }
}

sub CreateStubInstaller()
{
  # copy the lean installer to stub\ dir
  print "\n****************************\n";
  print "*                          *\n";
  print "*  creating Stub files...  *\n";
  print "*                          *\n";
  print "****************************\n";
  if(-d "$inDistPath\\stub")
  {
    unlink <$inDistPath\\stub\\*>;
  }
  else
  {
    mkdir ("$inDistPath\\stub",0775);
  }
  if(system("copy $inDistPath\\$seiFileNameSpecificStub $inDistPath\\stub"))
  {
    die "\n Error: copy $inDistPath\\$seiFileNameSpecificStub $inDistPath\\stub\n";
  }

  # create the xpi for launching the stub installer
  print "\n**********************************\n";
  print "*                                  *\n";
  print "*  creating stub installer xpi...  *\n";
  print "*                                  *\n";
  print "************************************\n";
  if(-d "$inStagePath\\$seiStubRootName")
  {
    unlink <$inStagePath\\$seiStubRootName\\*>;
  }
 else
  {
    mkdir ("$inStagePath\\$seiStubRootName",0775);
  }
  if(system("copy $inDistPath\\stub\\$seiFileNameSpecificStub $gLocalTmpStage\\$seiStubRootName"))
  {
    die "\n Error: copy $inDistPath\\stub\\$seiFileNameSpecificStub $gLocalTmpStage\\$seiStubRootName\n";
  }

  # Make .js files
  if(MakeJsFile($seiStubRootName, $StubInstJstDir))
  {
    return(1);
  }

  # Make .xpi file
  if(system("perl makexpi.pl $seiStubRootName $StubInstJstDir $gLocalTmpStage $inDistPath"))
  {
    print "\n Error: perl makexpi.pl $seiStubRootName $StubInstJstDir $gLocalTmpStage $inDistPath\n";
    return(1);
  }
}

sub GroupFilesForCD()
{
  # group files for CD
  print "\n************************************\n";
  print "*                                  *\n";
  print "*  creating Compact Disk files...  *\n";
  print "*                                  *\n";
  print "************************************\n";
  if(-d "$inDistPath\\cd")
  {
    unlink <$inDistPath\\cd\\*>;
  }
  else
  {
    mkdir ("$inDistPath\\cd",0775);
  }
  if(system("copy $inDistPath\\$seiFileNameSpecificStub $inDistPath\\cd"))
  {
    die "\n Error: copy $inDistPath\\$seiFileNameSpecificStub $inDistPath\\cd\n";
  }
  if(system("copy $inDistPath\\xpi $inDistPath\\cd"))
  {
    die "\n Error: copy $inDistPath\\xpi $inDistPath\\cd\n";
  }
}
