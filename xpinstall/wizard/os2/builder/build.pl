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
# The Original Code is Mozilla Communicator client code, 
# released March 31, 1998. 
#
# The Initial Developer of the Original Code is Netscape Communications 
# Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#   Sean Su <ssu@netscape.com>

#
# Purpose:
#    To build the mozilla self-extracting installer and its corresponding .xpi files
#    given a mozilla build on a local system.
#
# Requirements:
# 1. perl needs to be installed correctly on the build system because cwd.pm is used.
# 2. mozilla\xpinstall\wizard\os2 needs to be built.
#

if($ENV{MOZ_SRC} eq "")
{
  print "Error: MOZ_SRC not set!";
  exit(1);
}

if($ENV{MOZ_OBJDIR} eq "")
{
  print "Error: MOZ_OBJDIR not set!";
  exit(1);
}

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

$DEPTH         = "$ENV{MOZ_SRC}/mozilla";
$cwdBuilder    = "$DEPTH/xpinstall/wizard/os2/builder";
$cwdDist       = GetCwd("dist",     $DEPTH, $ENV{MOZ_OBJDIR}, $cwdBuilder);
$cwdInstall    = GetCwd("install",  $DEPTH, $ENV{MOZ_OBJDIR}, $cwdBuilder);
$cwdPackager   = GetCwd("packager", $DEPTH, $ENV{MOZ_OBJDIR}, $cwdBuilder);
$verPartial    = "1.2b.0.";
$ver           = $verPartial . GetVersion($DEPTH, $ENV{MOZ_OBJDIR});

if(-d "$DEPTH/stage")
{
  system("perl $cwdPackager/os2/rdir.pl $DEPTH/stage");
}

# The destination cannot be a sub directory of the source.
# pkgcp.pl will get very unhappy.

mkdir("$DEPTH/stage", 775);
system("perl $cwdPackager/pkgcp.pl -s $cwdDist -d $DEPTH/stage -f $cwdPackager/packages-os2 -o unix -v");
system("perl $cwdPackager/xptlink.pl -s $cwdDist -d $DEPTH/stage -o unix -v");

chdir("$cwdPackager/os2");
if(system("perl makeall.pl $ver $DEPTH/stage $cwdDist/install -aurl $inXpiURL -rurl $inRedirIniURL"))
{
  print "\n Error: perl makeall.pl $ver $DEPTH/stage $cwdDist/install $inXpiURL $inRedirIniURL\n";
  exit(1);
}

chdir($cwdBuilder);

# Copy the .xpi files to the same directory as setup.exe.
# This is so that setup.exe can find the .xpi files
# in the same directory and use them.
#
# Mozilla-os2-installer.exe (a self extracting file) will use the .xpi
# files from its current directory as well, but it is not a requirement
# that they exist because it already contains the .xpi files within itself.
if(system("cp $cwdDist/install/xpi/*.* $cwdDist/install"))
{
  print "Error: cp $cwdDist/install/xpi/*.* $cwdDist/install\n";
  exit(1);
}

print "\n";
print "**\n";
print "*\n";
print "*  A self-extracting installer has been built and delivered:\n";
print "*\n";
print "*      $cwdDist/install/mozilla-os2-installer.exe\n";
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
  my($whichPath, $depthPath, $objdirName, $returnCwd) = @_;
  my($distCwd);
  my($distPath);

  if($whichPath eq "dist")
  {
    # verify the existance of path
    if(!(-e "$depthPath/$objdirName/dist"))
    {
      print "path not found: $depthPath/$objDirName/dist\n";
      exit(1);
    }

    $distPath = "$depthPath/$objdirName/dist";
  }
  elsif($whichPath eq "install")
  {
    # verify the existance of path
    if(!(-e "$depthPath/$objdirName/dist/install"))
    {
      print "path not found: $depthPath/$objdirName/dist/install\n";
      exit(1);
    }

    $distPath = "$depthPath/$objdirname/install";
  }
  elsif($whichPath eq "packager")
  {
    # verify the existance of path
    if(!(-e "$depthPath/xpinstall/packager"))
    {
      print "path not found: $depthPath/xpinstall/packager\n";
      exit(1);
    }

    $distPath = "$depthPath/xpinstall/packager";
  }

  return($distPath);
}

sub GetVersion
{
  my($depthPath, $objdirName) = @_;
  my($fileMozilla);
  my($fileMozillaVer);
  my($monAdjusted);
  my($yy);
  my($mm);
  my($dd);
  my($hh);

  $fileMozilla = "$depthPath/$objdirName/dist/bin/mozilla.exe";
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
