#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
#  First attempt at using module-graph.pl to generate
#  a cvs checkout list, and building the resulting tree.
#

use Cwd;

sub get_system_cwd {
    my $a = Cwd::getcwd()||`pwd`;
    chomp($a);
    return $a;
}



# main
{

 # Pull core build stuff.
 print "Pulling core build files...\n";
 my $core_build_files = "mozilla/client.mk mozilla/config mozilla/configure mozilla/allmakefiles.sh mozilla/configure.in mozilla/Makefile.in mozilla/build mozilla/include mozilla/tools/module-deps";
 system("cvs co $core_build_files");


 # Pull nspr
 print "Pulling nspr...\n";
 my $nspr_cvs_cmd = "cvs co -rNSPRPUB_PRE_4_2_CLIENT_BRANCH  mozilla/nsprpub";
 system("$nspr_cvs_cmd");

 # Assume all.dot is checked in somewhere.


 # Pull modules, write out to file to grap stdout.
 # Hard-coding this for xpcom to start off.
 print "Pulling modules...\n";
 my $modules;
 my $cmd = "cvs co `mozilla/tools/module-deps/module-graph\.pl --file all\.dot --start-module xpcom --list-only | mozilla/config/module2dir\.pl --list-only`";
 print "cmd = $cmd\n";
 system($cmd);

 #open LOG, ">modules.log" or die "Couldn't open logfile, modules.log: $!";
 #open CMD, "$cmd 2>&1|" or die "open: $!";
 #print LOG $_ while <CMD>;
 #close CMD;
 #close LOG;

 #open LOG, "modules.log" or die "can't open modules.log, $!\n";
 #while (<LOG>) {
 #  $modules = $_;
 #}
 #close LOG;

 #print "modules = $modules\n";



 # Try a build.
 my $base = get_system_cwd();

 #print "Configuring nspr ... \n";
 #chdir("$base/mozilla/nsprpub");
 #my $nspr_configure_cmd = "./configure";
 #system("$nspr_configure_cmd");

 print "Configuring ... \n";
 unlink("$base/mozilla/config.cache");
 chdir("$base/mozilla");
 my $configure_cmd = "./configure --enable-standalone-modules=xpcom";
 system("$configure_cmd");

 print "Building ... \n";
 system("gmake");
}
