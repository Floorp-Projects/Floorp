#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
#  First attempt at using module-graph.pl to generate
#  a cvs checkout list, and building the resulting tree.
#
#  bootstrap.pl starts with an "all.dot" requires map file
#  and nothing else, and does the following:
#  * Figures out the module dependencies for the module
#    you want to build, via module-graph.pl.
#  * Creates a list of directories from the modules via
#    modules2dir.pl.
#  * Checks out core config files, nspr, and module directories.
#  * Based on this resulting tree, allmakefiles.sh is generated
#    on the fly with a module name "bootstrap".
#  * A build is attemped with this configure option:
#      --enable-standalone-modules=bootstrap
#
#  Bug: currently, the build descends into the nspr configure
#  and never comes back, bailing with the error
#  "configure: warning: Recreating autoconf.mk with updated nspr-config output"
#

use strict;
use File::Find();

# For --option1, --option2, ...
use Getopt::Long;
Getopt::Long::Configure("bundling_override");
Getopt::Long::Configure("auto_abbrev");

use Cwd;

my $debug = 1;

sub PrintUsage {
  die <<END_USAGE
  usage: $0 --modules=mod1,mod2,.. --skip-cvs
  (Assumes all.dot is in cwd, and you can check out a new cvs tree here)
END_USAGE
}

# Globals.
my $root_modules = 0;  # modules we want to build.
my $skip_cvs = 0;

sub parse_args() {
  PrintUsage() if $#ARGV < 0;

  # Stuff arguments into variables.
  # Print usage if we get an unknown argument.
  PrintUsage() if !GetOptions('modules=s' => \$root_modules,
                             'skip-cvs' => \$skip_cvs);
  if ($root_modules) {
    print "root_modules = $root_modules\n";

    # Test for last tree, or remember what tree we're doing.
    if (-e "last_module.txt") {
      open LAST_MODULE, "last_module.txt";

      while(<LAST_MODULE>) {
        # Assume module name is first line
        unless($_ eq $root_modules) {
          print "Error: Last module pulled ($_) doesn't match \"$root_modules\", remove last_module.txt and mozilla tree, then try again.\n";
          exit 1;
        } else {
          print "Checking out same module...\n";
        }
      }
      close LAST_MODULE;

    } else {
      # Save off file to remember which tree we're using    
      open LAST_MODULE, ">last_module.txt";
      print LAST_MODULE $root_modules;
      close LAST_MODULE;
    }
  }
  
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

# Global and Compare routine for Find()
my @foundMakefiles;

sub FindMakefiles
  {
	# Don't descend into CVS dirs.
	/CVS/ and $File::Find::prune = 1;
	
	if($_ eq "Makefile.in") {
	  #print "$File::Find::dir $_\n";
      #strip off the ".in"
	  $_ =~ s/.in//;
	  push(@foundMakefiles, "$File::Find::dir/$_");
	} else {
	  #print "  $File::Find::dir $_\n";
	}

    
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
  unless($skip_cvs) {
    print "\n\nPulling core build files...\n";
    my $core_build_files = "mozilla/client.mk mozilla/config mozilla/configure mozilla/configure.in mozilla/Makefile.in mozilla/build mozilla/include mozilla/tools/module-deps";
    $rv = run_shell_command("cvs co $core_build_files");
  }

  # Pull nspr
  unless($skip_cvs) {  
    print "\n\nPulling nspr...\n";
    my $nspr_cvs_cmd = "cvs co -rNSPRPUB_PRE_4_2_CLIENT_BRANCH  mozilla/nsprpub";
    $rv = run_shell_command("$nspr_cvs_cmd");
  }

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

  print "\nGenerating directories list...\n";
  $dirs_string = run_shell_command($dirs_cmd, 0);
  #print "dirs_string = $dirs_string\n";

  @dirs = split(' ', $dirs_string);     # Create dirs array for find command.

  # Checkout directories.
  unless($skip_cvs) { 
    print "\nChecking out directories...\n";
    my $dirs_cvs_cmd = "cvs co $dirs_string";
    run_shell_command($dirs_cvs_cmd);
  }

  # Try a build.
  my $base = get_system_cwd();

  #
  # Construct an allmakefiles.sh file
  #

  # Look for previously generated allmakefiles.sh file.
  my $generated_allmakefiles_file = 0;
  if(-e "mozilla/allmakefiles.sh") {
    open ALLMAKEFILES, "mozilla/allmakefiles.sh";
    while(<ALLMAKEFILES>) {
      if(/# Generated by bootstrap.pl/) {
         $generated_allmakefiles_file = 1;
       }
    }
    close ALLMAKEFILES;
    
    if($generated_allmakefiles_file == 0) {
      print "Error: non-generated mozilla/allmakefiles.sh found.\n";
      exit 1;
    } 
  }

  # Stomp on generated file, or open a new one.
  open ALLMAKEFILES, ">mozilla/allmakefiles.sh";
  
  # Add in stub allmakefiles.sh file.
  open STUB, "mozilla/tools/module-deps/allmakefiles.stub";
  while(<STUB>) {
    print ALLMAKEFILES $_;
  }
  close STUB;

  # Add in our hack
  print ALLMAKEFILES "MAKEFILES_bootstrap=\"\n";

  # Recursively decend the tree looking for Makefiles
  File::Find::find(\&FindMakefiles, @dirs);
  
  # Write Makefiles out to allmakefiles.sh.
  foreach (@foundMakefiles) {
    print ALLMAKEFILES "$_\n";
  }

  print ALLMAKEFILES "\"";
  close ALLMAKEFILES;

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
