package WizCfg;
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(SetCfgVars, PrepareDistArea, CopyExtraDistFiles, CreateFullInstaller);


sub SetCfgVars 
{
  $::bPrepareDistArea     = 1;
  $::bCreateFullInstaller = 0;
  $::bCreateStubInstaller = 0;

  print "\n+++++++++++++++++++gre.pl++++++++++++++++++++++++";
  $::ProdDir                = "$::DEPTH\\xpinstall\\packager\\build\\win\\gre";
  $::XPI_JST_Dir            = "$::ProdDir\\XPI_JSTs";
  $::StubInstJstDir         = "$::ProdDir\\StubInstJst";
#  $cwdBuilder               = "$::DEPTH\\xpinstall\\wizard\\windows\\builder";
#  $cwdDistWin              = GetCwd("distwin",  $::DEPTH, $cwdBuilder);
  $::inStagePath            = "$::DEPTH\\dist\\stage";
  $::inDistPath             = "$::DEPTH\\dist\\installer\\gre";

  $::seiFileNameGeneric       = "nsinstall.exe";
  $::seiFileNameSpecific      = "gre-win32-installer.exe";
  $::seiStubRootName          = "gre-win32-stub-installer";
  $::seiFileNameSpecificStub  = "$::seiStubRootName.exe";
  $::sebiFileNameSpecific     = "";                 # filename for the big blob installer
  $::seuFileNameSpecific      = "GREUninstall.exe";
  $::seuzFileNameSpecific     = "greuninstall.zip";
  $::versionLanguage          = "en";
  $::seiBetaRelease           = "";

# set environment vars for use by other .pl scripts called from this script.
  $ENV{WIZ_nameCompany}          = "mozilla.org";
  $ENV{WIZ_nameProduct}          = "GRE";
  $ENV{WIZ_nameProductInternal}  = "GRE";
  $ENV{WIZ_nameProductNoVersion} = "GRE";
  $ENV{WIZ_fileMainExe}          = "none.exe";
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
