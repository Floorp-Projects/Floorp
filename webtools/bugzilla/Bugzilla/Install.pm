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

use Bugzilla::Group;
use Bugzilla::Product;
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
    per_bug_queries    => { options => ['on', 'off'], default => 'on' },
    # 2006-05-01 olav@bkor.dhs.org -- Bug 7710
    state_addselfcc    => { options => ['always', 'never',  'cc_unless_role'],
                            default => 'cc_unless_role' },

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
        add_setting($setting, $settings{$setting}->{options}, 
                    $settings{$setting}->{default});
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
