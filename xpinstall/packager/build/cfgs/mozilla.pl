package WizCfg;
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(SetCfgVars, CopyExtraDistFiles, CreateFullInstaller);


sub SetCfgVars 
{
  print"What is going on here?\n";
  $::ProdDir                = "$::DEPTH\\xpinstall\\packager\\build\\win\\mozilla";
  $::XPI_JST_Dir            = "$::ProdDir\\XPI_JSTs";
  $::StubInstJstDir         = "$::ProdDir\\StubInstJst";
#  $cwdBuilder               = "$::DEPTH\\xpinstall\\wizard\\windows\\builder";
#  $cwdDistWin              = GetCwd("distwin",  $::DEPTH, $cwdBuilder);
  $::inStagePath            = "$::DEPTH\\stage";
  $::inDistPath             = "$::DEPTH\\dist\\installer\\mozilla";

  $::seiFileNameGeneric       = "nsinstall.exe";
  $::seiFileNameSpecific      = "mozilla-win32-installer.exe";
  $::seiStubRootName          = "mozilla-win32-stub-installer";
  $::seiFileNameSpecificStub  = "$::seiStubRootName.exe";
  $::sebiFileNameSpecific     = "";                 # filename for the big blob installer
  $::seuFileNameSpecific      = "MozillaUninstall.exe";
  $::seuzFileNameSpecific     = "mozillauninstall.zip";
  $::versionLanguage          = "en";
  $::seiBetaRelease           = "";

# set environment vars for use by other .pl scripts called from this script.
  $ENV{WIZ_nameCompany}          = "mozilla.org";
  $ENV{WIZ_nameProduct}          = "Mozilla";
  $ENV{WIZ_nameProductInternal}  = "Mozilla";
  $ENV{WIZ_nameProductNoVersion} = "Mozilla";
  $ENV{WIZ_fileMainExe}          = "Mozilla.exe";
  $ENV{WIZ_fileUninstall}        = $::seuFileNameSpecific;
  $ENV{WIZ_fileUninstallZip}     = $::seuzFileNameSpecific;
  $ENV{WIZ_descShortcut}         = "";

}

sub GetStarted
{
  print "\nNo Getting Started actions required for this product\n";
}

sub PrepareDistArea
{
  print "\n Don't need prep the dist area for this product  \n";
}

sub MakeExeZipFiles
{
  print "\n Don't need to create any Self-extracting zip files for this product  \n";
}

sub CopyExtraDistFiles
{
  # copy license file for the installer
  if(system("copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\license.txt"))
  {
    die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\license.txt\n";
  }
  if(system("copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\setup\\license.txt"))
  {
    die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\setup\\license.txt\n";
  }
}

sub CreateFullInstaller()
{
  # create the big self extracting .exe installer
  print "\n**************************************************************\n";
  print "*                                                            *\n";
  print "*  creating Self Extracting Executable Full Install file...  *\n";
  print "*                                                            *\n";
  print "**************************************************************\n";
  if(-d "$::inDistPath\\sea")
  {
    unlink <$::inDistPath\\sea\\*>;
  }
  else
  {
    mkdir ("$::inDistPath\\sea",0775);
  }
  if(system("copy $::inDistPath\\$::seiFileNameGeneric $::inDistPath\\$::seiFileNameSpecific"))
  {
    die "\n Error: copy $::inDistPath\\$::seiFileNameGeneric $::inDistPath\\$::seiFileNameSpecific\n";
  }
  if(system("$::inDistPath\\nsztool.exe $::inDistPath\\$::seiFileNameSpecific $::inDistPath\\setup\\*.* $::inDistPath\\xpi\\*.*"))
  {
    die "\n Error: $::inDistPath\\nsztool.exe $::inDistPath\\$::seiFileNameSpecific $::inDistPath\\setup\\*.* $::inDistPath\\xpi\\*.*\n";
  }
  if(system("copy $::inDistPath\\$::seiFileNameSpecific $::inDistPath\\sea"))
  {
    die "\n Error: copy $::inDistPath\\$::seiFileNameSpecific $::inDistPath\\sea\n";
  }
  unlink <$::inDistPath\\$::seiFileNameSpecificStub>;

  return(0);
}

sub FinishUp
{
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

  return(0);
}
