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

package Bugzilla::Install;

# Functions in this this package can assume that the database 
# has been set up, params are available, localconfig is
# available, and any module can be used.
#
# If you want to write an installation function that can't
# make those assumptions, then it should go into one of the
# packages under the Bugzilla::Install namespace.

use strict;

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Group;
use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::User::Setting;
use Bugzilla::Version;

use constant SETTINGS => {
    # 2005-03-03 travis@sedsystems.ca -- Bug 41972
    display_quips      => { options => ["on", "off"], default => "on" },
    # 2005-03-10 travis@sedsystems.ca -- Bug 199048
    comment_sort_order => { options => ["oldest_to_newest", "newest_to_oldest",
                                        "newest_to_oldest_desc_first"],
                            default => "oldest_to_newest" },
    # 2005-05-12 bugzilla@glob.com.au -- Bug 63536
    post_bug_submit_action => { options => ["next_bug", "same_bug", "nothing"],
                                default => "next_bug" },
    # 2005-06-29 wurblzap@gmail.com -- Bug 257767
    csv_colsepchar     => { options => [',',';'], default => ',' },
    # 2005-10-26 wurblzap@gmail.com -- Bug 291459
    zoom_textareas     => { options => ["on", "off"], default => "on" },
    # 2005-10-21 LpSolit@gmail.com -- Bug 313020
    per_bug_queries    => { options => ['on', 'off'], default => 'off' },
    # 2006-05-01 olav@bkor.dhs.org -- Bug 7710
    state_addselfcc    => { options => ['always', 'never',  'cc_unless_role'],
                            default => 'cc_unless_role' },
    # 2006-08-04 wurblzap@gmail.com -- Bug 322693
    skin               => { subclass => 'Skin', default => 'standard' },

};

use constant DEFAULT_CLASSIFICATION => {
    name        => 'Unclassified',
    description => 'Unassigned to any classification'
};

use constant DEFAULT_PRODUCT => {
    name => 'TestProduct',
    description => 'This is a test product.'
        . ' This ought to be blown away and replaced with real stuff in a'
        . ' finished installation of bugzilla.'
};

use constant DEFAULT_COMPONENT => {
    name => 'TestComponent',
    description => 'This is a test component in the test product database.'
        . ' This ought to be blown away and replaced with real stuff in'
        . ' a finished installation of Bugzilla.'
};

sub update_settings {
    my %settings = %{SETTINGS()};
    foreach my $setting (keys %settings) {
        add_setting($setting,
                    $settings{$setting}->{options}, 
                    $settings{$setting}->{default},
                    $settings{$setting}->{subclass});
    }
}

# This function should be called only after creating the admin user.
sub create_default_product {
    my $dbh = Bugzilla->dbh;

    # Make the default Classification if it doesn't already exist.
    if (!$dbh->selectrow_array('SELECT 1 FROM classifications')) {
        my $class = DEFAULT_CLASSIFICATION;
        print "Creating default classification '$class->{name}'...\n";
        $dbh->do('INSERT INTO classifications (name, description)
                       VALUES (?, ?)',
                 undef, $class->{name}, $class->{description});
    }

    # And same for the default product/component.
    if (!$dbh->selectrow_array('SELECT 1 FROM products')) {
        my $default_prod = DEFAULT_PRODUCT;
        print "Creating initial dummy product '$default_prod->{name}'...\n";

        $dbh->do(q{INSERT INTO products (name, description)
                        VALUES (?,?)}, 
                 undef, $default_prod->{name}, $default_prod->{description});

        my $product = new Bugzilla::Product({name => $default_prod->{name}});

        # The default version.
        Bugzilla::Version::create(Bugzilla::Version::DEFAULT_VERSION, $product);

        # And we automatically insert the default milestone.
        $dbh->do(q{INSERT INTO milestones (product_id, value, sortkey)
                        SELECT id, defaultmilestone, 0
                          FROM products});

        # Get the user who will be the owner of the Product.
        # We pick the admin with the lowest id, or we insert
        # an invalid "0" into the database, just so that we can
        # create the component.
        my $admin_group = new Bugzilla::Group({name => 'admin'});
        my ($admin_id)  = $dbh->selectrow_array(
            'SELECT user_id FROM user_group_map WHERE group_id = ?
           ORDER BY user_id ' . $dbh->sql_limit(1),
            undef, $admin_group->id) || 0;
 
        my $default_comp = DEFAULT_COMPONENT;

        $dbh->do("INSERT INTO components (name, product_id, description,
                                          initialowner)
                       VALUES (?, ?, ?, ?)", undef, $default_comp->{name},
                 $product->id, $default_comp->{description}, $admin_id);
    }

}

