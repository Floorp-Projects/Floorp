
# Unix implementation of platform-specific installer functions:
#
# BuildPlatformInstaller()
#

# Define file lists and locations for makeall.pl:

$exe_suffix = '';
@wizard_files = (
		 "mozilla-installer",
                 "mozilla-installer-bin",
		 "installer.ini",
		 "watermark.png",
		 "header.png"
		 );
@extra_ini_files = ();

sub BuildPlatformInstaller
{
    print "Creating sea installer:\n";
    print " $gDirDistInstall/$seiFileNameSpecific.tar.gz\n\n";

    my $mainExe = $ENV{WIZ_fileMainExe};
    my $instRoot = "$gDirDistInstall/sea/$mainExe-installer";

    system ("rm -rf $gDirDistInstall/sea");
    mkdir ("$gDirDistInstall/sea", 0775);
    mkdir ($instRoot, 0775);
    mkdir ("$instRoot/xpi", 0775);

    system ("cp -a $gDirDistInstall/setup/* $instRoot/");
    system ("cp -a $gDirDistInstall/xpi/* $instRoot/xpi/");

    system ("chmod +x $instRoot/mozilla-installer $instRoot/mozilla-installer-bin");

    rename ("$instRoot/mozilla-installer", "$instRoot/$mainExe-installer");
    rename ("$instRoot/mozilla-installer-bin", "$instRoot/$mainExe-installer-bin");

    system ("cd $gDirDistInstall/sea && tar -zcv --owner=0 --group=0 --numeric-owner --mode='go-w' -f $seiFileNameSpecific.tar.gz $mainExe-installer");
    return 0;
}

1;
