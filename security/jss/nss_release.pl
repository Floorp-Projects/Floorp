#! /usr/local/bin/perl
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
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

require('coreconf.pl');

###############################################
# Read in variables on command line into %var
###############################################

$var{ZIP} = "zip";

&parse_argv;


############################
# check variables
############################

if ($var{RELEASE_TREE} eq "") { exit; }
if ($var{RELEASE} eq "") { exit; }
if ($var{RELEASE_VERSION} eq "") { exit; }
if ($var{PLATFORM} eq "") { exit; }
if ($var{OS_ARCH} eq "") { exit; }

############################
# cd to the dist directory
############################

print STDERR "chdir ../dist/classes\n";
chdir("../dist/classes");


###############################################################################
# Specify all class files to be packaged, the load_library path, and the dest
###############################################################################

$filelist = 
     'org/mozilla/jss/ssl/*.class '.
     'org/mozilla/jss/crypto/AlreadyInitializedException.class '.
     'org/mozilla/jss/pkcs11/TokenCallbackInfo.class '.
     'org/mozilla/jss/NSSInit.class '.
     'org/mozilla/jss/CertDatabaseException.class '.
     'org/mozilla/jss/KeyDatabaseException.class '.
     'org/mozilla/jss/util/Assert.class '.
     'org/mozilla/jss/util/AssertionException.class '.
     'org/mozilla/jss/util/ConsolePasswordCallback.class '.
     'org/mozilla/jss/util/Debug.class '.
     'org/mozilla/jss/util/Password.class '.
     'org/mozilla/jss/util/PasswordCallback.class '.
     'org/mozilla/jss/util/PasswordCallback?GiveUpException.class '.
     'org/mozilla/jss/util/PasswordCallbackInfo.class '.
     'org/mozilla/jss/util/UTF8Converter.class';

$load_library = "../$var{'PLATFORM'}/lib/";

$dest = "$var{'RELEASE_TREE'}/$var{'RELEASE'}/$var{'RELEASE_VERSION'}/$var{'PLATFORM'}";

#####################################################################
# Dependent upon platform, package the files into the proper format
#####################################################################

if ($var{OS_ARCH} eq 'WINNT') {
	$filelist =~ s/\//\\/;
	$load_library =~ s/\//\\/;
	$dest =~ s/\//\\/;

	$load_library .= 'jss.dll';

	print STDERR "cp $load_library .\n";
	system("cp $load_library .");

	print STDERR "zip -T -r jss.jar $filelist\n";
	system("zip -T -r jss.zip $filelist");

	print STDERR "zip -T winjss.zip jss.zip jss.dll\n";
	system("zip -T winjss.zip jss.zip jss.dll");

	if (! (-e "$dest" && -d "$dest")) {
		print STDERR "making dir $dest \n";
		&rec_mkdir("$dest");
	}

	print STDERR "cp winjss.zip $dest\n";
	system("cp winjss.zip $dest");

	print STDERR "rm winjss.zip jss.zip jss.dll\n";
	system("rm winjss.zip jss.zip jss.dll");
}
elsif ($var{OS_ARCH} eq 'HP-UX') {
	$load_library .= 'libjss.sl';

	print STDERR "cp $load_library .\n";
	system("cp $load_library .");

	print STDERR "zip -T -r jss.jar $filelist\n";
	system("zip -T -r jss.zip $filelist");

	print STDERR "tar -cvf unixjss.tar jss.zip libjss.sl\n";
	system("tar -cvf unixjss.tar jss.zip libjss.sl");

	if (! (-e "$dest" && -d "$dest")) {
		print STDERR "making dir $dest \n";
		&rec_mkdir("$dest");
	}

	print STDERR "cp unixjss.tar $dest\n";
	system("cp unixjss.tar $dest");

	print STDERR "rm unixjss.tar jss.zip libjss.so\n";
	system("rm unixjss.tar jss.zip libjss.so");
}
else {
	$load_library .= 'libjss.so';

	print STDERR "cp $load_library .\n";
	system("cp $load_library .");

	print STDERR "zip -T -r jss.jar $filelist\n";
	system("zip -T -r jss.zip $filelist");

	print STDERR "tar -cvf unixjss.tar jss.zip libjss.so\n";
	system("tar -cvf unixjss.tar jss.zip libjss.so");

	if (! (-e "$dest" && -d "$dest")) {
		print STDERR "making dir $dest \n";
		&rec_mkdir("$dest");
	}

	print STDERR "cp unixjss.tar $dest\n";
	system("cp unixjss.tar $dest");

	print STDERR "rm unixjss.tar jss.zip libjss.so\n";
	system("rm unixjss.tar jss.zip libjss.so");
}

