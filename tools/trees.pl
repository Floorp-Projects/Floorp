#!/usr/bin/env perl

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is this file as it was released upon February 25, 1999.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Stephen Lamm <slamm@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# trees.pl - Navigate mozilla source trees.
#            Depends on mozconfig.sh to find object directory.
#            Use http://cvs-mirror.mozilla.org/webtools/build/config.cgi
#            to create a mozconfig.sh file.
#
# Be sure to define a ~/.moztrees file (Run 'trees.pl -c' for an example).
#

require 'getopts.pl';

sub usage
{
  my ($progname) = $0 =~ /([^\/]+)$/;
  die "Usage: $progname [options] [<tree_name>] [<dir> <dir>...]
   Getting Started:
      1. trees.pl -e > ~/.moztrees
      2. Edit ~/.moztrees for your setup.
      3. trees.pl -t (tcsh users), or trees.pl -b (bash users)
      4. Save output of step #3 into ~/.cshrc or ~/.bash_profile
   Options:
      -l          List the available trees.
      -o          Move to the object directory.
      -s          Move to the source directory.
      -h          Print this usage message.
    <tree_name>   Directory name below mozilla (e.g. gecko/mozilla)
                  You only need to specify tree name when switching trees.
    <dir>         An alias or sub directory list.
                  Use '/' to move to the root of the tree.
   Examples (with 'trees.pl' wrapped in 'moz' alias or function):
     moz gecko cmd/xfe    - cd to mozilla/cmd/xfe in gecko tree
     moz editor public    - cd to mozilla/editor/public
     moz /                - cd to mozilla/
     moz gecko2           - cd to tree 'gecko2' (subdir is preserved)
";
}

&usage if !&Getopts('behlost');
&usage if $opt_h;

