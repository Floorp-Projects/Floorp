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

package PLIF::DataSource::Strings;
use strict;
use vars qw(@ISA);
use PLIF::DataSource;
use HTTP::Negotiate; # DEPENDENCY
use HTTP::Headers; # DEPENDENCY
@ISA = qw(PLIF::DataSource);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    # XXX this class should provide a 'clear caches' service (as should some others)
    return ($service eq 'dataSource.strings' or 
            $service eq 'setup.install' or 
            $class->SUPER::provides($service));
}

sub init {
    my $self = shift;
    $self->SUPER::init(@_);
    $self->variantsCache({});
    $self->stringsCache({});
}

sub databaseName {
    return 'default';
}

# returns ($type, $string)
sub get {
    my $self = shift;
    my($app, $session, $protocol, $string) = @_;
    # error handling makes code ugly :-)
    my $variant;
    if (defined($session)) {
        $variant = $session->selectVariant($protocol);
    }
    if (not defined($variant)) {
        # default session or $session didn't care, get stuff from
        # $app->input instead
        $variant = $self->selectVariant($app, $protocol);
    }
    if (not defined($self->stringsCache->{$variant})) {
        $self->stringsCache->{$variant} = {};
    }
    if (not defined($self->stringsCache->{$variant}->{$string})) {
        my @results;
        eval {
            @results = $self->getString($app, $variant, $string);
        };
        if ($@) {
            # ok, so, er, it seems that didn't go to well
            # XXX do we want to do an error here or something?
            $self->warn(4, "While I was looking for the string '$string' in protocol '$protocol' using variant '$variant', I failed with: $@");
        }
        if (not scalar(@results)) {
            $self->dump(9, "Did not find a string for '$string', going to look in the defaults...");
            @results = $self->getDefaultString($app, $protocol, $string);
            $self->assert(scalar(@results), 1, "Couldn't find a string to display for '$string' in protocol '$protocol'");
        }
        $self->stringsCache->{$variant}->{$string} = \@results;
        return @results;
    } else {
        return @{$self->stringsCache->{$variant}->{$string}};
    }
}

sub selectVariant {
    my $self = shift;
    my($app, $protocol) = @_;
    # Find list of options from DB.
    my $variants = $self->variants($app, $protocol);
    # Initialize the fake header
    my $request = new HTTP::Headers;
    foreach my $header (['Accept', $self->acceptType($app, $protocol)],
                        ['Accept-Encoding', $self->acceptEncoding($app, $protocol)],
                        ['Accept-Charset', $self->acceptCharset($app, $protocol)],
                        ['Accept-Language', $self->acceptLanguage($app, $protocol)]) {
        # only add headers that exist -- HTTP::Negotiate isn't very bullet-proof :-)
        if ($header->[1]) {
            $request->header(@$header);
        }
    }
    # Do Content Negotiation :-D
    my $choice;
    if (scalar(@$variants) > 0) {
        # $HTTP::Negotiate::DEBUG = 1; # enable debugging
        $choice = choose($variants, $request);
    }
    if (not defined($choice)) {
        $choice = 0; # XXX we could maybe not hard code the default variant some how... ;-)
    }
    return $choice;
}

# Variants returns an arrayref or arrayrefs, typically to be passed to
# HTTP::Negotiate, containing:
#    variant id, quality, content type, encoding, character set,
#    language, size
# Note that we don't support 'size', since doing so would require the
# unbelivably slow operation of calculating the length of the every
# possible string for everyone. No thanks. ;-)
sub variants {
    my $self = shift;
    my($app, $protocol) = @_;
    if (not defined($self->variantsCache->{$protocol})) {
        eval {
            $self->variantsCache->{$protocol} = $self->getVariants($app, $protocol);
        };
        if ($@) {
            # ok, so, er, it seems that didn't go to well
            # XXX do we want to do an error here or something?
            $self->warn(4, "Just so you know, I'm going to silently ignore the fact that I completely failed to get any variants... For what it's worth, the error was: $@");
            return []; # no variants here, no sir!
        }
    }
    return $self->variantsCache->{$protocol};
}


# XXX The next four SO have to change...
sub acceptType {
    my $self = shift;
    my($app, $protocol) = @_;
    return $app->input->acceptType;
}

sub acceptEncoding {
    my $self = shift;
    my($app, $protocol) = @_;
    return $app->input->acceptEncoding;
}

sub acceptCharset {
    my $self = shift;
    my($app, $protocol) = @_;
    return $app->input->acceptCharset;
}

sub acceptLanguage {
    my $self = shift;
    my($app, $protocol) = @_;
    return $app->input->acceptLanguage;
}


# "Low Level" API

sub getString {
    my $self = shift;
    # my($app, $variant, $string) = @_;
    $self->notImplemented();
    # return type, data
}

sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    return $app->getSelectingServiceList('dataSource.strings.default')->getDefaultString($app, $protocol, $string);
}

sub getVariants {
    my $self = shift;
    # my($app, $protocol) = @_;
    $self->notImplemented();
    # return id, quality, type, encoding, charset, language
}

sub getVariant {
    my $self = shift;
    # my($app, $id) = @_;
    $self->notImplemented();
    # return name, protocol, quality, type, encoding, charset, language, description, translator
}

sub getVariantStrings {
    my $self = shift;
    # my($app, $variant) = @_;
    $self->notImplemented();
    # return ( string => [ type, data ] )*;
}

sub getStringVariants {
    my $self = shift;
    # my($app, $string) = @_;
    $self->notImplemented();
    # return ( variant => [ type, data ] )*;
}

sub getDescribedVariants {
    my $self = shift;
    # my($app) = @_;
    $self->notImplemented();
    # return { id => { name, protocol, quality, type, encoding, charset, language, description, translator } }*
}

# an undefined $id means "add me please"
sub setVariant {
    my $self = shift;
    # my($app, $id, $name, $protocol, $quality, $type, $encoding, $charset, $language, $description, $translator) = @_;
    $self->notImplemented();
}

sub setString {
    my $self = shift;
    # my($app, $variant, $string, $type, $data) = @_;
    # if $data = '' then delete the relevant string from the database
    $self->notImplemented();
}

sub setupInstall {
    my $self = shift;
    $self->notImplemented();
}
