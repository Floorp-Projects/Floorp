#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#                 Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Dave Miller <justdave@syndicomm.com>
#                 Zach Lipton  <zach@zachlipton.com>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Tobias Burnus <burnus@net-b.de>
#                 Gervase Markham <gerv@gerv.net>
#                 Erik Stambaugh <erik@dasbistro.com>
#
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>
#
#
#
# Hey, what's this?
#
# 'checksetup.pl' is a script that is supposed to run during installation
# time and also after every upgrade.
#
# The goal of this script is to make the installation even more easy.
# It does so by doing things for you as well as testing for problems
# early.
#
# And you can re-run it whenever you want. Especially after Bugzilla
# gets updated you SHOULD rerun it. Because then it may update your
# SQL table definitions so that they are again in sync with the code.
#
# So, currently this module does:
#
#     - check for required perl modules
#     - set defaults for local configuration variables
#     - create and populate the data directory after installation
#     - set the proper rights for the *.cgi, *.html ... etc files
#     - check if the code can access MySQL
#     - creates the database 'bugs' if the database does not exist
#     - creates the tables inside the database if they don't exist
#     - automatically changes the table definitions of older BugZilla
#       installations
#     - populates the groups
#     - put the first user into all groups so that the system can
#       be administrated
#     - changes already existing SQL tables if you change your local
#       settings, e.g. when you add a new platform
#
# People that install this module locally are not supposed to modify
# this script. This is done by shifting the user settable stuff into
# a local configuration file 'localconfig'. When this file get's
# changed and 'checkconfig.pl' will be re-run, then the user changes
# will be reflected back into the database.
#
# Developers however have to modify this file at various places. To
# make this easier, I have added some special comments that one can
# search for.
#
#     To                                               Search for
#
#     add/delete local configuration variables         --LOCAL--
#     check for more prerequired modules               --MODULES--
#     change the defaults for local configuration vars --LOCAL--
#     update the assigned file permissions             --CHMOD--
#     add more MySQL-related checks                    --MYSQL--
#     change table definitions                         --TABLE--
#     add more groups                                  --GROUPS--
#     create initial administrator account             --ADMIN--
#
# Note: sometimes those special comments occur more then once. For
# example, --LOCAL-- is at least 3 times in this code!  --TABLE--
# also is used more than once. So search for every occurence!
#
# To operate checksetup non-interactively, run it with a single argument
# specifying a filename with the information usually obtained by
# prompting the user or by editing localconfig. Only information
# superceding defaults from LocalVar() function calls needs to be
# specified.
#
# The format of that file is....
#
# $answer{'db_host'} = '$db_host = "localhost";
# $db_port = 3306;
# $db_name = "mydbname";
# $db_user = "mydbuser";';
#
# $answer{'db_pass'} = q[$db_pass = 'mydbpass';];
#
# $answer{'ADMIN_OK'} = 'Y';
# $answer{'ADMIN_EMAIL'} = 'myadmin@mydomain.net';
# $answer{'ADMIN_PASSWORD'} = 'fooey';
# $answer{'ADMIN_REALNAME'} = 'Joel Peshkin';
#
#

use strict;
use lib ".";

use vars qw( $db_name %answer );
use Bugzilla::Constants;

my $silent;

# The use of some Bugzilla modules brings in modules we need to test for
# Check first, via BEGIN
BEGIN {

    # However, don't run under -c (because of tests)
    if (!$^C) {

###########################################################################
# Non-interactive override. Pass a filename on the command line which is
# a Perl script. This script defines a %answer hash whose names are tags
# and whose values are answers to all the questions checksetup.pl asks. 
# Grep this file for references to that hash to see the tags to use for the 
# possible answers. One example is ADMIN_EMAIL.
###########################################################################
if ($ARGV[0] && ($ARGV[0] !~ /^--/)) {
    do $ARGV[0] 
        or ($@ && die("Error $@ processing $ARGV[0]"))
        or die("Error $! processing $ARGV[0]");
    $silent = !grep(/^--no-silent$/, @ARGV);
}

###########################################################################
# Check required module
###########################################################################

#
# Here we check for --MODULES--
#

print "\nChecking perl modules ...\n" unless $silent;
unless ($] >= 5.006) {
    die "Sorry, you need at least Perl 5.6\n";
}

# vers_cmp is adapted from Sort::Versions 1.3 1996/07/11 13:37:00 kjahds,
# which is not included with Perl by default, hence the need to copy it here.
# Seems silly to require it when this is the only place we need it...
sub vers_cmp {
  if (@_ < 2) { die "not enough parameters for vers_cmp" }
  if (@_ > 2) { die "too many parameters for vers_cmp" }
  my ($a, $b) = @_;
  my (@A) = ($a =~ /(\.|\d+|[^\.\d]+)/g);
  my (@B) = ($b =~ /(\.|\d+|[^\.\d]+)/g);
  my ($A,$B);
  while (@A and @B) {
    $A = shift @A;
    $B = shift @B;
    if ($A eq "." and $B eq ".") {
      next;
    } elsif ( $A eq "." ) {
      return -1;
    } elsif ( $B eq "." ) {
      return 1;
    } elsif ($A =~ /^\d+$/ and $B =~ /^\d+$/) {
      return $A <=> $B if $A <=> $B;
    } else {
      $A = uc $A;
      $B = uc $B;
      return $A cmp $B if $A cmp $B;
    }
  }
  @A <=> @B;
}

# This was originally clipped from the libnet Makefile.PL, adapted here to
# use the above vers_cmp routine for accurate version checking.
sub have_vers {
  my ($pkg, $wanted) = @_;
  my ($msg, $vnum, $vstr);
  no strict 'refs';
  printf("Checking for %15s %-9s ", $pkg, !$wanted?'(any)':"(v$wanted)") unless $silent;

  # Modules may change $SIG{__DIE__} and $SIG{__WARN__}, so localise them here
  # so that later errors display 'normally'
  local $::SIG{__DIE__};
  local $::SIG{__WARN__};

  eval "require $pkg;";

  # do this twice to avoid a "used only once" error for these vars
  $vnum = ${"${pkg}::VERSION"} || ${"${pkg}::Version"} || 0;
  $vnum = ${"${pkg}::VERSION"} || ${"${pkg}::Version"} || 0;
  $vnum = -1 if $@;

  # CGI's versioning scheme went 2.75, 2.751, 2.752, 2.753, 2.76
  # That breaks the standard version tests, so we need to manually correct
  # the version
  if ($pkg eq 'CGI' && $vnum =~ /(2\.7\d)(\d+)/) {
      $vnum = $1 . "." . $2;
  }

  if ($vnum eq "-1") { # string compare just in case it's non-numeric
    $vstr = "not found";
  }
  elsif (vers_cmp($vnum,"0") > -1) {
    $vstr = "found v$vnum";
  }
  else {
    $vstr = "found unknown version";
  }

  my $vok = (vers_cmp($vnum,$wanted) > -1);
  print ((($vok) ? "ok: " : " "), "$vstr\n") unless $silent;
  return $vok;
}

# Check versions of dependencies.  0 for version = any version acceptible
my $modules = [ 
    { 
        name => 'AppConfig',  
        version => '1.52' 
    }, 
    { 
        name => 'CGI', 
        version => '2.93' 
    }, 
    {
        name => 'Data::Dumper', 
        version => '0' 
    }, 
    {        
        name => 'Date::Format', 
        version => '2.21' 
    }, 
    { 
        name => 'DBI', 
        version => '1.36' 
    }, 
    { 
        name => 'DBD::mysql', 
        version => '2.1010' 
    }, 
    { 
        name => 'File::Spec', 
        version => '0.82' 
    }, 
    {
        name => 'File::Temp',
        version => '0'
    },
    { 
        name => 'Template', 
        version => '2.08' 
    }, 
    { 
        name => 'Text::Wrap', 
        version => '2001.0131' 
    } 
];

my %ppm_modules = (
    'AppConfig'         => 'AppConfig',
    'Chart::Base'       => 'Chart',
    'CGI'               => 'CGI',
    'Data::Dumper'      => 'Data-Dumper',
    'Date::Format'      => 'TimeDate',
    'DBI'               => 'DBI',
    'DBD::mysql'        => 'DBD-mysql',
    'Template'          => 'Template-Toolkit',
    'PatchReader'       => 'PatchReader',
    'GD'                => 'GD',
    'GD::Graph'         => 'GDGraph',
    'GD::Text::Align'   => 'GDTextUtil',
);

sub install_command {
    my $module = shift;
    if ($^O =~ /MSWin32/i) {
        return "ppm install " . $ppm_modules{$module} if exists $ppm_modules{$module};
        return "ppm install " . $module;
    } else {
        return "$^X -MCPAN -e 'install \"$module\"'";
    }    
}

my %missing = ();

foreach my $module (@{$modules}) {
    unless (have_vers($module->{name}, $module->{version})) { 
        $missing{$module->{name}} = $module->{version};
    }
}

print "\nThe following Perl modules are optional:\n" unless $silent;
my $gd          = have_vers("GD","1.20");
my $chartbase   = have_vers("Chart::Base","1.0");
my $xmlparser   = have_vers("XML::Parser",0);
my $gdgraph     = have_vers("GD::Graph",0);
my $gdtextalign = have_vers("GD::Text::Align",0);
my $patchreader = have_vers("PatchReader","0.9.4");

print "\n" unless $silent;

if ($^O =~ /MSWin32/i && !$silent) {
    if ($^V lt pack('U*',5,8,0)) {
        # If something older than 5.8 is detected, then point to the oi
        # repository using ppm version 2 commands (which is the default
        # for Perl 5.6)

        print "The required ActivePerl modules are available at OpenInteract's ppm repository.\n";
        print "You can add the repository with the following command:\n";
        print "    ppm set rep oi http://openinteract.sourceforge.net/ppmpackages/\n";
        print "You can then save the changes by typing:\n";
        print "    ppm set save\n\n";
    } else {
        # For Perl 5.8 or higher, point the user to a 5.8 repository,
        # using ppm version 3 commands (which is the default for Perl 5.8)

        print "Most ActivePerl modules are available at Apache's ppm repository.\n";
        print "A list of mirrors is available at\n";
        print "    http://www.apache.org/dyn/closer.cgi/perl/win32-bin/ppms/\n";
        print "You can add the repository with the following command:\n";
        print "    ppm rep add apache http://www.apache.org/dist/perl/win32-bin/ppms/\n\n";
    }
}

if ((!$gd || !$chartbase) && !$silent) {
    print "If you you want to see graphical bug charts (plotting historical ";
    print "data over \ntime), you should install libgd and the following Perl ";     print "modules:\n\n";
    print "GD:          " . install_command("GD") ."\n" if !$gd;
    print "Chart:       " . install_command("Chart::Base") . "\n" if !$chartbase;
    print "\n";
}
if (!$xmlparser && !$silent) {
    print "If you want to use the bug import/export feature to move bugs to or from\n",
    "other bugzilla installations, you will need to install the XML::Parser module by\n",
    "running (as root):\n\n",
    "   " . install_command("XML::Parser") . "\n\n";
}
if ((!$gd || !$gdgraph || !$gdtextalign) && !$silent) {
    print "If you you want to see graphical bug reports (bar, pie and line ";
    print "charts of \ncurrent data), you should install libgd and the ";
    print "following Perl modules:\n\n";
    print "GD:              " . install_command("GD") . "\n" if !$gd;
    print "GD::Graph:       " . install_command("GD::Graph") . "\n" 
        if !$gdgraph;
    print "GD::Text::Align: " . install_command("GD::Text::Align") . "\n"
        if !$gdtextalign;
    print "\n";
}
if (!$patchreader && !$silent) {
    print "If you want to see pretty HTML views of patches, you should ";
    print "install the \nPatchReader module:\n";
    print "PatchReader: " . install_command("PatchReader") . "\n";
}

if (%missing) {
    print "\n\n";
    print "Bugzilla requires some Perl modules which are either missing from your\n",
    "system, or the version on your system is too old.\n",
    "They can be installed by running (as root) the following:\n";
    foreach my $module (keys %missing) {
        print "   " . install_command("$module") . "\n";
        if ($missing{$module} > 0) {
            print "   Minimum version required: $missing{$module}\n";
        }
    }
    print "\n";
    exit;
}

}
}

# Break out if checking the modules is all we have been asked to do.
exit if grep(/^--check-modules$/, @ARGV);

# If we're running on Windows, reset the input line terminator so that 
# console input works properly - loading CGI tends to mess it up

if ($^O =~ /MSWin/i) {
    $/ = "\015\012";
}

###########################################################################
# Global definitions
###########################################################################

use Bugzilla::Config qw(:DEFAULT :admin :locations);

# 12/17/00 justdave@syndicomm.com - removed declarations of the localconfig
# variables from this location.  We don't want these declared here.  They'll
# automatically get declared in the process of reading in localconfig, and
# this way we can look in the symbol table to see if they've been declared
# yet or not.

###########################################################################
# Check and update local configuration
###########################################################################

#
# This is quite tricky. But fun!
#
# First we read the file 'localconfig'. Then we check if the variables we
# need are defined. If not, localconfig will be amended by the new settings
# and the user informed to check this. The program then stops.
#
# Why do it this way around?
#
# Assume we will enhance Bugzilla and eventually more local configuration
# stuff arises on the horizon.
#
# But the file 'localconfig' is not in the Bugzilla CVS or tarfile. You
# know, we never want to overwrite your own version of 'localconfig', so
# we can't put it into the CVS/tarfile, can we?
#
# Now, when we need a new variable, we simply add the necessary stuff to
# checksetup. When the user gets the new version of Bugzilla from CVS and
# runs checksetup, it finds out "Oh, there is something new". Then it adds
# some default value to the user's local setup and informs the user to
# check that to see if it is what the user wants.
#
# Cute, ey?
#

print "Checking user setup ...\n" unless $silent;
$@ = undef;
do $localconfig;
if ($@) { # capture errors in localconfig, bug 97290
   print STDERR <<EOT;
An error has occurred while reading your 
'localconfig' file.  The text of the error message is:

$@

Please fix the error in your 'localconfig' file.  
Alternately rename your 'localconfig' file, rerun 
checksetup.pl, and re-enter your answers.

  \$ mv -f localconfig localconfig.old
  \$ ./checksetup.pl


EOT
    die "Syntax error in localconfig";
}

sub LocalVarExists ($)
{
    my ($name) = @_;
    return $main::{$name}; # if localconfig declared it, we're done.
}

my $newstuff = "";
sub LocalVar ($$)
{
    my ($name, $definition) = @_;
    return if LocalVarExists($name); # if localconfig declared it, we're done.
    $newstuff .= " " . $name;
    open FILE, '>>', $localconfig;
    print FILE ($answer{$name} or $definition), "\n\n";
    close FILE;
}


#
# Set up the defaults for the --LOCAL-- variables below:
#

LocalVar('index_html', <<'END');
#
# With the introduction of a configurable index page using the
# template toolkit, Bugzilla's main index page is now index.cgi.
# Most web servers will allow you to use index.cgi as a directory
# index and many come preconfigured that way, however if yours
# doesn't you'll need an index.html file that provides redirection
# to index.cgi. Setting $index_html to 1 below will allow
# checksetup.pl to create one for you if it doesn't exist.
# NOTE: checksetup.pl will not replace an existing file, so if you
#       wish to have checksetup.pl create one for you, you must
#       make sure that there isn't already an index.html
$index_html = 0;
END


if (!LocalVarExists('cvsbin')) {
    my $cvs_executable;
    if ($^O !~ /MSWin32/i) {
        $cvs_executable = `which cvs`;
        if ($cvs_executable =~ /no cvs/ || $cvs_executable eq '') {
            # If which didn't find it, just set to blank
            $cvs_executable = "";
        } else {
            chomp $cvs_executable;
        }
    } else {
        $cvs_executable = "";
    }

    LocalVar('cvsbin', <<"END");
#
# For some optional functions of Bugzilla (such as the pretty-print patch
# viewer), we need the cvs binary to access files and revisions.
# Because it's possible that this program is not in your path, you can specify
# its location here.  Please specify the full path to the executable.
\$cvsbin = "$cvs_executable";
END
}


if (!LocalVarExists('interdiffbin')) {
    my $interdiff_executable;
    if ($^O !~ /MSWin32/i) {
        $interdiff_executable = `which interdiff`;
        if ($interdiff_executable =~ /no interdiff/ || $interdiff_executable eq '') {
            if (!$silent) {
                print "\nOPTIONAL NOTE: If you want to ";
                print "be able to use the\n 'difference between two patches' ";
                print "feature of Bugzilla (requires\n the PatchReader Perl module ";
                print "as well), you should install\n patchutils from ";
                print "http://cyberelk.net/tim/patchutils/\n\n";
            }

            # If which didn't find it, set to blank
            $interdiff_executable = "";
        } else {
            chomp $interdiff_executable;
        }
    } else {
        $interdiff_executable = "";
    }

    LocalVar('interdiffbin', <<"END");

#
# For some optional functions of Bugzilla (such as the pretty-print patch
# viewer), we need the interdiff binary to make diffs between two patches.
# Because it's possible that this program is not in your path, you can specify
# its location here.  Please specify the full path to the executable.
\$interdiffbin = "$interdiff_executable";
END
}


if (!LocalVarExists('diffpath')) {
    my $diff_binaries;
    if ($^O !~ /MSWin32/i) {
        $diff_binaries = `which diff`;
        if ($diff_binaries =~ /no diff/ || $diff_binaries eq '') {
            # If which didn't find it, set to blank
            $diff_binaries = "";
        } else {
            $diff_binaries =~ s:/diff\n$::;
        }
    } else {
        $diff_binaries = "";
    }

    LocalVar('diffpath', <<"END");

#
# The interdiff feature needs diff, so we have to have that path.
# Please specify only the directory name, with no trailing slash.
\$diffpath = "$diff_binaries";
END
}


LocalVar('create_htaccess', <<'END');
#
# If you are using Apache for your web server, Bugzilla can create .htaccess
# files for you that will instruct Apache not to serve files that shouldn't
# be accessed from the web (like your local configuration data and non-cgi
# executable files).  For this to work, the directory your Bugzilla
# installation is in must be within the jurisdiction of a <Directory> block
# in the httpd.conf file that has 'AllowOverride Limit' in it.  If it has
# 'AllowOverride All' or other options with Limit, that's fine.
# (Older Apache installations may use an access.conf file to store these
# <Directory> blocks.)
# If this is set to 1, Bugzilla will create these files if they don't exist.
# If this is set to 0, Bugzilla will not create these files.
$create_htaccess = 1;
END

my $webservergroup_default;
if ($^O !~ /MSWin32/i) {
    $webservergroup_default = 'apache';
} else {
    $webservergroup_default = '';
}

LocalVar('webservergroup', <<"END");
#
# This is the group your web server runs on.
# If you have a windows box, ignore this setting.
# If you do not have access to the group your web server runs under,
# set this to "". If you do set this to "", then your Bugzilla installation
# will be _VERY_ insecure, because some files will be world readable/writable,
# and so anyone who can get local access to your machine can do whatever they
# want. You should only have this set to "" if this is a testing installation
# and you cannot set this up any other way. YOU HAVE BEEN WARNED.
# If you set this to anything besides "", you will need to run checksetup.pl
# as root, or as a user who is a member of the specified group.
\$webservergroup = "$webservergroup_default";
END



