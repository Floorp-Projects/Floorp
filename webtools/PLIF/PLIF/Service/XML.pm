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

package PLIF::Service::XML;
use strict;
use vars qw(@ISA);
use PLIF::Service;
use XML::Parser; # DEPENDENCY
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'service.xml' or $class->SUPER::provides($service));
}

sub parse {
    my $self = shift;
    my($string) = @_;
    return XML::Parser->new(Style => 'Tree', ErrorContext => 1)->parse($string);
}

sub parseNS {
    my $self = shift;
    my($string) = @_;
    return XML::Parser->new(Style => __PACKAGE__, Namespaces => 1, ErrorContext => 1)->parse($string);
}

sub escape {
    my $self = shift;
    my($value) = @_;
    $value =~ s/&/&amp;/go;
    $value =~ s/"/&quot;/go; # " (reset font-lock)
    $value =~ s/'/&apos;/go; # ' (reset font-lock)
    $value =~ s/</&lt;/go;
    $value =~ s/>/&gt;/go;
    return $value;
}


# This is a convenient way of walking a DOM. The first argument should
# be an object which has one or more of the following methods
# implemented:
#
#   walkAttributes -- takes an attributes hash and the local variable
#   scope and returns 1 if the element should be processed.
#
#   walkElement -- takes a tagname, an attribute hash, a tree
#   representing the contents of the element, and the local variable
#   scope and returns 1 if the element's contents should be processed.
#
#   walkText -- takes a string and the local variable scope.
#
#   walkNesting -- takes the local variable scope and should return a
#   deep copy of the local variable scope so that any changes that the
#   other walk* functions make to the local variable scope will not
#   affect any other copies of the local variable scope. If the local
#   variable scope is a non-reference scalar then simply returning a
#   copy is enough (this is the default behaviour). If the local
#   variable scope is a simple hash or array reference to non-
#   reference scalars, then returning a reference to a copy of the
#   dereferenced contents of the local variable scope is enough.
#   Anything more complex requires thought and is likely to be slower
#   than you would like.
#
# For convenience when documenting the implementation of such an
# interface, you may assume it has a service name of
# 'service.xml.sink' although in reality nothing ever looks for this
# service; it is merely a useful concept.
#
sub walk {
    my $self = shift;
    my($handler, $tree, $data) = @_;
    # initialise the handlers
    my $walkAttributes = $handler->can('walkAttributes') ? sub { $handler->walkAttributes(@_); } : sub { return 1; };
    my $walkElement = $handler->can('walkElement') ? sub { $handler->walkElement(@_); } : sub { return 1; };
    my $walkText = $handler->can('walkText') ? sub { $handler->walkText(@_); } : sub { return 1; };
    my $walkNesting = $handler->can('walkNesting') ? sub { $handler->walkNesting(@_); } : sub { shift; return @_; };
    # walk the tree
    my $index = 0;
    my @stack = ();
    do {
        while ($index <= $#$tree) {
            # more data to deal with at this level
            my $tagname = $tree->[$index];
            my $contents = $tree->[$index+1];
            $index += 2; # move the pointer on to the next node
            if ($tagname) { 
                # element node
                my $attributes = shift(@$contents);
                my $localdata = &$walkNesting($data);
                # process global attributes and element
                if ((&$walkAttributes($attributes, $localdata)) and
                    (&$walkElement($tagname, $attributes, $contents, $localdata))) {
                    push(@stack, [$tree, $index, $data]);
                    $tree = $contents;
                    $index = 0;
                    $data = $localdata;
                }
            } else {
                # raw text node
                &$walkText($contents, $data);
            }
        }
    } while (scalar(@stack) and (($tree, $index, $data) = @{pop(@stack)}));
}


# Internal routines for creating a namespace-aware XML tree.
#
# If I was clever, I could just merge this with walk() above and do it
# all in one step. Wouldn't that be nice. XXX

sub Init {
    my $parser = shift;
    $parser->{'Lists'} = [];
    $parser->{'Current List'} = [];
    $parser->{'Tree'} = $parser->{'Current List'};
}

sub Start {
    my $parser = shift;
    my($tagName, @attributes) = @_;
    # for those attributes in a particular namespace, expand their names
    my $name = 1;
    foreach my $attribute (@attributes) {
        if ($name) {
            my $ns = $parser->namespace($attribute);
            if (defined($ns)) {
                $attribute = "{$ns}$attribute";
            }
        } # else it's the value, skip it
        $name = not $name;
    }
    my $newList = [{@attributes}];
    # if the tag name is in a particular namespace, expand it too
    my $ns = $parser->namespace($tagName);
    if (defined($ns)) {
        $tagName = "{$ns}$tagName";
    }
    # push the current level onto the stack
    push(@{$parser->{'Current List'}}, $tagName => $newList);
    push(@{$parser->{'Lists'}}, $parser->{'Current List'});
    $parser->{'Current List'} = $newList;
}

sub End {
    my $parser = shift;
    my($tagName) = @_;
    # pop the current level off the stack
    $parser->{'Current List'} = pop(@{$parser->{'Lists'}});
}

sub Char {
    my $parser = shift;
    my($text) = @_;
    my $currentList = $parser->{'Current List'};
    my $position = $#$currentList;
    if (($position > 0) and ($currentList->[$position-1] eq '0')) {
        # we already have some text, just stick it on the end
        $currentList->[$position] .= $text;
    } else {
        # new text node
        push(@$currentList, 0 => $text);
    }
}

sub Final {
    my $parser = shift;
    delete($parser->{'Current List'});
    delete($parser->{'Lists'});
    return $parser->{'Tree'};
}