sub create_admin {
    my ($params) = @_;
    my $dbh      = Bugzilla->dbh;
    my $template = Bugzilla->template;

    my $admin_group = new Bugzilla::Group({ name => 'admin' });
    my $admin_inheritors = 
        Bugzilla::User->flatten_group_membership($admin_group->id);
    my $admin_group_ids = join(',', @$admin_inheritors);

    my ($admin_count) = $dbh->selectrow_array(
        "SELECT COUNT(*) FROM user_group_map 
          WHERE group_id IN ($admin_group_ids)");

    return if $admin_count;

    my %answer    = %{$params->{answer} || {}};
    my $login     = $answer{'ADMIN_EMAIL'};
    my $password  = $answer{'ADMIN_PASSWORD'};
    my $full_name = $answer{'ADMIN_REALNAME'};

    if (!$login || !$password || !$full_name) {
        print "\n" . get_text('install_admin_setup') . "\n\n";
    }

    while (!$login) {
        print get_text('install_admin_get_email') . ' ';
        $login = <STDIN>;
        chomp $login;
        eval { Bugzilla::User->check_login_name_for_creation($login); };
        if ($@) {
            print $@ . "\n";
            undef $login;
        }
    }

    while (!defined $full_name) {
        print get_text('install_admin_get_name') . ' ';
        $full_name = <STDIN>;
        chomp($full_name);
    }

    while (!$password) {
        # trap a few interrupts so we can fix the echo if we get aborted.
        local $SIG{HUP}  = \&_create_admin_exit;
        local $SIG{INT}  = \&_create_admin_exit;
        local $SIG{QUIT} = \&_create_admin_exit;
        local $SIG{TERM} = \&_create_admin_exit;

        system("stty","-echo") unless ON_WINDOWS;  # disable input echoing

        print get_text('install_admin_get_password') . ' ';
        $password = <STDIN>;
        print "\n", get_text('install_admin_get_password2') . ' ';
        my $pass2 = <STDIN>;
        eval { validate_password($password, $pass2); };
        if ($@) {
            print "\n$@\n";
            undef $password;
        }
        system("stty","echo") unless ON_WINDOWS;
    }

    my $admin = Bugzilla::User->create({ login_name    => $login, 
                                         realname      => $full_name,
                                         cryptpassword => $password });
    make_admin($admin);
}

sub make_admin {
    my ($user) = @_;
    my $dbh = Bugzilla->dbh;

    $user = ref($user) ? $user 
            : new Bugzilla::User(login_to_id($user, THROW_ERROR));

    my $admin_group = new Bugzilla::Group({ name => 'admin' });

    # Admins get explicit membership and bless capability for the admin group
    $dbh->selectrow_array("SELECT id FROM groups WHERE name = 'admin'");

    my $group_insert = $dbh->prepare(
        'INSERT INTO user_group_map (user_id, group_id, isbless, grant_type)
              VALUES (?, ?, ?, ?)');
    # These are run in an eval so that we can ignore the error of somebody
    # already being granted these things.
    eval { 
        $group_insert->execute($user->id, $admin_group->id, 0, GRANT_DIRECT); 
    };
    eval {
        $group_insert->execute($user->id, $admin_group->id, 1, GRANT_DIRECT);
    };

    # Admins should also have editusers directly, even though they'll usually
    # inherit it. People could have changed their inheritance structure.
    my $editusers = new Bugzilla::Group({ name => 'editusers' });
    eval { 
        $group_insert->execute($user->id, $editusers->id, 0, GRANT_DIRECT); 
    };

    print "\n", get_text('install_admin_created', { user => $user }), "\n";
}

# This is just in case we get interrupted while getting the admin's password.
sub _create_admin_exit {
    # re-enable input echoing
    system("stty","echo") unless ON_WINDOWS;
    exit 1;
}

sub get_text {
    my ($name, $vars) = @_;
    my $template = Bugzilla->template;
    $vars ||= {};
    $vars->{'message'} = $name;
    my $message;
    $template->process('global/message.txt.tmpl', $vars, \$message)
        || ThrowTemplateError($template->error());
    $message =~ s/^\s+//gm;
    return $message;
}

1;

__END__

=head1 NAME

Bugzilla::Install - Functions and variables having to do with
  installation.

=head1 SYNOPSIS

 use Bugzilla::Install;
 Bugzilla::Install::update_settings();

=head1 DESCRIPTION

This module is used primarily by L<checksetup.pl> during installation.
This module contains functions that deal with general installation
issues after the database is completely set up and configured.

=head1 CONSTANTS

=over

=item C<SETTINGS>

Contains information about Settings, used by L</update_settings()>.

=back

=head1 SUBROUTINES

=over

=item C<update_settings()>

Description: Adds and updates Settings for users.

Params:      none

Returns:     nothing.

=item C<create_default_product()>

Description: Creates the default product and classification if
             they don't exist.

Params:      none

Returns:     nothing

=back
