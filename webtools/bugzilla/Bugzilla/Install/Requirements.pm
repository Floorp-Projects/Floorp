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

package Bugzilla::Install::Requirements;

# NOTE: This package MUST NOT "use" any Bugzilla modules other than
# Bugzilla::Constants, anywhere. We may "use" standard perl modules.
#
# Subroutines may "require" and "import" from modules, but they
# MUST NOT "use."

use strict;

use base qw(Exporter);
our @EXPORT = qw(
    REQUIRED_MODULES

    vers_cmp
    have_vers
    install_command
);

use Bugzilla::Constants;
use constant REQUIRED_MODULES => [
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
        version => '1.38'
    },
    {
        name => 'File::Spec',
        version => '0.84'
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
    },
    {
        name => 'Mail::Mailer',
        version => '1.67'
    },
    {
        name => 'MIME::Base64',
        version => '3.01'
    },
    {
        # MIME::Parser is packaged as MIME::Tools on ActiveState Perl
        name => $^O =~ /MSWin32/i ? 'MIME::Tools' : 'MIME::Parser',
        version => '5.406'
    },
    {
        name => 'Storable',
        version => '0'
    },
];

# Remember that you only have to add modules to this hash if their
# names are significantly different on ActiveState than on normal perl.
# If it's just a difference between "::" and "-" in the name, don't worry
# about it--install_command() handles that automatically.
use constant WIN32_MODULE_NAMES => {
    'Chart::Base'       => 'Chart',
    'Date::Format'      => 'TimeDate',
    'Template'          => 'Template-Toolkit',
    'GD::Graph'         => 'GDGraph',
    'GD::Text::Align'   => 'GDTextUtil',
    'Mail::Mailer'      => 'MailTools',
};

# This was originally clipped from the libnet Makefile.PL, adapted here to
# use the above vers_cmp routine for accurate version checking.
sub have_vers {
    my ($pkg, $wanted) = @_;
    my ($msg, $vnum, $vstr);
    no strict 'refs';
    printf("Checking for %15s %-9s ", $pkg, !$wanted?'(any)':"(v$wanted)") 
        unless $::silent;

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
    print ((($vok) ? "ok: " : " "), "$vstr\n") unless $::silent;
    return $vok ? 1 : 0;
}

# This is taken straight from Sort::Versions 1.5, which is not included
# with perl by default.
sub vers_cmp {
    my @A = ($_[0] =~ /([-.]|\d+|[^-.\d]+)/g);
    my @B = ($_[1] =~ /([-.]|\d+|[^-.\d]+)/g);

    my ($A, $B);
    while (@A and @B) {
        $A = shift @A;
        $B = shift @B;
        if ($A eq '-' and $B eq '-') {
            next;
        } elsif ( $A eq '-' ) {
            return -1;
        } elsif ( $B eq '-') {
            return 1;
        } elsif ($A eq '.' and $B eq '.') {
            next;
        } elsif ( $A eq '.' ) {
            return -1;
        } elsif ( $B eq '.' ) {
            return 1;
        } elsif ($A =~ /^\d+$/ and $B =~ /^\d+$/) {
            if ($A =~ /^0/ || $B =~ /^0/) {
                return $A cmp $B if $A cmp $B;
            } else {
                return $A <=> $B if $A <=> $B;
            }
        } else {
            $A = uc $A;
            $B = uc $B;
            return $A cmp $B if $A cmp $B;
        }
    }
    @A <=> @B;
}

sub install_command {
    my $module = shift;
    if ($^O =~ /MSWin32/i) {
        return "ppm install " . WIN32_MODULE_NAMES->{$module} if
            WIN32_MODULE_NAMES->{$module};
        $module =~ s/::/-/g;
        return "ppm install " . $module;
    } else {
        return "$^X -MCPAN -e 'install \"$module\"'";
    }
}


1;

__END__

=head1 NAME

Bugzilla::Install::Requirements - Functions and variables dealing
  with Bugzilla's perl-module requirements.

=head1 DESCRIPTION

This module is used primarily by C<checksetup.pl> to determine whether
or not all of Bugzilla's prerequisites are installed. (That is, all the
perl modules it requires.)

=head1 CONSTANTS

=over 4

=item C<REQUIRED_MODULES>

An arrayref of hashrefs that describes the perl modules required by 
Bugzilla. The hashes have two keys, C<name> and C<version>, which
represent the name of the module and the version that we require.

=back

=head1 SUBROUTINES

=over 4

=item C<vers_cmp($a, $b)>

 Description: This is a comparison function, like you would use in
              C<sort>, except that it compares two version numbers.
              It's actually identical to versioncmp from 
              L<Sort::Versions>.

 Params:      c<$a> and C<$b> are versions you want to compare.

 Returns:     -1 if $a is less than $b, 0 if they are equal, and
              1 if $a is greater than $b.

=item C<have_vers($pkg, $wanted)>

 Description: Tells you whether or not you have the appropriate
              version of the module requested. It also prints
              out a message to the user explaining the check
              and the result.

 Params:      C<$pkg> - A string, the name of the package you're checking.
              C<$wanted> - The version of the package you require.
                           Set this to 0 if you don't require any
                           particular version.

 Returns:   C<1> if you have the module installed and you have the
            appropriate version. C<0> otherwise.

 Notes:     If you set C<$main::silent> to a true value, this function
            won't print out anything.

=item C<install_command($module)>

 Description: Prints out the appropriate command to install the
              module specified, depending on whether you're
              on Windows or Linux.

 Params:      C<$module> - The name of the module.

 Returns:     nothing

=back
