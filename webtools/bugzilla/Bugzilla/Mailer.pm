# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>,
#                 Bryce Nesbitt <bryce-mozilla@nextbus.com>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Alan Raetz <al_raetz@yahoo.com>
#                 Jacob Steenhagen <jake@actex.net>
#                 Matthew Tuck <matty@chariot.net.au>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 J. Paul Reed <preed@sigkill.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Byron Jones <bugzilla@glob.com.au>
#                 Frédéric Buclin <LpSolit@gmail.com>

package Bugzilla::Mailer;

use strict;

use base qw(Exporter);
@Bugzilla::Mailer::EXPORT = qw(MessageToMTA);

use Bugzilla::Constants;
use Bugzilla::Util;

use Mail::Header;
use Mail::Mailer;
use Mail::Address;
use MIME::Parser;
use MIME::QuotedPrint;
use MIME::Base64;


sub MessageToMTA {
    my ($msg) = (@_);
    my $params = Bugzilla->params;
    return if ($params->{'mail_delivery_method'} eq "none");

    my ($header, $body) = $msg =~ /(.*?\n)\n(.*)/s ? ($1, $2) : ('', $msg);
    my $headers;

    if ($params->{'utf8'} 
        and (!is_7bit_clean($header) or !is_7bit_clean($body))) 
    {
        ($headers, $body) = encode_message($msg);
    } else {
        my @header_lines = split(/\n/, $header);
        $headers = new Mail::Header \@header_lines, Modify => 0;
    }

    # Use trim to remove any whitespace (incl. newlines)
    my $from = trim($headers->get('from'));

    if ($params->{"mail_delivery_method"} eq "sendmail" && $^O =~ /MSWin32/i) {
        my $cmd = '|' . SENDMAIL_EXE . ' -t -i';
        if ($from) {
            # We're on Windows, thus no danger of command injection
            # via $from. In other words, it is safe to embed $from.
            $cmd .= qq# -f"$from"#;
        }
        open(SENDMAIL, $cmd) ||
            die "Failed to execute " . SENDMAIL_EXE . ": $!\n";
        print SENDMAIL $headers->as_string;
        print SENDMAIL "\n";
        print SENDMAIL $body;
        close SENDMAIL;
        return;
    }

    my @args;
    if ($params->{"mail_delivery_method"} eq "sendmail") {
        push @args, "-i";
        if ($from) {
            push(@args, "-f$from");
        }
    }
    if ($params->{"mail_delivery_method"} eq "sendmail" 
        && !$params->{"sendmailnow"}) 
    {
        push @args, "-ODeliveryMode=deferred";
    }
    if ($params->{"mail_delivery_method"} eq "smtp") {
        push @args, Server => $params->{"smtpserver"};
        if ($from) {
            $ENV{'MAILADDRESS'} = $from;
        }
    }
    my $mailer = new Mail::Mailer($params->{"mail_delivery_method"}, @args);
    if ($params->{"mail_delivery_method"} eq "testfile") {
        $Mail::Mailer::testfile::config{outfile} = 
            bz_locations()->{'datadir'} . '/mailer.testfile';
    }
    
    $mailer->open($headers->header_hashref);
    print $mailer $body;
    $mailer->close;
}

sub encode_message {
    my ($msg) = @_;

    my $parser = MIME::Parser->new;
    $parser->output_to_core(1);
    $parser->tmp_to_core(1);
    my $entity = $parser->parse_data($msg);
    $entity = encode_message_entity($entity);

    my @header_lines = split(/\n/, $entity->header_as_string);
    my $head = new Mail::Header \@header_lines, Modify => 0;

    my $body = $entity->body_as_string;

    return ($head, $body);
}

sub encode_message_entity {
    my ($entity) = @_;

    my $head = $entity->head;

    # encode the subject

    my $subject = $head->get('subject');
    if (defined $subject && !is_7bit_clean($subject)) {
        $subject =~ s/[\r\n]+$//;
        $head->replace('subject', encode_qp_words($subject));
    }

    # encode addresses

    foreach my $field (qw(from to cc reply-to sender errors-to)) {
        my $high = $head->count($field) - 1;
        foreach my $index (0..$high) {
            my $value = $head->get($field, $index);
            my @addresses;
            my $changed = 0;
            foreach my $addr (Mail::Address->parse($value)) {
                my $phrase = $addr->phrase;
                if (is_7bit_clean($phrase)) {
                    push @addresses, $addr->format;
                } else {
                    push @addresses, encode_qp_phrase($phrase) . 
                        ' <' . $addr->address . '>';
                    $changed = 1;
                }
            }
            $changed && $head->replace($field, join(', ', @addresses), $index);
        }
    }

    # process the body

    if (scalar($entity->parts)) {
        my $newparts = [];
        foreach my $part ($entity->parts) {
            my $newpart = encode_message_entity($part);
            push @$newparts, $newpart;
        }
        $entity->parts($newparts);
    }
    else {
        # Extract the body from the entity, for examination
        # At this point, we can rely on MIME::Tools to do our encoding for us!
        my $bodyhandle = $entity->bodyhandle;
        my $body = $bodyhandle->as_string;
        if (!is_7bit_clean($body)) {
            # count number of 7-bit chars, and use quoted-printable if more
            # than half the message is 7-bit clean
            my $count = ($body =~ tr/\x20-\x7E\x0A\x0D//);
            if ($count > length($body) / 2) {
                $head->mime_attr('Content-Transfer-Encoding' => 'quoted-printable');
            } else {
                $head->mime_attr('Content-Transfer-Encoding' => 'base64');
            }
        }

        # Set the content/type and charset of the part, if not set
        $head->mime_attr('Content-Type' => 'text/plain')
            unless defined $head->mime_attr('content-type');
        $head->mime_attr('Content-Type.charset' => 'UTF-8');
    }

    $head->fold(75);
    return $entity;
}

sub encode_qp_words {
    my ($line) = (@_);
    my @encoded;
    foreach my $word (split / /, $line) {
        if (!is_7bit_clean($word)) {
            push @encoded, '=?UTF-8?Q?_' . encode_qp($word, '') . '?=';
        } else {
            push @encoded, $word;
        }
    }
    return join(' ', @encoded);
}

1;
