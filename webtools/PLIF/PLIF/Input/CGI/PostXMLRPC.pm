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

package PLIF::Input::CGI::PostXMLRPC;
use strict;
use vars qw(@ISA);
use PLIF::Input::CGI;
@ISA = qw(PLIF::Input::CGI);
1;

sub applies {
    my $class = shift;
    return ($class->SUPER::applies(@_) and
            defined($ENV{'REQUEST_METHOD'}) and
            $ENV{'REQUEST_METHOD'} eq 'POST' and
            defined($ENV{'CONTENT_TYPE'}) and
            $ENV{'CONTENT_TYPE'} eq 'text/xml');
}

sub decodeHTTPArguments {
    my $self = shift;

    # slurp the input
    local $/ = undef;
    my $input = <STDIN>;

    $self->dump(9, 'XML-RPC call. Input was:', '=' x 72 . "\n$input", '=' x 72);

    # find someone who understands XML RPC
    my $service = $self->app->getService('service.xmlrpc');

    # decode the XML stream and stuff the arguments from it
    my($method, $arguments) = $service->decodeXMLRPC($self->app, $input);
    $self->addArgument('command', $method);
    foreach my $argument (keys(%$arguments)) {
        $self->addArgument($argument, $arguments->{$argument});        
    }

    # tell the XML RPC expert to that our input is being used
    $service->registerHook($self->app);
}
