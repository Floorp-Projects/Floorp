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

package PLIF::Database::ConfigurationFile;
use strict;
use vars qw(@ISA);
use PLIF::Database;
use Data::Dumper; # DEPENDENCY
@ISA = qw(PLIF::Database);
1;

# WARNING XXX
# Reading without create a file first will FAIL!
#
# You must run the equivalent of an installer program to ensure the
# configuration file exists

sub init {
    my $self = shift;
    $self->SUPER::init(@_);
    my($app) = @_;
    $self->{'_FILENAME'} = $app->getService('dataSource.configuration')->configurationFilename;
}

sub class {
    return 'configuration';
}

sub type {
    return 'property';
}

sub filename {
    my $self = shift;
    return $self->{'_FILENAME'};
}

# typically you won't call this directly, but will use ensureRead below.
sub read {
    my $self = shift;
    my $settings;
    eval {
        $settings = $self->doRead($self->filename);
    };
    if ($@) {
        $self->warn(3, $@);
        return;
    }
    $self->{'_DIRTY'} = undef; # to prevent recursion: eval -> propertySet -> ensureRead (dirty check) -> read -> eval
    if ($settings) {
        $settings =~ /^(.*)$/so;
        eval($1); # untaint the configuration file
        $self->assert(defined($@), 1, 'Error processing configuration file \''.($self->filename).'\': '.$@);
    }
    $self->{'_DIRTY'} = 0;
}

# reads the database unless that was already done
sub ensureRead {
    my $self = shift;
    if (not exists($self->{'_DIRTY'})) {
        # not yet read configuration
        $self->read();
    }
}

# don't call this unless you know very well what you are doing
# it basically results in the file being overwritten (if you 
# call it before using propertyGet, anyway)
sub assumeRead {
    my $self = shift;
    $self->{'_DIRTY'} = 0;
}

# typically you won't call this directly, but will just rely on the
# DESTROY handler below.
sub write {
    my $self = shift;
    my $settings = "# This is the configuration file.\n# You may edit this file, so long as it remains valid Perl.\n";
    local $Data::Dumper::Terse = 1;
    foreach my $variable (sort(keys(%$self))) {
        if ($variable !~ /^_/o) { # we skip the internal variables (prefixed with '_')
            my $contents = Data::Dumper->Dump([$self->{$variable}]);
            chop($contents); # remove the newline (newline is guarenteed so no need to chomp)
            $settings .= "\$self->propertySet('$variable', $contents);\n";
        }
    }
    $self->doWrite($self->filename, $settings);
    $self->{'_DIRTY'} = 0;
}

sub propertySet {
    my $self = shift;
    $self->ensureRead();
    my $result = $self->SUPER::propertySet(@_);
    $self->{'_DIRTY'} = 1;
    return $result;
}

sub propertyExists {
    my $self = shift;
    $self->ensureRead();
    return $self->SUPER::propertyExists(@_);
}

sub propertyGet {
    my $self = shift;
    $self->ensureRead();
    return $self->SUPER::propertyGet(@_);
}

sub DESTROY {
    my $self = shift;
    if ($self->{'_DIRTY'}) {
        $self->write();
    }
    $self->SUPER::DESTROY(@_);
}


# internal low-level implementation routines

sub doRead {
    my $self = shift;
    my($filename) = @_;
    if (-e $filename) {
        local *FILE; # ugh
        $self->assert(open(FILE, "<$filename"), 1, "Could not open configuration file '$filename' for reading: $!");
        local $/ = undef; # slurp entire file (no record delimiter)
        my $settings = <FILE>;
        $self->assert(close(FILE), 3, "Could not close configuration file '$filename': $!");
        return $settings;
    } else {
        # file doesn't exist, so no configuration to read in
        return '';
    }
}

sub doWrite {
    my $self = shift;
    my($filename, $contents) = @_;
    local *FILE; # ugh
    $self->assert(open(FILE, ">$filename"), 1, "Could not open configuration file '$filename' for writing: $!");
    $self->assert(FILE->print($contents), 1, "Could not dump settings to configuration file '$filename': $!");
    $self->assert(close(FILE), 1, "Could not close configuration file '$filename': $!");
}
