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

package PLIF::Input::CGI::PostMultipart;
use strict;
use vars qw(@ISA);
use PLIF::Input::CGI;
@ISA = qw(PLIF::Input::CGI);
1;

sub init {
    my $self = shift;
    my($app) = @_;
    require MIME::Parser; import MIME::Parser; # DEPENDENCY
    $self->SUPER::init(@_);
}

sub applies {
    my $class = shift;
    return ($class->SUPER::applies(@_) and
            defined($ENV{'REQUEST_METHOD'}) and
            $ENV{'REQUEST_METHOD'} eq 'POST' and
            defined($ENV{'CONTENT_TYPE'}) and
            $ENV{'CONTENT_TYPE'} =~ m/^multipart\/form-data *;/os);
}

sub decodeHTTPArguments {
    my $self = shift;

    # initialise the parser
    my $parser = MIME::Parser->new();
    $parser->decode_headers(1);

    # XXX THIS IS PLATFORM SPECIFIC CODE XXX
    if ($^O eq 'linux') {
        $parser->output_dir('/tmp');
    } else {
        $self->error(0, "Platform '$^O' not supported yet.");
    }
    # XXX END OF PLATFORM SPECIFIC CODE XXX

    $self->dump(9, 'HTTP POST. Input was in multipart/form-data format.');

    # parse the MIME body
    local $/ = undef;
    my $entity = $parser->parse_data('Content-Type: '   . $self->CONTENT_TYPE   . "\n" .
                                     'Content-Length: ' . $self->CONTENT_LENGTH . "\n" .
                                     "\n" . <STDIN>);

    # handle the parts of the MIME body
    # read up to 16KB, no more
    # this prevents nasty DOS attacks (XXX in theory)
    my $maxLength = 16*1024; # XXX HARDCODED CONSTANT ALERT
    my $currentSize = 0;
    foreach my $part ($entity->parts) {
        my $head = $part->head;
        if (lc($head->mime_attr('content-disposition')) eq 'form-data') {

            # perform I/O
            my $data = '';
            my $handle = $part->bodyhandle->open("r");
            my $readLength = $handle->read($data, $maxLength+1);
            $handle->close();

            # check we read the data
            $self->assert(defined($readLength), 1,
                          "Something failed while reading input");

            # check we are within the limit
            $currentSize += $readLength;
            $self->assert($currentSize <= $maxLength, 1,
                          "More than $maxLength bytes of data sent; aborted");

            # ok, add string
            $self->addArgument($head->mime_attr('content-disposition.name'), $data);

        } else {
            # not form-data
            # XXX over HTTP this should cause a 4xx error not a 5xx error
            $self->error(1, 'malformed submission (an entity was not form-data)');
        }
    }

    # store the entity so that we can purge the files later
    $self->entity($entity);
}

sub DESTROY {
    my $self = shift;
    $self->entity->purge();
    $self->SUPER::destroy();
}
