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
#    on the fly with a module name "bootstrap".  modules.mk file
#    generated with DIRS in leaf-first order.
#  * A build is attempted with this configure option:
#      --enable-standalone-modules=bootstrap --disable-ldap --disable-tests
#
#  Example usage, to build xpcom:
#  
#    cvs co mozilla/tools/module-deps/bootstrap.pl
#    mozilla/tools/module-deps/bootstrap.pl --module=xpcom
#


use strict;
use English;
use File::Find();

# For --option1, --option2, ...
use Getopt::Long;
Getopt::Long::Configure("bundling_override");
Getopt::Long::Configure("auto_abbrev");

use Cwd;

my $debug = 1;

sub PrintUsage {
  die <<END_USAGE
  usage: $0 --modules=mod1,mod2,.. [--skip-cvs] [--skip-core-cvs]
  (Assumes you can check out a new cvs tree here)
END_USAGE
}

# Globals.
my $root_modules = 0;  # modules we want to build.
my $skip_cvs = 0;      # Skip all cvs checkouts.
my $skip_core_cvs = 0; # Only skip core cvs checkout, useful for hacking.

sub parse_args() {
  PrintUsage() if $#ARGV < 0;

  # Stuff arguments into variables.
  # Print usage if we get an unknown argument.
  PrintUsage() if !GetOptions('module=s' => \$root_modules,
                              'modules=s' => \$root_modules,
                              'skip-cvs' => \$skip_cvs,
                              'skip-core-cvs' => \$skip_core_cvs);
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

  open CMD, "$shell_command 2>&1|" or die "command failed: $!";

  while(<CMD>) {

    if($echo) {
      print $_;
    }
    chomp($_);
    $output .= "$_ ";
  }

  close CMD or $status = 1;

  if($status) {
    print "Warning, cmd exited with status $status.\n";
  }

  return $output;
}

# Global and Compare routines for Find()
my @foundMakefiles;

