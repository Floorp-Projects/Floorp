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
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Install::Filesystem;

# NOTE: This package may "use" any modules that it likes,
# and localconfig is available. However, all functions in this
# package should assume that:
#
# * Templates are not available.
# * Files do not have the correct permissions.
# * The database does not exist.

use strict;

use Bugzilla::Constants;
use Bugzilla::Install::Localconfig;

use Fcntl;
use IO::File;

use base qw(Exporter);
our @EXPORT = qw(
    update_filesystem
    create_htaccess
);

# This looks like a constant because it effectively is, but
# it has to call other subroutines and read the current filesystem,
# so it's defined as a sub. This is not exported, so it doesn't have
# a perldoc. However, look at the various hashes defined inside this 
# function to understand what it returns. (There are comments throughout.)
sub FILESYSTEM {
    my $datadir       = bz_locations()->{'datadir'};
    my $attachdir     = bz_locations()->{'attachdir'};
    my $extensionsdir = bz_locations()->{'extensionsdir'};
    my $webdotdir     = bz_locations()->{'webdotdir'};
    my $templatedir   = bz_locations()->{'templatedir'};
    my $libdir        = bz_locations()->{'libpath'};

    my $ws_group      = read_localconfig()->{'webservergroup'};

    # The name of each directory, pointing at its default permissions.
    my %dirs = (
        $datadir                => 0770,
        "$datadir/mimedump-tmp" => 01777,
        "$datadir/mining"       => 0700,
        "$datadir/duplicates"   => $ws_group ? 0770 : 01777,
        $attachdir              => 0770,
        $extensionsdir          => 0770,
        graphs                  => 0770,
        $webdotdir              => 0700,
        'skins/custom'          => 0700,
    );

    # The name of each file, pointing at its default permissions and
    # default contents.
    my %files = (
        "$datadir/mail"    => {},
        'skins/.cvsignore' => { contents => ".cvsignore\ncustom\n" },
    );

    # Each standard stylesheet has an associated custom stylesheet that
    # we create.
    foreach my $standard (<skins/standard/*.css>) {
        my $custom = $standard;
        $custom =~ s|^skins/standard|skins/custom|;
        $files{$custom} = { contents => <<EOT
/*
 * Custom rules for $standard.
 * The rules you put here override rules in that stylesheet.
 */
EOT
        };
    }

    # Because checksetup controls the creation of index.html separately
    # from all other files, it gets its very own hash.
    my %index_html = (
        'index.html' => { contents => <<EOT
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
  <meta http-equiv="Refresh" content="0; URL=index.cgi">
</head>
<body>
  <h1>I think you are looking for <a href="index.cgi">index.cgi</a></h1>
</body>
</html>
EOT
        }
    );

    # Because checksetup controls the .htaccess creation separately
    # by a localconfig variable, these go in a separate variable from
    # %files.
    my $ht_perms = $ws_group ? 0640 : 0644;
    my $ht_default_deny = <<EOT;
# nothing in this directory is retrievable unless overridden by an .htaccess
# in a subdirectory
deny from all
EOT

    my %htaccess = (
        "$attachdir/.htaccess"       => { perms    => $ht_perms,
                                          contents => $ht_default_deny },
        "$libdir/Bugzilla/.htaccess" => { perms    => $ht_perms,
                                          contents => $ht_default_deny },
        "$templatedir/.htaccess"     => { perms    => $ht_perms,
                                          contents => $ht_default_deny },

        '.htaccess' => { perms => $ht_perms, contents => <<EOT
# Don't allow people to retrieve non-cgi executable files or our private data
<FilesMatch ^(.*\\.pm|.*\\.pl|.*localconfig.*)\$>
  deny from all
</FilesMatch>
EOT
        },

        "$webdotdir/.htaccess" => { perms => $ht_perms, contents => <<EOT
# Restrict access to .dot files to the public webdot server at research.att.com
# if research.att.com ever changes their IP, or if you use a different
# webdot server, you'll need to edit this
<FilesMatch \\.dot\$>
  Allow from 192.20.225.10
  Deny from all
</FilesMatch>

# Allow access to .png files created by a local copy of 'dot'
<FilesMatch \\.png\$>
  Allow from all
</FilesMatch>

# And no directory listings, either.
Deny from all
EOT
        },

        # Even though $datadir may not (and should not) be in the webtree,
        # we can't know for sure, so create the .htaccess anyway. It's harmless
        # if it's not accessible...
        "$datadir/.htaccess" => { perms    => $ht_perms, contents => <<EOT
# Nothing in this directory is retrievable unless overridden by an .htaccess
# in a subdirectory; the only exception is duplicates.rdf, which is used by
# duplicates.xul and must be loadable over the web
deny from all
<Files duplicates.rdf>
  allow from all
</Files>
EOT


        },
    );

    return {
        dirs       => \%dirs,
        files      => \%files,
        htaccess   => \%htaccess,
        index_html => \%index_html,
    };
}

