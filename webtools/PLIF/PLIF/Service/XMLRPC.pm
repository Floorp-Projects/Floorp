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

package PLIF::Service::XMLRPC;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'service.xmlrpc' or 
            # $service eq 'dispatcher.output.generic' or # disabled - see end of file
            # $service eq 'dispatcher.output' or # disabled - see end of file
            $class->SUPER::provides($service));
}

sub objectProvides {
    my $self = shift;
    my($service) = @_;
    return ($service eq 'output.hook' or 
            $self->SUPER::objectProvides($service));
}

sub init {
    my $self = shift;
    my($app) = @_;
    require RPC::XML::Parser; import RPC::XML::Parser; # DEPENDENCY
    $self->SUPER::init(@_);
}

# service.xmlrpc
sub decodeXMLRPC {
    my $self = shift;
    my($app, $input) = @_;
    my $parser = RPC::XML::Parser->new();
    my $request = $parser->parse($input);
    $self->assert(ref($request), 1, "Invalid XML-RPC input: $request. Aborted");
    my $name = $request->name;
    my $values = $request->args;
    my $arguments;
    if (@$values == 1 and $values->[0]->type eq 'struct') {
        $arguments = $values->[0]->value;
    } else {
        my $index = 1;
        $arguments = {};
        foreach my $argument (@$values) {
            $arguments->{$index++} = $argument->value;
        }
    }
    return ($name, $arguments);
}

# service.xmlrpc
sub registerHook {
    my $self = shift;
    my($app) = @_;
    $app->addObject($self);
}

# output.hook
sub getOutputHook {
    my $self = shift;
    my($output) = @_;
    $self->output($output);
    return $self;
}

# output.hook
sub reportFatalError {
    my $self = shift;
    my($error) = @_;
    my $response = RPC::XML::response->new(RPC::XML::fault->new(0, "$error"));
    $self->output->XMLRPC($response->as_string);
}

# output.hook
sub methodMissing {
    my $self = shift;
    my($method, @arguments) = @_;
    # We drop 'method' on the floor, since it is assumed that an XML
    # RPC command can only result in a single output.
    # In theory, since any command can be an XML::RPC call, that's not
    # true. However, in practice, the XML RPC calls that are used will
    # have been designed so that there is enough information in the
    # arguments to be useful.
    my $response;
    @arguments = RPC::XML::smart_encode(@arguments);
    if (@arguments > 1) {
        $response = RPC::XML::response->new(RPC::XML::array->new(@arguments));
    } elsif (@arguments) {
        $response = RPC::XML::response->new(@arguments);
    } else {
        $response = RPC::XML::response->new(RPC::XML::boolean->new(1)); # XXX
    }
    $self->output->XMLRPC($response->as_string);
}

# disable implied property access so that all method calls are routed
# through methodMissing() above.
sub propertyImpliedAccessAllowed {
    my $self = shift;
    my($name) = @_;
    return ($name eq 'output');
}

# This is commented out because the default generic output module
# defaults to this behaviour anyway, and we don't really want this
# module being probed for output.generic.dispatcher stuff since it has
# an expensive initialisation cost.
# 
# # dispatcher.output.generic
# sub outputXMLRPC {
#     my $self = shift;
#     my($app, $output, $response) = @_;
#     $output->output('XMLRPC', {
#         'data' => [$response],
#     });
# }
# 
# # dispatcher.output
# sub strings {
#     return (
#             'XMLRPC' => 'Response to an XML RPC query. The XML document is in the argument data.0. It should be sent unadorned, except for wrappers relevant to the transport protocol (e.g. HTTP headers).',
#             );
# }
