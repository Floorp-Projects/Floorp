#!c:\perl\bin\perl

# This script creates a setuptmp.bat file that needs to be run
# to setup the proper environment variables.  This is because
# environment variables set inside perl scripts will be unset when
# the perl script quits.
#
# This does not happen with .bat files.
#
# This script also creates a unsetup.bat file to undo what
# setuptmp.bat sets.

use Cwd;

if ($#ARGV < 0) { die "$0 requires an argument!\n"; }

# set variables to be used to create the tmp .bat files
# to set environment variables.
$Path_Saved                       = $ENV{PATH};
$Path                             = $ENV{PATH};

$Product_User_Agent               = "4.72 (en)";
$Product_Version                  = "4.72";
$Product_Version_Info             = "4.72.0";
$Product_Version_Name             = "472";
$RSH_Box                          = "grok";
$SMTP_Host                        = "grok";
$ISScriptsDir                     = cwd();        # scripts directory path
$Zigbert                          = "\\\\lingcod\\certs\\bin\\signtool.exe";
$SecurityDescription              = "Netscape Communications's VeriSign, Inc. ID #2";
$ParentCertsPath                  = "\\\\lingcod\\certs\\keys\\javabase.021025";
$CertsPath                        = "\\\\lingcod\\certs\\asd.ns6";

# get environment vars
$userAgent        = $ENV{WIZ_userAgent};
$userAgentShort   = $ENV{WIZ_userAgentShort};
$xpinstallVersion = $ENV{WIZ_xpinstallVersion};
$nameCompany      = $ENV{WIZ_nameCompany};
$nameProduct      = $ENV{WIZ_nameProduct};
$nameProductInternal = $ENV{WIZ_nameProductInternal};
$fileMainExe      = $ENV{WIZ_fileMainExe};
$fileUninstall    = $ENV{WIZ_fileUninstall};

$BuildDir         = "binary";
$SetupName        = "NSSetup.exe";
$ScriptName       = "install.js";
$save_cwd         = cwd();
$DEPTH            = "..\\..\\..";

# append_path($Path, "\\\\lingcod\\certs\\bin");

if($ENV{MOZCONFIG} eq "") # if we're defining mozconfig, we're using gmake
{
  if($ENV{MOZ_DEBUG} eq "")
  {
    $Dist = "$DEPTH\\dist\\win32_o.obj";
  }
  else
  {
    $Dist = "$DEPTH\\dist\\win32_d.obj";
  }
}
else
{
  $Dist = "$DEPTH\\dist";
}

set_JarName();
map_all();
create_dir_structure();
sign_files();

if(-e $BuildDir)
{
    if(system("perl $ISScriptsDir\\rdir.pl $BuildDir") != 0)
    {
        exit(1);
    }
}

print "done!\n";
chdir($save_cwd);
exit(0);
# end

sub append_path
{
    if($_[0] eq "")
    {
        $_[0] = $_[1];
    }
    elsif($_[1] ne "")
    {
        $_[0] = $_[1] . ";" . $_[0];
    }
}

sub map_all
{
    print "\n";
    print "verifying connection to \\\\lingcod\\certs...\n";
    if(!(-e "\\\\lingcod\\certs\\bin"))
    {
        print "\nError: \\\\lingcod\\certs\\bin does not exist!\n";
        exit(1);
    }
    if(!(-e $ParentCertsPath))
    {
        print "\nError: $ParentCertsPath does not exist!\n";
        exit(1);
    }

    if(-e $CertsPath)
    {
        system("echo y | del $CertsPath");
    }
    print "system(\"xcopy /F $ParentCertsPath\\*.* $CertsPath\\\")\;\n";
    system("xcopy /F $ParentCertsPath\\*.* $CertsPath\\");
    print "done!\n";
}

sub print_usage
{
    print "\nusage: $0 <lang> <bit> <product>\n";
    print "\n";
    print "       lang   : en\n";
    print "                ja\n";
    print "\n";
    print "       bit    : 16\n";
    print "                32\n";
    print "\n";
    print "       product: nova\n";
    print "                gromit\n";
    print "                ratbert\n";
}

sub set_JarName
{
    $JarName = "NSSetup.jar";
}

sub sign_files
{
    print "\nsigning files...\n";

    if(-e "deliver\\$JarName")
    {
        unlink "deliver\\$JarName";
    }
    if(-e "$BuildDir\\META-INF")
    {
        if(system("perl $ISScriptsDir\\rdir.pl $BuildDir\\META-INF") != 0)
        {
            exit(1);
        }
    }

    if(system("$Zigbert -i$ScriptName -d$CertsPath -k\"$SecurityDescription\" -p$ARGV[0] $BuildDir") != 0)
    {
        print "error trying to sign $BuildDir\n";
        exit(1);
    }

    chdir($BuildDir);
    print "\ncreating $JarName\n";
    if(system("zip -9 -r $JarName *") != 0)
    {
        chdir("..");
        print "error trying to create $JarName\n";
        exit(1);
    }
    chdir("..");

    if(!(-e "$Dist\\install"))
    {
        mkdir ("$Dist\\install",0775);
    }
    $xcopy_cmd = "copy $BuildDir\\$JarName $Dist\\install";
    if(system($xcopy_cmd) != 0)
    {
        print "$xcopy_cmd failed!\n";
        {
            exit(1);
        }
    }
    unlink "$BuildDir\\$JarName";
}

sub create_dir_structure
{
    print "creating build structure...\n";

    if(-e $BuildDir)
    {
        if(system("perl $ISScriptsDir\\rdir.pl $BuildDir") != 0)
        {
            exit(1);
        }
    }

    if(!(-e "$BuildDir"))
    {
        mkdir ("$BuildDir",0775);
    }
    $xcopy_cmd = "copy $Dist\\install\\stub\\$SetupName $BuildDir";
    if(system($xcopy_cmd) != 0)
    {
        print "$xcopy_cmd failed!\n";
        exit(1);
    }
    $xcopy_cmd = "copy setupjar.js $BuildDir\\install.js";
    if(system($xcopy_cmd) != 0)
    {
        print "$xcopy_cmd failed!\n";
        exit(1);
    }
}

sub ParseUserAgentShort()
{
  my($aUserAgent) = @_;
  my($aUserAgentShort);

  @spaceSplit = split(/ /, $aUserAgent);
  if($#spaceSplit >= 0)
  {
    $aUserAgentShort = $spaceSplit[0];
  }

  return($aUserAgentShort);
}

