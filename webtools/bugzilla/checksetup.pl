#!/usr/bonsaitools/bin/perl -w
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


###########################################################################
# Global definitions
###########################################################################

use diagnostics;
use strict;

# 12/17/00 justdave@syndicomm.com - removed declarations of the localconfig
# variables from this location.  We don't want these declared here.  They'll
# automatically get declared in the process of reading in localconfig, and
# this way we can look in the symbol table to see if they've been declared
# yet or not.

# Trim whitespace from front and back.

sub trim {
    ($_) = (@_);
    s/^\s+//g;
    s/\s+$//g;
    return $_;
}



###########################################################################
# Check required module
###########################################################################

#
# Here we check for --MODULES--
#

print "\nChecking perl modules ...\n";
unless (eval "require 5.005") {
    die "Sorry, you need at least Perl 5.005\n";
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
  printf("Checking for %15s %-9s ", $pkg, !$wanted?'(any)':"(v$wanted)");

  eval { my $p; ($p = $pkg . ".pm") =~ s!::!/!g; require $p; };

  $vnum = ${"${pkg}::VERSION"} || ${"${pkg}::Version"} || 0;
  $vnum = -1 if $@;

  if ($vnum < 0) {
    $vstr = "not found";
  }
  elsif ($vnum > 0) {
    $vstr = "found v$vnum";
  }
  else {
    $vstr = "found unknown version";
  }

  my $vok = (vers_cmp($vnum,$wanted) > -1);
  print ((($vok) ? "ok: " : " "), "$vstr\n");
  return $vok;
}

# Check versions of dependencies.  0 for version = any version acceptible

my @missing = ();
unless (have_vers("DBI","1.13"))          { push @missing,"DBI" }
unless (have_vers("Data::Dumper",0))      { push @missing,"Data::Dumper" }
unless (have_vers("DBD::mysql","1.2209")) { push @missing,"DBD::mysql" }
unless (have_vers("Date::Parse",0))       { push @missing,"Date::Parse" }
unless (have_vers("AppConfig","1.52"))    { push @missing,"AppConfig" }
unless (have_vers("Template","2.01"))     { push @missing,"Template" }
unless (have_vers("Text::Wrap","2001.0131")) { push @missing,"Text::Wrap" }

# If CGI::Carp was loaded successfully for version checking, it changes the
# die and warn handlers, we don't want them changed, so we need to stash the
# original ones and set them back afterwards -- justdave@syndicomm.com
my $saved_die_handler = $::SIG{__DIE__};
my $saved_warn_handler = $::SIG{__WARN__};
unless (have_vers("CGI::Carp",0))    { push @missing,"CGI::Carp" }
$::SIG{__DIE__} = $saved_die_handler;
$::SIG{__WARN__} = $saved_warn_handler;

print "\nThe following Perl modules are optional:\n";
my $charts = 0;
$charts++ if have_vers("GD","1.19");
$charts++ if have_vers("Chart::Base","0.99");
my $xmlparser = have_vers("XML::Parser",0);

print "\n";
if ($charts != 2) {
    print "If you you want to see graphical bug dependency charts, you may install\n",
    "the optional libgd and the Perl modules GD-1.19 and Chart::Base-0.99b, e.g. by\n",
    "running (as root)\n\n",
    "   perl -MCPAN -e'install \"LDS/GD-1.19.tar.gz\"'\n",
    "   perl -MCPAN -e'install \"N/NI/NINJAZ/Chart-0.99b.tar.gz\"'\n\n";
}
if (!$xmlparser) {
    print "If you want to use the bug import/export feature to move bugs to or from\n",
    "other bugzilla installations, you will need to install the XML::Parser module by\n",
    "running (as root)\n\n",
    "   perl -MCPAN -e'install \"XML::Parser\"'\n\n";
}
if (@missing > 0) {
    print "\n\n";
    print "You are missing some Perl modules which are required by Bugzilla.\n";
    print "They can be installed by running (as root) the following:\n";
    foreach my $module (@missing) {
        print "   perl -MCPAN -e 'install \"$module\"'\n";
    }
    print "\n";
    exit;
}


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
# Now, we need a new variable. We simply add the necessary stuff to checksetup.
# The user get's the new version of Bugzilla from the CVS, runs checksetup
# and checksetup finds out "Oh, there is something new". Then it adds some
# default value to the user's local setup and informs the user to check that
# to see if that is what the user wants.
#
# Cute, ey?
#

