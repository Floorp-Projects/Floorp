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
use File::Copy;

# For --option1, --option2, ...
use Getopt::Long;
Getopt::Long::Configure("bundling_override");
Getopt::Long::Configure("auto_abbrev");

use Cwd;

my $debug = 0;

sub PrintUsage {
  die <<END_USAGE
  usage: $0 --modules=mod1,mod2,.. [--topsrcdir topsrcdir] [--skip-cvs] [--skip-core-cvs] [--module-file-only]
  (Assumes you can check out a new cvs tree here)
END_USAGE
}

# Globals.
my $root_modules = 0;  # modules we want to build.
my $skip_cvs = 0;      # Skip all cvs checkouts.
my $skip_core_cvs = 0; # Only skip core cvs checkout, useful for hacking.
my $modfile_only = 0; # Only generate module.mk
my $topsrcdir = 0;
my $toolsdir;

sub get_system_cwd {
  my $a = Cwd::getcwd()||`pwd`;
  chomp($a);
  return $a;
}

sub parse_args() {
  PrintUsage() if $#ARGV < 0;

  # Stuff arguments into variables.
  # Print usage if we get an unknown argument.
  PrintUsage() if !GetOptions('module=s' => \$root_modules,
                              'modules=s' => \$root_modules,
                              'skip-cvs' => \$skip_cvs,
                              'skip-core-cvs' => \$skip_core_cvs,
                              'module-file-only' => \$modfile_only,
                              'topsrcdir=s' => \$topsrcdir);
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
  $topsrcdir = "mozilla" if (!$topsrcdir);
  chdir($topsrcdir) || die("chdir($topsrcdir): $!\n");
  $topsrcdir =  get_system_cwd();
  $toolsdir = "$topsrcdir/tools/module-deps";
}