sub FindMakefiles {
  # Don't descend into CVS dirs.
  /CVS/ and $File::Find::prune = 1;
  
  if($_ eq "Makefile.in") {
    #print "$File::Find::dir $_\n";
    
    $_ =~ s/.in//;  # Strip off the ".in"
    
    $File::Find::dir =~ s/^mozilla\///;  # Strip off mozilla/
    
    #$_ =~ s/mozilla//;
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

  # Module map file is currently mozilla/tools/module-deps/all.dot

  # Pull core build stuff.
  unless($skip_cvs || $skip_core_cvs) {
    print "\n\nPulling core build files...\n";
    my $core_build_files = "mozilla/client.mk mozilla/config mozilla/configure mozilla/configure.in mozilla/aclocal.m4 mozilla/Makefile.in mozilla/build mozilla/include mozilla/tools/module-deps";
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
  
  #
  # Add in virtual dependencies from extra.dot
  # meta.dot = all.dot + extra.dot
  #

  # Create meta.dot
  if (-e "mozilla/tools/module-deps/meta.dot") {
    unlink("mozilla/tools/module-deps/meta.dot");
  }

  my $meta_cmd = "cp mozilla/tools/module-deps/all.dot  mozilla/tools/module-deps/meta.dot";
  print "$meta_cmd\n";
  system($meta_cmd);

  open METADOT, ">>mozilla/tools/module-deps/meta.dot";
  
  open EXTRADOT, "mozilla/tools/module-deps/extra.dot";
  while (<EXTRADOT>) {
    print METADOT $_;
  }
  close EXTRADOT;

  close METADOT;


  # Print out module dependency tree.
  print "\nDependency tree:\n";
  my $tree_cmd = "mozilla/tools/module-deps/module-graph\.pl --file mozilla/tools/module-deps/meta\.dot --start-module $root_modules --force-order mozilla/tools/module-deps/force_order\.txt --skip-dep-map --skip-list";
  system("$tree_cmd");
  print "\n";

  # Figure out the modules list.
  my @modules;
  my $modules_string = "";
  my $num_modules = 0;
  my $modules_cmd = "mozilla/tools/module-deps/module-graph\.pl --file mozilla/tools/module-deps/meta\.dot --start-module $root_modules --list-only --force-order mozilla/tools/module-deps/force_order\.txt";
  $modules_string = run_shell_command($modules_cmd, 0);

  # Yank modules we know we don't want.
  $modules_string =~ s/mozldap //; # no ldap.

  @modules = split(' ', $modules_string);
  $num_modules = $#modules + 1;
  print "modules = $num_modules\n";


  # Map modules list to directories list.
  my @dirs;
  my $dirs_string = "";
  my $dirs_string_no_mozilla = "";  # dirs_string, stripping off mozilla/
  
  my $dirs_cmd = "echo $modules_string | mozilla/config/module2dir\.pl --list-only";

  print "\nGenerating directories list...\n";
  $dirs_string = run_shell_command($dirs_cmd, 0);
  #print "dirs_string = $dirs_string\n";

  @dirs = split(' ', $dirs_string);     # Create dirs array for find command.

  $dirs_string_no_mozilla = $dirs_string;
  $dirs_string_no_mozilla =~ s/mozilla\/+//g;

  print "\ndirs_string_no_mozilla = $dirs_string_no_mozilla\n";

  # Checkout directories.
  unless($skip_cvs) { 
    print "\nChecking out directories...\n";
    my $dirs_cvs_cmd = "cvs co $dirs_string";
    run_shell_command($dirs_cvs_cmd);
  }

  # Try a build.
  my $basedir = get_system_cwd();

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
  print "\nGenerating allmakefiles.sh ...\n";
  my $allmakefiles_cmd = "cp mozilla/tools/module-deps/allmakefiles.stub mozilla/allmakefiles.sh";
  print "$allmakefiles_cmd\n";
  system("$allmakefiles_cmd");

  # Open copy of stub file.
  open ALLMAKEFILES, ">>mozilla/allmakefiles.sh";

  # Add in our hack
  print ALLMAKEFILES "MAKEFILES_bootstrap=\"\n";

  # Recursively decend the tree looking for Makefiles
  File::Find::find(\&FindMakefiles, @dirs);
  
  # Write Makefiles out to allmakefiles.sh.
  foreach (@foundMakefiles) {
    print ALLMAKEFILES "$_\n";
  }

  print ALLMAKEFILES "\"\n\n";
  print ALLMAKEFILES "add_makefiles \"\$MAKEFILES_bootstrap\"";
  close ALLMAKEFILES;

  #print "Configuring nspr ... \n";
  #chdir("$basedir/mozilla/nsprpub");
  #my $nspr_configure_cmd = "./configure";
  #system("$nspr_configure_cmd");

  print "\nConfiguring ... \n";
  unlink("$basedir/mozilla/config.cache");
  chdir("$basedir/mozilla");
  my $configure_cmd = "./configure --enable-standalone-modules=$root_modules --disable-ldap --disable-tests --disable-installer";
  $rv = run_shell_command("$configure_cmd");

  #
  # Construct a build/unix/modules.mk file, this let's us do
  # a top-level build instead of n $makecmd -C <dir> commands.
  #

  my $makecmd = "gmake";
  if( "$OSNAME" eq "cygwin" ) {
    # Cygwin likes to use "make" for win32.
    $makecmd = "make";
  }


  chdir("$basedir");

  # Look for previously generated modules.mk file.
  my $generated_modulesmk_file = 0;
  if(-e "mozilla/build/unix/modules.mk") {
    open MODULESMK, "mozilla/allmakefiles.sh";
    while(<MODULESMK>) {
      if(/# Generated by bootstrap.pl/) {
         $generated_modulesmk_file = 1;
       }
    }
    close MODULESMK;
    
    if($generated_modulesmk_file == 0) {
      print "Error: non-generated mozilla/build/unix/modules.mk found.\n";
      exit 1;
    } 
  }

  # Stomp on generated file, or open a new one.
  print "\nGenerating modules.mk ...\n";
  my $modulesmk_cmd = "cp mozilla/tools/module-deps/modules.mk.stub mozilla/build/unix/modules.mk";
  print "$modulesmk_cmd\n";
  system("$modulesmk_cmd");

  # Open copy of stub file.
  open MODULESMK, ">>mozilla/build/unix/modules.mk";

  # Add in our hack
  print MODULESMK "BUILD_MODULE_DIRS := config build include $dirs_string_no_mozilla\n";

  close MODULESMK;

  # Now try and build.
  # Not a top-level build, but build each directory.
  print "\nBuilding ... \n";

  print "basedir = $basedir\n";
  chdir($basedir);
  my $dir;

  #
  # Inherent build dependencies, we need to build some inital
  # tools & directories first before attacking the modules.
  #

  # Build nspr
  print "$makecmd -C mozilla/nsprpub\n";
  system("$makecmd -C mozilla/nsprpub");

  # Build config
  print "$makecmd -C mozilla/config\n";
  system("$makecmd -C mozilla/config");

  # Build xpidl
  print "$makecmd -C mozilla/xpcom/typelib\n";
  system("$makecmd -C mozilla/xpcom/typelib");

  # Now try the modules.

  chdir("$basedir/mozilla");

  # Export-phase first.  Export IDL stuff first to avoid IDL order problems.
  system("$makecmd export-idl"); # workaround for idl order problems.
  system("$makecmd export");     # run_shell_command("$makecmd export");

  # Libs-phase next.
  system("$makecmd libs");  # run_shell_command("$makecmd libs");
}
