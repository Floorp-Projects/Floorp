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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): David Koenig <dkoenig@novell.com>

=head1 NAME

Bugzilla::Testopia::XmlReferences - Testopia XmlReferences object

=head1 DESCRIPTION

This module maintains references to objects while the XML data is
being imported.  Test plans and Test cases can be referenced by 
database id, database description or XML description.  The references
are stored here and processed as needed.

=head1 SYNOPSIS

use Bugzilla::Testopia::XmlReferences;


=cut

package Bugzilla::Testopia::XmlReferences;

use constant IGNORECASE => "ignorecase";

#use strict;

sub new
{
    my ($invocant,$ignorecase,$fields) = @_;

    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    $self{IGNORECASE} = $ignorecase;
    for my $field ( split(/ /, $fields) )
    {
        $field = uc $field if ( $self{IGNORECASE} );
        $self->{$field} = [];
    }
    return $self;
}

sub add
{
    my ($self, $type, $object) = @_;
    
    $type = uc $type if ( $self{IGNORECASE} );

    return 0 if ( ! exists $self->{$type} );

    push @{$self->{$type}}, $object;
}

sub display
{
    my ($self) = @_;
    
    print "display() self=" . $self . "\n";
    foreach $key (keys %$self)
    {
        if ( defined $self->{$key} )
        {
            print "display()   key=$key value=" . $self->{$key} . "\n";
        }
        else
        {
            print "display()   key=$key value=undefined\n";
        }
    }
}

sub get 
{
        my ($self, $type) = @_;
        
        $type = uc $type if ( $self{IGNORECASE} );
        
        return 0 if ( ! exists $self->{$type} );
        
        return $self->{$type};
}

1;
