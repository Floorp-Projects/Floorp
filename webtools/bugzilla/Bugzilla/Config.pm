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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dawn Endico <endico@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Jake <jake@bugzilla.org>
#                 J. Paul Reed <preed@sigkill.com>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>

package Bugzilla::Config;

use strict;

use base qw(Exporter);

use Bugzilla::Util;

# Module stuff
@Bugzilla::Config::EXPORT = qw(Param);

# Don't export localvars by default - people should have to explicitly
# ask for it, as a (probably futile) attempt to stop code using it
# when it shouldn't
# ChmodDataFile is here until that stuff all moves out of globals.pl
# into this file
@Bugzilla::Config::EXPORT_OK = qw($contenttypes ChmodDataFile);
%Bugzilla::Config::EXPORT_TAGS =
  (
   admin => [qw(GetParamList UpdateParams SetParam WriteParams)],
   db => [qw($db_host $db_port $db_name $db_user $db_pass)],
  );
Exporter::export_ok_tags('admin', 'db');

# Bugzilla version
$Bugzilla::Config::VERSION = "2.17.2";

use Safe;

use vars qw(@param_list);

# Data::Dumper is required as needed, below. The problem is that then when
# the code locally sets $Data::Dumper::Foo, this triggers 'used only once'
# warnings.
# We can't predeclare another package's vars, though, so just use them
{
    local $Data::Dumper::Sortkeys;
    local $Data::Dumper::Terse;
    local $Data::Dumper::Indent;
}

my %param;

# INITIALISATION CODE

# XXX - mod_perl - need to register Apache init handler for params
sub _load_datafiles {
    # read in localconfig variables
    do 'localconfig';

    if (-e 'data/params') {
        # Handle reading old param files by munging the symbol table
        # Don't have to do this if we use safe mode, since its evaled
        # in a sandbox where $foo is in the same module as $::foo
        #local *::param = \%param;

        # Note that checksetup.pl sets file permissions on 'data/params'

        # Using Safe mode is _not_ a guarantee of safety if someone does
        # manage to write to the file. However, it won't hurt...
        # See bug 165144 for not needing to eval this at all
        my $s = new Safe;

        $s->rdo('data/params');
        die "Error evaluating data/params: $@" if $@;

        # Now read the param back out from the sandbox
        %param = %{$s->varglob('param')};
    }
}

# Load in the datafiles
_load_datafiles();

# Load in the param defintions
unless (my $ret = do 'defparams.pl') {
    die "Couldn't parse defparams.pl: $@" if $@;
    die "Couldn't do defparams.pl: $!" unless defined $ret;
    die "Couldn't run defparams.pl" unless $ret;
}

# Stick the params into a hash
my %params;
foreach my $item (@param_list) {
    $params{$item->{'name'}} = $item;
}

# END INIT CODE

# Subroutines go here

sub Param {
    my ($param) = @_;

    # By this stage, the param must be in the hash
    die "Can't find param named $param" unless (exists $param{$param});

    return $param{$param};
}

sub GetParamList {
    return @param_list;
}

sub SetParam {
    my ($name, $value) = @_;

    die "Unknown param $name" unless (exists $params{$name});

    my $entry = $params{$name};

    # sanity check the value

    # XXX - This runs the checks. Which would be good, except that
    # check_shadowdb creates the database as a sideeffect, and so the
    # checker fails the second time arround...
    if ($name ne 'shadowdb' && exists $entry->{'checker'}) {
        my $err = $entry->{'checker'}->($value, $entry);
        die "Param $name is not valid: $err" unless $err eq '';
    }

    $param{$name} = $value;
}

sub UpdateParams {
    # --- PARAM CONVERSION CODE ---

    # Note that this isn't particuarly 'clean' in terms of separating
    # the backend code (ie this) from the actual params.
    # We don't care about that, though

    # Old bugzilla versions stored the version number in the params file
    # We don't want it, so get rid of it
    delete $param{'version'};

    # Change from a boolean for quips to multi-state
    if (exists $param{'usequip'} && !exists $param{'allowquips'}) {
        $param{'allowquips'} = $param{'usequip'} ? 'on' : 'off';
        delete $param{'usequip'};
    }

    # Change from old product groups to controls for group_control_map
    # 2002-10-14 bug 147275 bugreport@peshkin.net
    if (exists $param{'usebuggroups'} && !exists $param{'makeproductgroups'}) {
        $param{'makeproductgroups'} = $param{'usebuggroups'};
    }
    if (exists $param{'usebuggroupsentry'} 
       && !exists $param{'useentrygroupdefault'}) {
        $param{'useentrygroupdefault'} = $param{'usebuggroupsentry'};
    }

    # --- DEFAULTS FOR NEW PARAMS ---

    foreach my $item (@param_list) {
        my $name = $item->{'name'};
        $param{$name} = $item->{'default'} unless exists $param{$name};
    }

    # --- REMOVE OLD PARAMS ---

    my @oldparams = ();

    # Remove any old params
    foreach my $item (keys %param) {
        if (!grep($_ eq $item, map ($_->{'name'}, @param_list))) {
            require Data::Dumper;

            local $Data::Dumper::Terse = 1;
            local $Data::Dumper::Indent = 0;
            push (@oldparams, [$item, Data::Dumper->Dump([$param{$item}])]);
            delete $param{$item};
        }
    }

    return @oldparams;
}