# Run shell command, return output string.
sub run_shell_command {
  my ($shell_command, $echo) = @_;
  local $_;
  
  my $status = 0;
  my $output = "";

  chomp($shell_command);

  print "cmd = $shell_command\n" if ($debug);

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

# Return directory list for module
sub get_dirs_for_module($) {

  my ($root_modules, $silence) = @_;
  # Figure out the modules list.
  my @modules;
  my $modules_string = "";
  my $num_modules = 0;
  my $modules_cmd = "$toolsdir/module-graph\.pl --file $toolsdir/meta\.dot --start-module $root_modules --list-only --force-order $toolsdir/force_order\.txt";
  $modules_string = run_shell_command($modules_cmd, 0);
  $silence = 0 if (!defined($silence));

  @modules = split(' ', $modules_string);
  #$num_modules = $#modules + 1;
  print "modules = $num_modules\n" if (!$silence);

  # Map modules list to directories list.
  my @dirs;
  my $dirs_string = "";
  my $dirs_string_no_mozilla = "";  # dirs_string, stripping off mozilla/
  
  my $dirs_cmd = "echo $modules_string | $topsrcdir/config/module2dir\.pl --list-only";

  print "\nGenerating directories list for $root_modules\n" if (!$silence);
  $dirs_string = run_shell_command($dirs_cmd, 0);
  print "dirs_string = $dirs_string\n"  if ($debug);

  return $dirs_string;
}


# Create stub version of modules.mk
sub create_stub_modules_mk() {  

    # Look for previously generated modules.mk file.
    my $generated_modulesmk_file = 0;
    if(-e "$topsrcdir/build/unix/modules.mk" && !$modfile_only) {
        open MODULESMK, "$topsrcdir/build/unix/modules.mk";
        while(<MODULESMK>) {
            if(/# Generated by bootstrap.pl/) {
               $generated_modulesmk_file = 1;
           }
        }
        close MODULESMK;
        
        if($generated_modulesmk_file == 0) {
            print "Error: non-generated $topsrcdir/build/unix/modules.mk found.\n";
            exit 1;
        } 
    }
    
    # Stomp on generated file, or open a new one.
    print "\nGenerating stub modules.mk ...\n";
    copy("$toolsdir/modules.mk.stub", "$topsrcdir/build/unix/modules.mk");
}

sub add_modules_mk_footer() {
    print "Adding modules.mk footer\n";
    open(FOOTER, "$toolsdir/modules.mk.footer") || die ("modules.mk.footer:  $!\n");
    open(MK, ">>$topsrcdir/build/unix/modules.mk") || die ("modules.mk: $!\n");
    while (<FOOTER>) {
        print MK $_;
    }
    close(MK);
    close(FOOTER);
}

# Generate modules.mk with all modules
sub create_full_modules_mk ($)  {

    my ($basedir) =  @_;
    my ($cmd, @modlist, $modules_string, $module, @dirlist, $tmpmod);
    my $tooldir = "$topsrcdir/tools/module-deps";
    my %module_dirs;

    # Get the list of modules
    $cmd = "$tooldir/module-graph.pl --file $tooldir/meta.dot --force-order $tooldir/force_order.txt --list-only --start-module ALL";
    $modules_string = run_shell_command($cmd, 0);
    @modlist = sort(split(/\s+/, $modules_string));
    print "There are $#modlist modules\n";

    # Get directory list of each module
    print "Generating directory lists for all modules\n";
    foreach $module (@modlist)  {
        $module_dirs{$module} = &get_dirs_for_module($module, 1);
        $module_dirs{$module} =~ s/mozilla\///g;
#        print  "$module: $module_dirs{$module}\n";
    }

    &create_stub_modules_mk();

    # Add new module entries to modules.mk
    print "Adding module entries to modules.mk\n";
    open(MK, ">>$topsrcdir/build/unix/modules.mk") || die ("modules.mk: $!\n");
    print MK "# Available modules are:\n";
    print MK "# @modlist\n";
    foreach $module (@modlist) {
        print MK "\n";
        print MK "BM_DIRS_$module\t= $module_dirs{$module}\n";
        print MK "BM_CVS_$module\t= $module_dirs{$module}\n";
        print MK "\n";
    }
    close(MK);

    &add_modules_mk_footer;

    # Re-write allmakefiles.sh
    print "Rewritting allmakefiles.sh\n";
    open(ALLMK, "$topsrcdir/allmakefiles.sh") || die("allmakefiles.sh: $!\n");
    open(NEWMK, ">$topsrcdir/allmakefiles.sh.new") || die("allmakefile.sh.new: $!\n");
    while (<ALLMK>) {
        if (/^# Standalone modules go here/) {
            print NEWMK $_;
            print NEWMK "\n";
            foreach $module (@modlist) {
                ($tmpmod = $module) =~ s/\-/_/g;
                @dirlist = map "$_/Makefile", split(/\s+/,$module_dirs{$module});
                print NEWMK "MAKEFILES_$tmpmod=\"@dirlist\"\n\n";
            }
            print NEWMK "    for mod in \$BUILD_MODULES; do\n";
            print NEWMK "        case \$mod in\n";
            foreach $module (@modlist) {
                ($tmpmod = $module) =~ s/\-/_/g;
                print NEWMK "        $module\) add_makefiles \"\$MAKEFILES_$tmpmod\" ;;\n";
            }
            print NEWMK "        esac\n";
            print NEWMK "    done\n";
            print NEWMK "\nfi\n";
            last;
        } else {
            print NEWMK $_;
        }
    }
    close(NEWMK);
    close(ALLMK);
    move("$topsrcdir/allmakefiles.sh.new", "$topsrcdir/allmakefiles.sh") || 
        die("move failed for $topsrcdir/allmakefiles.sh\n");
    exit(0);
}

# main
{
  my $rv = 0;  # 0 = success.
  
  # Get options.
  parse_args();

  # Module map file is currently $toolsdir/all.dot

  chdir("$topsrcdir/..");
  my $basedir = get_system_cwd();
  print "basedir = $basedir\n";

  # Pull core build stuff.
  unless($skip_cvs || $skip_core_cvs || $modfile_only) {
    print "\n\nPulling core build files...\n";
    my $core_build_files = "mozilla/client.mk mozilla/config mozilla/configure mozilla/configure.in mozilla/aclocal.m4 mozilla/Makefile.in mozilla/build mozilla/include mozilla/tools/module-deps";
    $rv = run_shell_command("cvs co $core_build_files");
  }

  # Pull nspr
  unless($skip_cvs || $modfile_only) {  
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
  if (-e "$toolsdir/meta.dot") {
    unlink("$toolsdir/meta.dot");
  }

  print "Creating meta.dot\n";
  copy("$toolsdir/all.dot", "$toolsdir/meta.dot");

  open METADOT, ">>$toolsdir/meta.dot";
  
  open EXTRADOT, "$toolsdir/extra.dot";
  while (<EXTRADOT>) {
    print METADOT $_;
  }
  close EXTRADOT;

  close METADOT;

  # If we only want the updated modules.mk, turn left at Albuquerque
  if ($modfile_only) {
      &create_full_modules_mk();
      exit(0);
  }

  # Print out module dependency tree.
  print "\nDependency tree:\n";
  my $tree_cmd = "$toolsdir/module-graph\.pl --file $toolsdir/meta\.dot --start-module $root_modules --force-order $toolsdir/force_order\.txt --skip-dep-map --skip-list";
  system("$tree_cmd");
  print "\n";

  my @dirs;
  my $dirs_string;
  my $dirs_string_no_mozilla;

  $dirs_string = get_dirs_for_module($root_modules);

  # Yank modules we know we don't want.
  #$modules_string =~ s/mozldap //; # no ldap.

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

  #
  # Construct an allmakefiles.sh file
  #

  # Look for previously generated allmakefiles.sh file.
  my $generated_allmakefiles_file = 0;
  if(-e "$topsrcdir/allmakefiles.sh") {
    open ALLMAKEFILES, "$topsrcdir/allmakefiles.sh";
    while(<ALLMAKEFILES>) {
      if(/# Generated by bootstrap.pl/) {
         $generated_allmakefiles_file = 1;
       }
    }
    close ALLMAKEFILES;
    
    if($generated_allmakefiles_file == 0) {
      print "Error: non-generated $topsrcdir/allmakefiles.sh found.\n";
      exit 1;
    } 
  }

  # Stomp on generated file, or open a new one.
  print "\nGenerating allmakefiles.sh ...\n";
  copy("$toolsdir/allmakefiles.stub", "$topsrcdir/allmakefiles.sh");

  # Open copy of stub file.
  open ALLMAKEFILES, ">>$topsrcdir/allmakefiles.sh";

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
  #my $nspr_configure_cmd = "sh ./configure";
  #system("$nspr_configure_cmd");

  print "\nConfiguring ... \n";
  unlink("$basedir/mozilla/config.cache");
  chdir("$topsrcdir");
  my $configure_cmd = "sh ./configure --enable-standalone-modules=$root_modules --disable-ldap --disable-tests --disable-installer";
  $rv = run_shell_command("$configure_cmd");

  #
  # Construct a build/unix/modules.mk file, this let's us do
  # a top-level build instead of n $makecmd -C <dir> commands.
  #

  &create_stub_modules_mk();

  # Open copy of custom modules.mk.
  open MODULESMK, ">>$topsrcdir/build/unix/modules.mk";
  
  # Add in our hack
  print MODULESMK "BUILD_MODULE_DIRS +=  $dirs_string_no_mozilla\n";
      
  close MODULESMK;

  &add_modules_mk_footer;

  my $makecmd = "gmake";
  if( "$OSNAME" eq "cygwin" ) {
    # Cygwin likes to use "make" for win32.
    $makecmd = "make";
  }

  # Now try and build.
  # Not a top-level build, but build each directory.
  print "\nBuilding ... \n";

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

  chdir("$topsrcdir");

  # Export-phase first.  Export IDL stuff first to avoid IDL order problems.
  system("$makecmd export-idl"); # workaround for idl order problems.
  system("$makecmd export");     # run_shell_command("$makecmd export");

  # Libs-phase next.
  system("$makecmd libs");  # run_shell_command("$makecmd libs");
}
