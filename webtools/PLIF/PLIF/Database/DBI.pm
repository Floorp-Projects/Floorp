# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::Database::DBI;
use strict;
use vars qw(@ISA);
use PLIF::Database;
use PLIF::Database::ResultsFrame::DBI;
use DBI; # DEPENDENCY
@ISA = qw(PLIF::Database);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'setup.configure' or $class->SUPER::provides($service));
}

# the name used to identify this database in the configuration
sub class {
    return 'default';
}

sub init {
    my $self = shift;
    $self->SUPER::init(@_);
    $self->openDB(@_);
}

sub openDB {
    my $self = shift;
    my($app) = @_;
    eval {
        $self->getConfig($app);
    };
    if ($@) {
        $self->handle(undef);
        $self->errstr($@);
        $self->dump(9, "failed to get the database configuration, not going to bother to connect: $@");
    } else {
        eval {
            $self->handle(DBI->connect($self->connectString, $self->username, $self->password,
                                       {RaiseError => 0, PrintError => 0, AutoCommit => 1, Taint => 1}));
            $self->errstr($DBI::errstr);
            $self->dump(9, 'created a database object without raising an exception');
        };
        if ($@) {
            $self->handle(undef);
            $self->errstr($@);
            $self->error(1, "failed to connect to the database because of $@");
        }
    }
}

sub closeDB {
    my $self = shift;
    if ($self->handle) {
        $self->handle->disconnect();
        $self->handle(undef);
    }
}

sub connectString {
    my $self = shift;
    my($name) = @_;
    if (not defined($name)) {
        $name = $self->name;
    }
    return 'DBI:'.($self->type).':'.($name).':'.($self->host).':'.($self->port);
}

sub lastError {
    my $self = shift;
    return $self->handle->err;
}

sub prepare {
    my $self = shift;
    my($statement) = @_;
    $self->createResultsFrame($statement);
}

sub execute {
    my $self = shift;
    my($statement, @values) = @_;
    $self->createResultsFrame($statement, 1, @values); # note: @values might be empty
}

sub createResultsFrame {
    my $self = shift;
    my($statement, $execute, @values) = @_;
    $self->assert($self->handle, 1, 'No database handle: '.(defined($self->errstr) ? $self->errstr : 'unknown error'));
    my $handle = $self->handle->prepare($statement);
    # untaint the values... (XXX?)
    foreach my $value (@values) {
        if (defined($value)) {
            $value =~ /^(.*)$/os;
            $value = $1;
        } else {
            $value = '';
        }
    }
    if ($handle and ((not defined($execute)) or $handle->execute(@values))) {
        return PLIF::Database::ResultsFrame::DBI->create($handle, $self, $execute);
    } else {
        $self->error(1, $handle->errstr);
    }
}

sub getConfig {
    my $self = shift;
    my($app) = @_;
    $app->getService('dataSource.configuration')->getDBIDatabaseSettings($app, $self);
}

sub propertyGetUndefined {
    my $self = shift;
    my($name) = @_;
    foreach my $property ($self->settings) {
        if ($name eq $property) {
            return '';
        }
    }
    return $self->SUPER::propertyGetUndefined(@_);
}

sub settings {
    # if you change this, check out setupConfigure to make sure it is still up to date
    return qw(type name host port username password);
}

