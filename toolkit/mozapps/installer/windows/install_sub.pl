# Windows implementation of platform-specific installer functions:
#
# BuildPlatformInstaller()

# Define wizard file locations
$exe_suffix = '.exe';
@wizard_files = (
		 "setup.exe",
		 "setuprsc.dll"
		 );
@extra_ini_files = ("install.ini");

sub BuildPlatformInstaller
{
    print "Making uninstaller...\n";
    MakeUninstall() && die;

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

sub MakeUninstall
{
  chdir($inConfigFiles);
  if(MakeUninstallIniFile())
  {
    return(1);
  }

  # Copy the uninstall files to the dist uninstall directory.
  copy("uninstall.ini", "$gDirDistInstall") ||
    die "copy uninstall.ini $gDirDistInstall: $!\n";
  copy("uninstall.ini", "$gDirDistInstall/uninstall") ||
    die "copy uninstall.ini $gDirDistInstall/uninstall: $!\n";
  copy("$gDirDistInstall/uninstall.exe", "$gDirDistInstall/uninstall") ||
    die "copy $gDirDistInstall/uninstall.exe $gDirDistInstall/uninstall: $!\n";

  # build the self-extracting .exe (uninstaller) file.
  print "\nbuilding self-extracting uninstaller ($seuFileNameSpecific)...\n";
  copy("$gDirDistInstall/$seiFileNameGeneric", "$gDirDistInstall/$seuFileNameSpecific") ||
    die "copy $gDirDistInstall/$seiFileNameGeneric $gDirDistInstall/$seuFileNameSpecific: $!\n";

  $origCwd = cwd();
  chdir($gDirDistInstall);

  if(system("./nsztool.exe $seuFileNameSpecific uninstall/*.*"))
  {
    print "\n Error: ./nsztool.exe $seuFileNameSpecific uninstall/*.*\n";
    return(1);
  }

  chdir($origCwd);

  MakeExeZip($gDirDistInstall, $seuFileNameSpecific, $seuzFileNameSpecific);
  unlink <$gDirDistInstall/$seuFileNameSpecific>;
  return(0);
}

sub MakeUninstallIniFile
{
  # Make config.ini file
  chdir($inConfigFiles);
  if(system("perl $gDirPackager/windows/makeuninstallini.pl uninstall.it $gDefaultProductVersion"))
  {
    print "\n Error: perl $gDirPackager/windows/makeuninstallini.pl uninstall.it $gDefaultProductVersion\n";
    return(1);
  }
  return(0);
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

