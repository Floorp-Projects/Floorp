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

package PLIF::Service::GenericOutputs;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dispatcher.output.generic' or 
            $service eq 'dispatcher.output' or 
            $service eq 'dataSource.strings.default' or
            $class->SUPER::provides($service));
}

# dispatcher.output.generic
# this is typically used by input devices
sub outputRequest {
    my $self = shift;
    my($app, $output, $argument, @defaults) = @_;
    $output->output('request', {
        'command' => $app->command,
        'argument' => $argument,
        'defaults' => \@defaults,
    });
}

# dispatcher.output.generic
sub outputReportFatalError {
    my $self = shift;
    my($app, $output, $error) = @_;
    $output->output('error', {
        'error' => $error,
    });   
}

# dispatcher.output
sub strings {
    return (
            'request' => 'A prompt for user input (only required for interactive protocols, namely stdout)',
            'error' => 'The message given to the user when something goes horribly wrong',
            );
}

# dataSource.strings.default
sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    if ($protocol eq 'stdout') {
        if ($string eq 'request') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">\'<text value="(data.argument)"/>\'<if lvalue="(data.defaults.length)" condition=">" rvalue="0"> (default: \'<set variable="default" value="(data.defaults)" source="keys" order="default"><text value="(data.defaults.(default))"/><if lvalue="(default)" condition="!=" rvalue="0">\', \'</if></set>\')</if>? </text>');
        } elsif ($string eq 'error') {
            $self->dump(9, 'Looks like an error occured, because the string \'error\' is being requested');
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">Error:<br/><text value="(data.error)"/><br/></text>');
        }
    } elsif ($protocol eq 'http') {
        if ($string eq 'error') {
            $self->dump(9, 'Looks like an error occured, because the string \'error\' is being requested');
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 500 Internal Error<br/>Content-Type: text/plain<br/><br/>Error:<br/><text value="(data.error)"/></text>');
        }
    }
    return; # nope, sorry
}