LocalVar('db_host', '
#
# How to access the SQL database:
#
$db_host = "localhost";         # where is the database?
$db_port = 3306;                # which port to use
$db_name = "bugs";              # name of the MySQL database
$db_user = "bugs";              # user to attach to the MySQL database
');
LocalVar('db_pass', '
#
# Enter your database password here. It\'s normally advisable to specify
# a password for your bugzilla database user.
# If you use apostrophe (\') or a backslash (\\) in your password, you\'ll
# need to escape it by preceding it with a \\ character. (\\\') or (\\\\)
#
$db_pass = \'\';
');

LocalVar('db_sock', '
# Enter a path to the unix socket for mysql. If this is blank, then mysql\'s
# compiled-in default will be used. You probably want that.
$db_sock = \'\';
');

LocalVar('db_check', '
#
# Should checksetup.pl try to check if your MySQL setup is correct?
# (with some combinations of MySQL/Msql-mysql/Perl/moonphase this doesn\'t work)
#
$db_check = 1;
');


LocalVar('severities', '
#
# Which bug and feature-request severities do you want?
#
@severities = (
        "blocker",
        "critical",
        "major",
        "normal",
        "minor",
        "trivial",
        "enhancement"
);
');



LocalVar('priorities', '
#
# Which priorities do you want to assign to bugs and feature-request?
#
@priorities = (
        "P1",
        "P2",
        "P3",
        "P4",
        "P5"
);
');



LocalVar('opsys', '
#
# What operatings systems may your products run on?
#
@opsys = (
        "All",
        "Windows 3.1",
        "Windows 95",
        "Windows 98",
        "Windows ME",  # Millenium Edition (upgrade of 98)
        "Windows 2000",
        "Windows NT",
        "Windows XP",
        "Windows Server 2003",
        "Mac System 7",
        "Mac System 7.5",
        "Mac System 7.6.1",
        "Mac System 8.0",
        "Mac System 8.5",
        "Mac System 8.6",
        "Mac System 9.x",
        "Mac OS X 10.0",
        "Mac OS X 10.1",
        "Mac OS X 10.2",
        "Mac OS X 10.3",
        "Linux",
        "BSD/OS",
        "FreeBSD",
        "NetBSD",
        "OpenBSD",
        "AIX",
        "BeOS",
        "HP-UX",
        "IRIX",
        "Neutrino",
        "OpenVMS",
        "OS/2",
        "OSF/1",
        "Solaris",
        "SunOS",
        "other"
);
');



LocalVar('platforms', '
#
# What hardware platforms may your products run on?
#
@platforms = (
        "All",
        "DEC",
        "HP",
        "Macintosh",
        "PC",
        "SGI",
        "Sun",
        "Other"
);
');

if (LocalVarExists('mysqlpath')) {
    print "\nThe \$mysqlpath setting in your localconfig file ",
          "is no longer required.\nWe recommend you remove it.\n";
}

if ($newstuff ne "") {
    print "\nThis version of Bugzilla contains some variables that you may want\n",
          "to change and adapt to your local settings. Please edit the file\n",
          "'$localconfig' and rerun checksetup.pl\n\n",
          "The following variables are new to localconfig since you last ran\n",
          "checksetup.pl:  $newstuff\n\n";
    exit;
}

# 2000-Dec-18 - justdave@syndicomm.com - see Bug 52921
# This is a hack to read in the values defined in localconfig without getting
# them predeclared at compile time if they're missing from localconfig.
# Ideas swiped from pp. 281-282, O'Reilly's "Programming Perl 2nd Edition"
# Note that we won't need to do this in globals.pl because globals.pl couldn't
# care less whether they were defined ahead of time or not. 
my $my_db_check = ${*{$main::{'db_check'}}{SCALAR}};
my $my_db_host = ${*{$main::{'db_host'}}{SCALAR}};
my $my_db_port = ${*{$main::{'db_port'}}{SCALAR}};
my $my_db_name = ${*{$main::{'db_name'}}{SCALAR}};
my $my_db_user = ${*{$main::{'db_user'}}{SCALAR}};
my $my_db_sock = ${*{$main::{'db_sock'}}{SCALAR}};
my $my_db_pass = ${*{$main::{'db_pass'}}{SCALAR}};
my $my_index_html = ${*{$main::{'index_html'}}{SCALAR}};
my $my_create_htaccess = ${*{$main::{'create_htaccess'}}{SCALAR}};
my $my_webservergroup = ${*{$main::{'webservergroup'}}{SCALAR}};
my @my_severities = @{*{$main::{'severities'}}{ARRAY}};
my @my_priorities = @{*{$main::{'priorities'}}{ARRAY}};
my @my_platforms = @{*{$main::{'platforms'}}{ARRAY}};
my @my_opsys = @{*{$main::{'opsys'}}{ARRAY}};

if ($my_webservergroup && !$silent) {
    if ($< != 0) { # zach: if not root, yell at them, bug 87398 
        print <<EOF;

Warning: you have entered a value for the "webservergroup" parameter
in localconfig, but you are not running this script as root.
This can cause permissions problems and decreased security.  If you
experience problems running Bugzilla scripts, log in as root and re-run
this script, or remove the value of the "webservergroup" parameter.
Note that any warnings about "uninitialized values" that you may
see below are caused by this.

EOF
    }

    if ($^O =~ /MSWin32/i) {
        print <<EOF;
      
Warning: You have set webservergroup in your localconfig.
Please understand that this does not bring you any security when
running under Windows.
Verify that the file permissions in your Bugzilla directory are
suitable for your system.
Avoid unnecessary write access.

EOF
    }

} else {
    # Theres no webservergroup, this is very very very very bad.
    # However, if we're being run on windows, then this option doesn't
    # really make sense. Doesn't make it any more secure either, though,
    # but don't print the message, since they can't do anything about it.
    if (($^O !~ /MSWin32/i) && !$silent) {
        print <<EOF;

********************************************************************************
WARNING! You have not entered a value for the "webservergroup" parameter
in localconfig. This means that certain files and directories which need
to be editable by both you and the webserver must be world writable, and
other files (including the localconfig file which stores your database
password) must be world readable. This means that _anyone_ who can obtain
local access to this machine can do whatever they want to your Bugzilla
installation, and is probably also able to run arbitrary Perl code as the
user that the webserver runs as.

You really, really, really need to change this setting.
********************************************************************************

EOF
    }
}

###########################################################################
# Check data directory
###########################################################################

#
# Create initial --DATA-- directory and make the initial empty files there:
#

# The |require "globals.pl"| above ends up creating a template object with
# a COMPILE_DIR of "$datadir". This means that TT creates the directory for us,
# so this code wouldn't run if we just checked for the existence of the
# directory. Instead, check for the existence of '$datadir/nomail', which is
# created in this block
unless (-d $datadir && -e "$datadir/nomail") {
    print "Creating data directory ($datadir) ...\n";
    # permissions for non-webservergroup are fixed later on
    mkdir $datadir, 0770;
    mkdir "$datadir/mimedump-tmp", 01777;
    open FILE, '>>', "$datadir/nomail"; close FILE;
    open FILE, '>>', "$datadir/mail"; close FILE;
}

# 2000-12-14 New graphing system requires a directory to put the graphs in
# This code copied from what happens for the data dir above.
# If the graphs dir is not present, we assume that they have been using
# a Bugzilla with the old data format, and so upgrade their data files.

# NB - the graphs dir isn't movable yet, unlike the datadir
unless (-d 'graphs') {
    print "Creating graphs directory...\n";
    # permissions for non-webservergroup are fixed later on
    mkdir 'graphs', 0770;
    # Upgrade data format
    foreach my $in_file (glob("$datadir/mining/*"))
    {
        # Don't try and upgrade image or db files!
        if (($in_file =~ /\.gif$/i) || 
            ($in_file =~ /\.png$/i) ||
            ($in_file =~ /\.db$/i) ||
            ($in_file =~ /\.orig$/i)) {
            next;
        }

        rename("$in_file", "$in_file.orig") or next;        
        open(IN, "$in_file.orig") or next;
        open(OUT, '>', $in_file) or next;
        
        # Fields in the header
        my @declared_fields = ();

        # Fields we changed to half way through by mistake
        # This list comes from an old version of collectstats.pl
        # This part is only for people who ran later versions of 2.11 (devel)
        my @intermediate_fields = qw(DATE UNCONFIRMED NEW ASSIGNED REOPENED 
                                     RESOLVED VERIFIED CLOSED);

        # Fields we actually want (matches the current collectstats.pl)                             
        my @out_fields = qw(DATE NEW ASSIGNED REOPENED UNCONFIRMED RESOLVED
                            VERIFIED CLOSED FIXED INVALID WONTFIX LATER REMIND
                            DUPLICATE WORKSFORME MOVED);

        while (<IN>) {
            if (/^# fields?: (.*)\s$/) {
                @declared_fields = map uc, (split /\||\r/, $1);
                print OUT "# fields: ", join('|', @out_fields), "\n";
            }
            elsif (/^(\d+\|.*)/) {
                my @data = split /\||\r/, $1;
                my %data = ();
                if (@data == @declared_fields) {
                    # old format
                    for my $i (0 .. $#declared_fields) {
                        $data{$declared_fields[$i]} = $data[$i];
                    }
                }
                elsif (@data == @intermediate_fields) {
                    # Must have changed over at this point 
                    for my $i (0 .. $#intermediate_fields) {
                        $data{$intermediate_fields[$i]} = $data[$i];
                    }
                }
                elsif (@data == @out_fields) {
                    # This line's fine - it has the right number of entries 
                    for my $i (0 .. $#out_fields) {
                        $data{$out_fields[$i]} = $data[$i];
                    }
                }
                else {
                    print "Oh dear, input line $. of $in_file had " . scalar(@data) . " fields\n";
                    print "This was unexpected. You may want to check your data files.\n";
                }

                print OUT join('|', map { 
                              defined ($data{$_}) ? ($data{$_}) : "" 
                                                          } @out_fields), "\n";
            }
            else {
                print OUT;
            }
        }

        close(IN);
        close(OUT);
    }
}

unless (-d "$datadir/mining") {
    mkdir "$datadir/mining", 0700;
}

unless (-d "$webdotdir") {
    # perms/ownership are fixed up later
    mkdir "$webdotdir", 0700;
}

if ($my_create_htaccess) {
  my $fileperm = 0644;
  my $dirperm = 01777;
  if ($my_webservergroup) {
    $fileperm = 0640;
    $dirperm = 0770;
  }
  if (!-e ".htaccess") {
    print "Creating .htaccess...\n";
    open HTACCESS, '>', '.htaccess';
    print HTACCESS <<'END';
# don't allow people to retrieve non-cgi executable files or our private data
<FilesMatch ^(.*\.pl|.*localconfig.*|runtests.sh)$>
  deny from all
</FilesMatch>
<FilesMatch ^(localconfig.js|localconfig.rdf)$>
  allow from all
</FilesMatch>
END
    close HTACCESS;
    chmod $fileperm, ".htaccess";
  } else {
    # 2002-12-21 Bug 186383
    open HTACCESS, ".htaccess";
    my $oldaccess = "";
    while (<HTACCESS>) {
      $oldaccess .= $_;
    }
    close HTACCESS;
    if ($oldaccess =~ s/\|localconfig\|/\|.*localconfig.*\|/) {
      print "Repairing .htaccess...\n";
      open HTACCESS, '>', '.htaccess';
      print HTACCESS $oldaccess;
      print HTACCESS <<'END';
<FilesMatch ^(localconfig.js|localconfig.rdf)$>
  allow from all
</FilesMatch>
END
      close HTACCESS;
    }

  }
  if (!-e "Bugzilla/.htaccess") {
    print "Creating Bugzilla/.htaccess...\n";
    open HTACCESS, '>', 'Bugzilla/.htaccess';
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overriden by an .htaccess
# in a subdirectory
deny from all
END
    close HTACCESS;
    chmod $fileperm, "Bugzilla/.htaccess";
  }
  # Even though $datadir may not (and should not) be in the webtree,
  # we can't know for sure, so create the .htaccess anyeay. Its harmless
  # if its not accessible...
  if (!-e "$datadir/.htaccess") {
    print "Creating $datadir/.htaccess...\n";
    open HTACCESS, '>', "$datadir/.htaccess";
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overriden by an .htaccess
# in a subdirectory; the only exception is duplicates.rdf, which is used by
# duplicates.xul and must be loadable over the web
deny from all
<Files duplicates.rdf>
  allow from all
</Files>
END
    close HTACCESS;
    chmod $fileperm, "$datadir/.htaccess";
  }
  # Ditto for the template dir
  if (!-e "$templatedir/.htaccess") {
    print "Creating $templatedir/.htaccess...\n";
    open HTACCESS, '>', "$templatedir/.htaccess";
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overriden by an .htaccess
# in a subdirectory
deny from all
END
    close HTACCESS;
    chmod $fileperm, "$templatedir/.htaccess";
  }
  if (!-e "$webdotdir/.htaccess") {
    print "Creating $webdotdir/.htaccess...\n";
    open HTACCESS, '>', "$webdotdir/.htaccess";
    print HTACCESS <<'END';
# Restrict access to .dot files to the public webdot server at research.att.com 
# if research.att.com ever changed their IP, or if you use a different
# webdot server, you'll need to edit this
<FilesMatch \.dot$>
  Allow from 192.20.225.10
  Deny from all
</FilesMatch>

# Allow access to .png files created by a local copy of 'dot'
<FilesMatch \.png$>
  Allow from all
</FilesMatch>

# And no directory listings, either.
Deny from all
END
    close HTACCESS;
    chmod $fileperm, "$webdotdir/.htaccess";
  }

}

if ($my_index_html) {
    if (!-e "index.html") {
        print "Creating index.html...\n";
        open HTML, '>', 'index.html';
        print HTML <<'END';
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<meta http-equiv="Refresh" content="0; URL=index.cgi">
</head>
<body>
<h1>I think you are looking for <a href="index.cgi">index.cgi</a></h1>
</body>
</html>
END
        close HTML;
    }
    else {
        open HTML, "index.html";
        if (! grep /index\.cgi/, <HTML>) {
            print "\n\n";
            print "*** It appears that you still have an old index.html hanging\n";
            print "    around.  The contents of this file should be moved into a\n";
            print "    template and placed in the 'en/custom' directory within";
            print "    your template directory.\n\n";
        }
        close HTML;
    }
}

{
    if (-e "$datadir/template") {
        print "Removing existing compiled templates ...\n" unless $silent;

       File::Path::rmtree("$datadir/template");

       #Check that the directory was really removed
       if(-e "$datadir/template") {
           print "\n\n";
           print "The directory '$datadir/template' could not be removed. Please\n";
           print "remove it manually and rerun checksetup.pl.\n\n";
           exit;
       }
    }

    # Search for template directories
    # We include the default and custom directories separately to make
    # sure we compile all templates
    my @templatepaths = ();
    {
        use File::Spec; 
        opendir(DIR, $templatedir) || die "Can't open '$templatedir': $!";
        my @files = grep { /^[a-z-]+$/i } readdir(DIR);
        closedir DIR;

        foreach my $dir (@files) {
            next if($dir =~ /^CVS$/i);
            my $path = File::Spec->catdir($templatedir, $dir, 'custom');
            push(@templatepaths, $path) if(-d $path);
            $path = File::Spec->catdir($templatedir, $dir, 'extension');
            push(@templatepaths, $path) if(-d $path);
            $path = File::Spec->catdir($templatedir, $dir, 'default');
            push(@templatepaths, $path) if(-d $path);
        }
    }

    # Precompile stuff. This speeds up initial access (so the template isn't
    # compiled multiple times simulataneously by different servers), and helps
    # to get the permissions right.
    sub compile {
        my $name = $File::Find::name;

        return if (-d $name);
        return if ($name =~ /\/CVS\//);
        return if ($name !~ /\.tmpl$/);
        $name =~ s/\Q$::templatepath\E\///; # trim the bit we don't pass to TT

        # Do this to avoid actually processing the templates
        my ($data, $err) = $::provider->fetch($name);
        die "Could not compile $name: " . $data . "\n" if $err;
    }
    
    eval("use Template");

    {
        print "Precompiling templates ...\n" unless $silent;

        use File::Find;

        # Don't hang on templates which use the CGI library
        eval("use CGI qw(-no_debug)");
        foreach $::templatepath (@templatepaths) {
           $::provider = Template::Provider->new(
           {
               # Directories containing templates.
               INCLUDE_PATH => $::templatepath,

               PRE_CHOMP => 1 ,
               TRIM => 1 ,

               # => $datadir/template/`pwd`/template/{en, ...}/{custom,default}
               COMPILE_DIR => "$datadir/template",

               # Initialize templates (f.e. by loading plugins like Hook).
               PRE_PROCESS => "global/initialize.none.tmpl",

               # These don't actually need to do anything here, just exist
               FILTERS =>
               {
                inactive => sub { return $_; } ,
                closed => sub { return $_; },
                obsolete => sub { return $_; },
                js => sub { return $_; },
                html_linebreak => sub { return $_; },
                url_quote => sub { return $_; },
                xml => sub { return $_; },
                quoteUrls => sub { return $_; },
                bug_link => [ sub { return sub { return $_; } }, 1],
                csv => sub { return $_; },
                unitconvert => sub { return $_; },
                time => sub { return $_; },
                none => sub { return $_; } ,
               },
           }) || die ("Could not create Template Provider: "
                       . Template::Provider->error() . "\n");

           # Traverse the template hierachy. 
           find({ wanted => \&compile, no_chdir => 1 }, $::templatepath);
       }
    }
}

# Just to be sure ...
unlink "$datadir/versioncache";

# Remove parameters from the params file that no longer exist in Bugzilla,
# and set the defaults for new ones

my @oldparams = UpdateParams();

if (@oldparams) {
    open(PARAMFILE, '>>', 'old-params.txt') 
      || die "$0: Can't open old-params.txt for writing: $!\n";

    print "The following parameters are no longer used in Bugzilla, " .
      "and so have been\nremoved from your parameters file and " .
      "appended to old-params.txt:\n";

    foreach my $p (@oldparams) {
        my ($item, $value) = @{$p};

        print PARAMFILE "\n\n$item:\n$value\n";

        print $item;
        print ", " unless $item eq $oldparams[$#oldparams]->[0];
    }
    print "\n";
    close PARAMFILE;
}

# WriteParams will only write out still-valid entries
WriteParams();

###########################################################################
# Set proper rights
###########################################################################

#
# Here we use --CHMOD-- and friends to set the file permissions
#
# The rationale is that the web server generally runs as apache and so the cgi
# scripts should not be writable for apache, otherwise someone may be possible
# to change the cgi's when exploiting some security flaw somewhere (not
# necessarily in Bugzilla!)
#
# Also, some *.pl files are executable, some are not.
#
# +++ Can anybody tell me what a Windows Perl would do with this code?
#
# Changes 03/14/00 by SML
#
# This abstracts out what files are executable and what ones are not.  It makes
# for slightly neater code and lets us do things like determine exactly which
# files are executable and which ones are not.
#
# Not all directories have permissions changed on them.  i.e., changing ./CVS
# to be 0640 is bad.
#
# Fixed bug in chmod invokation.  chmod (at least on my linux box running perl
# 5.005 needs a valid first argument, not 0.
#
# (end changes, 03/14/00 by SML)
#
# Changes 15/06/01 kiko@async.com.br
# 
# Fix file permissions for non-webservergroup installations (see
# http://bugzilla.mozilla.org/show_bug.cgi?id=71555). I'm setting things
# by default to world readable/executable for all files, and
# world-writeable (with sticky on) to data and graphs.
#

# These are the files which need to be marked executable
my @executable_files = ('whineatnews.pl', 'collectstats.pl',
   'checksetup.pl', 'importxml.pl', 'runtests.sh', 'testserver.pl',
   'whine.pl');

# tell me if a file is executable.  All CGI files and those in @executable_files
# are executable
sub isExecutableFile {
  my ($file) = @_;
  if ($file =~ /\.cgi/) {
    return 1;
  }

  my $exec_file;
  foreach $exec_file (@executable_files) {
    if ($file eq $exec_file) {
      return 1;
    }
  }
  return undef;
}

# fix file (or files - wildcards ok) permissions 
sub fixPerms {
    my ($file_pattern, $owner, $group, $umask, $do_dirs) = @_;
    my @files = glob($file_pattern);
    my $execperm = 0777 & ~ $umask;
    my $normperm = 0666 & ~ $umask;
    foreach my $file (@files) {
        next if (!-e $file);
        # do not change permissions on directories here unless $do_dirs is set
        if (!(-d $file)) {
            chown $owner, $group, $file;
            # check if the file is executable.
            if (isExecutableFile($file)) {
                #printf ("Changing $file to %o\n", $execperm);
                chmod $execperm, $file;
            } else {
                #printf ("Changing $file to %o\n", $normperm);
                chmod $normperm, $file;
            }
        }
        elsif ($do_dirs) {
            chown $owner, $group, $file;
            if ($file =~ /CVS$/) {
                chmod 0700, $file;
            }
            else {
                #printf ("Changing $file to %o\n", $execperm);
                chmod $execperm, $file;
                fixPerms("$file/.htaccess", $owner, $group, $umask, $do_dirs);
                fixPerms("$file/*", $owner, $group, $umask, $do_dirs); # do the contents of the directory
            }
        }
    }
}

if ($^O !~ /MSWin32/i) {
    if ($my_webservergroup) {
        # Funny! getgrname returns the GID if fed with NAME ...
        my $webservergid = getgrnam($my_webservergroup) 
        or die("no such group: $my_webservergroup");
        # chown needs to be called with a valid uid, not 0.  $< returns the
        # caller's uid.  Maybe there should be a $bugzillauid, and call with that
        # userid.
        fixPerms('.htaccess', $<, $webservergid, 027); # glob('*') doesn't catch dotfiles
        fixPerms("$datadir/.htaccess", $<, $webservergid, 027);
        fixPerms("$datadir/duplicates", $<, $webservergid, 027, 1);
        fixPerms("$datadir/mining", $<, $webservergid, 027, 1);
        fixPerms("$datadir/template", $<, $webservergid, 007, 1); # webserver will write to these
        fixPerms($webdotdir, $<, $webservergid, 007, 1);
        fixPerms("$webdotdir/.htaccess", $<, $webservergid, 027);
        fixPerms("$datadir/params", $<, $webservergid, 017);
        fixPerms('*', $<, $webservergid, 027);
        fixPerms('Bugzilla', $<, $webservergid, 027, 1);
        fixPerms($templatedir, $<, $webservergid, 027, 1);
        fixPerms('css', $<, $webservergid, 027, 1);
        fixPerms('js', $<, $webservergid, 027, 1);
        chmod 0644, 'globals.pl';
        
        # Don't use fixPerms here, because it won't change perms on the directory
        # unless its using recursion
        chown $<, $webservergid, $datadir;
        chmod 0771, $datadir;
        chown $<, $webservergid, 'graphs';
        chmod 0770, 'graphs';
    } else {
        # get current gid from $( list
        my $gid = (split " ", $()[0];
        fixPerms('.htaccess', $<, $gid, 022); # glob('*') doesn't catch dotfiles
        fixPerms("$datadir/.htaccess", $<, $gid, 022);
        fixPerms("$datadir/duplicates", $<, $gid, 022, 1);
        fixPerms("$datadir/mining", $<, $gid, 022, 1);
        fixPerms("$datadir/template", $<, $gid, 000, 1); # webserver will write to these
        fixPerms($webdotdir, $<, $gid, 000, 1);
        chmod 01777, $webdotdir;
        fixPerms("$webdotdir/.htaccess", $<, $gid, 022);
        fixPerms("$datadir/params", $<, $gid, 011);
        fixPerms('*', $<, $gid, 022);
        fixPerms('Bugzilla', $<, $gid, 022, 1);
        fixPerms($templatedir, $<, $gid, 022, 1);
        fixPerms('css', $<, $gid, 022, 1);
        fixPerms('js', $<, $gid, 022, 1);
        
        # Don't use fixPerms here, because it won't change perms on the directory
        # unless its using recursion
        chown $<, $gid, $datadir;
        chmod 0777, $datadir;
        chown $<, $gid, 'graphs';
        chmod 01777, 'graphs';
    }
}

###########################################################################
# Global Utility Library
###########################################################################

# This is done here, because some modules require params to be set up, which
# won't have happened earlier.

# The only use for loading globals.pl is for Crypt(), which should at some
# point probably be factored out into Bugzilla::Auth::*

# globals.pl clears the PATH, but File::Find uses Cwd::cwd() instead of
# Cwd::getcwd(), which we need to do because `pwd` isn't in the path - see
# http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2001-09/msg00115.html
# As a workaround, since we only use File::Find in checksetup, which doesn't
# run in taint mode anyway, preserve the path...
my $origPath = $::ENV{'PATH'};

# Use the Bugzilla utility library for various functions.  We do this
# here rather than at the top of the file so globals.pl doesn't define
# localconfig variables for us before we get a chance to check for
# their existence and create them if they don't exist.  Also, globals.pl
# removes $ENV{'path'}, which we need in order to run `which mysql` above.
require "globals.pl";

# ...and restore it. This doesn't change tainting, so this will still cause
# errors if this script ever does run with -T.
$::ENV{'PATH'} = $origPath;

###########################################################################
# Check MySQL setup
###########################################################################

#
# Check if we have access to --MYSQL--
#

# This settings are not yet changeable, because other code depends on
# the fact that we use MySQL and not, say, PostgreSQL.

my $db_base = 'mysql';

# No need to "use" this here.  It should already be loaded from the
# version-checking routines above, and this file won't even compile if
# DBI isn't installed so the user gets nasty errors instead of our
# pretty one saying they need to install it. -- justdave@syndicomm.com
#use DBI;

if ($my_db_check) {
    # Do we have the database itself?

    my $sql_want = "3.23.41";  # minimum version of MySQL

# original DSN line was:
#    my $dsn = "DBI:$db_base:$my_db_name;$my_db_host;$my_db_port";
# removed the $db_name because we don't know it exists yet, and this will fail
# if we request it here and it doesn't. - justdave@syndicomm.com 2000/09/16
    my $dsn = "DBI:$db_base:;$my_db_host;$my_db_port";
    if ($my_db_sock ne "") {
        $dsn .= ";mysql_socket=$my_db_sock";
    }
    my $dbh = DBI->connect($dsn, $my_db_user, $my_db_pass)
      or die "Can't connect to the $db_base database. Is the database " .
        "installed and\nup and running?  Do you have the correct username " .
        "and password selected in\nlocalconfig?\n\n";
    printf("Checking for %15s %-9s ", "MySQL Server", "(v$sql_want)") unless $silent;
    my $qh = $dbh->prepare("SELECT VERSION()");
    $qh->execute;
    my ($sql_vers) = $qh->fetchrow_array;
    $qh->finish;

    # Check what version of MySQL is installed and let the user know
    # if the version is too old to be used with Bugzilla.
    if ( vers_cmp($sql_vers,$sql_want) > -1 ) {
        print "ok: found v$sql_vers\n" unless $silent;
    } else {
        die "\nYour MySQL server v$sql_vers is too old.\n" . 
            "   Bugzilla requires version $sql_want or later of MySQL.\n" . 
            "   Please visit http://www.mysql.com/ and download a newer version.\n";
    }
    if (( $sql_vers =~ /^4\.0\.(\d+)/ ) && ($1 < 2)) {
        die "\nYour MySQL server is incompatible with Bugzilla.\n" .
            "   Bugzilla does not support versions 4.x.x below 4.0.2.\n" .
            "   Please visit http://www.mysql.com/ and download a newer version.\n";
    }

    my @databases = $dbh->func('_ListDBs');
    unless (grep /^$my_db_name$/, @databases) {
       print "Creating database $my_db_name ...\n";
       $dbh->func('createdb', $my_db_name, 'admin')
            or die <<"EOF"

The '$my_db_name' database is not accessible. This might have several reasons:

* MySQL is not running.
* MySQL is running, but the rights are not set correct. Go and read the
  Bugzilla Guide in the doc directory and all parts of the MySQL
  documentation.
* There is an subtle problem with Perl, DBI, DBD::mysql and MySQL. Make
  sure all settings in '$localconfig' are correct. If all else fails, set
  '\$db_check' to zero.\n
EOF
    }
    $dbh->disconnect if $dbh;
}

# now get a handle to the database:
my $connectstring = "dbi:$db_base:$my_db_name:host=$my_db_host:port=$my_db_port";
if ($my_db_sock ne "") {
    $connectstring .= ";mysql_socket=$my_db_sock";
}

my $dbh = DBI->connect($connectstring, $my_db_user, $my_db_pass)
    or die "Can't connect to the table '$connectstring'.\n",
           "Have you read the Bugzilla Guide in the doc directory?  Have you read the doc of '$db_base'?\n";

END { $dbh->disconnect if $dbh }

###########################################################################
# Check for LDAP
###########################################################################

for my $verifymethod (split /,\s*/, Param('user_verify_class')) {
    if ($verifymethod eq 'LDAP') {
        my $netLDAP = have_vers("Net::LDAP", 0);
        if (!$netLDAP && !$silent) {
            print "If you wish to use LDAP authentication, then you must install Net::LDAP\n\n";
        }
    }
}

###########################################################################
# Check GraphViz setup
###########################################################################

#
# If we are using a local 'dot' binary, verify the specified binary exists
# and that the generated images are accessible.
#

if( Param('webdotbase') && Param('webdotbase') !~ /^https?:/ ) {
    printf("Checking for %15s %-9s ", "GraphViz", "(any)") unless $silent;
    if(-x Param('webdotbase')) {
        print "ok: found\n" unless $silent;
    } else {
        print "not a valid executable: " . Param('webdotbase') . "\n";
    }

    # Check .htaccess allows access to generated images
    if(-e "$webdotdir/.htaccess") {
      open HTACCESS, "$webdotdir/.htaccess";
      if(! grep(/png/,<HTACCESS>)) {
        print "Dependency graph images are not accessible.\n";
        print "Delete $webdotdir/.htaccess and re-run checksetup.pl to rectify.\n";
      }
      close HTACCESS;
    }
}

print "\n" unless $silent;


###########################################################################
# Table definitions
###########################################################################

#
# The following hash stores all --TABLE-- definitions. This will be used
# to automatically create those tables that don't exist. The code is
# safer than the make*.sh shell scripts used to be, because they won't
# delete existing tables.
#
# If you want to intentionally do this, you can always drop a table and re-run
# checksetup, e.g. like this:
#
#    $ mysql bugs
#    mysql> drop table votes;
#    mysql> exit;
#    $ ./checksetup.pl
#
# If you change one of those field definitions, then also go below to the
# next occurence of the string --TABLE-- (near the end of this file) to
# add the code that updates older installations automatically.
#


my %table;

$table{bugs_activity} = 
   'bug_id mediumint not null,
    attach_id mediumint null,
    who mediumint not null,
    bug_when datetime not null,
    fieldid mediumint not null,
    added tinytext,
    removed tinytext,

    index (bug_id),
    index (bug_when),
    index (fieldid)';


$table{attachments} =
   'attach_id mediumint not null auto_increment primary key,
    bug_id mediumint not null,
    creation_ts datetime not null,
    description mediumtext not null,
    mimetype mediumtext not null,
    ispatch tinyint,
    filename varchar(100) not null,
    thedata longblob not null,
    submitter_id mediumint not null,
    isobsolete tinyint not null default 0, 
    isprivate tinyint not null default 0,

    index(bug_id),
    index(creation_ts)';

# September 2002 myk@mozilla.org: Tables to support status flags,
# which replace attachment statuses and allow users to flag bugs
# or attachments with statuses (review+, approval-, etc.).
#
# "flags" stores one record for each flag on each bug/attachment.
# "flagtypes" defines the types of flags that can be set.
# "flaginclusions" and "flagexclusions" specify the products/components
#     a bug/attachment must belong to in order for flags of a given type
#     to be set for them.

$table{flags} =
    'id                 MEDIUMINT     NOT NULL  PRIMARY KEY , 
     type_id            SMALLINT      NOT NULL , 
     status             CHAR(1)       NOT NULL , 
     
     bug_id             MEDIUMINT     NOT NULL , 
     attach_id          MEDIUMINT     NULL , 
     
     creation_date      DATETIME      NOT NULL , 
     modification_date  DATETIME      NULL , 
     
     setter_id          MEDIUMINT     NULL , 
     requestee_id       MEDIUMINT     NULL , 
     
     is_active          TINYINT       NOT NULL  DEFAULT 1, 
   
     INDEX(bug_id, attach_id) , 
     INDEX(setter_id) , 
     INDEX(requestee_id)
   ';

$table{flagtypes} =
   'id                  SMALLINT      NOT NULL  PRIMARY KEY , 
    name                VARCHAR(50)   NOT NULL , 
    description         TEXT          NULL , 
    cc_list             VARCHAR(200)  NULL , 
    
    target_type         CHAR(1)       NOT NULL  DEFAULT \'b\' , 
    
    is_active           TINYINT       NOT NULL  DEFAULT 1 , 
    is_requestable      TINYINT       NOT NULL  DEFAULT 0 , 
    is_requesteeble     TINYINT       NOT NULL  DEFAULT 0 , 
    is_multiplicable    TINYINT       NOT NULL  DEFAULT 0 , 
    
    sortkey             SMALLINT      NOT NULL  DEFAULT 0 
   ';

$table{flaginclusions} =
   'type_id             SMALLINT      NOT NULL , 
    product_id          SMALLINT      NULL ,
    component_id        SMALLINT      NULL , 
    
    INDEX(type_id, product_id, component_id)
   ';

$table{flagexclusions} =
   'type_id             SMALLINT      NOT NULL , 
    product_id          SMALLINT      NULL ,
    component_id        SMALLINT      NULL , 
    
    INDEX(type_id, product_id, component_id)
   ';

#
# Apostrophe's are not supportied in the enum types.
# See http://bugzilla.mozilla.org/show_bug.cgi?id=27309
#
$table{bugs} =
   'bug_id mediumint not null auto_increment primary key,
    assigned_to mediumint not null, # This is a comment.
    bug_file_loc text,
    bug_severity enum($my_severities) not null,
    bug_status enum("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED", "RESOLVED", "VERIFIED", "CLOSED") not null,
    creation_ts datetime not null,
    delta_ts timestamp not null,
    short_desc mediumtext not null,
    op_sys enum($my_opsys) not null,
    priority enum($my_priorities) not null,
    product_id smallint not null,
    rep_platform enum($my_platforms),
    reporter mediumint not null,
    version varchar(64) not null,
    component_id smallint not null,
    resolution enum("", "FIXED", "INVALID", "WONTFIX", "LATER", "REMIND", "DUPLICATE", "WORKSFORME", "MOVED") not null,
    target_milestone varchar(20) not null default "---",
    qa_contact mediumint not null,
    status_whiteboard mediumtext not null,
    votes mediumint not null,
    keywords mediumtext not null, ' # Note: keywords field is only a cache;
                                # the real data comes from the keywords table.
    . '
    lastdiffed datetime not null,
    everconfirmed tinyint not null,
    reporter_accessible tinyint not null default 1,
    cclist_accessible tinyint not null default 1,
    estimated_time decimal(5,2) not null default 0,
    remaining_time decimal(5,2) not null default 0,
    alias varchar(20),
    
    index (assigned_to),
    index (creation_ts),
    index (delta_ts),
    index (bug_severity),
    index (bug_status),
    index (op_sys),
    index (priority),
    index (product_id),
    index (reporter),
    index (version),
    index (component_id),
    index (resolution),
    index (target_milestone),
    index (qa_contact),
    index (votes),

    fulltext (short_desc),
    
    unique(alias)';


$table{cc} =
   'bug_id mediumint not null,
    who mediumint not null,

    index(who),
    unique(bug_id,who)';

$table{watch} =
   'watcher mediumint not null,
    watched mediumint not null,

    index(watched),
    unique(watcher,watched)';


$table{longdescs} = 
   'bug_id mediumint not null,
    who mediumint not null,
    bug_when datetime not null,
    work_time decimal(5,2) not null default 0,
    thetext mediumtext,
    isprivate tinyint not null default 0,
    index(bug_id),
    index(who),
    index(bug_when),
    fulltext (thetext)';


$table{components} =
   'id smallint not null auto_increment primary key,
    name varchar(64) not null,
    product_id smallint not null,
    initialowner mediumint not null,
    initialqacontact mediumint not null,
    description mediumtext not null,

    unique(product_id,name),
    index(name)';


$table{dependencies} =
   'blocked mediumint not null,
    dependson mediumint not null,

    index(blocked),
    index(dependson)';


# User regexp is which email addresses are put into this group.
#
# 2001-04-10 myk@mozilla.org:
# isactive determines whether or not a group is active.  An inactive group
# cannot have bugs added to it.  Deactivation is a much milder form of
# deleting a group that allows users to continue to work on bugs in the group
# without enabling them to extend the life of the group by adding bugs to it.
# http://bugzilla.mozilla.org/show_bug.cgi?id=75482

$table{groups} =
   'id mediumint not null auto_increment primary key,
    name varchar(255) not null,
    description text not null,
    isbuggroup tinyint not null,
    last_changed datetime not null,
    userregexp tinytext not null,
    isactive tinyint not null default 1,

    unique(name)';

$table{logincookies} =
   'cookie mediumint not null auto_increment primary key,
    userid mediumint not null,
    ipaddr varchar(40) NOT NULL,
    lastused timestamp,

    index(lastused)';


$table{products} =
   'id smallint not null auto_increment primary key,
    name varchar(64) not null,
    description mediumtext,
    milestoneurl tinytext not null,
    disallownew tinyint not null,
    votesperuser smallint not null,
    maxvotesperbug smallint not null default 10000,
    votestoconfirm smallint not null,
    defaultmilestone varchar(20) not null default "---",

    unique(name)';


$table{profiles} =
   'userid mediumint not null auto_increment primary key,
    login_name varchar(255) not null,
    cryptpassword varchar(34),
    realname varchar(255),
    disabledtext mediumtext not null,
    mybugslink tinyint not null default 1,
    emailflags mediumtext,
    refreshed_when datetime not null,
    unique(login_name)';


$table{profiles_activity} = 
   'userid mediumint not null,
    who mediumint not null,
    profiles_when datetime not null,
    fieldid mediumint not null,
    oldvalue tinytext,
    newvalue tinytext,

    index (userid),
    index (profiles_when),
    index (fieldid)';


$table{namedqueries} =
    'userid mediumint not null,
     name varchar(64) not null,
     linkinfooter tinyint not null,
     query mediumtext not null,

     unique(userid, name)';

$table{fielddefs} =
   'fieldid mediumint not null auto_increment primary key,
    name varchar(64) not null,
    description mediumtext not null,
    mailhead tinyint not null default 0,
    sortkey smallint not null,

    unique(name),
    index(sortkey)';

$table{versions} =
   'value tinytext,
    product_id smallint not null';


$table{votes} =
   'who mediumint not null,
    bug_id mediumint not null,
    vote_count smallint not null,

    index(who),
    index(bug_id)';

$table{keywords} =
    'bug_id mediumint not null,
     keywordid smallint not null,

     index(keywordid),
     unique(bug_id,keywordid)';

$table{keyworddefs} =
    'id smallint not null primary key,
     name varchar(64) not null,
     description mediumtext,

     unique(name)';


$table{milestones} =
    'product_id smallint not null,
     value varchar(20) not null,
     sortkey smallint not null,
     unique (product_id, value)';

# GRM
$table{duplicates} =
    'dupe_of mediumint(9) not null,
     dupe mediumint(9) not null primary key';

# 2001-06-21, myk@mozilla.org, bug 77473:
# Stores the tokens users receive when they want to change their password 
# or email address.  Tokens provide an extra measure of security for these changes.
$table{tokens} =
    'userid mediumint not null , 
     issuedate datetime not null , 
     token varchar(16) not null primary key ,  
     tokentype varchar(8) not null , 
     eventdata tinytext null , 

     index(userid)';

# group membership tables for tracking group and privilege 
# 
# This table determines the groups that a user belongs to
# directly or due to regexp and which groups can be blessed
# by a user. 
#
# grant_type: 
# if GRANT_DIRECT - record was explicitly granted
# if GRANT_DERIVED - record was derived from expanding a group hierarchy
# if GRANT_REGEXP - record was created by evaluating a regexp
$table{user_group_map} =
    'user_id mediumint not null,
     group_id mediumint not null,
     isbless tinyint not null default 0,
     grant_type tinyint not null default 0,

     unique(user_id, group_id, grant_type, isbless)';

# This table determines which groups are made a member of another
# group, given the ability to bless another group, or given
# visibility to another groups existence and membership
# grant_type:
# if GROUP_MEMBERSHIP - member groups are made members of grantor
# if GROUP_BLESS - member groups may grant membership in grantor
# if GROUP_VISIBLE - member groups may see grantor group
$table{group_group_map} =
    'member_id mediumint not null,
     grantor_id mediumint not null,
     grant_type tinyint not null default 0,

     unique(member_id, grantor_id, grant_type)';

# This table determines which groups a user must be a member of
# in order to see a bug.
$table{bug_group_map} =
    'bug_id mediumint not null,
     group_id mediumint not null,
     unique(bug_id, group_id),
     index(group_id)';

# 2002-07-19, davef@tetsubo.com, bug 67950:
# Store quips in the db.
$table{quips} =
    'quipid mediumint not null auto_increment primary key,
     userid mediumint not null default 0, 
     quip text not null,
     approved tinyint(1) not null default 1';

$table{group_control_map} =
    'group_id mediumint not null,
     product_id mediumint not null,
     entry tinyint not null,
     membercontrol tinyint not null,
     othercontrol tinyint not null,
     canedit tinyint not null,
     
     unique(product_id, group_id),
     index(group_id)';

# 2003-06-26 gerv@gerv.net, bug 16009
# Generic charting over time of arbitrary queries.
# Queries are disabled when frequency == 0.
$table{series} =
    'series_id    mediumint   auto_increment primary key,
     creator      mediumint   not null,
     category     smallint    not null,
     subcategory  smallint    not null,
     name         varchar(64) not null,
     frequency    smallint    not null,
     last_viewed  datetime    default null,
     query        mediumtext  not null,
     
     index(creator),
     unique(creator, category, subcategory, name)';

$table{series_data} = 
    'series_id mediumint not null,
     date      datetime  not null,
     value     mediumint not null,
     
     unique(series_id, date)';

$table{user_series_map} =
    'user_id   mediumint not null,
     series_id mediumint not null, 
     
     index(series_id),
     unique(user_id, series_id)';
     
$table{series_categories} =
    'category_id smallint    auto_increment primary key,
     name        varchar(64) not null,
     
     unique(name)';
     


# whine system

$table{whine_queries} =
    'id             mediumint       auto_increment primary key,
     eventid        mediumint       not null,
     query_name     varchar(64)     not null default \'\',
     sortkey        smallint        not null default 0,
     onemailperbug  tinyint         not null default 0,
     title          varchar(128)    not null,
     
     index(eventid)';

$table{whine_schedules} =
    'id             mediumint       auto_increment primary key,
     eventid        mediumint       not null,
     run_day        varchar(32),
     run_time       varchar(32),
     run_next       datetime,
     mailto_userid  mediumint       not null,
     
     index(run_next),
     index(eventid)';

$table{whine_events} =
    'id             mediumint       auto_increment primary key,
     owner_userid   mediumint       not null,
     subject        varchar(128),
     body           mediumtext';

###########################################################################
# Create tables
###########################################################################

# Figure out if any existing tables are of type ISAM and convert them
# to type MyISAM if so.  ISAM tables are deprecated in MySQL 3.23,
# which Bugzilla now requires, and they don't support more than 16 
# indexes per table, which Bugzilla needs.
my $sth = $dbh->prepare("SHOW TABLE STATUS FROM $::db_name");
$sth->execute;
my @isam_tables = ();
while (my ($name, $type) = $sth->fetchrow_array) {
    push(@isam_tables, $name) if $type eq "ISAM";
}

if(scalar(@isam_tables)) {
    print "One or more of the tables in your existing MySQL database are of type ISAM.\n" . 
          "ISAM tables are deprecated in MySQL 3.23 and don't support more than 16 indexes\n" . 
          "per table, which Bugzilla needs.  Converting your ISAM tables to type MyISAM:\n\n";
    foreach my $table (@isam_tables) {
        print "Converting table $table... ";
        $dbh->do("ALTER TABLE $table TYPE = MYISAM");
        print "done.\n";
    }
    print "\nISAM->MyISAM table conversion done.\n\n";
}


# Get a list of the existing tables (if any) in the database
$sth = $dbh->table_info(undef, undef, undef, "TABLE");
my @tables = @{$dbh->selectcol_arrayref($sth, { Columns => [3] })};
#print 'Tables: ', join " ", @tables, "\n";

# add lines here if you add more --LOCAL-- config vars that end up in the enums:

my $my_severities = '"' . join('", "', @my_severities) . '"';
my $my_priorities = '"' . join('", "', @my_priorities) . '"';
my $my_opsys      = '"' . join('", "', @my_opsys)      . '"';
my $my_platforms  = '"' . join('", "', @my_platforms)  . '"';

# go throught our %table hash and create missing tables
while (my ($tabname, $fielddef) = each %table) {
    next if grep /^$tabname$/, @tables;
    print "Creating table $tabname ...\n";

    # add lines here if you add more --LOCAL-- config vars that end up in
    # the enums:

    $fielddef =~ s/\$my_severities/$my_severities/;
    $fielddef =~ s/\$my_priorities/$my_priorities/;
    $fielddef =~ s/\$my_opsys/$my_opsys/;
    $fielddef =~ s/\$my_platforms/$my_platforms/;

    $dbh->do("CREATE TABLE $tabname (\n$fielddef\n)")
        or die "Could not create table '$tabname'. Please check your '$db_base' access.\n";
}

###########################################################################
# Populate groups table
###########################################################################

sub GroupDoesExist ($)
{
    my ($name) = @_;
    my $sth = $dbh->prepare("SELECT name FROM groups WHERE name='$name'");
    $sth->execute;
    if ($sth->rows) {
        return 1;
    }
    return 0;
}


#
# This subroutine checks if a group exist. If not, it will be automatically
# created with the next available groupid
#

sub AddGroup {
    my ($name, $desc, $userregexp) = @_;
    $userregexp ||= "";

    return if GroupDoesExist($name);
    
    print "Adding group $name ...\n";
    my $sth = $dbh->prepare('INSERT INTO groups
                          (name, description, userregexp, isbuggroup)
                          VALUES (?, ?, ?, ?)');
    $sth->execute($name, $desc, $userregexp, 0);

    $sth = $dbh->prepare("select last_insert_id()");
    $sth->execute();
    my ($last) = $sth->fetchrow_array();
    return $last;
}


###########################################################################
# Populate the list of fields.
###########################################################################

my $headernum = 1;

sub AddFDef ($$$) {
    my ($name, $description, $mailhead) = (@_);

    $name = $dbh->quote($name);
    $description = $dbh->quote($description);

    my $sth = $dbh->prepare("SELECT fieldid FROM fielddefs " .
                            "WHERE name = $name");
    $sth->execute();
    my ($fieldid) = ($sth->fetchrow_array());
    if (!$fieldid) {
        $fieldid = 'NULL';
        $dbh->do("INSERT INTO fielddefs " .
             "(fieldid, name, description, mailhead, sortkey) VALUES " .
             "($fieldid, $name, $description, $mailhead, $headernum)");
        $headernum++;
    } else {
        $dbh->do("UPDATE fielddefs SET name = $name, description = $description, " .
                 "mailhead = $mailhead WHERE fieldid = $fieldid");
    }
}


# Note that all of these entries are unconditional, from when GetFieldID
# used to create an entry if it wasn't found. New fielddef columns should
# be created with their associated schema change.
AddFDef("bug_id", "Bug \#", 1);
AddFDef("short_desc", "Summary", 1);
AddFDef("product", "Product", 1);
AddFDef("version", "Version", 1);
AddFDef("rep_platform", "Platform", 1);
AddFDef("bug_file_loc", "URL", 1);
AddFDef("op_sys", "OS/Version", 1);
AddFDef("bug_status", "Status", 1);
AddFDef("status_whiteboard", "Status Whiteboard", 0);
AddFDef("keywords", "Keywords", 1);
AddFDef("resolution", "Resolution", 0);
AddFDef("bug_severity", "Severity", 1);
AddFDef("priority", "Priority", 1);
AddFDef("component", "Component", 1);
AddFDef("assigned_to", "AssignedTo", 1);
AddFDef("reporter", "ReportedBy", 1);
AddFDef("votes", "Votes", 0);
AddFDef("qa_contact", "QAContact", 1);
AddFDef("cc", "CC", 1);
AddFDef("dependson", "BugsThisDependsOn", 1);
AddFDef("blocked", "OtherBugsDependingOnThis", 1);
AddFDef("attachments.description", "Attachment description", 0);
AddFDef("attachments.thedata", "Attachment data", 0);
AddFDef("attachments.filename", "Attachment filename", 0);
AddFDef("attachments.mimetype", "Attachment mime type", 0);
AddFDef("attachments.ispatch", "Attachment is patch", 0);
AddFDef("attachments.isobsolete", "Attachment is obsolete", 0);
AddFDef("attachments.isprivate", "Attachment is private", 0);

AddFDef("target_milestone", "Target Milestone", 0);
AddFDef("delta_ts", "Last changed date", 0);
AddFDef("(to_days(now()) - to_days(bugs.delta_ts))", "Days since bug changed",
        0);
AddFDef("longdesc", "Comment", 0);
AddFDef("alias", "Alias", 0);
AddFDef("everconfirmed", "Ever Confirmed", 0);
AddFDef("reporter_accessible", "Reporter Accessible", 0);
AddFDef("cclist_accessible", "CC Accessible", 0);
AddFDef("bug_group", "Group", 0);
AddFDef("estimated_time", "Estimated Hours", 1);
AddFDef("remaining_time", "Remaining Hours", 0);

# Oops. Bug 163299
$dbh->do("DELETE FROM fielddefs WHERE name='cc_accessible'");

AddFDef("flagtypes.name", "Flag", 0);
AddFDef("requesters.login_name", "Flag Requester", 0);
AddFDef("setters.login_name", "Flag Setter", 0);
AddFDef("work_time", "Hours Worked", 0);
AddFDef("percentage_complete", "Percentage Complete", 0);

AddFDef("content", "Content", 0);

###########################################################################
# Detect changed local settings
###########################################################################

sub GetFieldDef ($$)
{
    my ($table, $field) = @_;
    my $sth = $dbh->prepare("SHOW COLUMNS FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[0] ne $field;
        return $ref;
   }
}

sub GetIndexDef ($$)
{
    my ($table, $field) = @_;
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    while (my $ref = $sth->fetchrow_arrayref) {
        next if $$ref[2] ne $field;
        return $ref;
   }
}

sub CountIndexes ($)
{
    my ($table) = @_;
    
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    if ( $sth->rows == -1 ) {
      die ("Unexpected response while counting indexes in $table:" .
           " \$sth->rows == -1");
    }
    
    return ($sth->rows);
}

sub DropIndexes ($)
{
    my ($table) = @_;
    my %SEEN;

    # get the list of indexes
    #
    my $sth = $dbh->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    # drop each index
    #
    while ( my $ref = $sth->fetchrow_arrayref) {
      
      # note that some indexes are described by multiple rows in the
      # index table, so we may have already dropped the index described
      # in the current row.
      # 
      next if exists $SEEN{$$ref[2]};

      my $dropSth = $dbh->prepare("ALTER TABLE $table DROP INDEX $$ref[2]");
      $dropSth->execute;
      $dropSth->finish;
      $SEEN{$$ref[2]} = 1;

    }

}
#
# Check if the enums in the bugs table return the same values that are defined
# in the various locally changeable variables. If this is true, then alter the
# table definition.
#

sub CheckEnumField ($$@)
{
    my ($table, $field, @against) = @_;

    my $ref = GetFieldDef($table, $field);
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";
    
    $_ = "enum('" . join("','", @against) . "')";
    if ($$ref[1] ne $_) {
        print "Updating field $field in table $table ...\n";
        $_ .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $field $_");
    }
}



#
# This code changes the enum types of some SQL tables whenever you change
# some --LOCAL-- variables. Once you have a running system, to add new 
# severities, priorities, operating systems and platforms, add them to 
# the localconfig file and then re-run checksetup.pl which will make the 
# necessary changes to your database. Additions to these fields in
# checksetup.pl after the initial installation of bugzilla on a system
# are ignored.
#

CheckEnumField('bugs', 'bug_severity', @my_severities);
CheckEnumField('bugs', 'priority',     @my_priorities);
CheckEnumField('bugs', 'op_sys',       @my_opsys);
CheckEnumField('bugs', 'rep_platform', @my_platforms);


###########################################################################
# Create initial test product if there are no products present.
###########################################################################
$sth = $dbh->prepare("SELECT description FROM products");
$sth->execute;
unless ($sth->rows) {
    print "Creating initial dummy product 'TestProduct' ...\n";
    $dbh->do('INSERT INTO products(name, description, milestoneurl, disallownew, votesperuser, votestoconfirm) VALUES ("TestProduct",
              "This is a test product.  This ought to be blown away and ' .
             'replaced with real stuff in a finished installation of ' .
             'bugzilla.", "", 0, 0, 0)');
    # We could probably just assume that this is "1", but better
    # safe than sorry...
    $sth = $dbh->prepare("SELECT LAST_INSERT_ID()");
    $sth->execute;
    my ($product_id) = $sth->fetchrow_array;
    $dbh->do(qq{INSERT INTO versions (value, product_id) VALUES ("other", $product_id)});
    # note: since admin user is not yet known, components gets a 0 for 
    # initialowner and this is fixed during final checks.
    $dbh->do("INSERT INTO components (name, product_id, description, initialowner, initialqacontact)
             VALUES (" .
             "'TestComponent', $product_id, " .
             "'This is a test component in the test product database.  " .
             "This ought to be blown away and replaced with real stuff in " .
             "a finished installation of Bugzilla.', 0, 0)");
    $dbh->do(qq{INSERT INTO milestones (product_id, value) VALUES ($product_id,"---")});
}




###########################################################################
# Update the tables to the current definition
###########################################################################

#
# As time passes, fields in tables get deleted, added, changed and so on.
# So we need some helper subroutines to make this possible:
#

sub ChangeFieldType ($$$)
{
    my ($table, $field, $newtype) = @_;

    my $ref = GetFieldDef($table, $field);
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

    my $oldtype = $ref->[1];
    if (! $ref->[2]) {
        $oldtype .= qq{ not null};
    }
    if ($ref->[4]) {
        $oldtype .= qq{ default "$ref->[4]"};
    }

    if ($oldtype ne $newtype) {
        print "Updating field type $field in table $table ...\n";
        print "old: $oldtype\n";
        print "new: $newtype\n";
#        'not null' should be passed as part of the call to ChangeFieldType()
#        $newtype .= " NOT NULL" if $$ref[3];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $field $newtype");
    }
}

sub RenameField ($$$)
{
    my ($table, $field, $newname) = @_;

    my $ref = GetFieldDef($table, $field);
    return unless $ref; # already fixed?
    #print "0: $$ref[0]   1: $$ref[1]   2: $$ref[2]   3: $$ref[3]  4: $$ref[4]\n";

    if ($$ref[1] ne $newname) {
        print "Updating field $field in table $table ...\n";
        my $type = $$ref[1];
        $type .= " NOT NULL" if !$$ref[2];
        $dbh->do("ALTER TABLE $table
                  CHANGE $field
                  $newname $type");
    }
}

sub AddField ($$$)
{
    my ($table, $field, $definition) = @_;

    my $ref = GetFieldDef($table, $field);
    return if $ref; # already added?

    print "Adding new field $field to table $table ...\n";
    $dbh->do("ALTER TABLE $table
              ADD COLUMN $field $definition");
}

sub DropField ($$)
{
    my ($table, $field) = @_;

    my $ref = GetFieldDef($table, $field);
    return unless $ref; # already dropped?

    print "Deleting unused field $field from table $table ...\n";
    $dbh->do("ALTER TABLE $table
              DROP COLUMN $field");
}

# this uses a mysql specific command. 
sub TableExists ($)
{
   my ($table) = @_;
   my @tables;
   my $dbtable;
   my $exists = 0;
   my $sth = $dbh->prepare("SHOW TABLES");
   $sth->execute;
   while ( ($dbtable) = $sth->fetchrow_array ) {
      if ($dbtable eq $table) {
         $exists = 1;
      } 
   } 
   return $exists;
}   


# really old fields that were added before checksetup.pl existed
# but aren't in very old bugzilla's (like 2.1)
# Steve Stock (sstock@iconnect-inc.com)

# bug 157756 - groupsets replaced by maps
# AddField('bugs', 'groupset', 'bigint not null'); 
AddField('bugs', 'target_milestone', 'varchar(20) not null default "---"');
AddField('bugs', 'qa_contact', 'mediumint not null');
AddField('bugs', 'status_whiteboard', 'mediumtext not null');
AddField('products', 'disallownew', 'tinyint not null');
AddField('products', 'milestoneurl', 'tinytext not null');
AddField('components', 'initialqacontact', 'tinytext not null');
AddField('components', 'description', 'mediumtext not null');

# 1999-06-22 Added an entry to the attachments table to record who the
# submitter was.  Nothing uses this yet, but it still should be recorded.

AddField('attachments', 'submitter_id', 'mediumint not null');

#
# One could even populate this field automatically, e.g. with
#
# unless (GetField('attachments', 'submitter_id') {
#    AddField ...
#    populate
# }
#
# For now I was too lazy, so you should read the documentation :-)



# 1999-9-15 Apparently, newer alphas of MySQL won't allow you to have "when"
# as a column name.  So, I have had to rename a column in the bugs_activity
# table.

RenameField ('bugs_activity', 'when', 'bug_when');



# 1999-10-11 Restructured voting database to add a cached value in each bug
# recording how many total votes that bug has.  While I'm at it, I removed
# the unused "area" field from the bugs database.  It is distressing to
# realize that the bugs table has reached the maximum number of indices
# allowed by MySQL (16), which may make future enhancements awkward.
# (P.S. All is not lost; it appears that the latest betas of MySQL support
# a new table format which will allow 32 indices.)

DropField('bugs', 'area');
AddField('bugs',     'votes',        'mediumint not null, add index (votes)');
AddField('products', 'votesperuser', 'mediumint not null');



# The product name used to be very different in various tables.
#
# It was   varchar(16)   in bugs
#          tinytext      in components
#          tinytext      in products
#          tinytext      in versions
#
# tinytext is equivalent to varchar(255), which is quite huge, so I change
# them all to varchar(64).

# Only do this if these fields still exist - they're removed below, in
# a later change
if (GetFieldDef('products', 'product')) {
    ChangeFieldType ('bugs',       'product', 'varchar(64) not null');
    ChangeFieldType ('components', 'program', 'varchar(64)');
    ChangeFieldType ('products',   'product', 'varchar(64)');
    ChangeFieldType ('versions',   'program', 'varchar(64) not null');
}

# 2000-01-16 Added a "keywords" field to the bugs table, which
# contains a string copy of the entries of the keywords table for this
# bug.  This is so that I can easily sort and display a keywords
# column in bug lists.

if (!GetFieldDef('bugs', 'keywords')) {
    AddField('bugs', 'keywords', 'mediumtext not null');

    my @kwords;
    print "Making sure 'keywords' field of table 'bugs' is empty ...\n";
    $dbh->do("UPDATE bugs SET delta_ts = delta_ts, keywords = '' " .
             "WHERE keywords != ''");
    print "Repopulating 'keywords' field of table 'bugs' ...\n";
    my $sth = $dbh->prepare("SELECT keywords.bug_id, keyworddefs.name " .
                            "FROM keywords, keyworddefs " .
                            "WHERE keyworddefs.id = keywords.keywordid " .
                            "ORDER BY keywords.bug_id, keyworddefs.name");
    $sth->execute;
    my @list;
    my $bugid = 0;
    my @row;
    while (1) {
        my ($b, $k) = ($sth->fetchrow_array());
        if (!defined $b || $b ne $bugid) {
            if (@list) {
                $dbh->do("UPDATE bugs SET delta_ts = delta_ts, keywords = " .
                         $dbh->quote(join(', ', @list)) .
                         " WHERE bug_id = $bugid");
            }
            if (!$b) {
                last;
            }
            $bugid = $b;
            @list = ();
        }
        push(@list, $k);
    }
}


# 2000-01-18 Added a "disabledtext" field to the profiles table.  If not
# empty, then this account has been disabled, and this field is to contain
# text describing why.

AddField('profiles', 'disabledtext',  'mediumtext not null');



# 2000-01-20 Added a new "longdescs" table, which is supposed to have all the
# long descriptions in it, replacing the old long_desc field in the bugs 
# table.  The below hideous code populates this new table with things from
# the old field, with ugly parsing and heuristics.

sub WriteOneDesc {
    my ($id, $who, $when, $buffer) = (@_);
    $buffer = trim($buffer);
    if ($buffer eq '') {
        return;
    }
    $dbh->do("INSERT INTO longdescs (bug_id, who, bug_when, thetext) VALUES " .
             "($id, $who, " .  time2str("'%Y/%m/%d %H:%M:%S'", $when) .
             ", " . $dbh->quote($buffer) . ")");
}


if (GetFieldDef('bugs', 'long_desc')) {
    eval("use Date::Parse");
    eval("use Date::Format");
    my $sth = $dbh->prepare("SELECT count(*) FROM bugs");
    $sth->execute();
    my ($total) = ($sth->fetchrow_array);

    print "Populating new long_desc table.  This is slow.  There are $total\n";
    print "bugs to process; a line of dots will be printed for each 50.\n\n";
    $| = 1;

    $dbh->do("LOCK TABLES bugs write, longdescs write, profiles write");

    $dbh->do('DELETE FROM longdescs');

    $sth = $dbh->prepare("SELECT bug_id, creation_ts, reporter, long_desc " .
                         "FROM bugs ORDER BY bug_id");
    $sth->execute();
    my $count = 0;
    while (1) {
        my ($id, $createtime, $reporterid, $desc) = ($sth->fetchrow_array());
        if (!$id) {
            last;
        }
        print ".";
        $count++;
        if ($count % 10 == 0) {
            print " ";
            if ($count % 50 == 0) {
                print "$count/$total (" . int($count * 100 / $total) . "%)\n";
            }
        }
        $desc =~ s/\r//g;
        my $who = $reporterid;
        my $when = str2time($createtime);
        my $buffer = "";
        foreach my $line (split(/\n/, $desc)) {
            $line =~ s/\s+$//g;       # Trim trailing whitespace.
            if ($line =~ /^------- Additional Comments From ([^\s]+)\s+(\d.+\d)\s+-------$/) {
                my $name = $1;
                my $date = str2time($2);
                $date += 59;    # Oy, what a hack.  The creation time is
                                # accurate to the second.  But we the long
                                # text only contains things accurate to the
                                # minute.  And so, if someone makes a comment
                                # within a minute of the original bug creation,
                                # then the comment can come *before* the
                                # bug creation.  So, we add 59 seconds to
                                # the time of all comments, so that they
                                # are always considered to have happened at
                                # the *end* of the given minute, not the
                                # beginning.
                if ($date >= $when) {
                    WriteOneDesc($id, $who, $when, $buffer);
                    $buffer = "";
                    $when = $date;
                    my $s2 = $dbh->prepare("SELECT userid FROM profiles " .
                                           "WHERE login_name = " .
                                           $dbh->quote($name));
                    $s2->execute();
                    ($who) = ($s2->fetchrow_array());
                    if (!$who) {
                        # This username doesn't exist.  Try a special
                        # netscape-only hack (sorry about that, but I don't
                        # think it will hurt any other installations).  We
                        # have many entries in the bugsystem from an ancient
                        # world where the "@netscape.com" part of the loginname
                        # was omitted.  So, look up the user again with that
                        # appended, and use it if it's there.
                        if ($name !~ /\@/) {
                            my $nsname = $name . "\@netscape.com";
                            $s2 =
                                $dbh->prepare("SELECT userid FROM profiles " .
                                              "WHERE login_name = " .
                                              $dbh->quote($nsname));
                            $s2->execute();
                            ($who) = ($s2->fetchrow_array());
                        }
                    }
                            
                    if (!$who) {
                        # This username doesn't exist.  Maybe someone renamed
                        # him or something.  Invent a new profile entry,
                        # disabled, just to represent him.
                        $dbh->do("INSERT INTO profiles " .
                                 "(login_name, cryptpassword," .
                                 " disabledtext) VALUES (" .
                                 $dbh->quote($name) .
                                 ", " . $dbh->quote(Crypt('okthen')) . ", " . 
                                 "'Account created only to maintain database integrity')");
                        $s2 = $dbh->prepare("SELECT LAST_INSERT_ID()");
                        $s2->execute();
                        ($who) = ($s2->fetchrow_array());
                    }
                    next;
                } else {
#                    print "\nDecided this line of bug $id has a date of " .
#                        time2str("'%Y/%m/%d %H:%M:%S'", $date) .
#                            "\nwhich is less than previous line:\n$line\n\n";
                }

            }
            $buffer .= $line . "\n";
        }
        WriteOneDesc($id, $who, $when, $buffer);
    }
                

    print "\n\n";

    DropField('bugs', 'long_desc');

    $dbh->do("UNLOCK TABLES");
}


# 2000-01-18 Added a new table fielddefs that records information about the
# different fields we keep an activity log on.  The bugs_activity table
# now has a pointer into that table instead of recording the name directly.

if (GetFieldDef('bugs_activity', 'field')) {
    AddField('bugs_activity', 'fieldid',
             'mediumint not null, ADD INDEX (fieldid)');
    print "Populating new fieldid field ...\n";

    $dbh->do("LOCK TABLES bugs_activity WRITE, fielddefs WRITE");

    my $sth = $dbh->prepare('SELECT DISTINCT field FROM bugs_activity');
    $sth->execute();
    my %ids;
    while (my ($f) = ($sth->fetchrow_array())) {
        my $q = $dbh->quote($f);
        my $s2 =
            $dbh->prepare("SELECT fieldid FROM fielddefs WHERE name = $q");
        $s2->execute();
        my ($id) = ($s2->fetchrow_array());
        if (!$id) {
            $dbh->do("INSERT INTO fielddefs (name, description) VALUES " .
                     "($q, $q)");
            $s2 = $dbh->prepare("SELECT LAST_INSERT_ID()");
            $s2->execute();
            ($id) = ($s2->fetchrow_array());
        }
        $dbh->do("UPDATE bugs_activity SET fieldid = $id WHERE field = $q");
    }
    $dbh->do("UNLOCK TABLES");

    DropField('bugs_activity', 'field');
}

        

# 2000-01-18 New email-notification scheme uses a new field in the bug to 
# record when email notifications were last sent about this bug.  Also,
# added a user pref whether a user wants to use the brand new experimental
# stuff.
# 2001-04-29 jake@bugzilla.org - The newemailtech field is no longer needed
#   http://bugzilla.mozilla.org/show_bugs.cgi?id=71552

if (!GetFieldDef('bugs', 'lastdiffed')) {
    AddField('bugs', 'lastdiffed', 'datetime not null');
    $dbh->do('UPDATE bugs SET lastdiffed = now(), delta_ts = delta_ts');
}


# 2000-01-22 The "login_name" field in the "profiles" table was not
# declared to be unique.  Sure enough, somehow, I got 22 duplicated entries
# in my database.  This code detects that, cleans up the duplicates, and
# then tweaks the table to declare the field to be unique.  What a pain.

if (GetIndexDef('profiles', 'login_name')->[1]) {
    print "Searching for duplicate entries in the profiles table ...\n";
    while (1) {
        # This code is weird in that it loops around and keeps doing this
        # select again.  That's because I'm paranoid about deleting entries
        # out from under us in the profiles table.  Things get weird if
        # there are *three* or more entries for the same user...
        $sth = $dbh->prepare("SELECT p1.userid, p2.userid, p1.login_name " .
                             "FROM profiles AS p1, profiles AS p2 " .
                             "WHERE p1.userid < p2.userid " .
                             "AND p1.login_name = p2.login_name " .
                             "ORDER BY p1.login_name");
        $sth->execute();
        my ($u1, $u2, $n) = ($sth->fetchrow_array);
        if (!$u1) {
            last;
        }
        print "Both $u1 & $u2 are ids for $n!  Merging $u2 into $u1 ...\n";
        foreach my $i (["bugs", "reporter"],
                       ["bugs", "assigned_to"],
                       ["bugs", "qa_contact"],
                       ["attachments", "submitter_id"],
                       ["bugs_activity", "who"],
                       ["cc", "who"],
                       ["votes", "who"],
                       ["longdescs", "who"]) {
            my ($table, $field) = (@$i);
            print "   Updating $table.$field ...\n";
            my $extra = "";
            if ($table eq "bugs") {
                $extra = ", delta_ts = delta_ts";
            }
            $dbh->do("UPDATE $table SET $field = $u1 $extra " .
                     "WHERE $field = $u2");
        }
        $dbh->do("DELETE FROM profiles WHERE userid = $u2");
    }
    print "OK, changing index type to prevent duplicates in the future ...\n";
    
    $dbh->do("ALTER TABLE profiles DROP INDEX login_name");
    $dbh->do("ALTER TABLE profiles ADD UNIQUE (login_name)");

}    


# 2000-01-24 Added a new field to let people control whether the "My
# bugs" link appears at the bottom of each page.  Also can control
# whether each named query should show up there.

AddField('profiles', 'mybugslink', 'tinyint not null default 1');
AddField('namedqueries', 'linkinfooter', 'tinyint not null');


# 2000-02-12 Added a new state to bugs, UNCONFIRMED.  Added ability to confirm
# a vote via bugs.  Added user bits to control which users can confirm bugs
# by themselves, and which users can edit bugs without their names on them.
# Added a user field which controls which groups a user can put other users 
# into.

my @resolutions = ("", "FIXED", "INVALID", "WONTFIX", "LATER", "REMIND",
                  "DUPLICATE", "WORKSFORME", "MOVED");
CheckEnumField('bugs', 'resolution', @resolutions);

if (($_ = GetFieldDef('components', 'initialowner')) and ($_->[1] eq 'tinytext')) {
    $sth = $dbh->prepare("SELECT program, value, initialowner, initialqacontact FROM components");
    $sth->execute();
    while (my ($program, $value, $initialowner) = $sth->fetchrow_array()) {
        $initialowner =~ s/([\\\'])/\\$1/g; $initialowner =~ s/\0/\\0/g;
        $program =~ s/([\\\'])/\\$1/g; $program =~ s/\0/\\0/g;
        $value =~ s/([\\\'])/\\$1/g; $value =~ s/\0/\\0/g;

        my $s2 = $dbh->prepare("SELECT userid FROM profiles WHERE login_name = '$initialowner'");
        $s2->execute();

        my $initialownerid = $s2->fetchrow_array();

        unless (defined $initialownerid) {
            print "Warning: You have an invalid initial owner '$initialowner' in program '$program', component '$value'!\n";
            $initialownerid = 0;
        }

        my $update = "UPDATE components SET initialowner = $initialownerid ".
            "WHERE program = '$program' AND value = '$value'";
        my $s3 = $dbh->prepare("UPDATE components SET initialowner = $initialownerid ".
                               "WHERE program = '$program' AND value = '$value';");
        $s3->execute();
    }

    ChangeFieldType('components','initialowner','mediumint');
}

if (($_ = GetFieldDef('components', 'initialqacontact')) and ($_->[1] eq 'tinytext')) {
    $sth = $dbh->prepare("SELECT program, value, initialqacontact, initialqacontact FROM components");
    $sth->execute();
    while (my ($program, $value, $initialqacontact) = $sth->fetchrow_array()) {
        $initialqacontact =~ s/([\\\'])/\\$1/g; $initialqacontact =~ s/\0/\\0/g;
        $program =~ s/([\\\'])/\\$1/g; $program =~ s/\0/\\0/g;
        $value =~ s/([\\\'])/\\$1/g; $value =~ s/\0/\\0/g;

        my $s2 = $dbh->prepare("SELECT userid FROM profiles WHERE login_name = '$initialqacontact'");
        $s2->execute();

        my $initialqacontactid = $s2->fetchrow_array();

        unless (defined $initialqacontactid) {
            if ($initialqacontact ne '') {
                print "Warning: You have an invalid initial QA contact '$initialqacontact' in program '$program', component '$value'!\n";
            }
            $initialqacontactid = 0;
        }

        my $update = "UPDATE components SET initialqacontact = $initialqacontactid ".
            "WHERE program = '$program' AND value = '$value'";
        my $s3 = $dbh->prepare("UPDATE components SET initialqacontact = $initialqacontactid ".
                               "WHERE program = '$program' AND value = '$value';");
        $s3->execute();
    }

    ChangeFieldType('components','initialqacontact','mediumint');
}



my @states = ("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED", "RESOLVED",
              "VERIFIED", "CLOSED");
CheckEnumField('bugs', 'bug_status', @states);

if (!GetFieldDef('bugs', 'everconfirmed')) {
    AddField('bugs', 'everconfirmed',  'tinyint not null');
    $dbh->do("UPDATE bugs SET everconfirmed = 1, delta_ts = delta_ts");
}
AddField('products', 'maxvotesperbug', 'smallint not null default 10000');
AddField('products', 'votestoconfirm', 'smallint not null');
# bug 157756 - groupsets replaced by maps
# AddField('profiles', 'blessgroupset', 'bigint not null');

# 2000-03-21 Adding a table for target milestones to 
# database - matthew@zeroknowledge.com

$sth = $dbh->prepare("SELECT count(*) from milestones");
$sth->execute();
if (!($sth->fetchrow_arrayref()->[0])) {
    print "Replacing blank milestones...\n";
    $dbh->do("UPDATE bugs SET target_milestone = '---', delta_ts=delta_ts WHERE target_milestone = ' '");
    
# Populate milestone table with all exisiting values in database
    $sth = $dbh->prepare("SELECT DISTINCT target_milestone, product FROM bugs");
    $sth->execute();
    
    print "Populating milestones table...\n";
    
    my $value;
    my $product;
    while(($value, $product) = $sth->fetchrow_array())
    {
        # check if the value already exists
        my $sortkey = substr($value, 1);
        if ($sortkey !~ /^\d+$/) {
            $sortkey = 0;
        } else {
            $sortkey *= 10;
        }
        $value = $dbh->quote($value);
        $product = $dbh->quote($product);
        my $s2 = $dbh->prepare("SELECT value FROM milestones WHERE value = $value AND product = $product");
        $s2->execute();
        
        if(!$s2->fetchrow_array())
        {
            $dbh->do("INSERT INTO milestones(value, product, sortkey) VALUES($value, $product, $sortkey)");
        }
    }
}

# 2000-03-22 Changed the default value for target_milestone to be "---"
# (which is still not quite correct, but much better than what it was 
# doing), and made the size of the value field in the milestones table match
# the size of the target_milestone field in the bugs table.

ChangeFieldType('bugs', 'target_milestone',
                'varchar(20) not null default "---"');
ChangeFieldType('milestones', 'value', 'varchar(20) not null');


# 2000-03-23 Added a defaultmilestone field to the products table, so that
# we know which milestone to initially assign bugs to.

if (!GetFieldDef('products', 'defaultmilestone')) {
    AddField('products', 'defaultmilestone',
             'varchar(20) not null default "---"');
    $sth = $dbh->prepare("SELECT product, defaultmilestone FROM products");
    $sth->execute();
    while (my ($product, $defaultmilestone) = $sth->fetchrow_array()) {
        $product = $dbh->quote($product);
        $defaultmilestone = $dbh->quote($defaultmilestone);
        my $s2 = $dbh->prepare("SELECT value FROM milestones " .
                               "WHERE value = $defaultmilestone " .
                               "AND product = $product");
        $s2->execute();
        if (!$s2->fetchrow_array()) {
            $dbh->do("INSERT INTO milestones(value, product) " .
                     "VALUES ($defaultmilestone, $product)");
        }
    }
}

# 2000-03-24 Added unique indexes into the cc and keyword tables.  This
# prevents certain database inconsistencies, and, moreover, is required for
# new generalized list code to work.

if ( CountIndexes('cc') != 3 ) {

    # XXX should eliminate duplicate entries before altering
    #
    print "Recreating indexes on cc table.\n";
    DropIndexes('cc');
    $dbh->do("ALTER TABLE cc ADD UNIQUE (bug_id,who)");
    $dbh->do("ALTER TABLE cc ADD INDEX (who)");
}    

if ( CountIndexes('keywords') != 3 ) {

    # XXX should eliminate duplicate entries before altering
    #
    print "Recreating indexes on keywords table.\n";
    DropIndexes('keywords');
    $dbh->do("ALTER TABLE keywords ADD INDEX (keywordid)");
    $dbh->do("ALTER TABLE keywords ADD UNIQUE (bug_id,keywordid)");

}    

# 2000-07-15 Added duplicates table so Bugzilla tracks duplicates in a better 
# way than it used to. This code searches the comments to populate the table
# initially. It's executed if the table is empty; if it's empty because there
# are no dupes (as opposed to having just created the table) it won't have
# any effect anyway, so it doesn't matter.
$sth = $dbh->prepare("SELECT count(*) from duplicates");
$sth->execute();
if (!($sth->fetchrow_arrayref()->[0])) {
        # populate table
        print("Populating duplicates table...\n") unless $silent;

        $sth = $dbh->prepare("SELECT longdescs.bug_id, thetext FROM longdescs left JOIN bugs using(bug_id) WHERE (thetext " . 
                "regexp '[.*.]{3,3} This bug has been marked as a duplicate of [[:digit:]]{1,5} [.*.]{3,3}') AND (resolution = 'DUPLICATE') ORDER" .
                        " BY longdescs.bug_when");
        $sth->execute();

        my %dupes;
        my $key;

        # Because of the way hashes work, this loop removes all but the last dupe
        # resolution found for a given bug.
        while (my ($dupe, $dupe_of) = $sth->fetchrow_array()) {
                $dupes{$dupe} = $dupe_of;
        }

        foreach $key (keys(%dupes))
        {
                $dupes{$key} =~ /^.*\*\*\* This bug has been marked as a duplicate of (\d+) \*\*\*$/ms;
                $dupes{$key} = $1;
                $dbh->do("INSERT INTO duplicates VALUES('$dupes{$key}', '$key')");
                #                                        BugItsADupeOf   Dupe
        }
}

# 2000-12-18.  Added an 'emailflags' field for storing preferences about
# when email gets sent on a per-user basis.
if (!GetFieldDef('profiles', 'emailflags')) {
    AddField('profiles', 'emailflags', 'mediumtext');
}

# 2000-11-27 For Bugzilla 2.5 and later. Change table 'comments' to 
# 'longdescs' - the new name of the comments table.
if (&TableExists('comments')) {
    RenameField ('comments', 'when', 'bug_when');
    ChangeFieldType('comments', 'bug_id', 'mediumint not null');
    ChangeFieldType('comments', 'who', 'mediumint not null');
    ChangeFieldType('comments', 'bug_when', 'datetime not null');
    RenameField('comments','comment','thetext');
    # Here we rename comments to longdescs
    $dbh->do("DROP TABLE longdescs");
    $dbh->do("ALTER TABLE comments RENAME longdescs");
}

# 2001-04-08 Added a special directory for the duplicates stats.
unless (-d "$datadir/duplicates") {
    print "Creating duplicates directory...\n";
    mkdir "$datadir/duplicates", 0770; 
    if ($my_webservergroup eq "") {
        chmod 01777, "$datadir/duplicates";
    } 
}

#
# 2001-04-10 myk@mozilla.org:
# isactive determines whether or not a group is active.  An inactive group
# cannot have bugs added to it.  Deactivation is a much milder form of
# deleting a group that allows users to continue to work on bugs in the group
# without enabling them to extend the life of the group by adding bugs to it.
# http://bugzilla.mozilla.org/show_bug.cgi?id=75482
#
AddField('groups', 'isactive', 'tinyint not null default 1');

#
# 2001-06-15 myk@mozilla.org:
# isobsolete determines whether or not an attachment is pertinent/relevant/valid.
#
AddField('attachments', 'isobsolete', 'tinyint not null default 0');

# 2001-04-29 jake@bugzilla.org - Remove oldemailtech
#   http://bugzilla.mozilla.org/show_bugs.cgi?id=71552
if (-d 'shadow') {
    print "Removing shadow directory...\n";
    unlink glob("shadow/*");
    unlink glob("shadow/.*");
    rmdir "shadow";
}
DropField("profiles", "emailnotification");
DropField("profiles", "newemailtech");

# 2001-06-12; myk@mozilla.org; bugs 74032, 77473:
# Recrypt passwords using Perl &crypt instead of the mysql equivalent
# and delete plaintext passwords from the database.
if ( GetFieldDef('profiles', 'password') ) {
    
    print <<ENDTEXT;
Your current installation of Bugzilla stores passwords in plaintext 
in the database and uses mysql's encrypt function instead of Perl's 
crypt function to crypt passwords.  Passwords are now going to be 
re-crypted with the Perl function, and plaintext passwords will be 
deleted from the database.  This could take a while if your  
installation has many users. 
ENDTEXT

    # Re-crypt everyone's password.
    my $sth = $dbh->prepare("SELECT userid, password FROM profiles");
    $sth->execute();

    my $i = 1;

    print "Fixing password #1... ";
    while (my ($userid, $password) = $sth->fetchrow_array()) {
        my $cryptpassword = $dbh->quote(Crypt($password));
        $dbh->do("UPDATE profiles SET cryptpassword = $cryptpassword WHERE userid = $userid");
        ++$i;
        # Let the user know where we are at every 500 records.
        print "$i... " if !($i%500);
    }
    print "$i... Done.\n";

    # Drop the plaintext password field and resize the cryptpassword field.
    DropField('profiles', 'password');
    ChangeFieldType('profiles', 'cryptpassword', 'varchar(34)');

}

#
# 2001-06-06 justdave@syndicomm.com:
# There was no index on the 'who' column in the long descriptions table.
# This caused queries by who posted comments to take a LONG time.
#   http://bugzilla.mozilla.org/show_bug.cgi?id=57350
if (!defined GetIndexDef('longdescs','who')) {
    print "Adding index for who column in longdescs table...\n";
    $dbh->do('ALTER TABLE longdescs ADD INDEX (who)');
}

# 2001-06-15 kiko@async.com.br - Change bug:version size to avoid
# truncates re http://bugzilla.mozilla.org/show_bug.cgi?id=9352
ChangeFieldType('bugs', 'version','varchar(64) not null');

# 2001-07-20 jake@bugzilla.org - Change bugs_activity to only record changes
#  http://bugzilla.mozilla.org/show_bug.cgi?id=55161
if (GetFieldDef('bugs_activity', 'oldvalue')) {
    AddField("bugs_activity", "removed", "tinytext");
    AddField("bugs_activity", "added", "tinytext");

    # Need to get fieldid's for the fields that have multipule values
    my @multi = ();
    foreach my $f ("cc", "dependson", "blocked", "keywords") {
        my $sth = $dbh->prepare("SELECT fieldid FROM fielddefs WHERE name = '$f'");
        $sth->execute();
        my ($fid) = $sth->fetchrow_array();
        push (@multi, $fid);
    } 

    # Now we need to process the bugs_activity table and reformat the data
    my $i = 0;
    print "Fixing activity log ";
    my $sth = $dbh->prepare("SELECT bug_id, who, bug_when, fieldid,
                            oldvalue, newvalue FROM bugs_activity");
    $sth->execute;
    while (my ($bug_id, $who, $bug_when, $fieldid, $oldvalue, $newvalue) = $sth->fetchrow_array()) {
        # print the iteration count every 500 records so the user knows we didn't die
        print "$i..." if !($i++ % 500); 
        # Make sure (old|new)value isn't null (to suppress warnings)
        $oldvalue ||= "";
        $newvalue ||= "";
        my ($added, $removed) = "";
        if (grep ($_ eq $fieldid, @multi)) {
            $oldvalue =~ s/[\s,]+/ /g;
            $newvalue =~ s/[\s,]+/ /g;
            my @old = split(" ", $oldvalue);
            my @new = split(" ", $newvalue);
            my (@add, @remove) = ();
            # Find values that were "added"
            foreach my $value(@new) {
                if (! grep ($_ eq $value, @old)) {
                    push (@add, $value);
                }
            }
            # Find values that were removed
            foreach my $value(@old) {
                if (! grep ($_ eq $value, @new)) {
                    push (@remove, $value);
                }
            }
            $added = join (", ", @add);
            $removed = join (", ", @remove);
            # If we can't determine what changed, put a ? in both fields
            unless ($added || $removed) {
                $added = "?";
                $removed = "?";
            }
            # If the origianl field (old|new)value was full, then this
            # could be incomplete data.
            if (length($oldvalue) == 255 || length($newvalue) == 255) {
                $added = "? $added";
                $removed = "? $removed";
            }
        } else {
            $removed = $oldvalue;
            $added = $newvalue;
        }
        $added = $dbh->quote($added);
        $removed = $dbh->quote($removed);
        $dbh->do("UPDATE bugs_activity SET removed = $removed, added = $added
                  WHERE bug_id = $bug_id AND who = $who
                   AND bug_when = '$bug_when' AND fieldid = $fieldid");
    }
    print ". Done.\n";
    DropField("bugs_activity", "oldvalue");
    DropField("bugs_activity", "newvalue");
} 

# 2001-07-24 jake@bugzilla.org - disabledtext was being handled inconsitantly
# http://bugzilla.mozilla.org/show_bug.cgi?id=90933
ChangeFieldType("profiles", "disabledtext", "mediumtext not null");

# 2001-07-26 myk@mozilla.org bug39816: 
# Add fields to the bugs table that record whether or not the reporter,
# assignee, QA contact, and users on the cc: list can see bugs even when
# they are not members of groups to which the bugs are restricted.
# 2002-02-06 bbaetz@student.usyd.edu.au - assignee/qa can always see the bug
AddField("bugs", "reporter_accessible", "tinyint not null default 1");
#AddField("bugs", "assignee_accessible", "tinyint not null default 1");
#AddField("bugs", "qacontact_accessible", "tinyint not null default 1");
AddField("bugs", "cclist_accessible", "tinyint not null default 1");

# 2001-08-21 myk@mozilla.org bug84338:
# Add a field for the attachment ID to the bugs_activity table, so installations
# using the attachment manager can record changes to attachments.
AddField("bugs_activity", "attach_id", "mediumint null");

# 2002-02-04 bbaetz@student.usyd.edu.au bug 95732
# Remove logincookies.cryptpassword, and delete entries which become
# invalid
if (GetFieldDef("logincookies", "cryptpassword")) {
    # We need to delete any cookies which are invalid, before dropping the
    # column

    print "Removing invalid login cookies...\n";

    # mysql doesn't support DELETE with multi-table queries, so we have
    # to iterate
    my $sth = $dbh->prepare("SELECT cookie FROM logincookies, profiles " .
                            "WHERE logincookies.cryptpassword != " .
                            "profiles.cryptpassword AND " .
                            "logincookies.userid = profiles.userid");
    $sth->execute();
    while (my ($cookie) = $sth->fetchrow_array()) {
        $dbh->do("DELETE FROM logincookies WHERE cookie = $cookie");
    }

    DropField("logincookies", "cryptpassword");
}

# 2002-02-13 bbaetz@student.usyd.edu.au - bug 97471
# qacontact/assignee should always be able to see bugs,
# so remove their restriction column
if (GetFieldDef("bugs","qacontact_accessible")) {
    print "Removing restrictions on bugs for assignee and qacontact...\n";

    DropField("bugs", "qacontact_accessible");
    DropField("bugs", "assignee_accessible");
}

# 2002-02-20 jeff.hedlund@matrixsi.com - bug 24789 time tracking
AddField("longdescs", "work_time", "decimal(5,2) not null default 0");
AddField("bugs", "estimated_time", "decimal(5,2) not null default 0");
AddField("bugs", "remaining_time", "decimal(5,2) not null default 0");

# 2002-03-15 bbaetz@student.usyd.edu.au - bug 129466
# 2002-05-13 preed@sigkill.com - bug 129446 patch backported to the 
#  BUGZILLA-2_14_1-BRANCH as a security blocker for the 2.14.2 release
# 
# Use the ip, not the hostname, in the logincookies table
if (GetFieldDef("logincookies", "hostname")) {
    # We've changed what we match against, so all entries are now invalid
    $dbh->do("DELETE FROM logincookies");

    # Now update the logincookies schema
    DropField("logincookies", "hostname");
    AddField("logincookies", "ipaddr", "varchar(40) NOT NULL");
}

# 2002-08-19 - bugreport@peshkin.net bug 143826
# Add private comments and private attachments on less-private bugs
AddField('longdescs', 'isprivate', 'tinyint not null default 0');
AddField('attachments', 'isprivate', 'tinyint not null default 0');


# 2002-07-03 myk@mozilla.org bug99203:
# Add a bug alias field to the bugs table so bugs can be referenced by alias
# in addition to ID.
if (!GetFieldDef("bugs", "alias")) {
    AddField("bugs", "alias", "VARCHAR(20)");
    $dbh->do("ALTER TABLE bugs ADD UNIQUE (alias)");
}

# 2002-07-15 davef@tetsubo.com - bug 67950
# Move quips to the db.
if (-r "$datadir/comments" && -s "$datadir/comments"
    && open (COMMENTS, "<$datadir/comments")) {
    print "Populating quips table from $datadir/comments...\n\n";
    while (<COMMENTS>) {
        chomp;
        $dbh->do("INSERT INTO quips (quip) VALUES ("
                 . $dbh->quote($_) . ")");
    }
    print "The $datadir/comments file (used to store quips) has been copied into\n" .
      "the database, and the $datadir/comments file moved to $datadir/comments.bak - \n" .
      "you can delete this fileonce you're satisfied the migration worked\n" .
      "correctly.\n\n";
    close COMMENTS;
    rename("$datadir/comments", "$datadir/comments.bak");
}

# 2002-07-31 bbaetz@student.usyd.edu.au bug 158236
# Remove unused column
if (GetFieldDef("namedqueries", "watchfordiffs")) {
    DropField("namedqueries", "watchfordiffs");
}

# 2002-08-12 jake@bugzilla.org/bbaetz@student.usyd.edu.au - bug 43600
# Use integer IDs for products and components.
if (GetFieldDef("products", "product")) {
    print "Updating database to use product IDs.\n";

    # First, we need to remove possible NULL entries
    # NULLs may exist, but won't have been used, since all the uses of them
    # are in NOT NULL fields in other tables
    $dbh->do("DELETE FROM products WHERE product IS NULL");
    $dbh->do("DELETE FROM components WHERE value IS NULL");

    AddField("products", "id", "smallint not null auto_increment primary key");
    AddField("components", "product_id", "smallint not null");
    AddField("versions", "product_id", "smallint not null");
    AddField("milestones", "product_id", "smallint not null");
    AddField("bugs", "product_id", "smallint not null");
    # The attachstatusdefs table was added in version 2.15, but removed again
    # in early 2.17.  If it exists now, we still need to perform this change
    # with product_id because the code further down which converts the
    # attachment statuses to flags depends on it.  But we need to avoid this
    # if the user is upgrading from 2.14 or earlier (because it won't be
    # there to convert).
    AddField("attachstatusdefs", "product_id", "smallint not null") if TableExists("attachstatusdefs");
    my %products;
    my $sth = $dbh->prepare("SELECT id, product FROM products");
    $sth->execute;
    while (my ($product_id, $product) = $sth->fetchrow_array()) {
        if (exists $products{$product}) {
            print "Ignoring duplicate product $product\n";
            $dbh->do("DELETE FROM products WHERE id = $product_id");
            next;
        }
        $products{$product} = 1;
        $dbh->do("UPDATE components SET product_id = $product_id " .
                 "WHERE program = " . $dbh->quote($product));
        $dbh->do("UPDATE versions SET product_id = $product_id " .
                 "WHERE program = " . $dbh->quote($product));
        $dbh->do("UPDATE milestones SET product_id = $product_id " .
                 "WHERE product = " . $dbh->quote($product));
        $dbh->do("UPDATE bugs SET product_id = $product_id, delta_ts=delta_ts " .
                 "WHERE product = " . $dbh->quote($product));
        $dbh->do("UPDATE attachstatusdefs SET product_id = $product_id " .
                 "WHERE product = " . $dbh->quote($product)) if TableExists("attachstatusdefs");
    }

    print "Updating the database to use component IDs.\n";
    AddField("components", "id", "smallint not null auto_increment primary key");
    AddField("bugs", "component_id", "smallint not null");
    my %components;
    $sth = $dbh->prepare("SELECT id, value, product_id FROM components");
    $sth->execute;
    while (my ($component_id, $component, $product_id) = $sth->fetchrow_array()) {
        if (exists $components{$component}) {
            if (exists $components{$component}{$product_id}) {
                print "Ignoring duplicate component $component for product $product_id\n";
                $dbh->do("DELETE FROM components WHERE id = $component_id");
                next;
            }
        } else {
            $components{$component} = {};
        }
        $components{$component}{$product_id} = 1;
        $dbh->do("UPDATE bugs SET component_id = $component_id, delta_ts=delta_ts " .
                 "WHERE component = " . $dbh->quote($component) .
                 " AND product_id = $product_id");
    }
    print "Fixing Indexes and Uniqueness.\n";
    $dbh->do("ALTER TABLE milestones DROP INDEX product");
    $dbh->do("ALTER TABLE milestones ADD UNIQUE (product_id, value)");
    $dbh->do("ALTER TABLE bugs DROP INDEX product");
    $dbh->do("ALTER TABLE bugs ADD INDEX (product_id)");
    $dbh->do("ALTER TABLE bugs DROP INDEX component");
    $dbh->do("ALTER TABLE bugs ADD INDEX (component_id)");

    print "Removing, renaming, and retyping old product and component fields.\n";
    DropField("components", "program");
    DropField("versions", "program");
    DropField("milestones", "product");
    DropField("bugs", "product");
    DropField("bugs", "component");
    DropField("attachstatusdefs", "product") if TableExists("attachstatusdefs");
    RenameField("products", "product", "name");
    ChangeFieldType("products", "name", "varchar(64) not null");
    RenameField("components", "value", "name");
    ChangeFieldType("components", "name", "varchar(64) not null");

    print "Adding indexes for products and components tables.\n";
    $dbh->do("ALTER TABLE products ADD UNIQUE (name)");
    $dbh->do("ALTER TABLE components ADD UNIQUE (product_id, name)");
    $dbh->do("ALTER TABLE components ADD INDEX (name)");
}

# 2002-08-14 - bbaetz@student.usyd.edu.au - bug 153578
# attachments creation time needs to be a datetime, not a timestamp
my $fielddef;
if (($fielddef = GetFieldDef("attachments", "creation_ts")) &&
    $fielddef->[1] =~ /^timestamp/) {
    print "Fixing creation time on attachments...\n";

    my $sth = $dbh->prepare("SELECT COUNT(attach_id) FROM attachments");
    $sth->execute();
    my ($attach_count) = $sth->fetchrow_array();

    if ($attach_count > 1000) {
        print "This may take a while...\n";
    }
    my $i = 0;

    # This isn't just as simple as changing the field type, because
    # the creation_ts was previously updated when an attachment was made
    # obsolete from the attachment creation screen. So we have to go
    # and recreate these times from the comments..
    $sth = $dbh->prepare("SELECT bug_id, attach_id, submitter_id " .
                         "FROM attachments");
    $sth->execute();

    # Restrict this as much as possible in order to avoid false positives, and
    # keep the db search time down
    my $sth2 = $dbh->prepare("SELECT bug_when FROM longdescs WHERE bug_id=? AND who=? AND thetext LIKE ? ORDER BY bug_when LIMIT 1");
    while (my ($bug_id, $attach_id, $submitter_id) = $sth->fetchrow_array()) {
        $sth2->execute($bug_id, $submitter_id, "Created an attachment (id=$attach_id)%");
        my ($when) = $sth2->fetchrow_array();
        if ($when) {
            $dbh->do("UPDATE attachments SET creation_ts='$when' WHERE attach_id=$attach_id");
        } else {
            print "Warning - could not determine correct creation time for attachment $attach_id on bug $bug_id\n";
        }
        ++$i;
        print "Converted $i of $attach_count attachments\n" if !($i % 1000);
    }
    print "Done - converted $i attachments\n";

    ChangeFieldType("attachments", "creation_ts", "datetime NOT NULL");
}

# 2002-09-22 - bugreport@peshkin.net - bug 157756
#
# If the whole groups system is new, but the installation isn't, 
# convert all the old groupset groups, etc...
#
# This requires:
# 1) define groups ids in group table
# 2) populate user_group_map with grants from old groupsets and blessgroupsets
# 3) populate bug_group_map with data converted from old bug groupsets
# 4) convert activity logs to use group names instead of numbers
# 5) identify the admin from the old all-ones groupset
#
# ListBits(arg) returns a list of UNKNOWN<n> if the group
# has been deleted for all bits set in arg. When the activity
# records are converted from groupset numbers to lists of
# group names, ListBits is used to fill in a list of references
# to groupset bits for groups that no longer exist.
# 
sub ListBits {
    my ($num) = @_;
    my @res = ();
    my $curr = 1;
    while (1) {
        # Convert a big integer to a list of bits 
        my $sth = $dbh->prepare("SELECT ($num & ~$curr) > 0, 
                                        ($num & $curr), 
                                        ($num & ~$curr), 
                                        $curr << 1");
        $sth->execute;
        my ($more, $thisbit, $remain, $nval) = $sth->fetchrow_array;
        push @res,"UNKNOWN<$curr>" if ($thisbit);
        $curr = $nval;
        $num = $remain;
        last if (!$more);
    }
    return @res;
}

my @admins = ();
# The groups system needs to be converted if groupset exists
if (GetFieldDef("profiles", "groupset")) {
    AddField('groups', 'last_changed', 'datetime not null');
    # Some mysql versions will promote any unique key to primary key
    # so all unique keys are removed first and then added back in
    $dbh->do("ALTER TABLE groups DROP INDEX bit") if GetIndexDef("groups","bit");
    $dbh->do("ALTER TABLE groups DROP INDEX name") if GetIndexDef("groups","name");
    $dbh->do("ALTER TABLE groups DROP PRIMARY KEY"); 
    AddField('groups', 'id', 'mediumint not null auto_increment primary key');
    $dbh->do("ALTER TABLE groups ADD UNIQUE (name)");
    AddField('profiles', 'refreshed_when', 'datetime not null');

    # Convert all existing groupset records to map entries before removing
    # groupset fields or removing "bit" from groups.
    $sth = $dbh->prepare("SELECT bit, id FROM groups
                WHERE bit > 0");
    $sth->execute();
    while (my ($bit, $gid) = $sth->fetchrow_array) {
        # Create user_group_map membership grants for old groupsets.
        # Get each user with the old groupset bit set
        my $sth2 = $dbh->prepare("SELECT userid FROM profiles
                   WHERE (groupset & $bit) != 0");
        $sth2->execute();
        while (my ($uid) = $sth2->fetchrow_array) {
            # Check to see if the user is already a member of the group
            # and, if not, insert a new record.
            my $query = "SELECT user_id FROM user_group_map 
                WHERE group_id = $gid AND user_id = $uid 
                AND isbless = 0"; 
            my $sth3 = $dbh->prepare($query);
            $sth3->execute();
            if ( !$sth3->fetchrow_array() ) {
                $dbh->do("INSERT INTO user_group_map
                       (user_id, group_id, isbless, grant_type)
                       VALUES($uid, $gid, 0, " . GRANT_DIRECT . ")");
            }
        }
        # Create user can bless group grants for old groupsets.
        # Get each user with the old blessgroupset bit set
        $sth2 = $dbh->prepare("SELECT userid FROM profiles
                   WHERE (blessgroupset & $bit) != 0");
        $sth2->execute();
        while (my ($uid) = $sth2->fetchrow_array) {
            $dbh->do("INSERT INTO user_group_map
                   (user_id, group_id, isbless, grant_type)
                   VALUES($uid, $gid, 1, " . GRANT_DIRECT . ")");
        }
        # Create bug_group_map records for old groupsets.
        # Get each bug with the old group bit set.
        $sth2 = $dbh->prepare("SELECT bug_id FROM bugs
                   WHERE (groupset & $bit) != 0");
        $sth2->execute();
        while (my ($bug_id) = $sth2->fetchrow_array) {
            # Insert the bug, group pair into the bug_group_map.
            $dbh->do("INSERT INTO bug_group_map
                   (bug_id, group_id)
                   VALUES($bug_id, $gid)");
        }
    }
    # Replace old activity log groupset records with lists of names of groups.
    # Start by defining the bug_group field and getting its id.
    AddFDef("bug_group", "Group", 0);
    $sth = $dbh->prepare("SELECT fieldid FROM fielddefs WHERE name = " . $dbh->quote('bug_group'));
    $sth->execute();
    my ($bgfid) = $sth->fetchrow_array;
    # Get the field id for the old groupset field
    $sth = $dbh->prepare("SELECT fieldid FROM fielddefs WHERE name = " . $dbh->quote('groupset'));
    $sth->execute();
    my ($gsid) = $sth->fetchrow_array;
    # Get all bugs_activity records from groupset changes
    if ($gsid) {
        $sth = $dbh->prepare("SELECT bug_id, bug_when, who, added, removed
                              FROM bugs_activity WHERE fieldid = $gsid");
        $sth->execute();
        while (my ($bug_id, $bug_when, $who, $added, $removed) = $sth->fetchrow_array) {
            $added ||= 0;
            $removed ||= 0;
            # Get names of groups added.
            my $sth2 = $dbh->prepare("SELECT name FROM groups WHERE (bit & $added) != 0 AND (bit & $removed) = 0");
            $sth2->execute();
            my @logadd = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logadd, $n;
            }
            # Get names of groups removed.
            $sth2 = $dbh->prepare("SELECT name FROM groups WHERE (bit & $removed) != 0 AND (bit & $added) = 0");
            $sth2->execute();
            my @logrem = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logrem, $n;
            }
            # Get list of group bits added that correspond to missing groups.
            $sth2 = $dbh->prepare("SELECT ($added & ~BIT_OR(bit)) FROM groups");
            $sth2->execute();
            my ($miss) = $sth2->fetchrow_array;
            if ($miss) {
                push @logadd, ListBits($miss);
                print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id CONTAINS DELETED GROUPS\n";
            }
            # Get list of group bits deleted that correspond to missing groups.
            $sth2 = $dbh->prepare("SELECT ($removed & ~BIT_OR(bit)) FROM groups");
            $sth2->execute();
            ($miss) = $sth2->fetchrow_array;
            if ($miss) {
                push @logrem, ListBits($miss);
                print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id CONTAINS DELETED GROUPS\n";
            }
            my $logr = "";
            my $loga = "";
            $logr = join(", ", @logrem) . '?' if @logrem;
            $loga = join(", ", @logadd) . '?' if @logadd;
            # Replace to old activity record with the converted data.
            $dbh->do("UPDATE bugs_activity SET fieldid = $bgfid, added = " .
                      $dbh->quote($loga) . ", removed = " . 
                      $dbh->quote($logr) .
                      " WHERE bug_id = $bug_id AND bug_when = " . $dbh->quote($bug_when) .
                      " AND who = $who AND fieldid = $gsid");
    
        }
        # Replace groupset changes with group name changes in profiles_activity.
        # Get profiles_activity records for groupset.
        $sth = $dbh->prepare("SELECT userid, profiles_when, who, newvalue, oldvalue
                              FROM profiles_activity WHERE fieldid = $gsid");
        $sth->execute();
        while (my ($uid, $uwhen, $uwho, $added, $removed) = $sth->fetchrow_array) {
            $added ||= 0;
            $removed ||= 0;
            # Get names of groups added.
            my $sth2 = $dbh->prepare("SELECT name FROM groups WHERE (bit & $added) != 0 AND (bit & $removed) = 0");
            $sth2->execute();
            my @logadd = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logadd, $n;
            }
            # Get names of groups removed.
            $sth2 = $dbh->prepare("SELECT name FROM groups WHERE (bit & $removed) != 0 AND (bit & $added) = 0");
            $sth2->execute();
            my @logrem = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logrem, $n;
            }
            my $ladd = "";
            my $lrem = "";
            $ladd = join(", ", @logadd) . '?' if @logadd;
            $lrem = join(", ", @logrem) . '?' if @logrem;
            # Replace profiles_activity record for groupset change with group list.
            $dbh->do("UPDATE profiles_activity SET fieldid = $bgfid, newvalue = " .
                      $dbh->quote($ladd) . ", oldvalue = " . 
                      $dbh->quote($lrem) .
                      " WHERE userid = $uid AND profiles_when = " . 
                      $dbh->quote($uwhen) .
                      " AND who = $uwho AND fieldid = $gsid");
    
        }
    }
    # Identify admin group.
    my $sth = $dbh->prepare("SELECT id FROM groups 
                WHERE name = 'admin'");
    $sth->execute();
    my ($adminid) = $sth->fetchrow_array();
    # find existing admins
    # Don't lose admins from DBs where Bug 157704 applies
    $sth = $dbh->prepare("SELECT userid, (groupset & 65536), login_name FROM profiles 
                WHERE (groupset | 65536) = 9223372036854775807");
    $sth->execute();
    while ( my ($userid, $iscomplete, $login_name) = $sth->fetchrow_array() ) {
        # existing administrators are made members of group "admin"
        print "\nWARNING - $login_name IS AN ADMIN IN SPITE OF BUG 157704\n\n"
            if (!$iscomplete);
        push @admins, $userid;
    }
    DropField('profiles','groupset');
    DropField('profiles','blessgroupset');
    DropField('bugs','groupset');
    DropField('groups','bit');
    $dbh->do("DELETE FROM fielddefs WHERE name = " . $dbh->quote('groupset'));
}

# September 2002 myk@mozilla.org bug 98801
# Convert the attachment statuses tables into flags tables.
if (TableExists("attachstatuses") && TableExists("attachstatusdefs")) {
    print "Converting attachment statuses to flags...\n";
    
    # Get IDs for the old attachment status and new flag fields.
    $sth = $dbh->prepare("SELECT fieldid FROM fielddefs " . 
                         "WHERE name='attachstatusdefs.name'");
    $sth->execute();
    my $old_field_id = $sth->fetchrow_arrayref()->[0] || 0;
    
    $sth = $dbh->prepare("SELECT fieldid FROM fielddefs " . 
                         "WHERE name='flagtypes.name'");
    $sth->execute();
    my $new_field_id = $sth->fetchrow_arrayref()->[0];

    # Convert attachment status definitions to flag types.  If more than one
    # status has the same name and description, it is merged into a single 
    # status with multiple inclusion records.
    $sth = $dbh->prepare("SELECT id, name, description, sortkey, product_id " . 
                         "FROM attachstatusdefs");
    
    # status definition IDs indexed by name/description
    my $def_ids = {};
    
    # merged IDs and the IDs they were merged into.  The key is the old ID,
    # the value is the new one.  This allows us to give statuses the right
    # ID when we convert them over to flags.  This map includes IDs that
    # weren't merged (in this case the old and new IDs are the same), since 
    # it makes the code simpler.
    my $def_id_map = {};
    
    $sth->execute();
    while (my ($id, $name, $desc, $sortkey, $prod_id) = $sth->fetchrow_array()) {
        my $key = $name . $desc;
        if (!$def_ids->{$key}) {
            $def_ids->{$key} = $id;
            my $quoted_name = $dbh->quote($name);
            my $quoted_desc = $dbh->quote($desc);
            $dbh->do("INSERT INTO flagtypes (id, name, description, sortkey, " .
                     "target_type) VALUES ($id, $quoted_name, $quoted_desc, " .
                     "$sortkey, 'a')");
        }
        $def_id_map->{$id} = $def_ids->{$key};
        $dbh->do("INSERT INTO flaginclusions (type_id, product_id) " . 
                "VALUES ($def_id_map->{$id}, $prod_id)");
    }
    
    # Note: even though we've converted status definitions, we still can't drop
    # the table because we need it to convert the statuses themselves.
    
    # Convert attachment statuses to flags.  To do this we select the statuses
    # from the status table and then, for each one, figure out who set it
    # and when they set it from the bugs activity table.
    my $id = 0;
    $sth = $dbh->prepare("SELECT attachstatuses.attach_id, attachstatusdefs.id, " . 
                         "attachstatusdefs.name, attachments.bug_id " . 
                         "FROM attachstatuses, attachstatusdefs, attachments " . 
                         "WHERE attachstatuses.statusid = attachstatusdefs.id " .
                         "AND attachstatuses.attach_id = attachments.attach_id");
    
    # a query to determine when the attachment status was set and who set it
    my $sth2 = $dbh->prepare("SELECT added, who, bug_when " . 
                             "FROM bugs_activity " . 
                             "WHERE bug_id = ? AND attach_id = ? " . 
                             "AND fieldid = $old_field_id " . 
                             "ORDER BY bug_when DESC");
    
    $sth->execute();
    while (my ($attach_id, $def_id, $status, $bug_id) = $sth->fetchrow_array()) {
        ++$id;
        
        # Determine when the attachment status was set and who set it.
        # We should always be able to find out this info from the bug activity,
        # but we fall back to default values just in case.
        $sth2->execute($bug_id, $attach_id);
        my ($added, $who, $when);
        while (($added, $who, $when) = $sth2->fetchrow_array()) {
            last if $added =~ /(^|[, ]+)\Q$status\E([, ]+|$)/;
        }
        $who = $dbh->quote($who); # "NULL" by default if $who is undefined
        $when = $when ? $dbh->quote($when) : "NOW()";
            
        
        $dbh->do("INSERT INTO flags (id, type_id, status, bug_id, attach_id, " .
                 "creation_date, modification_date, requestee_id, setter_id) " . 
                 "VALUES ($id, $def_id_map->{$def_id}, '+', $bug_id, " . 
                 "$attach_id, $when, $when, NULL, $who)");
    }
    
    # Now that we've converted both tables we can drop them.
    $dbh->do("DROP TABLE attachstatuses");
    $dbh->do("DROP TABLE attachstatusdefs");
    
    # Convert activity records for attachment statuses into records for flags.
    my $sth = $dbh->prepare("SELECT attach_id, who, bug_when, added, removed " .
                            "FROM bugs_activity WHERE fieldid = $old_field_id");
    $sth->execute();
    while (my ($attach_id, $who, $when, $old_added, $old_removed) = 
      $sth->fetchrow_array())
    {
        my @additions = split(/[, ]+/, $old_added);
        @additions = map("$_+", @additions);
        my $new_added = $dbh->quote(join(", ", @additions));
        
        my @removals = split(/[, ]+/, $old_removed);
        @removals = map("$_+", @removals);
        my $new_removed = $dbh->quote(join(", ", @removals));
        
        $old_added = $dbh->quote($old_added);
        $old_removed = $dbh->quote($old_removed);
        $who = $dbh->quote($who);
        $when = $dbh->quote($when);
        
        $dbh->do("UPDATE bugs_activity SET fieldid = $new_field_id, " . 
                 "added = $new_added, removed = $new_removed " . 
                 "WHERE attach_id = $attach_id AND who = $who " . 
                 "AND bug_when = $when AND fieldid = $old_field_id " . 
                 "AND added = $old_added AND removed = $old_removed");
    }
    
    # Remove the attachment status field from the field definitions.
    $dbh->do("DELETE FROM fielddefs WHERE name='attachstatusdefs.name'");

    print "done.\n";
}

# 2002-11-24 - bugreport@peshkin.net - bug 147275 
#
# If group_control_map is empty, backward-compatbility 
# usebuggroups-equivalent records should be created.
my $entry = Param('useentrygroupdefault');
$sth = $dbh->prepare("SELECT COUNT(*) FROM group_control_map");
$sth->execute();
my ($mapcnt) = $sth->fetchrow_array();
if ($mapcnt == 0) {
    # Initially populate group_control_map.
    # First, get all the existing products and their groups.
    $sth = $dbh->prepare("SELECT groups.id, products.id, groups.name, " .
                         "products.name FROM groups, products " .
                         "WHERE isbuggroup != 0 AND isactive != 0");
    $sth->execute();
    while (my ($groupid, $productid, $groupname, $productname) 
            = $sth->fetchrow_array()) {
        if ($groupname eq $productname) {
            # Product and group have same name.
            $dbh->do("INSERT INTO group_control_map " .
                     "(group_id, product_id, entry, membercontrol, " .
                     "othercontrol, canedit) " .
                     "VALUES ($groupid, $productid, $entry, " .
                     CONTROLMAPDEFAULT . ", " .
                     CONTROLMAPNA . ", 0)");
        } else {
            # See if this group is a product group at all.
            my $sth2 = $dbh->prepare("SELECT id FROM products WHERE name = " .
                                 $dbh->quote($groupname));
            $sth2->execute();
            my ($id) = $sth2->fetchrow_array();
            if (!$id) {
                # If there is no product with the same name as this
                # group, then it is permitted for all products.
                $dbh->do("INSERT INTO group_control_map " .
                         "(group_id, product_id, entry, membercontrol, " .
                         "othercontrol, canedit) " .
                         "VALUES ($groupid, $productid, 0, " .
                         CONTROLMAPSHOWN . ", " .
                         CONTROLMAPNA . ", 0)");
            }
        }
    }
}

# 2003-06-26 Copy the old charting data into the database, and create the
# queries that will keep it all running. When the old charting system goes
# away, if this code ever runs, it'll just find no files and do nothing.
my $series_exists = $dbh->selectrow_array("SELECT 1 FROM series LIMIT 1");

if (!$series_exists) {
    print "Migrating old chart data into database ...\n" unless $silent;
    
    require Bugzilla::Series;
      
    # We prepare the handle to insert the series data    
    my$seriesdatasth = $dbh->prepare("INSERT INTO series_data " . 
                                     "(series_id, date, value) " . 
                                     "VALUES (?, ?, ?)");
    
    my $deletesth = $dbh->prepare("DELETE FROM series_data 
                                   WHERE series_id = ? AND date = ?");
                                     
    # Fields in the data file (matches the current collectstats.pl)
    my @statuses = 
                qw(NEW ASSIGNED REOPENED UNCONFIRMED RESOLVED VERIFIED CLOSED);
    my @resolutions = 
             qw(FIXED INVALID WONTFIX LATER REMIND DUPLICATE WORKSFORME MOVED);
    my @fields = (@statuses, @resolutions);

    # We have a localisation problem here. Where do we get these values?
    my $all_name = "-All-";
    my $open_name = "All Open";
        
    # We can't give the Series we create a meaningful owner; that's not a big 
    # problem. But we do need to set this global, otherwise Series.pm objects.
    $::userid = 0;
    
    my $products = $dbh->selectall_arrayref("SELECT name FROM products");
     
    foreach my $product ((map { $_->[0] } @$products), "-All-") {
        # First, create the series
        my %queries;
        my %seriesids;
        
        my $query_prod = "";
        if ($product ne "-All-") {
            $query_prod = "product=" . html_quote($product) . "&";
        }
        
        # The query for statuses is different to that for resolutions.
        $queries{$_} = ($query_prod . "bug_status=$_") foreach (@statuses);
        $queries{$_} = ($query_prod . "resolution=$_") foreach (@resolutions);
        
        foreach my $field (@fields) {            
            # Create a Series for each field in this product
            my $series = new Bugzilla::Series(undef, $product, $all_name,
                                              $field, $::userid, 1,
                                              $queries{$field}, 1);
            $series->writeToDatabase();
            $seriesids{$field} = $series->{'series_id'};
        }
        
        # We also add a new query for "Open", so that migrated products get
        # the same set as new products (see editproducts.cgi.)
        my @openedstatuses = ("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED");
        my $query = join("&", map { "bug_status=$_" } @openedstatuses);
        my $series = new Bugzilla::Series(undef, $product, $all_name,
                                          $open_name, $::userid, 1, 
                                          $query_prod . $query, 1);
        $series->writeToDatabase();
        $seriesids{$open_name} = $series->{'series_id'};
        
        # Now, we attempt to read in historical data, if any
        # Convert the name in the same way that collectstats.pl does
        my $product_file = $product;
        $product_file =~ s/\//-/gs;
        $product_file = "$datadir/mining/$product_file";

        # There are many reasons that this might fail (e.g. no stats for this
        # product), so we don't worry if it does.        
        open(IN, $product_file) or next;

        # The data files should be in a standard format, even for old 
        # Bugzillas, because of the conversion code further up this file.
        my %data;
        my $last_date = "";
        
        while (<IN>) {
            if (/^(\d+\|.*)/) {
                my @numbers = split(/\||\r/, $1);
                
                # Only take the first line for each date; it was possible to
                # run collectstats.pl more than once in a day.
                next if $numbers[0] eq $last_date;
                
                for my $i (0 .. $#fields) {
                    # $numbers[0] is the date
                    $data{$fields[$i]}{$numbers[0]} = $numbers[$i + 1];
                    
                    # Keep a total of the number of open bugs for this day
                    if (IsOpenedState($fields[$i])) {
                        $data{$open_name}{$numbers[0]} += $numbers[$i + 1];
                    }
                }
                
                $last_date = $numbers[0];
            }
        }

        close(IN);

        foreach my $field (@fields, $open_name) {            
            # Insert values into series_data: series_id, date, value
            my %fielddata = %{$data{$field}};
            foreach my $date (keys %fielddata) {
                # We need to delete in case the text file had duplicate entries
                # in it.
                $deletesth->execute($seriesids{$field},
                                    $dbh->quote($date));
                         
                # We prepared this above
                $seriesdatasth->execute($seriesids{$field},
                                        $dbh->quote($date), 
                                        $fielddata{$date} || 0);
            }
        }        
    }
}

AddFDef("owner_idle_time", "Time Since Owner Touched", 0);

# 2004-04-12 - Keep regexp-based group permissions up-to-date - Bug 240325
if (GetFieldDef("user_group_map", "isderived")) {
    AddField('user_group_map', 'grant_type', 'tinyint not null default 0');
    $dbh->do("UPDATE user_group_map SET grant_type = " .
                             "IF(isderived, " . GRANT_DERIVED . ", " .
                             GRANT_DIRECT . ")");
    $dbh->do("DELETE FROM user_group_map 
              WHERE isbless = 0 AND grant_type != " . GRANT_DIRECT);
    DropField("user_group_map", "isderived");
    DropIndexes("user_group_map");
    $dbh->do("ALTER TABLE user_group_map 
              ADD UNIQUE (user_id, group_id, grant_type, isbless)");
    # Evaluate regexp-based group memberships
    my $sth = $dbh->prepare("SELECT profiles.userid, profiles.login_name,
                             groups.id, groups.userregexp 
                             FROM profiles, groups
                             WHERE userregexp != ''");
    $sth->execute();
    my $sth2 = $dbh->prepare("INSERT IGNORE INTO user_group_map 
                           (user_id, group_id, isbless, grant_type) 
                           VALUES(?, ?, 0, " . GRANT_REGEXP . ")");
    while (my ($uid, $login, $gid, $rexp) = $sth->fetchrow_array()) {
        if ($login =~ m/$rexp/i) {
            $sth2->execute($uid, $gid);
        }
    }
}

# 2004-07-03 - Make it possible to disable flags without deleting them
# from the database. Bug 223878, jouni@heikniemi.net

AddField('flags', 'is_active', 'tinyint not null default 1');

# 2004-07-16 - Make it possible to have group-group relationships other than
# membership and bless.
if (GetFieldDef("group_group_map", "isbless")) {
    AddField('group_group_map', 'grant_type', 'tinyint not null default 0');
    $dbh->do("UPDATE group_group_map SET grant_type = " .
                             "IF(isbless, " . GROUP_BLESS . ", " .
                             GROUP_MEMBERSHIP . ")");
    DropIndexes("group_group_map");
    DropField("group_group_map", "isbless");
    $dbh->do("ALTER TABLE group_group_map 
              ADD UNIQUE (member_id, grantor_id, grant_type)");
}    

# If you had to change the --TABLE-- definition in any way, then add your
# differential change code *** A B O V E *** this comment.
#
# That is: if you add a new field, you first search for the first occurence
# of --TABLE-- and add your field to into the table hash. This new setting
# would be honored for every new installation. Then add your
# AddField/DropField/ChangeFieldType/RenameField code above. This would then
# be honored by everyone who updates his Bugzilla installation.
#

#
# BugZilla uses --GROUPS-- to assign various rights to its users. 
#

AddGroup('tweakparams', 'Can tweak operating parameters');
AddGroup('editusers', 'Can edit or disable users');
AddGroup('creategroups', 'Can create and destroy groups.');
AddGroup('editcomponents', 'Can create, destroy, and edit components.');
AddGroup('editkeywords', 'Can create, destroy, and edit keywords.');
AddGroup('admin', 'Administrators');


if (!GroupDoesExist("editbugs")) {
    my $id = AddGroup('editbugs', 'Can edit all bug fields.', ".*");
    my $sth = $dbh->prepare("SELECT userid FROM profiles");
    $sth->execute();
    while (my ($userid) = $sth->fetchrow_array()) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $id, 0, " . GRANT_DIRECT . ")");
    }
}

if (!GroupDoesExist("canconfirm")) {
    my $id = AddGroup('canconfirm',  'Can confirm a bug.', ".*");
    my $sth = $dbh->prepare("SELECT userid FROM profiles");
    $sth->execute();
    while (my ($userid) = $sth->fetchrow_array()) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $id, 0, " . GRANT_DIRECT . ")");
    }

}

# Create bz_canusewhineatothers and bz_canusewhines
if (!GroupDoesExist('bz_canusewhines')) {
    my $whine_group = AddGroup('bz_canusewhines',
                               'User can configure whine reports for self');
    my $whineatothers_group = AddGroup('bz_canusewhineatothers',
                                       'Can configure whine reports for ' .
                                       'other users');
    $dbh->do("INSERT IGNORE INTO group_group_map " .
             "(member_id, grantor_id, grant_type) " .
             "VALUES (${whine_group}, ${whineatothers_group}, " .
             GROUP_MEMBERSHIP . ")");
}

###########################################################################
# Create Administrator  --ADMIN--
###########################################################################


sub bailout {   # this is just in case we get interrupted while getting passwd
    if ($^O !~ /MSWin32/i) {
        system("stty","echo"); # re-enable input echoing
    }
    exit 1;
}

if (@admins) {
    # Identify admin group.
    my $sth = $dbh->prepare("SELECT id FROM groups 
                WHERE name = 'admin'");
    $sth->execute();
    my ($adminid) = $sth->fetchrow_array();
    foreach my $userid (@admins) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $adminid, 0, " . GRANT_DIRECT . ")");
        # Existing administrators are made blessers of group "admin"
        # but only explitly defined blessers can bless group admin.
        # Other groups can be blessed by any admin (by default) or additional
        # defined blessers.
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $adminid, 1, " . GRANT_DIRECT . ")");
    }
    $sth = $dbh->prepare("SELECT id FROM groups");
    $sth->execute();
    while ( my ($id) = $sth->fetchrow_array() ) {
        # Admins can bless every group.
        $dbh->do("INSERT INTO group_group_map 
            (member_id, grantor_id, grant_type) 
            VALUES ($adminid, $id," . GROUP_BLESS . ")");
        # Admins can see every group.
        $dbh->do("INSERT INTO group_group_map 
            (member_id, grantor_id, grant_type) 
            VALUES ($adminid, $id," . GROUP_VISIBLE . ")");
        # Admins are initially members of every group.
        next if ($id == $adminid);
        $dbh->do("INSERT INTO group_group_map 
            (member_id, grantor_id, grant_type) 
            VALUES ($adminid, $id," . GROUP_MEMBERSHIP . ")");
    }
}


my @groups = ();
$sth = $dbh->prepare("select id from groups");
$sth->execute();
while ( my @row = $sth->fetchrow_array() ) {
    push (@groups, $row[0]);
}

#  Prompt the user for the email address and name of an administrator.  Create
#  that login, if it doesn't exist already, and make it a member of all groups.

$sth = $dbh->prepare("SELECT user_id FROM groups, user_group_map" .
                    " WHERE name = 'admin' AND id = group_id");
$sth->execute;
# when we have no admin users, prompt for admin email address and password ...
if ($sth->rows == 0) {
  my $login = "";
  my $realname = "";
  my $pass1 = "";
  my $pass2 = "*";
  my $admin_ok = 0;
  my $admin_create = 1;
  my $mailcheckexp = "";
  my $mailcheck    = ""; 

  # Here we look to see what the emailregexp is set to so we can 
  # check the email addy they enter. Bug 96675. If they have no 
  # params (likely but not always the case), we use the default.
  if (-e "$datadir/params") { 
    require "$datadir/params"; # if they have a params file, use that
  }
  if (Param('emailregexp')) {
    $mailcheckexp = Param('emailregexp');
    $mailcheck    = Param('emailregexpdesc');
  } else {
    $mailcheckexp = '^[\\w\\.\\+\\-=]+@[\\w\\.\\-]+\\.[\\w\\-]+$';
    $mailcheck    = 'A legal address must contain exactly one \'@\', 
      and at least one \'.\' after the @.';
  }

  print "\nLooks like we don't have an administrator set up yet.  Either this is your\n";
  print "first time using Bugzilla, or your administrator's privileges might have accidently\n";
  print "been deleted.\n";
  while(! $admin_ok ) {
    while( $login eq "" ) {
      print "Enter the e-mail address of the administrator: ";
      $login = $answer{'ADMIN_EMAIL'} 
          || ($silent && die("cant preload ADMIN_EMAIL")) 
          || <STDIN>;
      chomp $login;
      if(! $login ) {
        print "\nYou DO want an administrator, don't you?\n";
      }
      unless ($login =~ /$mailcheckexp/) {
        print "\nThe login address is invalid:\n";
        print "$mailcheck\n";
        print "You can change this test on the params page once checksetup has successfully\n";
        print "completed.\n\n";
        # Go round, and ask them again
        $login = "";
      }
    }
    $login = $dbh->quote($login);
    $sth = $dbh->prepare("SELECT login_name FROM profiles" .
                        " WHERE login_name=$login");
    $sth->execute;
    if ($sth->rows > 0) {
      print "$login already has an account.\n";
      print "Make this user the administrator? [Y/n] ";
      my $ok = $answer{'ADMIN_OK'} 
          || ($silent && die("cant preload ADMIN_OK")) 
          || <STDIN>;
      chomp $ok;
      if ($ok !~ /^n/i) {
        $admin_ok = 1;
        $admin_create = 0;
      } else {
        print "OK, well, someone has to be the administrator.  Try someone else.\n";
        $login = "";
      }
    } else {
      print "You entered $login.  Is this correct? [Y/n] ";
      my $ok = $answer{'ADMIN_OK'} 
          || ($silent && die("cant preload ADMIN_OK")) 
          || <STDIN>;
      chomp $ok;
      if ($ok !~ /^n/i) {
        $admin_ok = 1;
      } else {
        print "That's okay, typos happen.  Give it another shot.\n";
        $login = "";
      }
    }
  }

  if ($admin_create) {

    while( $realname eq "" ) {
      print "Enter the real name of the administrator: ";
      $realname = $answer{'ADMIN_REALNAME'} 
          || ($silent && die("cant preload ADMIN_REALNAME")) 
          || <STDIN>;
      chomp $realname;
      if(! $realname ) {
        print "\nReally.  We need a full name.\n";
      }
    }

    # trap a few interrupts so we can fix the echo if we get aborted.
    $SIG{HUP}  = \&bailout;
    $SIG{INT}  = \&bailout;
    $SIG{QUIT} = \&bailout;
    $SIG{TERM} = \&bailout;

    if ($^O !~ /MSWin32/i) {
        system("stty","-echo");  # disable input echoing
    }

    while( $pass1 ne $pass2 ) {
      while( $pass1 eq "" || $pass1 !~ /^[[:print:]]{3,16}$/ ) {
        print "Enter a password for the administrator account: ";
        $pass1 = $answer{'ADMIN_PASSWORD'} 
            || ($silent && die("cant preload ADMIN_PASSWORD")) 
            || <STDIN>;
        chomp $pass1;
        if(! $pass1 ) {
          print "\n\nAn empty password is a security risk. Try again!\n";
        } elsif ( $pass1 !~ /^.{3,16}$/ ) {
          print "\n\nThe password must be 3-16 characters in length.\n";
        } elsif ( $pass1 !~ /^[[:print:]]{3,16}$/ ) {
          print "\n\nThe password contains non-printable characters.\n";
        }
      }
      print "\nPlease retype the password to verify: ";
      $pass2 = $answer{'ADMIN_PASSWORD'} 
          || ($silent && die("cant preload ADMIN_PASSWORD")) 
          || <STDIN>;
      chomp $pass2;
      if ($pass1 ne $pass2) {
        print "\n\nPasswords don't match.  Try again!\n";
        $pass1 = "";
        $pass2 = "*";
      }
    }

    # Crypt the administrator's password
    my $cryptedpassword = Crypt($pass1);

    if ($^O !~ /MSWin32/i) {
        system("stty","echo"); # re-enable input echoing
    }

    $SIG{HUP}  = 'DEFAULT'; # and remove our interrupt hooks
    $SIG{INT}  = 'DEFAULT';
    $SIG{QUIT} = 'DEFAULT';
    $SIG{TERM} = 'DEFAULT';

    $realname = $dbh->quote($realname);
    $cryptedpassword = $dbh->quote($cryptedpassword);

    $dbh->do("INSERT INTO profiles (login_name, realname, cryptpassword)" .
            " VALUES ($login, $realname, $cryptedpassword)");
  }
    # Put the admin in each group if not already    
    my $query = "select userid from profiles where login_name = $login";    
    $sth = $dbh->prepare($query); 
    $sth->execute();
    my ($userid) = $sth->fetchrow_array();
   
    foreach my $group (@groups) {
        my $query = "SELECT user_id FROM user_group_map 
            WHERE group_id = $group AND user_id = $userid 
            AND isbless = 0";
        $sth = $dbh->prepare($query);
        $sth->execute();
        if ( !$sth->fetchrow_array() ) {
            $dbh->do("INSERT INTO user_group_map 
                (user_id, group_id, isbless, grant_type) 
                VALUES ($userid, $group, 0, " . GRANT_DIRECT . ")");
        }
    }
    # the admin also gets an explicit bless capability for the admin group
    my $sth = $dbh->prepare("SELECT id FROM groups 
                WHERE name = 'admin'");
    $sth->execute();
    my ($id) = $sth->fetchrow_array();
    $dbh->do("INSERT INTO user_group_map 
        (user_id, group_id, isbless, grant_type) 
        VALUES ($userid, $id, 1, " . GRANT_DIRECT . ")");
    foreach my $group ( @groups ) {
        $dbh->do("INSERT INTO group_group_map
            (member_id, grantor_id, grant_type)
            VALUES ($id, $group, " . GROUP_BLESS . ")");
    }

  print "\n$login is now set up as an administrator account.\n";
}

# Add fulltext indexes for bug summaries and descriptions/comments.
if (!defined GetIndexDef('bugs', 'short_desc')) {
    print "Adding full-text index for short_desc column in bugs table...\n";
    $dbh->do('ALTER TABLE bugs ADD FULLTEXT (short_desc)');
}
if (!defined GetIndexDef('longdescs', 'thetext')) {
    print "Adding full-text index for thetext column in longdescs table...\n";
    $dbh->do('ALTER TABLE longdescs ADD FULLTEXT (thetext)');
}

# 2002 November, myk@mozilla.org, bug 178841:
#
# Convert the "attachments.filename" column from a ridiculously large
# "mediumtext" to a much more sensible "varchar(100)".  Also takes
# the opportunity to remove paths from existing filenames, since they 
# shouldn't be there for security.  Buggy browsers include them, 
# and attachment.cgi now takes them out, but old ones need converting.
#
{
    my $ref = GetFieldDef("attachments", "filename");
    if ($ref->[1] ne 'varchar(100)') {
        print "Removing paths from filenames in attachments table...\n";
        
        $sth = $dbh->prepare("SELECT attach_id, filename FROM attachments " . 
                             "WHERE INSTR(filename, '/') " . 
                             "OR INSTR(filename, '\\\\')");
        $sth->execute;
        
        while (my ($attach_id, $filename) = $sth->fetchrow_array) {
            $filename =~ s/^.*[\/\\]//;
            my $quoted_filename = $dbh->quote($filename);
            $dbh->do("UPDATE attachments SET filename = $quoted_filename " . 
                     "WHERE attach_id = $attach_id");
        }
        
        print "Done.\n";
        
        print "Resizing attachments.filename from mediumtext to varchar(100).\n";
        ChangeFieldType("attachments", "filename", "varchar(100) not null");
    }
}

# 2003-01-11, burnus@net-b.de, bug 184309
# Support for quips approval
AddField('quips', 'approved', 'tinyint(1) NOT NULL  DEFAULT 1');
 
# 2002-12-20 Bug 180870 - remove manual shadowdb replication code
if (TableExists('shadowlog')) {
    print "Removing shadowlog table\n";
    $dbh->do("DROP TABLE shadowlog");
}

# 2003-04-27 - bugzilla@chimpychompy.org (GavinS)
#
# Bug 180086 (http://bugzilla.mozilla.org/show_bug.cgi?id=180086)
#
# Renaming the 'count' column in the votes table because Sybase doesn't
# like it
if (GetFieldDef('votes', 'count')) {
    # 2003-04-24 - myk@mozilla.org/bbaetz@acm.org, bug 201018
    # Force all cached groups to be updated at login, due to security bug
    # Do this here, inside the next schema change block, so that it doesn't
    # get invalidated on every checksetup run.
    $dbh->do("UPDATE profiles SET refreshed_when='1900-01-01 00:00:00'");

    RenameField ('votes', 'count', 'vote_count');
}

# 2004/02/15 - Summaries shouldn't be null - see bug 220232
if (GetFieldDef('bugs', 'short_desc')->[2]) { # if it allows nulls
    $dbh->do("UPDATE bugs SET short_desc = '' WHERE short_desc IS NULL");
    ChangeFieldType('bugs', 'short_desc', 'mediumtext not null');
}

# 2004-04-12 - Keep regexp-based group permissions up-to-date - Bug 240325
# Make sure groups get rederived
$dbh->do("UPDATE groups SET last_changed = NOW() WHERE name = 'admin'");


#
# Final checks...

$sth = $dbh->prepare("SELECT user_id FROM groups, user_group_map" .
                    " WHERE groups.name = 'admin'" .
                    " AND groups.id = user_group_map.group_id");
$sth->execute;
my ($adminuid) = $sth->fetchrow_array;
if (!$adminuid) { die "No administrator!" } # should never get here
# when test product was created, admin was unknown
$dbh->do("UPDATE components SET initialowner = $adminuid WHERE initialowner = 0");

unlink "$datadir/versioncache";

