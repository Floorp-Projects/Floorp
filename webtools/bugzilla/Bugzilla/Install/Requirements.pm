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
    OPTIONAL_MODULES
    MOD_PERL_MODULES

    check_requirements
    have_vers
    vers_cmp
);

our @EXPORT_OK = qw(
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
        version => '1.41'
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

use constant OPTIONAL_MODULES => [
    {
        name => 'GD',
        version => '1.20'
    },
    {
        # This module tells us whether or not Template-GD is installed
        # on Template-Toolkits after 2.14, and still works with 2.14 and lower.
        name => 'Template::Plugin::GD::Image',
        version => 0
    },
    {
        name => 'Chart::Base',
        version => '1.0'
    },
    {
        name => 'GD::Graph',
        version => 0
    },
    { 
        name => 'GD::Text::Align',
        version => 0
    },
    {
        name => 'XML::Twig',
        version => 0
    },
    {
        name => 'LWP::UserAgent',
        version => 0
    },
    {
        name => 'PatchReader',
        version => '0.9.4'
    },
    {
        name => 'Image::Magick',
        version => 0
    },
    {
        name => 'Net::LDAP',
        version => 0
    },
];

# These are only required if you want to use Bugzilla with
# mod_perl.
use constant MOD_PERL_MODULES => [
    {
        name => 'mod_perl2', 
        version => '1.999022'
    },
    # Even very new releases of perl (5.8.5) don't come with this version,
    # so I didn't want to make it a general requirement just for
    # running under mod_cgi.
    {
        name => 'CGI',
        version => '3.11'
    },
    {
        name => 'Apache::DBI',
        version => '0.96'
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
    # We provide Template 2.14 or lower for Win32, so it still includes
    # the GD plugin.
    'Template::Plugin::GD' => 'Template',
};

sub check_requirements {
    my ($output) = @_;

    print "\nChecking perl modules...\n" if $output;
    my $modules = REQUIRED_MODULES;
    my $root = ROOT_USER;
    my %missing;

    foreach my $module (@{$modules}) {
        unless (have_vers($module->{name}, $module->{version}, $output)) {
            $missing{$module->{name}} = $module->{version};
        }
    }

    print "\nYou need one of the following DBD modules installed, depending",
          " on\nwhich database you are using with Bugzilla:\n" if $output;

    my $have_one_dbd = 0;
    my $db_modules = DB_MODULE;
    foreach my $db (keys %$db_modules) {
        if (have_vers($db_modules->{$db}->{dbd},
                      $db_modules->{$db}->{dbd_version}, $output))
        {
            $have_one_dbd = 1;
        }
    }

    print "\nThe following Perl modules are optional:\n" if $output;
    my $opt_modules = OPTIONAL_MODULES;
    my %have_mod;
    foreach my $module (@$opt_modules) {
        $have_mod{$module->{name}} =
            have_vers($module->{name}, $module->{version}, $output);
    }

    print "\nThe following modules are required for mod_perl support:\n"
        if $output;
    my $mp_modules = MOD_PERL_MODULES;
    foreach my $module (@$mp_modules) {
        $have_mod{$module->{name}} =
            have_vers($module->{name}, $module->{version}, $output);
    }

    # If we're running on Windows, reset the input line terminator so that
    # console input works properly - loading CGI tends to mess it up
    $/ = "\015\012" if ON_WINDOWS;

    if ($output) {
        print "\n";

        if ($^O =~ /MSWin32/i) {
            print "All the required modules are available at:\n",
                  "    http://landfill.bugzilla.org/ppm/\n",
                  "You can add the repository with the following command:\n",
                  "    ppm rep add bugzilla http://landfill.bugzilla.org/ppm/",
                  "\n\n";
        }

       # New/Old Charts
       if ((!$have_mod{'GD'} || !$have_mod{'Chart::Base'})) {
            print "If you you want to see graphical bug charts (plotting",
                  " historical data over \ntime), you should install libgd",
                  " and the following Perl modules (as $root):\n\n";
            print "    GD:    " . install_command("GD") ."\n" 
                if !$have_mod{'GD'};
            print "    Chart: " . install_command("Chart::Base") . "\n"
                if !$have_mod{'Chart::Base'};
            print "\n";
        }

        # Bug Import/Export
        if (!$have_mod{'XML::Twig'}) {
            print "If you want to use the bug import/export feature to move",
                  " bugs to or from\nother bugzilla installations, you will",
                  " need to install the XML::Twig\nmodule by running",
                  " (as $root):\n\n",
                  "    " . install_command("XML::Twig") . "\n\n";
         }

         # Automatic Updates
         if (!$have_mod{'LWP::UserAgent'}) {
             print "If you want to use the automatic update notification",
                   " feature you will\nneed to install the LWP::UserAgent",
                   " module by running (as $root):\n\n",
                   "    " . install_command("LWP::UserAgent") . "\n\n";
        }

        # BMP to PNG
        if (!$have_mod{'Image::Magick'}) {
            print "If you want to convert BMP image attachments to PNG to",
                  " conserve\ndisk space, you will need to install the",
                  " ImageMagick application\nAvailable from",
                  " http://www.imagemagick.org, and the Image::Magick\n",
                  "Perl module by running (as $root):\n\n",
                  "    " . install_command("Image::Magick") . "\n\n";
        }

        # Graphical Reports
        if (!$have_mod{'GD'} || !$have_mod{'GD::Graph'}
            || !$have_mod{'GD::Text::Align'}
            || !$have_mod{'Template::Plugin::GD::Image'})
        {
            print "If you want to see graphical bug reports (bar, pie and",
                  " line charts of \ncurrent data), you should install libgd",
                  " and the following Perl modules:\n\n";
            print "    GD:              " . install_command("GD") . "\n" 
                if !$have_mod{'GD'};
            print "    GD::Graph:       " . install_command("GD::Graph") . "\n"
                if !$have_mod{'GD::Graph'};
            print "    GD::Text::Align: " . install_command("GD::Text::Align") 
                . "\n" if !$have_mod{'GD::Text::Align'};
            print "    Template::Plugin::GD: " 
                . install_command('Template::Plugin::GD') . "\n" 
                if !$have_mod{'Template::Plugin::GD::Image'};
            print "\n";
        }

        # Diff View
        if (!$have_mod{'PatchReader'}) {
            print "If you want to see pretty HTML views of patches, you",
                  " should install the \nPatchReader module by running",
                  " (as $root):\n\n",
                  "    " . install_command("PatchReader") . "\n\n";
        }

        # LDAP
        if (!$have_mod{'Net::LDAP'}) {
            print "If you wish to use LDAP authentication, then you must",
                  " install Net::LDAP\nby running (as $root):\n\n",
                  "    " . install_command('Net::LDAP') . "\n\n";
        }

        # mod_perl
        if (!$have_mod{'mod_perl2'}) {
            print "If you would like mod_perl support, you must install at",
                  " least the minimum\nrequired version of mod_perl. You",
                  " can download mod_perl from:\n",
                  "    http://perl.apache.org/download/binaries.html\n",
                  "Make sure that you get the 2.0 version, not the 1.0",
                  " version.\n\n";
        }

        if (!$have_mod{'Apache::DBI'} || !$have_mod{'CGI'}) {
            print "For mod_perl support, you must install the following",
                  " perl module(s):\n\n";
            print "    Apache::DBI: " . install_command('Apache::DBI') . "\n"
                if !$have_mod{'Apache::DBI'};
            print "    CGI:         " . install_command('CGI') . "\n"
                if !$have_mod{'CGI'};
            print "\n";
        }
    }

    if (!$have_one_dbd) {
        print "\n";
        print "Bugzilla requires that at least one DBD module be",
              " installed in order to\naccess a database. You can install",
              " the correct one by running (as $root) the\ncommand listed",
              " below for your database:\n\n";

        foreach my $db (keys %$db_modules) {
            print $db_modules->{$db}->{name} . ": "
                  . install_command($db_modules->{$db}->{dbd}) . "\n";
            print "   Minimum version required: "
                  . $db_modules->{$db}->{dbd_version} . "\n";
        }
        print "\n";
    }

    if (%missing) {
        print "\n";
        print "Bugzilla requires some Perl modules which are either",
              " missing from your\nsystem, or the version on your system",
              " is too old. They can be installed\nby running (as $root)",
              " the following:\n";

        foreach my $module (keys %missing) {
            print "   " . install_command("$module") . "\n";
            if ($missing{$module} > 0) {
                print "   Minimum version required: $missing{$module}\n";
            }
        }
        print "\n";
    }

    return {
        pass     => !scalar(keys %missing) && $have_one_dbd,
        missing  => \%missing,
        optional => \%have_mod,
    }

}


# This was originally clipped from the libnet Makefile.PL, adapted here to
# use the below vers_cmp routine for accurate version checking.
sub have_vers {
    my ($pkg, $wanted, $output) = @_;
    my ($msg, $vnum, $vstr);
    no strict 'refs';
    printf("Checking for %15s %-9s ", $pkg, !$wanted?'(any)':"(v$wanted)") 
        if $output;

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
    print ((($vok) ? "ok: " : " "), "$vstr\n") if $output;
    return $vok ? 1 : 0;
}

# This is taken straight from Sort::Versions 1.5, which is not included
# with perl by default.
sub vers_cmp {
    my ($a, $b) = @_;

    # Remove leading zeroes - Bug 344661
    $a =~ s/^0*(\d.+)/$1/;
    $b =~ s/^0*(\d.+)/$1/;

    my @A = ($a =~ /([-.]|\d+|[^-.\d]+)/g);
    my @B = ($b =~ /([-.]|\d+|[^-.\d]+)/g);

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

=item C<check_requirements($output)>

 Description: This checks what optional or required perl modules
              are installed, like C<checksetup.pl> does.

 Params:      C<$output> - C<true> if you want the function to print
                           out information about what it's doing,
                           and the versions of everything installed.
                           If you don't pass the minimum requirements,
                           the will always print out something, 
                           regardless of this parameter.

 Returns:    A hashref containing three values:
             C<pass> - Whether or not we have all the mandatory 
                       requirements.
             C<missing> - A hash showing which mandatory requirements
                          are missing. The key is the module name,
                          and the value is the version we require.
             C<optional> - Which optional modules are installed and
                           up-to-date enough for Bugzilla.

=item C<vers_cmp($a, $b)>

 Description: This is a comparison function, like you would use in
              C<sort>, except that it compares two version numbers.
              It's actually identical to versioncmp from 
              L<Sort::Versions>.

 Params:      c<$a> and C<$b> are versions you want to compare.

 Returns:     -1 if $a is less than $b, 0 if they are equal, and
              1 if $a is greater than $b.

=item C<have_vers($pkg, $wanted, $output)>

 Description: Tells you whether or not you have the appropriate
              version of the module requested. It also prints
              out a message to the user explaining the check
              and the result.

 Params:      C<$pkg> - A string, the name of the package you're checking.
              C<$wanted> - The version of the package you require.
                           Set this to 0 if you don't require any
                           particular version.
              C<$output> - Set to true if you want this function to
                           print information to STDOUT about what it's
                           doing.

 Returns:   C<1> if you have the module installed and you have the
            appropriate version. C<0> otherwise.

=item C<install_command($module)>

 Description: Prints out the appropriate command to install the
              module specified, depending on whether you're
              on Windows or Linux.

 Params:      C<$module> - The name of the module.

 Returns:     nothing

=back
