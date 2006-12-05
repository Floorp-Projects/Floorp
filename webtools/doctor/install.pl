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
# The Original Code is Doctor.
#
# Contributor(s): Frédéric Buclin <LpSolit@gmail.com>

####################################################################
# Initialisation                                                   #
####################################################################

use strict;

# Go to the directory where install.pl is.
#
# We require Perl 5.6.1 on Linux (where File::Basename is part of
# the core), and 5.8.1 on Windows (which also has CGI 2.93).
# CGI 2.93 fixes important issues such as handling UTF-8 in Perl 5.8
# correctly and also to work with mod_perl.

BEGIN {
    if ($^O =~ /MSWin32/i) {
        require 5.008001; # for CGI 2.93 or higher
    }
    require 5.006_001;
    use File::Basename;
    chdir dirname($0);
}

use lib qw(.);

####################################################################
# First of all, make sure all required Perl modules are installed. #
####################################################################

print "\nChecking Perl modules ...\n";

# Subroutine to compare versions.
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

# Make sure the module is installed and is recent enough, and display
# the result on the screen.
sub have_vers {
    my ($pkg, $wanted) = @_;
    my ($msg, $vnum, $vstr);
    no strict 'refs';
    printf("Checking for %15s %-12s ", $pkg, !$wanted?'(any)':"(v$wanted)");

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
    print ((($vok) ? "ok: " : " "), "$vstr\n");
    return $vok;
}

# Check versions of dependencies. '0' for version = any version acceptable.
my $modules = [ 
    {
        name => 'CGI',
        version => '2.93'
    },
    {
        name => 'AppConfig',
        version => '1.52' 
    },
    {
        name => 'Template',
        version => '2.08'
    },
    {
        name => 'File::Temp',
        version => '0'
    },
    {
        name => 'Text::Diff',
        version => '0'
    },
    {
        name => 'Email::Valid',
        version => '0'
    },
    {
        name => 'MIME::Entity',
        version => '0'
    },
];

my $missing_modules = 0;
foreach my $module (@$modules) {
    unless (have_vers($module->{name}, $module->{version})) {
        $missing_modules = 1;
    }
}

if ($missing_modules) {
    print "There are some missing modules or modules that are too old.\n" .
          "Please see the list above and install or update them accordingly.\n";
    exit;
}

# If we're running on Windows, reset the input line terminator so that
# console input works properly - loading CGI tends to mess it up.
if ($^O =~ /MSWin/i) {
    $/ = "\015\012";
}

####################################################################
# Now make sure that all required directories are created, have    #
# appropriate permissions on them and that all .htaccess files are #
# correctly configured.                                            #
####################################################################

foreach my $dir ('.', 'Doctor', 'templates') {
    unless (-d $dir) {
        mkdir $dir, 0700;
    }
    if (!-e "$dir/.htaccess") {
        print "Creating $dir/.htaccess...\n";
        open HTACCESS, '>', "$dir/.htaccess";
        if ($dir eq '.') {
            print HTACCESS <<'END';
# Do not allow people to retrieve non-CGI executable files or our private data
<FilesMatch ^(.*\.pm|.*\.pl|.*\.conf~?)$>
  deny from all
</FilesMatch>
END
        }
        else {
            print HTACCESS <<'END2';
# Nothing in this directory is retrievable unless overridden by an .htaccess
# in a subdirectory;
deny from all
END2
        }
        close HTACCESS;
        chmod 0600, "$dir/.htaccess";
    }
}


####################################################################
# Copy sample.conf as doctor.conf if this one doesn't exist yet.   #
#                                                                  #
####################################################################

if (!-e 'doctor.conf' && -e 'sample.conf') {
    require File::Copy;
    File::Copy::copy('sample.conf', 'doctor.conf');
    print "\nYou have to edit doctor.conf to set parameters correctly.\n\n";
}

####################################################################
# At this point, all directories and files have been created.      #
# We can now set the correct permissions on files and directories. #
####################################################################

sub fixPerms {
    my ($file_pattern, $group, $umask, $do_dirs) = @_;
    my @files = glob($file_pattern);
    my $execperm = 0777 & ~ $umask;
    my $normperm = 0666 & ~ $umask;

    foreach my $file (@files) {
        next if (!-e $file);
        # Do not change permissions on directories here
        # unless $do_dirs is set.
        if (!-d $file) {
            chown $<, $group, $file;
            # Check if the file is executable.
            if ($file =~ /(\.cgi|\.pl)$/) {
                # printf ("Changing $file to %o\n", $execperm);
                chmod $execperm, $file;
            } else {
                # printf ("Changing $file to %o\n", $normperm);
                chmod $normperm, $file;
            }
        }
        elsif ($do_dirs) {
            chown $<, $group, $file;
            # printf ("Changing $file to %o\n", $execperm);
            chmod $execperm, $file;
            fixPerms("$file/.htaccess", $group, $umask, $do_dirs);
            # do the contents of the directory
            fixPerms("$file/*", $group, $umask, $do_dirs);
        }
    }
}

# Only change permissions if we are not running Windows.
if ($^O !~ /MSWin32/i) {
    my $gid;
    my ($gr, $grw, $grwx, $gdata);

    print "\nEnter the name of the web server group (default: apache): ";
    my $webservergroup = <STDIN>;
    chomp($webservergroup);
    # Use 'apache' if no group is given.
    $webservergroup ||= 'apache';

    if ($webservergroup) {
        # Make sure the webserver group is valid.
        $gid = getgrnam($webservergroup)
          || die("no such group: $webservergroup");
        $gr = 027;
        $grw = 017;
        $grwx = 007;
        $gdata = 0771;
    } else {
        print "\nNo webserver group has been given.\n" .
              "Your installation will be pretty unsecure.\n" .
              "Please provide one and re-run install.pl.\n\n";
        # Get the current gid from the $( list.
        $gid = (split(" ", $())[0];
        $gr = 022;
        $grw = 011;
        $grwx = 000;
        $gdata = 0777;
    }

    fixPerms('.htaccess', $gid, $gr); # glob('*') doesn't catch dotfiles
    fixPerms('*', $gid, $gr);
    fixPerms('Doctor', $gid, $gr, 1);
    fixPerms('templates', $gid, $gr, 1);
}
