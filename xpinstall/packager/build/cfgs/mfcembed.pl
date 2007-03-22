package WizCfg;
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(SetCfgVars, PrepareDistArea, CopyExtraDistFiles, CreateFullInstaller);


sub SetCfgVars 
{
  $::bPrepareDistArea     = 1;
  $::bCreateFullInstaller = 0;
  $::bCreateStubInstaller = 0;

  print "\n+++++++++++++++++++mfcembed.pl++++++++++++++++++++++++";
  $::ProdDir                = "$::DEPTH\\xpinstall\\packager\\build\\win\\mfcembed";
  $::XPI_JST_Dir            = "$::ProdDir\\XPI_JSTs";
  $::StubInstJstDir         = "$::ProdDir\\StubInstJst";
#  $cwdBuilder               = "$::DEPTH\\xpinstall\\wizard\\windows\\builder";
#  $cwdDistWin              = GetCwd("distwin",  $::DEPTH, $cwdBuilder);
  $::inStagePath            = "$::DEPTH\\dist\\stage";
  $::inDistPath             = "$::DEPTH\\dist\\installer\\mfcembed";

  $::seiFileNameGeneric       = "nsinstall.exe";
  $::seiFileNameSpecific      = "mfcembed-win32-installer.exe";
  $::seiStubRootName          = "mfcembed-win32-stub-installer";
  $::seiFileNameSpecificStub  = "$::seiStubRootName.exe";
  $::sebiFileNameSpecific     = "";                 # filename for the big blob installer
  $::seuFileNameSpecific      = "MfcEmbedUninstall.exe";
  $::seuzFileNameSpecific     = "mfcembeduninstall.zip";
  $::versionLanguage          = "en";
  $::seiBetaRelease           = "";

# set environment vars for use by other .pl scripts called from this script.
  $ENV{WIZ_nameCompany}          = "mozilla.org";
  $ENV{WIZ_nameProduct}          = "MfcEmbed";
  $ENV{WIZ_nameProductInternal}  = "MfcEmbed";
  $ENV{WIZ_nameProductNoVersion} = "MfcEmbed";
  $ENV{WIZ_fileMainExe}          = "mfcembed.exe";
  $ENV{WIZ_fileUninstall}        = $::seuFileNameSpecific;
  $ENV{WIZ_fileUninstallZip}     = $::seuzFileNameSpecific;
  $ENV{WIZ_descShortcut}         = "";
}

sub GetStarted
{
  print "\nNo Getting Started actions required for this product\n";
}

sub MakeExeZipFiles
{
  print "\n Don't need to create any Self-extracting zip files for this product  \n";
}

sub CopyExtraDistFiles
{
  print "\Copying Extra Dist Files to copy\n";

  # copy license file for the installer
  if(system("copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\license.txt"))
  {
    die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\license.txt\n";
  }
  if(system("copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\setup\\license.txt"))
  {
    die "\n Error: copy $ENV{MOZ_SRC}\\mozilla\\LICENSE $::inDistPath\\setup\\license.txt\n";
  }

  # Copy GRE blob installer
  if(system("copy $::inDistPath\\..\\..\\inst_gre\\gre-win32-installer.exe $::inDistPath"))
  {
    die "\n Error: copy $::inDistPath\\..\\..\\inst_gre\\gre-win32-installer.exe $::inDistPath\n";
  }

  # Until we really figure out what to do with this batch file, this will get it
  #   into the installer, at least
  print "\n Copying $::ProdDir\\runapp.bat $::inStagePath\\mfcembed\\gre_app_support\n";
  if(system("copy $::ProdDir\\runapp.bat $::inStagePath\\mfcembed\\gre_app_support"))
  {
    die "\n Error: copy $::ProdDir\\runapp.bat $::inStagePath\\mfcembed\\gre_app_support\n";
  }
}

sub CreateFullInstaller()
{
  # create the big self extracting .exe installer
  print "\nNo Self Extracting Executable is created for this product\n";

  return(0);
}

sub FinishUp
{
  print "\nNo finish up step required for this product\n";
  return(0);
}
