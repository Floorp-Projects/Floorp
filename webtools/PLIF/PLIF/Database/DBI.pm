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
            my $type = $self->type;
            my $name = $self->name;
            my $host = $self->host;
            my $port = $self->port;
            $self->handle(DBI->connect("DBI:$type:$name:$host:$port", 
                                       $self->username, $self->password, 
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

sub settings {
    return qw(type name host port username password);
}

sub lastError {
    my $self = shift;
    return $self->handle->err;
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

sub setupConfigure {
    my $self = shift;
    my($app) = @_;
    $self->dump(9, 'about to configure DBI...');
    $app->output->setupProgress('database');
    my $prefix = 'database.'.$self->class;
    foreach my $property ($self->settings) {
        # XXX need to be able to offer current configuration as default values!
        if (not $self->propertyExists($property)) {
            my $value = $app->input->getArgument("$prefix.$property");
            $self->dump(9, "Setting value '$property' to '$value'");
            if (defined($value)) {
                $self->propertySet($property, $value);
            } else {
                return "$prefix.$property";
            }
        }
    }
    $app->getService('dataSource.configuration')->setDBIDatabaseSettings($app, $self);
    $self->dump(9, 'done configuring DBI');
    return;
}

sub DESTROY {
    my $self = shift;
    if ($self->handle) {
        $self->handle->disconnect();
        $self->handle(undef);
    }
    $self->SUPER::DESTROY(@_);
}
