#!c:\perl\bin\perl
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
# Contributor(s): Sean Su
#

#
# Purpose:
#    To build the mozilla self-extracting installer and its corresponding .xpi files
#    given a mozilla build on a local system.
#
# Requirements:
# 1. perl needs to be installed correctly on the build system because cwd.pm is used.
# 2. mozilla\xpinstall\wizard\windows needs to be built.
#    a. to build it, MFC must be installed with VC
#    b. set MOZ_MFC=1 in the build environment
#    c. run nmake -f makefile.win from the above directory
#

if($ENV{MOZ_SRC} eq "")
{
  print "Error: MOZ_SRC not set!";
  exit(1);
}

$inXpiURL = "";
$inRedirIniURL = "";

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

$DEPTH         = "$ENV{MOZ_SRC}\\mozilla";
$cwdBuilder    = "$DEPTH\\xpinstall\\wizard\\windows\\builder";
$cwdBuilder    =~ s/\//\\/g; # convert slashes to backslashes for Dos commands to work
$cwdDist       = GetCwd("dist",     $DEPTH, $cwdBuilder);
$cwdDistWin    = GetCwd("distwin",  $DEPTH, $cwdBuilder);
$cwdInstall    = GetCwd("install",  $DEPTH, $cwdBuilder);
$cwdPackager   = GetCwd("packager", $DEPTH, $cwdBuilder);
$verPartial    = "5.0.0.";
$ver           = $verPartial . GetVersion($DEPTH);

if(-d "$DEPTH\\stage")
{
  system("perl $cwdPackager\\windows\\rdir.pl $DEPTH\\stage");
}

# The destination cannot be a sub directory of the source
# pkgcp.pl will get very unhappy

mkdir("$DEPTH\\stage", 775);
system("perl $cwdPackager\\pkgcp.pl -s $cwdDistWin -d $DEPTH\\stage -f $cwdPackager\\packages-static-win -o dos -v");

chdir("$cwdPackager\\windows");
if(system("perl makeall.pl $ver $DEPTH\\stage $cwdDistWin\\install -aurl $inXpiURL -rurl $inRedirIniURL"))
{
  print "\n Error: perl makeall.pl $ver $DEPTH\\stage $cwdDistWin\\install $inXpiURL $inRedirIniURL\n";
  exit(1);
}

chdir($cwdBuilder);

# Copy the .xpi files to the same directory as setup.exe.
# This is so that setup.exe can find the .xpi files
# in the same directory and use them.
#
# Mozilla-win32-install.exe (a self extracting file) will use the .xpi
# files from its current directory as well, but it is not a requirement
# that they exist because it already contains the .xpi files within itself.
if(system("copy $cwdDistWin\\install\\xpi\\*.* $cwdDistWin\\install"))
{
  print "Error: copy $cwdDistWin\\install\\xpi\\*.* $cwdDistWin\\install\n";
  exit(1);
}

print "\n";
print "**\n";
print "*\n";
print "*  A self-extracting installer has been built and delivered:\n";
print "*\n";
print "*      $cwdDistWin\\install\\mozilla-win32-install.exe\n";
print "*\n";
print "**\n";
print "\n";

exit(0);

sub PrintUsage
{
  die "usage: $0 [options]

       options available are:

           -h    - this usage.

           -aurl - ftp or http url for where the archives (.xpi, exe, .zip, etx...) are.
                   ie: ftp://my.ftp.com/mysoftware/version1.0/xpi

           -rurl - ftp or http url for where the redirect.ini file is located at.
                   ie: ftp://my.ftp.com/mysoftware/version1.0
                   
                   This url can be the same as the archive url.
                   If -rurl is not supplied, it will be assumed that the redirect.ini
                   file is at -arul.
       \n";
}

sub ParseArgv
{
  my(@myArgv) = @_;
  my($counter);

  for($counter = 0; $counter <= $#myArgv; $counter++)
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

sub GetCwd
{
  my($whichPath, $depthPath, $returnCwd) = @_;
  my($distCwd);
  my($distWinPathName);
  my($distPath);

  # determine if build was built via gmake
  if(-e "$depthPath\\dist\\install")
  {
    $distWinPathName = "dist";
  }
  else
  {
    # determine if build is debug or optimized
    # (used only for nmake builds)
    if($ENV{MOZ_DEBUG} eq "")
    {
      $distWinPathName = "dist\\Win32_o.obj";
    }
    else
    {
      $distWinPathName = "dist\\Win32_d.obj";
    }
  }

  if($whichPath eq "dist")
  {
    # verify the existance of path
    if(!(-e "$depthPath\\dist"))
    {
      print "path not found: $depthPath\\dist\n";
      exit(1);
    }

    $distPath = "$depthPath\\dist";
  }
  elsif($whichPath eq "distwin")
  {
    # verify the existance of path
    if(!(-e "$depthPath\\$distWinPathName"))
    {
      print "path not found: $depthPath\\$distWinPathName\n";
      exit(1);
    }

    $distPath = "$depthPath\\$distWinPathName";
  }
  elsif($whichPath eq "install")
  {
    # verify the existance of path
    if(!(-e "$depthPath\\$distWinPathName\\install"))
    {
      print "path not found: $depthPath\\$distWinPathName\\install\n";
      exit(1);
    }

    $distPath = "$depthPath\\$distWinPathName\\install";
  }
  elsif($whichPath eq "packager")
  {
    # verify the existance of path
    if(!(-e "$depthPath\\xpinstall\\packager"))
    {
      print "path not found: $depthPath\\xpinstall\\packager\n";
      exit(1);
    }

    $distPath = "$depthPath\\xpinstall\\packager";
  }

  $distPath =~ s/\//\\/g; # convert slashes to backslashes for Dos commands to work
  return($distPath);
}

sub GetVersion
{
  my($depthPath) = @_;
  my($fileMozilla);
  my($fileMozillaVer);
  my($distWinPathName);
  my($monAdjusted);
  my($yy);
  my($mm);
  my($dd);
  my($hh);

  # determine if build was built via gmake
  if(-e "$depthPath\\dist\\install")
  {
    $distWinPathName = "dist";
  }
  else
  {
    # determine if build is debug or optimized
    # (used only for nmake builds)
    if($ENV{MOZ_DEBUG} eq "")
    {
      $distWinPathName = "dist\\Win32_o.obj";
    }
    else
    {
      $distWinPathName = "dist\\Win32_d.obj";
    }
  }

  $fileMozilla = "$depthPath\\$distWinPathName\\bin\\mozilla.exe";
  # verify the existance of file
  if(!(-e "$fileMozilla"))
  {
    print "file not found: $fileMozilla\n";
    exit(1);
  }

  ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, $ctime, $blksize, $blocks) = stat $fileMozilla;
  ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime($mtime);

  # calculate year
  # localtime() returns year 2000 as 100, we mod 100 to get at the last 2 digits
  $yy = $year % 100;
  $yy = "20" . sprintf("%.2d", $yy);

  # calculate month
  $monAdjusted = $mon + 1;
  $mm = sprintf("%.2d", $monAdjusted);

  # calculate month day
  $dd = sprintf("%.2d", $mday);

  # calculate day hour
  $hh = sprintf("%.2d", $hour);

  $fileMozillaVer = "$yy$mm$dd$hh";
  print "y2k compliant version string for $fileMozilla: $fileMozillaVer\n";
  return($fileMozillaVer);
}