sub WriteParams {
    require Data::Dumper;

    # This only has an affect for Data::Dumper >= 2.12 (ie perl >= 5.8.0)
    # Its just cosmetic, though, so that doesn't matter
    local $Data::Dumper::Sortkeys = 1;

    require File::Temp;
    my ($fh, $tmpname) = File::Temp::tempfile('params.XXXXX',
                                              DIR => 'data' );

    print $fh (Data::Dumper->Dump([ \%param ], [ '*param' ]))
      || die "Can't write param file: $!";

    close $fh;

    rename $tmpname, "data/params"
      || die "Can't rename $tmpname to data/params: $!";

    ChmodDataFile('data/params', 0666);
}

# Some files in the data directory must be world readable iff we don't have
# a webserver group. Call this function to do this.
# This will become a private function once all the datafile handling stuff
# moves into this package

# This sub is not perldoc'd for that reason - noone should know about it
sub ChmodDataFile {
    my ($file, $mask) = @_;
    my $perm = 0770;
    if ((stat('data'))[2] & 0002) {
        $perm = 0777;
    }
    $perm = $perm & $mask;
    chmod $perm,$file;
}

sub check_multi {
    my ($value, $param) = (@_);

    if ($param->{'type'} eq "s") {
        unless (lsearch($param->{'choices'}, $value) >= 0) {
            return "Invalid choice '$value' for single-select list param '$param'";
        }

        return "";
    }
    elsif ($param->{'type'} eq "m") {
        foreach my $chkParam (@$value) {
            unless (lsearch($param->{'choices'}, $chkParam) >= 0) {
                return "Invalid choice '$chkParam' for multi-select list param '$param'";
            }
        }

        return "";
    }
    else {
        return "Invalid param type '$param->{'type'}' for check_multi(); " .
          "contact your Bugzilla administrator";
    }
}

sub check_numeric {
    my ($value) = (@_);
    if ($value !~ /^[0-9]+$/) {
        return "must be a numeric value";
    }
    return "";
}

sub check_regexp {
    my ($value) = (@_);
    eval { qr/$value/ };
    return $@;
}

1;

__END__

=head1 NAME

Bugzilla::Config - Configuration parameters for Bugzilla

=head1 SYNOPSIS

  # Getting parameters
  use Bugzilla::Config;

  my $fooSetting = Param('foo');

  # Administration functions
  use Bugzilla::Config qw(:admin);

  my @valid_params = GetParamList();
  my @removed_params = UpgradeParams();
  SetParam($param, $value);
  WriteParams();

  # Localconfig variables may also be imported
  use Bugzilla::Config qw(:db);
  print "Connecting to $db_name as $db_user with $db_pass\n";

  # This variable does not belong in localconfig, and needs to go
  # somewhere better
  use Bugzilla::Config($contenttypes)

=head1 DESCRIPTION

This package contains ways to access Bugzilla configuration parameters.

=head1 FUNCTIONS

=head2 Parameters

Parameters can be set, retrieved, and updated.

=over 4

=item C<Param($name)>

Returns the Param with the specified name. Either a string, or, in the case
of multiple-choice parameters, an array reference.

=item C<GetParamList()>

Returns the list of known parameter types, from defparams.pl. Users should not
rely on this method; it is intended for editparams/doeditparams only

The format for the list is specified in defparams.pl

=item C<SetParam($name, $value)>

Sets the param named $name to $value. Values are checked using the checker
function for the given param if one exists.

=item C<UpdateParams()>

Updates the parameters, by transitioning old params to new formats, setting
defaults for new params, and removing obsolete ones.

Any removed params are returned in a list, with elements [$item, $oldvalue]
where $item is the entry in the param list.

=item C<WriteParams()>

Writes the parameters to disk.

=back

=head2 Parameter checking functions

All parameter checking functions are called with two parameters:

=over 4

=item *

The new value for the parameter

=item *

A reference to the entry in the param list for this parameter

=back

Functions should return error text, or the empty string if there was no error.

=over 4

=item C<check_multi>

Checks that a multi-valued parameter (ie type C<s> or type C<m>) satisfies
its contraints.

=item C<check_numeric>

Checks that the value is a valid number

=item C<check_regexp>

Checks that the value is a valid regexp

=back

