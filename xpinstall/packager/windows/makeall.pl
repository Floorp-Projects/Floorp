#!c:\perl\bin\perl
# 
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
# Sean Su <ssu@netscape.com>
# 

#
# This perl script builds the xpi, config.ini, and js files.
#

use Cwd;

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
$inStagePath          = $ARGV[1];
$inDistPath           = $ARGV[2];

$inXpiURL = "";
$inRedirIniURL = "";

ParseArgv(@ARGV);
if($inXpiURL eq "")
{
  # archive url not supplied, set it to default values
  $inXpiURL      = "ftp://not.supplied.com";
}
if($inRedirIniURL eq "")
{
  # redirect url not supplied, set it to default value.
  $inRedirIniURL = $inXpiURL;
}

$seiFileNameGeneric   = "nsinstall.exe";
$seiFileNameSpecific  = "mozilla-win32-installer.exe";
$seiFileNameSpecificStub  = "mozilla-win32-stub-installer.exe";
$seuFileNameSpecific  = "MozillaUninstall.exe";
$seuzFileNameSpecific = "mozillauninstall.zip";

# set environment vars for use by other .pl scripts called from this script.
$ENV{WIZ_userAgent}            = "0.9.5+ (en)"; # ie: "0.9 (en)"
$ENV{WIZ_userAgentShort}       = "0.9.5+";      # ie: "0.9"
$ENV{WIZ_xpinstallVersion}     = "0.9.5+";      # ie: "0.9.0"
$ENV{WIZ_nameCompany}          = "mozilla.org";
$ENV{WIZ_nameProduct}          = "Mozilla";
$ENV{WIZ_fileMainExe}          = "Mozilla.exe";
$ENV{WIZ_fileUninstall}        = $seuFileNameSpecific;
$ENV{WIZ_fileUninstallZip}     = $seuzFileNameSpecific;

# Set the location of the local tmp stage directory
$gLocalTmpStage = $inStagePath;

# Check for existence of staging path
if(!(-d "$inStagePath"))
{
  die "\n Invalid path: $inStagePath\n";
}

# List of components for to create xpi files from
@gComponentList = ("xpcom",
                   "browser",
                   "mail",
                   "psm",
                   "talkback",
                   "chatzilla",
                   "deflenus",
                   "langenus",
                   "regus",
                   "venkman");

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

if(MakeXpiFile())
{
  exit(1);
}
if(MakeUninstall())
{
  exit(1);
}
if(MakeConfigFile())
{
  exit(1);
}

# Copy the setup files to the dist setup directory.
if(system("copy install.ini $inDistPath"))
{
  die "\n Error: copy install.ini $inDistPath\n";
}
if(system("copy install.ini $inDistPath\\setup"))
{
  die "\n Error: copy install.ini $inDistPath\\setup\n";
}
if(system("copy config.ini $inDistPath"))
{
  die "\n Error: copy config.ini $inDistPath\n";
}
if(system("copy config.ini $inDistPath\\setup"))
{
  die "\n Error: copy config.ini $inDistPath\\setup\n";
}
if(system("copy $inDistPath\\setup.exe $inDistPath\\setup"))
{
  die "\n Error: copy $inDistPath\\setup.exe $inDistPath\\setup\n";
}
if(system("copy $inDistPath\\setuprsc.dll $inDistPath\\setup"))
{
  die "\n Error: copy $inDistPath\\setuprsc.dll $inDistPath\\setup\n";
}

# copy license file for the installer
if(system("copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\license.txt"))
{
  die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\license.txt\n";
}
if(system("copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\setup\\license.txt"))
{
  die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $inDistPath\\setup\\license.txt\n";
}

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

