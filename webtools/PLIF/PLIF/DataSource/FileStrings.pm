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

package PLIF::DataSource::FileStrings;
use strict;
use vars qw(@ISA $DIR);
use PLIF::DataSource;
@ISA = qw(PLIF::DataSource);
1;

# This is a default string data source, not a straight forward string
# data source, because it doesn't do content negotiation. It isn't
# really the absolute "default" though, in that if _this_ data source
# fails then in theory there will be one more fallback in the service
# (typically component) that 'invented' the string.
#
# This service is called by the straight forward string data source.

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dataSource.strings.default' or $class->SUPER::provides($service));
}

sub init {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    require File::Basename; import File::Basename; # DEPENDENCY
    $DIR = dirname(__FILE__);
}

sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    my @filenames = ();
    # XXX THIS IS PLATFORM SPECIFIC CODE XXX
    if ($^O eq 'linux') {
        foreach my $piece ($protocol, $string) {
            $piece =~ s/[^a-zA-Z\/0-9.]/_/gos;
        }
        # create a couple of paths relative to the application
        push(@filenames, "output-compiled/$protocol/$string");
        push(@filenames, "output/$protocol/$string");
        # and a couple relative to the PLIF library
        push(@filenames, $DIR."/../../output-compiled/$protocol/$string");
        push(@filenames, $DIR."/../../output/$protocol/$string");
    } else {
        $self->error(0, "Platform '$^O' not supported yet.");
    }
    # XXX END OF PLATFORM SPECIFIC CODE XXX
    foreach my $filename (@filenames) {
        if (-f $filename) {
            local *FILE; # ugh
            $self->assert(open(FILE, "<$filename"), 1, "Could not open output template file '$filename' for reading: $!");
            # get the data type (platform's line delimiter)
            local $/ = "\n";
            my @data;
            my $line;
            while (defined($line = <FILE>)) {
                chomp($line);
                if ($line eq '') {
                    # stop this when we reach the first blank line
                    last;
                }
                push(@data, $line);
            }
            # and then slurp entire file (no record delimiter)
            # while we're at it, untaint it (it came from the file system, it's safe).
            local $/ = undef;
            <FILE> =~ m/^(.*)$/os;
            push(@data, $1);
            $self->assert(close(FILE), 3, "Could not close output template file '$filename': $!");
            return @data;
        }
    }
    # no file exists
    return; # no can do, sir
}