print "Checking user setup ...\n";
do 'localconfig';
my $newstuff = "";
sub LocalVar ($$)
{
    my ($name, $definition) = @_;
    return if ($main::{$name}); # if localconfig declared it, we're done.
    $newstuff .= " " . $name;
    open FILE, '>>localconfig';
    print FILE $definition, "\n\n";
    close FILE;
}



#
# Set up the defaults for the --LOCAL-- variables below:
#

my $mysql_binaries = `which mysql`;
if ($mysql_binaries =~ /no mysql/) {
    # If which didn't find it, just provide a reasonable default
    $mysql_binaries = "/usr/bin";
} else {
    $mysql_binaries =~ s:/mysql\n$::;
}

LocalVar('mysqlpath', <<"END");
#
# In order to do certain functions in Bugzilla (such as sync the shadow
# database), we require the MySQL Binaries (mysql, mysqldump, and mysqladmin).
# Because it's possible that these files aren't in your path, you can specify
# their location here.
# Please specify only the directory name, with no trailing slash.
\$mysqlpath = "$mysql_binaries";
END


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

    
LocalVar('webservergroup', '
#
# This is the group your web server runs on.
# If you have a windows box, ignore this setting.
# If you do not wish for checksetup to adjust the permissions of anything,
# set this to "".
# If you set this to anything besides "", you will need to run checksetup.pl
# as root.
$webservergroup = "nobody";
');



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
# Some people actually use passwords with their MySQL database ...
#
$db_pass = "";
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
        "Mac System 7",
        "Mac System 7.5",
        "Mac System 7.6.1",
        "Mac System 8.0",
        "Mac System 8.5",
        "Mac System 8.6",
        "Mac System 9.x",
        "MacOS X",
        "Linux",
        "BSDI",
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




if ($newstuff ne "") {
    print "\nThis version of Bugzilla contains some variables that you may want\n",
          "to change and adapt to your local settings. Please edit the file\n",
          "'localconfig' and rerun checksetup.pl\n\n",
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
my $my_db_pass = ${*{$main::{'db_pass'}}{SCALAR}};
my $my_create_htaccess = ${*{$main::{'create_htaccess'}}{SCALAR}};
my $my_webservergroup = ${*{$main::{'webservergroup'}}{SCALAR}};
my @my_severities = @{*{$main::{'severities'}}{ARRAY}};
my @my_priorities = @{*{$main::{'priorities'}}{ARRAY}};
my @my_platforms = @{*{$main::{'platforms'}}{ARRAY}};
my @my_opsys = @{*{$main::{'opsys'}}{ARRAY}};


###########################################################################
# Check data directory
###########################################################################

#
# Create initial --DATA-- directory and make the initial empty files there:
#

unless (-d 'data') {
    print "Creating data directory ...\n";
    # permissions for non-webservergroup are fixed later on
    mkdir 'data', 0770; 
    mkdir 'data/mimedump-tmp', 01777; 
    open FILE, '>>data/comments'; close FILE;
    open FILE, '>>data/nomail'; close FILE;
    open FILE, '>>data/mail'; close FILE;
}

# 2000-12-14 New graphing system requires a directory to put the graphs in
# This code copied from what happens for the 'data' dir above.
# If the graphs dir is not present, we assume that they have been using
# a Bugzilla with the old data format, and so upgrade their data files.
unless (-d 'graphs') {
    print "Creating graphs directory...\n";
    # permissions for non-webservergroup are fixed later on
    mkdir 'graphs', 0770; 
    # Upgrade data format
    foreach my $in_file (glob("data/mining/*"))
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
        open(OUT, ">$in_file") or next;
        
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

if ($my_create_htaccess) {
  my $fileperm = 0644;
  my $dirperm = 01777;
  if ($my_webservergroup) {
    $fileperm = 0640;
    $dirperm = 0770;
  }
  if (!-e ".htaccess") {
    print "Creating .htaccess...\n";
    open HTACCESS, ">.htaccess";
    print HTACCESS <<'END';
# don't allow people to retrieve non-cgi executable files or our private data
<FilesMatch ^(.*\.pl|localconfig|processmail|syncshadowdb|runtests.sh)$>
  deny from all
</FilesMatch>
END
    close HTACCESS;
    chmod $fileperm, ".htaccess";
  }
  if (!-e "data/.htaccess") {
    print "Creating data/.htaccess...\n";
    open HTACCESS, ">data/.htaccess";
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overriden by an .htaccess
# in a subdirectory
deny from all
END
    close HTACCESS;
    chmod $fileperm, "data/.htaccess";
  }
  if (!-e "data/webdot/.htaccess") {
    if (!-d "data/webdot") {
      mkdir "data/webdot", $dirperm;
      chmod $dirperm, "data/webdot"; # the perms on mkdir don't seem to apply for some reason...
    }
    print "Creating data/webdot/.htaccess...\n";
    open HTACCESS, ">data/webdot/.htaccess";
    print HTACCESS <<'END';
# Allow access to nothing in this directory except for .dot files
# and don't allow access to those to anyone except research.att.com
# if research.att.com ever changed their IP, or if you use a different
# webdot server, you'll need to edit this
<FilesMatch ^[0-9]+\.dot$>
  Allow from 192.20.225.10
  Deny from all
</FilesMatch>

# And no directory listings, either.
Deny from all
END
    close HTACCESS;
    chmod $fileperm, "data/webdot/.htaccess";
  }

}


# Just to be sure ...
unlink "data/versioncache";





###########################################################################
# Set proper rights
###########################################################################

#
# Here we use --CHMOD-- and friends to set the file permissions
#
# The rationale is that the web server generally runs as nobody and so the cgi
# scripts should not be writable for nobody, otherwise someone may be possible
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
my @executable_files = ('processmail', 'whineatnews.pl', 'collectstats.pl',
   'checksetup.pl', 'syncshadowdb', 'importxml.pl', 'runtests.sh');

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
    my $file;
    my @files = glob($_[0]);
    my $exeperm = 0777 & ~ $_[1];
    my $normperm = 0666 & ~ $_[1];
    foreach $file (@files) {
      # do not change permissions on directories here
      if (!(-d $file)) {
        # check if the file is executable.
        if (isExecutableFile($file)) {
          #printf ("Changing $file to %o",$exeperm);
          chmod $exeperm, $file;
        } else {
          #print ("Changing $file to %o", $normperm);
          chmod $normperm, $file;
        }
      }
    }
}

if ($my_webservergroup) {
    # Funny! getgrname returns the GID if fed with NAME ...
    my $webservergid = getgrnam($my_webservergroup);
    # chown needs to be called with a valid uid, not 0.  $< returns the
    # caller's uid.  Maybe there should be a $bugzillauid, and call with that
    # userid.
    chown $<, $webservergid, glob('*');
    if (-e ".htaccess") { chown $<, $webservergid, ".htaccess" } # glob('*') doesn't catch dotfiles
    if (-e "data/.htaccess") { chown $<, $webservergid, "data/.htaccess" }
    if (-e "data/webdot/.htaccess") { chown $<, $webservergid, "data/webdot/.htaccess" }
    fixPerms('*',027);
    chmod 0644, 'globals.pl';
    chmod 0644, 'RelationSet.pm';
    chmod 0771, 'data';
    chmod 0770, 'graphs';
} else {
    # get current gid from $( list
    my $gid = (split " ", $()[0];
    chown $<, $gid, glob('*');
    fixPerms('*',022);
    chmod 01777, 'data', 'graphs';
}


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

# get a handle to the low-level DBD driver
my $drh = DBI->install_driver($db_base)
    or die "Can't connect to the $db_base. Is the database installed and up and running?\n";

if ($my_db_check) {
    # Do we have the database itself?

    # XXX - as part of the fix for dataloss bug 107718, we need the ~ 
    # operator, which isn't available in earlier versions, despite what
    # the docs say. This is temporary, as we won't need bit fiddling
    # once bug 60822 is fixed.
    # A requirement for 3.23.x may become permenant though - see
    # http://bugzilla.mozilla.org/show_bug.cgi?id=87958
    my $sql_want = "3.23.5";

    #my $sql_want = "3.22.5";  # minimum version of MySQL

# original DSN line was:
#    my $dsn = "DBI:$db_base:$my_db_name;$my_db_host;$my_db_port";
# removed the $db_name because we don't know it exists yet, and this will fail
# if we request it here and it doesn't. - justdave@syndicomm.com 2000/09/16
    my $dsn = "DBI:$db_base:;$my_db_host;$my_db_port";
    my $dbh = DBI->connect($dsn, $my_db_user, $my_db_pass)
      or die "Can't connect to the $db_base database. Is the database " .
        "installed and\nup and running?  Do you have the correct username " .
        "and password selected in\nlocalconfig?\n\n";
    printf("Checking for %15s %-9s ", "MySQL Server", "(v$sql_want)");
    my $qh = $dbh->prepare("SELECT VERSION()");
    $qh->execute;
    my ($sql_vers) = $qh->fetchrow_array;
    $qh->finish;

    # Check what version of MySQL is installed and let the user know
    # if the version is too old to be used with Bugzilla.
    if ( vers_cmp($sql_vers,$sql_want) > -1 ) {
        print "ok: found v$sql_vers\n\n";
    } else {
        die "Your MySQL server v$sql_vers is too old./n" . 
            "   Bugzilla requires version $sql_want or later of MySQL.\n" . 
            "   Please visit http://www.mysql.com/ and download a newer version.\n";
    }

    my @databases = $dbh->func('_ListDBs');
    unless (grep /^$my_db_name$/, @databases) {
       print "Creating database $my_db_name ...\n";
       $drh->func('createdb', $my_db_name, "$my_db_host:$my_db_port", $my_db_user, $my_db_pass, 'admin')
            or die <<"EOF"

The '$my_db_name' database is not accessible. This might have several reasons:

* MySQL is not running.
* MySQL is running, but the rights are not set correct. Go and read the
  Bugzilla Guide in the doc directory and all parts of the MySQL
  documentation.
* There is an subtle problem with Perl, DBI, DBD::mysql and MySQL. Make
  sure all settings in 'localconfig' are correct. If all else fails, set
  '\$db_check' to zero.\n
EOF
    }
    $dbh->disconnect if $dbh;
}

# now get a handle to the database:
my $connectstring = "dbi:$db_base:$my_db_name:host=$my_db_host:port=$my_db_port";
my $dbh = DBI->connect($connectstring, $my_db_user, $my_db_pass)
    or die "Can't connect to the table '$connectstring'.\n",
           "Have you read the Bugzilla Guide in the doc directory?  Have you read the doc of '$db_base'?\n";

END { $dbh->disconnect if $dbh }





###########################################################################
# Table definitions
###########################################################################

#
# The following hash stores all --TABLE-- definitions. This will be used
# to automatically create those tables that don't exist. The code is
# safer than the make*.sh shell scripts used to be, because they won't
# delete existing tables.
#
# If you want intentionally do this, yon can always drop a table and re-run
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
    creation_ts timestamp,
    description mediumtext not null,
    mimetype mediumtext not null,
    ispatch tinyint,
    filename mediumtext not null,
    thedata longblob not null,
    submitter_id mediumint not null,
    isobsolete tinyint not null default 0, 

    index(bug_id),
    index(creation_ts)';

# 2001-05-05 myk@mozilla.org: Tables to support the attachment tracker.
# "attachstatuses" stores one record for each status on each attachment.
# "attachstatusdefs" defines the statuses that can be set on attachments.
# Note: These tables are only used if the parameter "useattachmenttracker"
# is turned on via editparameters.cgi.

$table{attachstatuses} =
   '
     attach_id    MEDIUMINT    NOT NULL , 
     statusid     SMALLINT     NOT NULL , 
     PRIMARY KEY(attach_id, statusid) 
   ';

$table{attachstatusdefs} =
   '
     id           SMALLINT     NOT NULL  PRIMARY KEY , 
     name         VARCHAR(50)  NOT NULL , 
     description  MEDIUMTEXT   NULL , 
     sortkey      SMALLINT     NOT NULL  DEFAULT 0 , 
     product      VARCHAR(64)  NOT NULL 
   ';

#
# Apostrophe's are not supportied in the enum types.
# See http://bugzilla.mozilla.org/show_bug.cgi?id=27309
#
$table{bugs} =
   'bug_id mediumint not null auto_increment primary key,
    groupset bigint not null,
    assigned_to mediumint not null, # This is a comment.
    bug_file_loc text,
    bug_severity enum($my_severities) not null,
    bug_status enum("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED", "RESOLVED", "VERIFIED", "CLOSED") not null,
    creation_ts datetime not null,
    delta_ts timestamp,
    short_desc mediumtext,
    op_sys enum($my_opsys) not null,
    priority enum($my_priorities) not null,
    product varchar(64) not null,
    rep_platform enum($my_platforms),
    reporter mediumint not null,
    version varchar(64) not null,
    component varchar(50) not null,
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
    assignee_accessible tinyint not null default 1,
    qacontact_accessible tinyint not null default 1,
    cclist_accessible tinyint not null default 1,

    index (assigned_to),
    index (creation_ts),
    index (delta_ts),
    index (bug_severity),
    index (bug_status),
    index (op_sys),
    index (priority),
    index (product),
    index (reporter),
    index (version),
    index (component),
    index (resolution),
    index (target_milestone),
    index (qa_contact),
    index (votes)';


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
    thetext mediumtext,

    index(bug_id),
    index(who),
    index(bug_when)';


$table{components} =
   'value tinytext,
    program varchar(64),
    initialowner mediumint not null,
    initialqacontact mediumint not null,
    description mediumtext not null';


$table{dependencies} =
   'blocked mediumint not null,
    dependson mediumint not null,

    index(blocked),
    index(dependson)';


# Group bits must be a power of two. Groups are identified by a bit; sets of
# groups are indicated by or-ing these values together.
#
# isbuggroup is nonzero if this is a group that controls access to a set
# of bugs.  In otherword, the groupset field in the bugs table should only
# have this group's bit set if isbuggroup is nonzero.
#
# User regexp is which email addresses are initially put into this group.
# This is only used when an email account is created; otherwise, profiles
# may be individually tweaked to add them in and out of groups.
#
# 2001-04-10 myk@mozilla.org:
# isactive determines whether or not a group is active.  An inactive group
# cannot have bugs added to it.  Deactivation is a much milder form of
# deleting a group that allows users to continue to work on bugs in the group
# without enabling them to extend the life of the group by adding bugs to it.
# http://bugzilla.mozilla.org/show_bug.cgi?id=75482

$table{groups} =
   'bit bigint not null,
    name varchar(255) not null,
    description text not null,
    isbuggroup tinyint not null,
    userregexp tinytext not null,
    isactive tinyint not null default 1,

    unique(bit),
    unique(name)';


$table{logincookies} =
   'cookie mediumint not null auto_increment primary key,
    userid mediumint not null,
    cryptpassword varchar(34),
    hostname varchar(128),
    lastused timestamp,

    index(lastused)';


$table{products} =
   'product varchar(64),
    description mediumtext,
    milestoneurl tinytext not null,
    disallownew tinyint not null,
    votesperuser smallint not null,
    maxvotesperbug smallint not null default 10000,
    votestoconfirm smallint not null,
    defaultmilestone varchar(20) not null default "---"
';


$table{profiles} =
   'userid mediumint not null auto_increment primary key,
    login_name varchar(255) not null,
    cryptpassword varchar(34),
    realname varchar(255),
    groupset bigint not null,
    disabledtext mediumtext not null,
    mybugslink tinyint not null default 1,
    blessgroupset bigint not null default 0,
    emailflags mediumtext,


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
     watchfordiffs tinyint not null,
     linkinfooter tinyint not null,
     query mediumtext not null,

     unique(userid, name),
     index(watchfordiffs)';

# This isn't quite cooked yet...
#
#  $table{diffprefs} =
#     'userid mediumint not null,
#      fieldid mediumint not null,
#      mailhead tinyint not null,
#      maildiffs tinyint not null,
#
#      index(userid)';

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
    program varchar(64) not null';


$table{votes} =
   'who mediumint not null,
    bug_id mediumint not null,
    count smallint not null,

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
    'value varchar(20) not null,
     product varchar(64) not null,
     sortkey smallint not null,
     unique (product, value)';

$table{shadowlog} =
    'id int not null auto_increment primary key,
     ts timestamp,
     reflected tinyint not null,
     command mediumtext not null,
     index(reflected)';

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



###########################################################################
# Create tables
###########################################################################

# Get a list of the existing tables (if any) in the database
my @tables = map { $_ =~ s/.*\.//; $_ } $dbh->tables;
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

sub GroupExists ($)
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
# created with the next available bit set
#

sub AddGroup {
    my ($name, $desc, $userregexp) = @_;
    $userregexp ||= "";

    return if GroupExists($name);
    
    # get highest bit number
    my $sth = $dbh->prepare("SELECT bit FROM groups ORDER BY bit DESC");
    $sth->execute;
    my @row = $sth->fetchrow_array;

    # normalize bits
    my $bit;
    if (defined $row[0]) {
        $bit = $row[0] << 1;
    } else {
        $bit = 1;
    }

   
    print "Adding group $name ...\n";
    $sth = $dbh->prepare('INSERT INTO groups
                          (bit, name, description, userregexp, isbuggroup)
                          VALUES (?, ?, ?, ?, ?)');
    $sth->execute($bit, $name, $desc, $userregexp, 0);
    return $bit;
}


#
# BugZilla uses --GROUPS-- to assign various rights to its users. 
#

AddGroup 'tweakparams',      'Can tweak operating parameters';
AddGroup 'editusers',      'Can edit or disable users';
AddGroup 'creategroups',     'Can create and destroy groups.';
AddGroup 'editcomponents',   'Can create, destroy, and edit components.';
AddGroup 'editkeywords',   'Can create, destroy, and edit keywords.';

# Add the groupset field here because this code is run before the
# code that updates the database structure.
&AddField('profiles', 'groupset', 'bigint not null');

if (!GroupExists("editbugs")) {
    my $id = AddGroup('editbugs',  'Can edit all aspects of any bug.', ".*");
    $dbh->do("UPDATE profiles SET groupset = groupset | $id");
}

if (!GroupExists("canconfirm")) {
    my $id = AddGroup('canconfirm',  'Can confirm a bug.', ".*");
    $dbh->do("UPDATE profiles SET groupset = groupset | $id");
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
    }

    $dbh->do("REPLACE INTO fielddefs " .
             "(fieldid, name, description, mailhead, sortkey) VALUES " .
             "($fieldid, $name, $description, $mailhead, $headernum)");
    $headernum++;
}


AddFDef("bug_id", "Bug \#", 1);
AddFDef("short_desc", "Summary", 1);
AddFDef("product", "Product", 1);
AddFDef("version", "Version", 1);
AddFDef("rep_platform", "Platform", 1);
AddFDef("bug_file_loc", "URL", 1);
AddFDef("op_sys", "OS/Version", 1);
AddFDef("bug_status", "Status", 1);
AddFDef("status_whiteboard", "Status Whiteboard", 0);
AddFDef("keywords", "Keywords", 0);
AddFDef("resolution", "Resolution", 0);
AddFDef("bug_severity", "Severity", 1);
AddFDef("priority", "Priority", 1);
AddFDef("component", "Component", 1);
AddFDef("assigned_to", "AssignedTo", 1);
AddFDef("reporter", "ReportedBy", 1);
AddFDef("votes", "Votes", 0);
AddFDef("qa_contact", "QAContact", 1);
AddFDef("cc", "CC", 1);
AddFDef("dependson", "BugsThisDependsOn", 0);
AddFDef("blocked", "OtherBugsDependingOnThis", 0);
AddFDef("attachments.description", "Attachment description", 0);
AddFDef("attachments.thedata", "Attachment data", 0);
AddFDef("attachments.mimetype", "Attachment mime type", 0);
AddFDef("attachments.ispatch", "Attachment is patch", 0);
AddFDef("attachments.isobsolete", "Attachment is obsolete", 0);
AddFDef("attachstatusdefs.name", "Attachment Status", 0);
AddFDef("target_milestone", "Target Milestone", 0);
AddFDef("delta_ts", "Last changed date", 0);
AddFDef("(to_days(now()) - to_days(bugs.delta_ts))", "Days since bug changed",
        0);
AddFDef("longdesc", "Comment", 0);
    
    


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
# Create Administrator  --ADMIN--
###########################################################################

#  Prompt the user for the email address and name of an administrator.  Create
#  that login, if it doesn't exist already, and make it a member of all groups.

sub bailout {   # this is just in case we get interrupted while getting passwd
    system("stty","echo"); # re-enable input echoing
    exit 1;
}

my $sth = $dbh->prepare(<<_End_Of_SQL_);
  SELECT login_name
  FROM profiles
  WHERE groupset=9223372036854775807
_End_Of_SQL_
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
  if (-e "data/params") { 
    require "data/params"; # if they have a params file, use that
  }
  if ($::params{emailregexp}) {
    $mailcheckexp = $::params{emailregexp};
    $mailcheck    = $::params{emailregexpdesc};
  } else {
    $mailcheckexp = '^[^@]+@[^@]+\\.[^@]+$';
    $mailcheck    = 'A legal address must contain exactly one \'@\', 
      and at least one \'.\' after the @.';
  }

  print "\nLooks like we don't have an administrator set up yet.  Either this is your\n";
  print "first time using Bugzilla, or your administrator's privs might have accidently\n";
  print "gotten deleted at some point.\n";
  while(! $admin_ok ) {
    while( $login eq "" ) {
      print "Enter the e-mail address of the administrator: ";
      $login = <STDIN>;
      chomp $login;
      if(! $login ) {
        print "\nYou DO want an administrator, don't you?\n";
      }
      unless ($login =~ /$mailcheckexp/) {
        print "\nThe login address is invalid:\n";
        print "$mailcheck\n";
        die "Please try again\n";
      }
    }
    $login = $dbh->quote($login);
    $sth = $dbh->prepare(<<_End_Of_SQL_);
      SELECT login_name
      FROM profiles
      WHERE login_name=$login
_End_Of_SQL_
    $sth->execute;
    if ($sth->rows > 0) {
      print "$login already has an account.\n";
      print "Make this user the administrator? [Y/n] ";
      my $ok = <STDIN>;
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
      my $ok = <STDIN>;
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
      $realname = <STDIN>;
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

    system("stty","-echo");  # disable input echoing

    while( $pass1 ne $pass2 ) {
      while( $pass1 eq "" || $pass1 !~ /^[a-zA-Z0-9-_]{3,16}$/ ) {
        print "Enter a password for the administrator account: ";
        $pass1 = <STDIN>;
        chomp $pass1;
        if(! $pass1 ) {
          print "\n\nIt's just plain stupid to not have a password.  Try again!\n";
        } elsif ( $pass1 !~ /^.{3,16}$/ ) {
          print "The password must be 3-16 characters in length.";
        }
      }
      print "\nPlease retype the password to verify: ";
      $pass2 = <STDIN>;
      chomp $pass2;
      if ($pass1 ne $pass2) {
        print "\n\nPasswords don't match.  Try again!\n";
        $pass1 = "";
        $pass2 = "*";
      }
    }

    # Crypt the administrator's password
    my $cryptedpassword = Crypt($pass1);

    system("stty","echo"); # re-enable input echoing
    $SIG{HUP}  = 'DEFAULT'; # and remove our interrupt hooks
    $SIG{INT}  = 'DEFAULT';
    $SIG{QUIT} = 'DEFAULT';
    $SIG{TERM} = 'DEFAULT';

    $realname = $dbh->quote($realname);
    $cryptedpassword = $dbh->quote($cryptedpassword);

    $dbh->do(<<_End_Of_SQL_);
      INSERT INTO profiles
      (login_name, realname, cryptpassword, groupset)
      VALUES ($login, $realname, $cryptedpassword, 0x7fffffffffffffff)
_End_Of_SQL_
  } else {
    $dbh->do(<<_End_Of_SQL_);
      UPDATE profiles
      SET groupset=0x7fffffffffffffff
      WHERE login_name=$login
_End_Of_SQL_
  }
  print "\n$login is now set up as the administrator account.\n";
}


sub Crypt {
    # Crypts a password, generating a random salt to do it.
    # Random salts are generated because the alternative is usually
    # to use the first two characters of the password itself, and since
    # the salt appears in plaintext at the beginning of the crypted
    # password string this has the effect of revealing the first two
    # characters of the password to anyone who views the crypted version.

    my ($password) = @_;

    # The list of characters that can appear in a salt.  Salts and hashes
    # are both encoded as a sequence of characters from a set containing
    # 64 characters, each one of which represents 6 bits of the salt/hash.
    # The encoding is similar to BASE64, the difference being that the
    # BASE64 plus sign (+) is replaced with a forward slash (/).
    my @saltchars = (0..9, 'A'..'Z', 'a'..'z', '.', '/');

    # Generate the salt.  We use an 8 character (48 bit) salt for maximum
    # security on systems whose crypt uses MD5.  Systems with older
    # versions of crypt will just use the first two characters of the salt.
    my $salt = '';
    for ( my $i=0 ; $i < 8 ; ++$i ) {
        $salt .= $saltchars[rand(64)];
    }

    # Crypt the password.
    my $cryptedpassword = crypt($password, $salt);

    # Return the crypted password.
    return $cryptedpassword;
}




###########################################################################
# Create initial test product if there are no products present.
###########################################################################

$sth = $dbh->prepare(<<_End_Of_SQL_);
  SELECT userid
  FROM profiles
  WHERE groupset=9223372036854775807
_End_Of_SQL_
$sth->execute;
my ($adminuid) = $sth->fetchrow_array;
if (!$adminuid) { die "No administator!" } # should never get here
$sth = $dbh->prepare("SELECT product FROM products");
$sth->execute;
unless ($sth->rows) {
    print "Creating initial dummy product 'TestProduct' ...\n";
    $dbh->do('INSERT INTO products(product, description, milestoneurl, disallownew, votesperuser, votestoconfirm) VALUES ("TestProduct",
              "This is a test product.  This ought to be blown away and ' .
             'replaced with real stuff in a finished installation of ' .
             'bugzilla.", "", 0, 0, 0)');
    $dbh->do('INSERT INTO versions (value, program) VALUES ("other", "TestProduct")');
    $dbh->do("INSERT INTO components (value, program, description, initialowner, initialqacontact)
             VALUES (" .
             "'TestComponent', 'TestProduct', " .
             "'This is a test component in the test product database.  " .
             "This ought to be blown away and replaced with real stuff in " .
             "a finished installation of bugzilla.', $adminuid, 0)");
    $dbh->do('INSERT INTO milestones (product, value) VALUES ("TestProduct","---")');
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
        $type .= " NOT NULL" if $$ref[3];
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
AddField('bugs', 'target_milestone', 'varchar(20) not null default "---"');
AddField('bugs', 'groupset', 'bigint not null');
AddField('bugs', 'qa_contact', 'mediumint not null');
AddField('bugs', 'status_whiteboard', 'mediumtext not null');
AddField('products', 'disallownew', 'tinyint not null');
AddField('products', 'milestoneurl', 'tinytext not null');
AddField('components', 'initialqacontact', 'tinytext not null');
AddField('components', 'description', 'mediumtext not null');
ChangeFieldType('components', 'program', 'varchar(64)');


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

ChangeFieldType ('bugs',       'product', 'varchar(64) not null');
ChangeFieldType ('components', 'program', 'varchar(64)');
ChangeFieldType ('products',   'product', 'varchar(64)');
ChangeFieldType ('versions',   'program', 'varchar(64) not null');

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
# 2001-04-29 jake@acutex.net - The newemailtech field is no longer needed
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
AddField('profiles', 'blessgroupset', 'bigint not null');

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
        print("Populating duplicates table...\n");

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
                $dupes{$key} =~ s/.*\*\*\* This bug has been marked as a duplicate of (\d{1,5}) \*\*\*.*?/$1/sm;
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
unless (-d 'data/duplicates') {
    print "Creating duplicates directory...\n";
    mkdir 'data/duplicates', 0770; 
    if ($my_webservergroup eq "") {
        chmod 01777, 'data/duplicates';
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

# 2001-04-29 jake@acutex.net - Remove oldemailtech
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

# 2001-07-20 jake@acutex.net - Change bugs_activity to only record changes
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

# 2001-07-24 jake@acutex.net - disabledtext was being handled inconsitantly
# http://bugzilla.mozilla.org/show_bug.cgi?id=90933
ChangeFieldType("profiles", "disabledtext", "mediumtext not null");

# 2001-07-26 myk@mozilla.org bug39816: 
# Add fields to the bugs table that record whether or not the reporter,
# assignee, QA contact, and users on the cc: list can see bugs even when
# they are not members of groups to which the bugs are restricted.
AddField("bugs", "reporter_accessible", "tinyint not null default 1");
AddField("bugs", "assignee_accessible", "tinyint not null default 1");
AddField("bugs", "qacontact_accessible", "tinyint not null default 1");
AddField("bugs", "cclist_accessible", "tinyint not null default 1");

# 2001-08-21 myk@mozilla.org bug84338:
# Add a field for the attachment ID to the bugs_activity table, so installations
# using the attachment manager can record changes to attachments.
AddField("bugs_activity", "attach_id", "mediumint null");

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
# Final checks...

unlink "data/versioncache";
print "Reminder: Bugzilla now requires version 8.7 or later of sendmail.\n";
