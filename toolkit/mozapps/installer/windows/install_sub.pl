# Windows implementation of platform-specific installer functions:
#
# BuildPlatformInstaller()

# Define wizard file locations
$exe_suffix = '.exe';
@wizard_files = (
		 "setup.exe",
		 "setuprsc.dll"
		 );

sub BuildPlatformInstaller
{
  # copy the lean installer to stub\ dir
  print "\n****************************\n";
  print "*                          *\n";
  print "*  creating Stub files...  *\n";
  print "*                          *\n";
  print "****************************\n";
  print "\n $gDirDistInstall/stub/$seiFileNameSpecificStub\n";

  # build the self-extracting .exe (installer) file.
  copy("$gDirDistInstall/$seiFileNameGeneric", "$gDirDistInstall/$seiFileNameSpecificStub") ||
    die "copy $gDirDistInstall/$seiFileNameGeneric $gDirDistInstall/$seiFileNameSpecificStub: $!\n";

  $origCwd = cwd();
  chdir($gDirDistInstall);
  system("./nsztool.exe $seiFileNameSpecificStub setup/*.*") && die "Error creating self-extracting installer";
  chdir($origCwd);

  if(-d "$gDirDistInstall/stub")
  {
    unlink <$gDirDistInstall/stub/*>;
  }
  else
  {
    mkdir ("$gDirDistInstall/stub",0775);
  }
  copy("$gDirDistInstall/$seiFileNameSpecificStub", "$gDirDistInstall/stub") ||
    die "copy $gDirDistInstall/$seiFileNameSpecificStub $gDirDistInstall/stub: $!\n";


  # create the xpi for launching the stub installer
  print "\n************************************\n";
  print "*                                  *\n";
  print "*  creating stub installer xpi...  *\n";
  print "*                                  *\n";
  print "************************************\n";
  print "\n $gDirDistInstall/$seiStubRootName.xpi\n\n";

  if(-d "$gDirStageProduct/$seiStubRootName")
  {
    unlink <$gDirStageProduct/$seiStubRootName/*>;
  }
  else
  {
    mkdir ("$gDirStageProduct/$seiStubRootName",0775);
  }
  copy("$gDirDistInstall/stub/$seiFileNameSpecificStub", "$gDirStageProduct/$seiStubRootName") ||
    die "copy $gDirDistInstall/stub/$seiFileNameSpecificStub $gDirStageProduct/$seiStubRootName: $!\n";

  # Make .js files
  if(MakeJsFile($seiStubRootName))
  {
    return(1);
  }

  # Make .xpi file
  if(system("perl $gNGAppsScriptsDir/makexpi.pl $seiStubRootName $gDirStageProduct $gDirDistInstall"))
  {
    print "\n Error: perl $gNGAppsScriptsDir/makexpi.pl $seiStubRootName $gDirStageProduct $gDirDistInstall\n";
    return(1);
  }

  # group files for CD
  print "\n************************************\n";
  print "*                                  *\n";
  print "*  creating Compact Disk files...  *\n";
  print "*                                  *\n";
  print "************************************\n";
  print "\n $gDirDistInstall/cd\n";

  if(-d "$gDirDistInstall/cd")
  {
    unlink <$gDirDistInstall/cd/*>;
  }
  else
  {
    mkdir ("$gDirDistInstall/cd",0775);
  }

  copy("$gDirDistInstall/$seiFileNameSpecificStub", "$gDirDistInstall/cd") ||
    die "copy $gDirDistInstall/$seiFileNameSpecificStub $gDirDistInstall/cd: $!\n";

  StageUtils::CopyFiles("$gDirDistInstall/xpi", "$gDirDistInstall/cd");

  # create the big self extracting .exe installer
  print "\n**************************************************************\n";
  print "*                                                            *\n";
  print "*  creating Self Extracting Executable Full Install file...  *\n";
  print "*                                                            *\n";
  print "**************************************************************\n";
  print "\n $gDirDistInstall/$seiFileNameSpecific\n";

  if(-d "$gDirDistInstall/sea")
  {
    unlink <$gDirDistInstall/sea/*>;
  }
  else
  {
    mkdir ("$gDirDistInstall/sea",0775);
  }

  copy("$gDirDistInstall/$seiFileNameGeneric", "$gDirDistInstall/$seiFileNameSpecific") ||
    die "copy $gDirDistInstall/$seiFileNameGeneric $gDirDistInstall/$seiFileNameSpecific: $!\n";

  $origCwd = cwd();
  chdir($gDirDistInstall);

  system("./nsztool.exe $seiFileNameSpecific setup/*.* xpi/*.*") &&
    die "\n Error: ./nsztool.exe $seiFileNameSpecific setup/*.* xpi/*.*\n";
  chdir($origCwd);

  copy("$gDirDistInstall/$seiFileNameSpecific", "$gDirDistInstall/sea") ||
    die "copy $gDirDistInstall/$seiFileNameSpecific $gDirDistInstall/sea: $!\n";

  unlink <$gDirDistInstall/$seiFileNameSpecificStub>;

  if ($ENV{MOZ_INSTALLER_USE_7ZIP}) 
  {
    # 7-Zip Self Extracting Archive
    print "\n********************************************************************\n";
    print   "*                                                                  *\n";
    print   "*  creating 7-Zip Self Extracting Executable Full Install File...  *\n";
    print   "*                                                                  *\n";
    print   "********************************************************************\n";
    print "\n $gDirDistInstall/7-zip\n";
    
    if(-d "$gDirDistInstall/7z")
    {
      unlink <$gDirDistInstall/7z/*>;
    }
    else
    {
      mkdir ("$gDirDistInstall/7z",0775);
    }
    
    # Set up the 7-Zip stage
    if(-d "$gDirDistInstall/7zstage")
    {
      unlink <$gDirDistInstall/7zstage/*>;
    }
    else 
    {
      mkdir ("$gDirDistInstall/7zstage",0775);
    }
    
    # Copy the files into the stage
    chdir("$gDirDistInstall/setup");
    `cp *.* $gDirDistInstall/7zstage`;
    chdir("$gDirDistInstall/xpi");
    `cp *.* $gDirDistInstall/7zstage`;
    chdir("$gDirDistInstall");
    
    # Copy the 7zSD SFX launcher to dist/install/7z
    copy("$topsrcdir/$ENV{WIZ_sfxModule}", "$gDirDistInstall/7z") ||
      die "copy $topsrcdir/$ENV{WIZ_sfxModule} $gDirDistInstall/7z\n";
        
    # Copy the SEA manifest to dist/install/7z
    copy("$inConfigFiles/app.tag", "$gDirDistInstall/7z") ||
      die "copy $inConfigFiles/app.tag $gDirDistInstall/7z";
    
    # Copy the generation batch file to dist/install
    copy("$inConfigFiles/7zip.bat", "$gDirDistInstall") ||
      die "copy $inConfigFiles/7zip.bat $gDirDistInstall";
    
    chdir($gDirDistInstall);
    system("cmd /C 7zip.bat");
    move("$gDirDistInstall/7z/SetupGeneric.exe", "$gDirDistInstall/sea/$seiFileNameSpecific") ||
      die "move $gDirDistInstall/SetupGeneric.exe $gDirDistInstall/sea/$seiFileNameSpecific";
  }

  if ($ENV{MOZ_PACKAGE_NSIS}) 
  {
    # NSIS Installer - requires 7-Zip Self Extracting Archive
    print "\n********************************\n";
    print   "*                              *\n";
    print   "*  creating NSIS Installer...  *\n";
    print   "*                              *\n";
    print   "********************************\n";
    print "\n $gDirDistInstall/nsis\n";
    
    # Make sure this is compiling on a win32 system
    $win32 = ($^O =~ / ((MS)?win32)|cygwin|os2/i) ? 1 : 0;
    if (!$win32) {
        die "ERROR: NSIS installers can currently only be made on the Windows platforms\n";
    }
    
    # Make sure makensis.exe is available
    $makensis = `which makensis.exe`;
    if (!defined($makensis) or length($makensis) lt 1) {
        die "ERROR: makensis.exe not found.  Is NSIS installed and in your $PATH?\n";
    }
    
    # Make sure 7z.exe is available
    $zip = `which 7z.exe`;
    if (!defined($zip) or length($zip) lt 1) {
        die "ERROR: 7z.exe not found.  Is 7-Zip installed and in your $PATH?\n";
    }
    
    # Make sure installer-stage is available
    if (!(-d "$topobjdir/installer-stage")) {
        die "ERROR: $topobjdir/installer-stage not found.\n       ".
            "First make installer-stage in the application's installer ".
            "directory to stage the files.\n";
    }
    
    # Set up the NSIS stage
    if(-d "$gDirDistInstall/nsis")
    {
      unlink <$gDirDistInstall/nsis/*>;
    }
    else 
    {
      mkdir ("$gDirDistInstall/nsis",0775);
    }
    
    # Set up the NSIS config directory for inclusion in the SEA
    if(-d "$gDirDistInstall/nsis/config")
    {
      unlink <$gDirDistInstall/nsis/config/*>;
    }
    else 
    {
      mkdir ("$gDirDistInstall/nsis/config",0775);
    }
    
    # Copy application NSIS installer files
    copy("$inConfigFiles/7zipNSIS.bat", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/nsis/7zipNSIS.bat $gDirDistInstall/nsis";
    copy("$inConfigFiles/instfiles-extra.nsi", "$gDirDistInstall/nsis") ||
      die "copy $inConfigFiles/instfiles-extra.nsi $gDirDistInstall/nsis: $!\n";
    copy("$inConfigFiles/removed-files.log", "$gDirDistInstall/nsis/config") ||
      die "copy $inConfigFiles/removed-files.log $gDirDistInstall/nsis/config: $!\n";
    copy("$inConfigFiles/SetProgramAccess.nsi", "$gDirDistInstall/nsis") ||
      die "copy $inConfigFiles/SetProgramAccess.nsi $gDirDistInstall/nsis: $!\n";
    copy("$gDirDistInstall/license.txt", "$gDirDistInstall/nsis") ||
      die "copy $gDirDistInstall/license.txt $gDirDistInstall/nsis: $!\n";
    copy("$inDistPath/branding/wizHeader.bmp", "$gDirDistInstall/nsis") ||
      die "copy $inDistPath/branding/wizHeader.bmp $gDirDistInstall/nsis: $!\n";
    copy("$inDistPath/branding/wizWatermark.bmp", "$gDirDistInstall/nsis") ||
      die "copy $inDistPath/branding/wizWatermark.bmp $gDirDistInstall/nsis: $!\n";
    
    # Copy config files used during setip when the installer runs
    copy("$inConfigFiles/defines.nsi", "$gDirDistInstall/nsis") ||
      die "copy $inConfigFiles/defines.nsi $gDirDistInstall/nsis: $!\n";
    
    # Copy toolkit NSIS installer files
    copy("$gNGAppsScriptsDir/windows/nsis/common.nsh", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/windows/nsis/common.nsh $gDirDistInstall/nsis: $!\n";
    copy("$gNGAppsScriptsDir/windows/nsis/installer.nsi", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/windows/nsis/installer.nsi $gDirDistInstall/nsis: $!\n";
    copy("$gNGAppsScriptsDir/windows/nsis/options.ini", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/windows/nsis/options.ini $gDirDistInstall/nsis: $!\n";
    copy("$gNGAppsScriptsDir/windows/nsis/shortcuts.ini", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/windows/nsis/shortcuts.ini $gDirDistInstall/nsis: $!\n";
    copy("$gNGAppsScriptsDir/windows/nsis/ShellLink.dll", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/windows/nsis/ShellLink.dll $gDirDistInstall/nsis: $!\n";
    copy("$gNGAppsScriptsDir/windows/nsis/version.nsh", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/windows/nsis/version.nsh $gDirDistInstall/nsis: $!\n";
    copy("$gNGAppsScriptsDir/windows/wizard/setuprsc/setup.ico", "$gDirDistInstall/nsis") ||
      die "copy $gNGAppsScriptsDir/windows/wizard/setuprsc/setup.ico $gDirDistInstall/nsis: $!\n";

    # Copy toolkit NSIS installer locale files
    copy("$topsrcdir/toolkit/locales/en-US/installer/windows/commonLocale.nsh", "$gDirDistInstall/nsis") ||
      die "copy $topsrcdir/toolkit/locales/en-US/installer/windows/commonLocale.nsh $gDirDistInstall/nsis: $!\n";
    
    # Create the NSIS installer
    # makensis.exe commandline options that affect stdout logging
    # /Vx verbosity where x is 4=all,3=no script,2=no info,1=no warnings,0=none
    # /Ofile specifies a text file to log compiler output (default is stdout)
    chdir("$gDirDistInstall/nsis");
    system("makensis.exe installer.nsi") &&
      die "Error creating NSIS installer";
    
    # Copy the 7zSD SFX launcher to dist/install/nsis
    copy("$topsrcdir/$ENV{WIZ_sfxModule}", "$gDirDistInstall/nsis") ||
      die "copy $topsrcdir/$ENV{WIZ_sfxModule} $gDirDistInstall/nsis\n";
    
    # Copy the SEA manifest to dist/install/nsis
    copy("$inConfigFiles/app.tag", "$gDirDistInstall/nsis") ||
      die "copy $inConfigFiles/app.tag $gDirDistInstall/nsis";
    
    system("cmd /C 7zipNSIS.bat");

    # Temporary name change to include -nsis before .exe
    $nsisFileNameSpecific = $seiFileNameSpecific;
    $nsisFileNameSpecific =~ s/\.exe$/-nsis\.exe/;
    # Since we are using a unique temp name for the NSIS installer it is safe
    # to copy it alongside the xpinstall based installer.
    move("$gDirDistInstall/nsis/SetupGeneric.exe", "$gDirDistInstall/sea/$nsisFileNameSpecific") ||
      die "move $gDirDistInstall/nsis/SetupGeneric.exe $gDirDistInstall/sea/$nsisFileNameSpecific";
  }
  print " done!\n\n";

  if((!(-e "$topsrcdir/../redist/microsoft/system/msvcrt.dll")) ||
     (!(-e "$topsrcdir/../redist/microsoft/system/msvcirt.dll")))
  {
    print "***\n";
    print "**\n";
    print "**  The following required Microsoft redistributable system files were not found\n";
    print "**  in $topsrcdir/../redist/microsoft/system:\n";
    print "**\n";
    if(!(-e "$topsrcdir/../redist/microsoft/system/msvcrt.dll"))
    {
      print "**    msvcrt.dll\n";
    }
    if(!(-e "$topsrcdir/../redist/microsoft/system/msvcirt.dll"))
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
  return 0;
}

sub MakeExeZip
{
  my($aSrcDir, $aExeFile, $aZipFile) = @_;
  my($saveCwdir);

  $saveCwdir = cwd();
  chdir($aSrcDir);
  if(system("zip $gDirDistInstall/xpi/$aZipFile $aExeFile"))
  {
    chdir($saveCwdir);
    die "\n Error: zip $gDirDistInstall/xpi/$aZipFile $aExeFile";
  }
  chdir($saveCwdir);
}

