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

package PLIF::Input::CGI;
use strict;
use vars qw(@ISA);
use PLIF::Input::Arguments;
@ISA = qw(PLIF::Input::Arguments);
1;

sub init {
    my $self = shift;
    my($app) = @_;
    require MIME::Base64; import MIME::Base64; # DEPENDENCY
    $self->SUPER::init(@_);
}

sub applies {
    return defined($ENV{'GATEWAY_INTERFACE'});
}

sub defaultOutputProtocol {
    return 'http';
}

sub splitArguments {
    my $self = shift;
    $self->dump(5, "Invoked as: $ENV{'SCRIPT_NAME'}");
    # register typical CGI variables
    foreach my $property (qw(SERVER_SOFTWARE SERVER_NAME
      GATEWAY_INTERFACE SERVER_PROTOCOL SERVER_PORT REQUEST_METHOD
      PATH_INFO PATH_TRANSLATED SCRIPT_NAME QUERY_STRING REMOTE_HOST
      REMOTE_ADDR AUTH_TYPE REMOTE_USER REMOTE_IDENT CONTENT_TYPE
      CONTENT_LENGTH)) {
        $self->propertySet($property, $ENV{$property});
    }
    foreach my $property (keys(%ENV)) {
        if ($property =~ /^HTTP_/o) {
            $self->propertySet($property, $ENV{$property});
        }
    }
    # hook in the metadata variables
    $self->metaData({}); # empty the list of meta data first
    $self->registerPropertyAsMetaData('UA', 'HTTP_USER_AGENT');
    $self->registerPropertyAsMetaData('referrer', 'HTTP_REFERER');
    $self->registerPropertyAsMetaData('host', 'REMOTE_HOST', 'REMOTE_ADDR');
    $self->registerPropertyAsMetaData('acceptType', 'HTTP_ACCEPT');
    $self->registerPropertyAsMetaData('acceptCharset', 'HTTP_ACCEPT_CHARSET');
    $self->registerPropertyAsMetaData('acceptEncoding', 'HTTP_ACCEPT_ENCODING');
    $self->registerPropertyAsMetaData('acceptLanguage', 'HTTP_ACCEPT_LANGUAGE');
    # decode username and password data
    if (defined($ENV{'HTTP_AUTHORIZATION'})) {
        if ($self->HTTP_AUTHORIZATION =~ /^Basic +(.*)$/os) {
            # HTTP Basic Authentication
            my($username, $password) = split(/:/, decode_base64($1), 2);
            $self->username($username);
            $self->password($password);
        } else {
            # Some other authentication scheme
        }
    }
    # hook in cookies
    $self->cookies({}); # empty the list of cookies first
    if (defined($ENV{'HTTP_COOKIE'})) {
        foreach my $cookie (split(/; /os, $ENV{'HTTP_COOKIE'})) {
            my($field, $value) = split(/=/os, $cookie);
            $self->cookies->{$field} = $value;
        }
    }
    # decode the arguments
    $self->decodeHTTPArguments;
}

sub decodeHTTPArguments {
    my $self = shift;
    $self->notImplemented();
}

# Takes as input a string encoded as per the
#    application/x-www-form-urlencoded
# ...format, and a coderef to a routine expecting a key/value pair.
# Typically, the coderef will be  sub { $self->addArgument(@_); }
# This is used by several methods, including GET, HEAD and one POST.
sub splitURLEncodedForm {
    my $self = shift;
    my($input, $output) = @_;
    use re 'taint'; # don't untaint stuff
    foreach my $argument (split(/&/o, $input)) {
        if ($argument =~ /^(.*?)(?:=(.*))?$/os) {
            my $name = $1;
            my $value = $2;
            # decode the strings
            foreach my $string ($name, $value) {
                if (defined($string)) {
                    $string =~ tr/+/ /; # convert + to spaces
                    $string =~ s/% # a percent symbol
                                 ( # followed by
                    [0-9A-Fa-f]{2} # 2 hexidecimal characters
                                 ) # which we shall put in $1
                     /chr(hex($1)) # and convert back into a character
                            /egox; # (evaluate, globally, optimised, with comments)
                } else {
                    $string = '';
                }
            }
            &$output($name, $value);
        } else {
            $self->warn(2, "argument (|$argument|) did not match regexp (can't happen!)");
        }
    }
}

sub setCommandArgument {
    my $self = shift;
    my $argument = $self->getArgument('command');
    if ($argument) {
        $self->command($argument);
    } else {
        $self->command('');
    }
}

sub getMetaData {
    my $self = shift;
    my($field) = @_;
    return $self->metaData->{$field};
}

sub registerPropertyAsMetaData {
    my $self = shift;
    my($field, @propertys) = @_;
    foreach my $property (@propertys) {
        my $value = $self->propertyGet($property);
        if (defined($value)) {
            $self->metaData->{$field} = $value;
            last;
        }
    }
}

# cookies
sub getSessionData {
    my $self = shift;
    my($field) = @_;
    return $self->cookies->{$field};
}