sub setupConfigure {
    my $self = shift;
    my($app) = @_;
    $self->dump(9, 'about to configure DBI...');
    my $prefix = 'database.'.$self->class;
    $app->output->setupProgress($prefix);
    $self->closeDB();

    ## First, collect data from the user
    my %data;

    # connection details
    $app->output->setupProgress("$prefix.settings.connection");
    $data{'host'} = $self->setupConfigurePrompt($app, $prefix, 'host', 'localhost');
    $data{'port'} = $self->setupConfigurePrompt($app, $prefix, 'port', '3306');
    $data{'type'} = $self->setupConfigurePrompt($app, $prefix, 'type', 'mysql');

    # default the database name to the application name in lowercase with no punctuation, numbers, etc
    $app->output->setupProgress("$prefix.settings.database");
    $data{'name'} = lc($app->name);
    $data{'name'} =~ s/[^a-z]//gos;
    $data{'name'} = $self->setupConfigurePrompt($app, $prefix, 'name', $data{'name'});
    $data{'username'} = $self->setupConfigurePrompt($app, $prefix, 'username', $data{'name'}); # default username to the same thing
    $data{'password'} = $self->setupConfigurePrompt($app, $prefix, 'password');

    $self->dump(9, "got values, now sanity checking them.");

    # check that all values were given
    foreach my $property ($self->settings) {
        my $value = $data{$property};
        if (defined($value)) {
            $self->propertySet($property, $value);
        } else {
            $self->dump(9, "Did not have a value for '$property',aborting setup.");
            return "$prefix.$property";
        }
    }

    # save settings
    $app->output->setupProgress("$prefix.settings.saving");
    $app->getService('dataSource.configuration')->setDBIDatabaseSettings($app, $self);

    $self->dump(9, "checking to see if we can connect to the database.");

    ## Check the database itself
    $app->output->setupProgress("$prefix.admin.checking");
    eval {
        DBI->connect($self->connectString, $self->username, $self->password,
                     {RaiseError => 1, PrintError => 0, AutoCommit => 1, Taint => 1})->disconnect();
    };
    if ($@) {
        my $return = $self->setupConfigureDatabase($app, $prefix);
        if (defined($return)) {
            return $return; # propagate errors
        }
    }

    ## Finally, restart DBI
    $app->output->setupProgress("$prefix.connect");
    $self->openDB($app);
    $self->dump(9, 'done configuring DBI');
    return;
}

# This returns undef if we are in batch mode and the user didn't
# provide the information as an argument. In interactive mode, this
# will never return undef (unless the $default is undef). Therefore it
# is ok to delay the handling of the undef return value until after a
# few more calls of this routine. This allows us to check for undef
# just once instead of repeatedly.
sub setupConfigurePrompt {
    my $self = shift;
    my($app, $prefix, $property, $default) = @_;
    my $value = $self->propertyGet($property);
    if (not defined($value)) {
        $value = $default;
    }
    return $app->input->getArgument("$prefix.$property", $value);
}

# you should close the database handle before calling setupConfigureDatabase
sub setupConfigureDatabase {
    my $self = shift;
    my($app, $prefix) = @_;

    # get admin details for database
    $app->output->setupProgress("$prefix.settings.admin");

    my $adminUsername = $app->input->getArgument('database.adminUsername', 'root');
    if (not defined($adminUsername)) {
        return 'database.adminUsername';
    }

    my $adminPassword = $app->input->getArgument('database.adminPassword', '');
    if (not defined($adminPassword)) {
        return 'database.adminPassword';
    }

    my $adminHostname = $app->input->getArgument('database.adminHostname', 'localhost');
    if (not defined($adminHostname)) {
        return 'database.adminHostname';
    }

    # find a helper
    $app->output->setupProgress("$prefix.admin.configuring");
    my $helper;
    my @helpers = $app->getServiceList('database.helper');
    helper: foreach my $helperInstance (@helpers) {
        foreach my $helperType ($helperInstance->databaseType) {
            if ($helperType eq $self->type) {
                $helper = $helperInstance;
                last helper;
            }
        }
    }
    $self->assert(defined($helper), 1, 'No database helper installed for database type \''.$self->type.'\'');

    # connect
    eval {
        $self->handle(DBI->connect($self->connectString($helper->setupDatabaseName), $adminUsername, $adminPassword,
                                   {RaiseError => 0, PrintError => 1, AutoCommit => 1, Taint => 1}));
    };
    $self->assert((not $@), 1, "Could not connect to database: $@");
    $self->assert($self->handle, 1, 'Failed to connect to database: '.(defined($DBI::errstr) ? $DBI::errstr : 'unknown error'));

    # get the helper to do its stuff
    $helper->setupVerifyVersion($app, $self);
    $helper->setupCreateUser($app, $self, $self->username, $self->password, $adminHostname, $self->name);
    $helper->setupCreateDatabase($app, $self, $self->name);
    $helper->setupSetRights($app, $self, $self->username, $self->password, $adminHostname, $self->name);

    # disconnect
    $self->handle->disconnect();
    $self->handle(undef);
}

sub DESTROY {
    my $self = shift;
    $self->closeDB(@_);
    $self->SUPER::DESTROY(@_);
}