sub update_filesystem {
    my ($params) = @_;
    my $fs = FILESYSTEM();
    my %dirs  = %{$fs->{dirs}};
    my %files = %{$fs->{files}};

    my $datadir = bz_locations->{'datadir'};
    # If the graphs/ directory doesn't exist, we're upgrading from
    # a version old enough that we need to update the $datadir/mining 
    # format.
    if (-d "$datadir/mining" && !-d 'graphs') {
        _update_old_charts($datadir);
    }

    # By sorting the dirs, we assure that shorter-named directories
    # (meaning parent directories) are always created before their
    # child directories.
    foreach my $dir (sort keys %dirs) {
        unless (-d $dir) {
            print "Creating $dir directory...\n";
            mkdir $dir || die $!;
            # For some reason, passing in the permissions to "mkdir"
            # doesn't work right, but doing a "chmod" does.
            chmod $dirs{$dir}, $dir || die $!;
        }
    }

    _create_files(%files);
    if ($params->{index_html}) {
        _create_files(%{$fs->{index_html}});
    }
    elsif (-e 'index.html') {
        my $templatedir = bz_locations()->{'templatedir'};
        print <<EOT;

*** It appears that you still have an old index.html hanging around.
    Either the contents of this file should be moved into a template and 
    placed in the '$templatedir/en/custom' directory, or you should delete 
    the file.

EOT
    }
}

sub create_htaccess {
    _create_files(%{FILESYSTEM()->{htaccess}});

    # Repair old .htaccess files
    my $htaccess = new IO::File('.htaccess', 'r') || die ".htaccess: $!";
    my $old_data;
    { local $/; $old_data = <$htaccess>; }
    $htaccess->close;

    my $repaired = 0;
    if ($old_data =~ s/\|localconfig\|/\|.*localconfig.*\|/) {
        $repaired = 1;
    }
    if ($old_data !~ /\(\.\*\\\.pm\|/) {
        $old_data =~ s/\(/(.*\\.pm\|/;
        $repaired = 1;
    }
    if ($repaired) {
        print "Repairing .htaccess...\n";
        $htaccess = new IO::File('.htaccess', 'w') || die $!;
        print $htaccess $old_data;
        $htaccess->close;
    }
}

# A helper for the above functions.
sub _create_files {
    my (%files) = @_;

    # It's not necessary to sort these, but it does make the
    # output of checksetup.pl look a bit nicer.
    foreach my $file (sort keys %files) {
        unless (-e $file) {
            print "Creating $file...\n";
            my $info = $files{$file};
            my $fh = new IO::File($file, O_WRONLY | O_CREAT, $info->{perms})
                || die $!;
            print $fh $info->{contents} if $info->{contents};
            $fh->close;
        }
    }
}

# If you ran a REALLY old version of Bugzilla, your chart files are in the
# wrong format. This code is a little messy, because it's very old, and
# when moving it into this module, I couldn't test it so I left it almost 
# completely alone.
sub _update_old_charts {
    my ($datadir) = @_;
    print "Updating old chart storage format...\n";
    foreach my $in_file (glob("$datadir/mining/*")) {
        # Don't try and upgrade image or db files!
        next if (($in_file =~ /\.gif$/i) ||
                 ($in_file =~ /\.png$/i) ||
                 ($in_file =~ /\.db$/i) ||
                 ($in_file =~ /\.orig$/i));

        rename("$in_file", "$in_file.orig") or next;
        open(IN, "$in_file.orig") or next;
        open(OUT, '>', $in_file) or next;

        # Fields in the header
        my @declared_fields;

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
                my @data = split(/\||\r/, $1);
                my %data;
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
                    print "Oh dear, input line $. of $in_file had " .
                          scalar(@data) . " fields\nThis was unexpected.",
                          " You may want to check your data files.\n";
                }

                print OUT join('|', 
                    map { defined ($data{$_}) ? ($data{$_}) : "" } @out_fields),
                    "\n";
            }
            else {
                print OUT;
            }
        }

        close(IN);
        close(OUT);
    } 
}

1;

__END__

=head1 NAME

Bugzilla::Install::Filesystem - Fix up the filesystem during
  installation.

=head1 SYNOPSIS

=head1 DESCRIPTION

This module is used primarily by L<checksetup.pl> to modify the 
filesystem during installation, including creating the data/ directory.

=over

=back

=head1 SUBROUTINES

=over

=item C<update_filesystem({ index_html => 0 })>

Description: Creates all the directories and files that Bugzilla
             needs to function but doesn't ship with. Also does
             any updates to these files as necessary during an
             upgrade.

Params:      C<index_html> - Whether or not we should create
               the F<index.html> file.

Returns:     nothing

=item C<create_htaccess()>

Description: Creates all of the .htaccess files for Apache,
             in the various Bugzilla directories. Also updates
             the .htaccess files if they need updating.

Params:      none

Returns:     nothing

=back