# Options for Getting Started
#
if ($opt_e)
{
  print while <DATA>;
  exit;
}
if ($opt_b)
{
  # Print function for bash users
  print q(function moz()
{
  set -- `trees.pl $@`
  if [ $# -eq 1 ]; then
    cd $1
  fi
}
);
  exit;
}
if ($opt_t)
{
  # Print alias for tcsh users
  print q(alias moz 'set _moz=`trees.pl \!*`; if ("$_moz" != "") cd $_moz; endif; unset _moz'
);
  exit;
}
######################################################################
# begin defines

$root_dir = "mozilla";
$test_file = "client.mk";  # Some file to verify the existence of a tree.

if (not -f "$ENV{HOME}/.moztrees")
{
  die "No ~/.moztrees file. Run 'trees.pl -e' for example, or 'trees.pl -h' for help.\n";
}
push @INC, $ENV{HOME};
require '.moztrees';

# end defines
######################################################################
# begin main

# Option '-l': Print a list of trees.
#
if ($opt_l)
{
  &print_tree_list(@tree_parent_dirs);
  exit;
}

# Build regex pattern to match root directory.
$root_pat = "(?:\Q$root_dir\E)";

# Make 'moz /' goto '$tree_dir/mozilla'.
$dir_aliases{'/'} = '/';

# Get the current directory.
chop($pwd = `pwd`) unless defined($pwd = $ENV{PWD});


# Check if current directory to is a tree path.
#
($tree_dir, $tree_subdir, $topsrcdir, $objdir) = &parse_tree_path($pwd);

# Check for tree name as first argument.
#
if ($#ARGV >= 0)
{
  $dir = &tree_lookup($ARGV[0]);
  if ($dir ne '')
  {
    $tree_dir = $dir;
    shift @ARGV;
} }

if ($tree_dir eq '')
{
  warn "No tree found.\n";
  &usage;
}

# Move between source and object directories
# 
if ($opt_s) 
{
  $tree_dir = $topsrcdir;
} 
if ($opt_o)
{
  $objdir = find_objdir($tree_dir) if $objdir eq '';

  $tree_dir = $objdir;
}

# Finally, check for directories
#
if ($#ARGV >= 0) 
{
  ($tree_dir, $tree_subdir) = &dir_lookup($tree_dir, $tree_subdir, @ARGV);
}

$tree_dir =~ s|^(\S):|//$1|;  # For bash on Windows, change "C:" to "//C"

print "$tree_dir/$tree_subdir";

# end main
######################################################################

sub print_tree_list
{
  # Build a list of the trees
  foreach $parent_dir (@tree_parent_dirs)
  {
    opendir(DIRHANDLE, $parent_dir) || next;
    
    foreach $entry (readdir(DIRHANDLE))
    {
      next if $entry eq '.' || $entry eq '..' || $entry eq 'CVS';

      if (-d "$parent_dir/$entry/$root_dir")
      {
          $trees{$entry} = "$parent_dir/$entry/$root_dir";
          last; # Skip the inner loop
  } } }

  @treelist = ();
  $maxlen = 0;
  my ($tree_name, $tree_dir, $max_name_length);
  while (($tree_name, $tree_dir) = each %trees)
  {
    if ($max_name_length < length($tree_name))
    {
      $max_name_length = length($tree_name);
    }
    push (@treelist, "$tree_dir=$tree_name");
  }
  foreach (sort @treelist)
  {
    ($dir, $name) = split('=');
    print stderr sprintf("%-${max_name_length}s   %s\n", $name, $dir);
} }

# Check a path to see if it is in a source tree.
# Parses the path and returns the tree root and subdir.
#
sub parse_tree_path
{
  my ($pwd) = @_;
  my ($tree_dir) = '';
  my ($tree_subdir)  = '';

  # Adjust cygwin path to one perl understands
  $pwd =~ s@^//(\S)/@$1:/@;

  if ($pwd =~ m@^(.*/$root_pat)(/.*)?$@ and -e "$1/$test_file")
  {
    $topsrcdir = $1;
    $tree_dir = $topsrcdir;
    $tree_subdir = $2;
    $objdir = find_objdir($topsrcdir);
    if ($pwd =~ m@^$objdir(/.*)?$@) 
    {
	$tree_dir = $objdir;
	$tree_subdir = $1;
    }
  } elsif (-d "$pwd/$root_dir" and -e "$pwd/$root_dir/$test_file")
  {
    $tree_dir = "$pwd/$root_dir";
    $tree_subdir = "..";
  } elsif ($pwd =~ m@^((.*)/obj-[^/]+)(/.*)?$@)
  {
    if (-d "$2/$root_dir")
    {
      $topsrcdir = $1;
      $objdir = $2;
      $tree_dir = $objdir;
      $tree_subdir = $3;
  } }

  $tree_subdir = '/' if $tree_subdir eq '';

  return ($tree_dir, $tree_subdir, $topsrcdir, $objdir);
}

# Check a name against the possible list of trees
#
sub tree_lookup
{
  my $arg = $_[0];
  my ($dir, $root);

  return $trees{$arg} if defined($trees{$arg});
  
  foreach $parent_dir (@tree_parent_dirs)
  {
    return "$parent_dir/$arg/$root_dir" if -d "$parent_dir/$arg/$root_dir";
  }
  return '';
}

sub dir_lookup
{
  my ($tree_dir, $tree_subdir, $arg, @args) = @_;

  return $tree_subdir if not $arg;

  if (defined($dir_aliases{$arg}))
  {
    ($dir1, $dir2) = split(/:/, $dir_aliases{$arg});
    if ($dir1 eq 'objdir')
    {
      $objdir = find_objdir($tree_dir) if $objdir eq '';
      $tree_dir = $objdir;
      $tree_subdir = $dir2;
    } elsif ($dir1 eq 'topsrcdir')
    {
      $tree_dir = $topsrcdir;
      $tree_subdir = $dir2;
    } else
    {
      $tree_subdir = $dir1;
    }
  }
  else 
  {
    # Check for subdirectory of 'search' directory
    my ($found_dir) = 0;
    foreach $dir ('',@search_dirs,$tree_subdir) 
    {
      if (-d "$tree_dir/$dir/$arg") 
      {
        $tree_subdir = "$dir/$arg";
        $found_dir = 1;
        last;
    } }
    die "Directory not found: $arg\n" if !$found_dir;
  }    
  foreach $arg (@args)
  {
    # Subdirectory of current path
    if (-d "$tree_dir/$tree_subdir/$arg")
    {
      $tree_subdir = "$tree_subdir/$arg";
    }
    else
    {
      die "Directory not found: $tree_dir/$tree_subdir/$arg\n";
  } }
  $tree_subdir =~ s@^/+(.+)@$1@;
  return ($tree_dir, $tree_subdir);
}


# The only way to find the object directory is
# to find it in '.mozconfig'.
sub find_objdir {
  my $topsrcdir = $_[0];
  my $objdir    = '';
  my $home      = $ENV{HOME};
  my $mozconfig = '';

  foreach $config ("$ENV{MOZCONFIG}","$ENV{MOZ_MYCONFIG}",
		 "$topsrcdir/.mozconfig",
		 "$topsrcdir/mozconfig", "topsrcdir/mozconfig.sh",
		 "$home/.mozconfig","$home/.mozconfig.sh") {
    next if $config eq '';
    if (-f $config) {
      $mozconfig = $config;
      last;
  } }
  $objdir_script = qq{
    # sh
    MOZCONFIG=$mozconfig
    TOPSRCDIR=$topsrcdir
  }.q{
    ac_add_options() { :; }
    mk_add_options() {
      for _opt
      do
	case $_opt in
	  MOZ_OBJDIR=*) _objdir=`expr "$_opt" : "MOZ_OBJDIR=\(.*\)"` ;;
        esac
      done
    }
    . $MOZCONFIG
    echo $_objdir
  };

  if ($mozconfig ne '') {
    $objdir = `$objdir_script`;
    chomp($objdir);  
  }

  if ($objdir eq '')
  {
    $objdir = $topsrcdir;
  } else
  {
    $objdir =~ s/\@TOPSRCDIR\@/$topsrcdir/;
    if ($objdir =~ /\@CONFIG_GUESS\@/)
    {
      if (not defined($config_guess = $ENV{MOZ_CONFIG_GUESS})) {
	$config_guess = `$topsrcdir/build/autoconf/config.guess`;
	chomp($config_guess);
      }
      $objdir =~ s/\@CONFIG_GUESS\@/$config_guess/;
  } }
  return $objdir;
}

# end 'trees'
######################################################################
__END__
#! perl
# .moztrees - Defines the following perl variables for 'trees.pl' script,
#
#     @tree_parent_dirs   - Where to look for source trees.
#     %dir_aliases        - Aliases for common paths.
#     @search_dirs        - Additional directories to search for names.
#

# @tree_parent_dirs - Where to look for source trees.
#
#   The script will look for
#
#       @tree_parent_dirs/<tree_name>/mozilla
#
#   where <tree_name> is any directory you put in <tree_parent_dirs>.
#
#   For example, if you have the following source trees,
#
#       /h/dance/export/slamm/raptor/mozilla
#       /h/dance/export/slamm/raptor2/mozilla
#
#   then you would add to the list,
#
#       /h/dance/export/slamm    (Note: two directories stripped).
#
#   and you would use 'raptor' and 'raptor2' as the names of your trees,
#
#       moz raptor2 webshell
#  
@tree_parent_dirs =
(
	'/export/slamm',
        '/h/worms/export3/slamm'
);

# %dir_aliases - Directory aliases
#
#    %dir_aliases = ('alias', 'path' [,'alias', 'path'...]);
#
#    Possible 'path' settings:
#                  <path_relative_to_mozilla>
#        topsrcdir:<path_relative_to_topsrcdir>
#           objdir:<path_relative_to_objdir>
#
%dir_aliases = 
(
        'bin',    'objdir:dist/bin',
        'viewer', 'webshell/tests/viewer'
);

# @search_dirs - 'moz' will search these directories if no directory
#                 aliases match.
#
#   For example, if 'modules' is listed, then
#
#     moz libpref
#
#   would take you to mozilla/modules/libpref.
#
# (Give paths relative to mozilla.)
@search_dirs =
(
	'lib', 
	'modules'
);

# Need to end with a true value
1;