# create the big self extracting .exe installer
print "\n**************************************************************\n";
print "*                                                            *\n";
print "*  creating Self Extracting Executable Full Install file...  *\n";
print "*                                                            *\n";
print "**************************************************************\n";
if(-d "$inDistPath\\sea")
{
  unlink <$inDistPath\\sea\\*>;
}
else
{
  mkdir ("$inDistPath\\sea",0775);
}
if(system("copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecific"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameGeneric $inDistPath\\$seiFileNameSpecific\n";
}
if(system("$inDistPath\\nsztool.exe $inDistPath\\$seiFileNameSpecific $inDistPath\\setup\\*.* $inDistPath\\xpi\\*.*"))
{
  die "\n Error: $inDistPath\\nsztool.exe $inDistPath\\$seiFileNameSpecific $inDistPath\\setup\\*.* $inDistPath\\xpi\\*.*\n";
}
if(system("copy $inDistPath\\$seiFileNameSpecific $inDistPath\\sea"))
{
  die "\n Error: copy $inDistPath\\$seiFileNameSpecific $inDistPath\\sea\n";
}
unlink <$inDistPath\\$seiFileNameSpecificStub>;

print " done!\n\n";

if((!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcrt.dll")) ||
   (!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcirt.dll")))
{
  print "***\n";
  print "**\n";
  print "**  The following required Microsoft redistributable system files were not found\n";
  print "**  in $ENV{MOZ_SRC}\\redist\\microsoft\\system:\n";
  print "**\n";
  if(!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcrt.dll"))
  {
    print "**    msvcrt.dll\n";
  }
  if(!(-e "$ENV{MOZ_SRC}\\redist\\microsoft\\system\\msvcirt.dll"))
  {
    print "**    msvcirt.dll\n";
  }
  print "**\n";
  print "**  The above files are required by the installer and the browser.  If you attempt\n";
  print "**  to run the installer, you may encounter the following bug:\n";
  print "**\n";
  print "**    http://bugzilla.mozilla.org/show_bug.cgi?id=27601\n";
  print "**\n";
  print "***\n\n";
}

# end of script
exit(0);

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

       staging path      : full path to where the components are staged at

       dist install path : full path to where the dist install dir is at.
                           ie: d:\\builds\\mozilla\\dist\\win32_o.obj\\install

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
  }
}

sub MakeConfigFile
{
  # Make config.ini file
  if(system("perl makecfgini.pl config.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl config.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL\n";
    return(1);
  }

  # Make install.ini file
  if(system("perl makecfgini.pl install.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL"))
  {
    print "\n Error: perl makecfgini.pl install.it $inDefaultVersion $gLocalTmpStage $inDistPath\\xpi $inRedirIniURL $inXpiURL\n";
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
  if(system("copy uninstall.ini $inDistPath"))
  {
    print "\n Error: copy uninstall.ini $inDistPath\n";
    return(1);
  }
  if(system("copy uninstall.ini $inDistPath\\uninstall"))
  {
    print "\n Error: copy uninstall.ini $inDistPath\\uninstall\n";
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
  if(system("perl makeuninstallini.pl uninstall.it $inDefaultVersion"))
  {
    print "\n Error: perl makeuninstallini.pl uninstall.it $inDefaultVersion\n";
    return(1);
  }
  return(0);
}

sub MakeJsFile
{
  my($mComponent) = @_;

  # Make .js file
  if(system("perl makejs.pl $mComponent.jst $inDefaultVersion $gLocalTmpStage\\$mComponent"))
  {
    print "\n Error: perl makejs.pl $mComponent.jst $inDefaultVersion $gLocalTmpStage\\$mComponent\n";
    return(1);
  }
  return(0);
}

sub MakeXpiFile
{
  my($mComponent);

  foreach $mComponent (@gComponentList)
  {
    # Make .js files
    if(MakeJsFile($mComponent))
    {
      return(1);
    }

    # Make .xpi file
    if(system("perl makexpi.pl $mComponent $gLocalTmpStage $inDistPath\\xpi"))
    {
      print "\n Error: perl makexpi.pl $mComponent $gLocalTmpStage $inDistPath\\xpi\n";
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

sub CreateTmpStage()
{
  my($mComponent);

  # Remove previous tmpstage area if one was left around
  if(-d "$gLocalTmpStage")
  {
    system("perl rdir.pl $gLocalTmpStage");
  }

  print "\n Creating the local TmpStage directory:\n";
  print "   $gLocalTmpStage\n";

  # Copy the component's staging dir locally so that the chrome packages, locales, and skins dirs can be
  # removed prior to creating the .xpi file.
  mkdir("$gLocalTmpStage", 775);

  foreach $mComponent (@gComponentList)
  {
    print "\n Copying $mComponent:\n";
    print " From: $inStagePath\\$mComponent\n";
    print "   To: $gLocalTmpStage\\$mComponent\n\n";
    mkdir("$gLocalTmpStage\\$mComponent", 775);

    # If it's not talkback then copy the component over to the local tmp stage.
    # If it is, then skip the copy because there will be nothing at the source.
    # Talkback is a dummy place holder .xpi right now.  Mozilla release team 
    # replaces this place holder .xpi with a real talkback when delivering the
    # build to mozilla.org.
    if(!($mComponent =~ /talkback/i))
    {
      if(system("xcopy /s/e $inStagePath\\$mComponent $gLocalTmpStage\\$mComponent\\"))
      {
        print "\n Error: xcopy /s/e $inStagePath\\$mComponent $gLocalTmpStage\\$mComponent\\\n";
        return(1);
      }
    }

    if(-d "$gLocalTmpStage\\$mComponent\\bin\\chrome")
    {
      # Make chrome archive files
      if(&ZipChrome("win32", "noupdate", "$gLocalTmpStage\\$mComponent\\bin\\chrome", "$gLocalTmpStage\\$mComponent\\bin\\chrome"))
      {
        return(1);
      }

      # Remove the locales, packages, and skins dirs if they exist.
      my @dirs = <$gLocalTmpStage\\$mComponent\\bin\\chrome\\*>;
      foreach $d (@dirs) {
          if(-d "$d")
          {
              system("perl rdir.pl $d");
          }
      }
    }
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

