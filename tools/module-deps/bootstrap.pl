#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
#  First attempt at using module-graph.pl to generate
#  a cvs checkout list, and building the resulting tree.
#

use strict;

# For --option1, --option2, ...
use Getopt::Long;
Getopt::Long::Configure("bundling_override");
Getopt::Long::Configure("auto_abbrev");

use Cwd;

my $debug = 1;

sub PrintUsage {
  die <<END_USAGE
  usage: $0 --modules=mod1,mod2,..
  (Assumes all.dot is in cwd, and you can check out a new cvs tree here)
END_USAGE
}

my $root_modules;  # modules we want to build.

sub parse_args() {
  PrintUsage() if $#ARGV < 0;

  # Stuff arguments into variables.
  # Print usage if we get an unknown argument.
  PrintUsage() if !GetOptions('modules=s' => \$root_modules);

  print "root_modules = $root_modules\n";
}

sub get_system_cwd {
  my $a = Cwd::getcwd()||`pwd`;
  chomp($a);
  return $a;
}


# Run shell command, return output string.
sub run_shell_command {
  my ($shell_command, $echo) = @_;
  local $_;
  
  my $status = 0;
  my $output = "";

  chomp($shell_command);

  print "cmd = $shell_command\n";

  open CMD, "$shell_command 2>&1|" or die "open: $!";

  while(<CMD>) {

    if($echo) {
      print $_;
    }
    chomp($_);
    $output .= "$_ ";
  }

  close CMD or $status = 1;

  if($status) {
    exit 1;  # bail.
  }

  return $output;
}



# main
{
  my $rv = 0;  # 0 = success.
  
  # Get options.
  parse_args();

  #
  # Assume all.dot is checked in somewhere.
  #
  unless (-e "all.dot") {
    print "No all.dot file found.  Get one by running\n  tools/module-deps/module-graph.pl . > all.dot\non built tree from the mozilla directory, or find one checked in somewhere.\n\n";
    PrintUsage();
  }


  # Pull core build stuff.
  print "\n\nPulling core build files...\n";
  
  # mozilla/allmakefiles.sh 
  my $core_build_files = "mozilla/client.mk mozilla/config mozilla/configure mozilla/configure.in mozilla/Makefile.in mozilla/allmakefiles.sh mozilla/build mozilla/include mozilla/tools/module-deps";
  $rv = run_shell_command("cvs co $core_build_files");
  

  # Pull nspr
  print "\n\nPulling nspr...\n";
  my $nspr_cvs_cmd = "cvs co -rNSPRPUB_PRE_4_2_CLIENT_BRANCH  mozilla/nsprpub";
  $rv = run_shell_command("$nspr_cvs_cmd");


  #
  # Pull modules.
  # Only one root/start module to start.
  #
  print "\n\nPulling modules...\n";


  # Figure out the modules list.
  my @modules;
  my $modules_string = "";
  my $num_modules = 0;
  my $modules_cmd = "mozilla/tools/module-deps/module-graph\.pl --file all\.dot --start-module $root_modules --list-only";
  $modules_string = run_shell_command($modules_cmd, 0);
  @modules = split(' ', $modules_string);
  $num_modules = $#modules + 1;
  print "modules = $num_modules\n";


  # Map modules list to directories list.
  my @dirs;
  my $dirs_string = "";
  my $dirs_cmd = "echo $modules_string | mozilla/config/module2dir\.pl --list-only";
  $dirs_string = run_shell_command($dirs_cmd, 0);
  #print "dirs_string = $dirs_string\n";


  # Checkout directories.
  my $dirs_cvs_cmd = "cvs co $dirs_string";
  run_shell_command($dirs_cvs_cmd);
  

  # Try a build.
  my $base = get_system_cwd();

  #
  # Construct a allmakefiles.sh file
  #
  


  #print "Configuring nspr ... \n";
  #chdir("$base/mozilla/nsprpub");
  #my $nspr_configure_cmd = "./configure";
  #system("$nspr_configure_cmd");

  print "Configuring ... \n";
  unlink("$base/mozilla/config.cache");
  chdir("$base/mozilla");
  my $configure_cmd = "./configure --enable-standalone-modules=$root_modules";
  $rv = run_shell_command("$configure_cmd");

  unless($rv) {
    print "Building ... \n";
    $rv = run_shell_command("gmake");
  } else {
    print "Error: skipping build.\n";
  }
}
