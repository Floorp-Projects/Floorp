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

package PLIF::Service::Coses;
use strict;
use vars qw(@ISA);
use PLIF::Service;
use HTML::Entities; # DEPENDENCY
@ISA = qw(PLIF::Service);
1;

# XXX need to implement the three other places marked XXX in this
# module to enable real output to be created.

# COSES namespace is: http://bugzilla.mozilla.org/coses

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'string.expander.COSES' or $class->SUPER::provides($service));
}

sub expand {
    my $self = shift;
    my($app, $output, $session, $protocol, $string, $data) = @_;
    my $xmlService = $app->getService('service.xml');
    my @index = (); my $index = 0;
    my @stack = (); my $stack = $xmlService->parseNS($string);
    my @scope = (); my $scope = {'data' => $data};
    my $result = '';
    if (not $scope->{'coses: skip sanitation'}) {
        $self->sanitiseScope($scope);
    }
    node: while (1) {
        if ($index > $#$stack) {
            # end of this level, pop the stack
            if (@stack) {
                $stack = pop(@stack);
                $index = pop(@index);
                $scope = pop(@scope);
            } else {
                # end of stack, have a nice day!
                return $result;
            }
        } else {
            # more data to deal with at this level
            my $node = $stack->[$index];
            my $contents = $stack->[$index+1];
            my $superscope = $scope; # scope of parent element
            $index += 2; # move the pointer on to the next node
            if ($node) {
                # element node
                my $attributes = $contents->[0];
                if ($attributes->{'{http://www.w3.org/XML/1998/namespace}space'}) {
                    $scope = {%$scope}; # take a local copy of the root level for descendants
                    $scope->{'coses: white space'} = $attributes->{'{http://www.w3.org/XML/1998/namespace}space'} eq 'default'; 
                    # vs 'preserve', which is assumed
                }
                if ($node eq '{http://bugzilla.mozilla.org/coses}if') {
                    if (not $self->evaluateCondition($self->evaluateExpression($attributes->{'lvalue'}, $scope),
                                                     $self->evaluateExpression($attributes->{'rvalue'}, $scope),
                                                     $self->evaluateExpression($attributes->{'condition'}, $scope),
                                                     )) {
                        $superscope->{'coses: last condition'} = 0;
                        next node;
                    }
                    $superscope->{'coses: last condition'} = 1;
                    if ($scope == $superscope) {
                        $scope = {%$scope};
                    }
                    $scope->{'coses: last condition'} = 0;
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}set') {
                    my $variable = $self->evaluateExpression($attributes->{'variable'}, $scope);
                    $self->assert($variable !~ /[\(\.\)]/o, 1,
                                  "variable '$variable' contains one of '(', ')' or '.' and is therefore not valid to use as a variable name.");
                    my $value = $self->evaluateExpression($attributes->{'value'}, $scope);
                    my $order = $self->evaluateExpression($attributes->{'order'}, $scope);
                    my $source = $self->evaluateExpression($attributes->{'source'}, $scope);
                    if ($order or $source) {
                        my @items = $self->genericSort($order, $self->genericKeys($value, $source));
                        if (@items) {
                            push(@index, $index);
                            push(@stack, $stack);
                            push(@scope, $superscope);
                            # now we push all but one of the items onto
                            # the stack -- so first take that item...
                            my $firstItem = pop(@items); # (@items is sorted backwards)
                            # and then take a copy of the scope if we didn't already
                            $superscope->{'coses: last condition'} = 1;
                            if ($scope == $superscope) {
                                $scope = {%$scope};
                            }
                            $scope->{'coses: last condition'} = 0;
                            foreach my $item (@items) {
                                push(@index, 1);
                                push(@stack, $contents);
                                $scope->{$variable} = $item;
                                push(@scope, $scope);
                                # make sure we create a new scope for the
                                # next item -- otherwise each part of the
                                # loop would just have a reference to the
                                # same shared hash, and so they would all
                                # have the same value!
                                $scope = {%$scope};
                            }
                            # and finally create the first scope (not pushed on the stack, it is the next, live one)
                            $index = 1; # skip past attributes
                            $stack = $contents;
                            $scope->{$variable} = $firstItem;
                        } else {
                            $superscope->{'coses: last condition'} = 0;
                        }
                        next node;
                    } else {
                        if ($scope == $superscope) {
                            # take a copy since we haven't yet
                            $scope = {%$scope};
                        }
                        $scope->{$variable} = $value;
                    }
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}text') {
                    if ($attributes->{'escape'}) {
                        if ($scope == $superscope) {
                            $scope = {%$scope};
                        }
                        $scope->{'coses: escapes'} = [$attributes->{'escape'}, @{$scope->{'coses: escapes'}}];
                    }
                    if ($attributes->{'value'}) {
                        $result .= $self->escape($app, $self->evaluateExpression($attributes->{'value'}, $scope), $scope);
                        if ($attributes->{'escape'}) {
                            $scope = $superscope;
                        }
                        next node; # skip contents if attribute 'value' is present
                    }
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}br') {
                    # useful if xml:space is set to 'default'
                    $result .= $self->escape($app, "\n", $scope);
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}include') {
                    if ((not exists($attributes->{'parse'})) or ($attributes->{'parse'} eq 'xml')) {
                        # This is similar to an XInclude, but is done
                        # later (during processing of the infoset, not
                        # while it is being made) and doesn't support
                        # any value for 'href'. Plus it has an
                        # extension to the 'parse' attribute.
                        push(@index, $index);
                        push(@stack, $stack);
                        $index = 0;
                        $stack = $xmlService->parseNS($self->getString($app, $session, $protocol, 
                                                                       $self->evaluateExpression($attributes->{'href'}, $scope)));
                        push(@scope, $superscope);
                    } elsif ($attributes->{'parse'} eq 'text') {
                        # raw text inclusion
                        $result .= $self->escape($app, $self->getString($app, $session, $protocol, 
                                                                  $self->evaluateExpression($attributes->{'href'}, $scope)), $scope);
                    } elsif ($attributes->{'parse'} eq 'x-auto') {
                        # Get the string expanded automatically and
                        # insert it into the result.
                        $result .= $self->escape($app, $output->getString($session, $self->evaluateExpression($attributes->{'href'}, $scope), $scope), $scope);
                    }
                    next node; # skip default handling
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}else') {
                    if ($superscope->{'coses: last condition'}) {
                        next node; # skip this block if the variable IS there
                    }
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}with') {
                    my $variable = $self->evaluateExpression($attributes->{'variable'}, $scope);
                    if (not defined($scope->{$variable})) {
                        next node; # skip this block if the variable isn't there
                    }
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}without') {
                    my $variable = $self->evaluateExpression($attributes->{'variable'}, $scope);
                    if (defined($scope->{$variable})) {
                        next node; # skip this block if the variable IS there
                    }
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}flatten') {
                    my $source = $self->evaluateExpression($attributes->{'source'}, $scope);
                    my $target = $self->evaluateExpression($attributes->{'target'}, $scope);
                    $self->assert($target !~ /[\(\.\)]/o, 1,
                                  "variable '$target' contains one of '(', ')' or '.' and is therefore not valid to use as a variable name.");
                    my @result;
                    if (defined($source)) {
                        $self->assert(ref($source) eq 'HASH', 1, "source variable is not a hash of arrays and thus cannot be flattened.");
                        # shrink it
                        local $" = ',';
                        foreach my $key (keys(%{$source})) {
                            $self->assert(ref($source->{$key}) eq 'ARRAY', 1, "source variable is not a hash of arrays and cannot be flattened.");
                            my @value = @{$source->{$key}};
                            if (scalar(@value)) {
                                # escape all "\", "|" and "," characters in key and values
                                foreach my $piece ($key, @value) {
                                    if (defined($piece) and ($piece ne '')) {
                                        $piece =~ s/\\/\\s/go;
                                        $piece =~ s/\,/\\c/go;
                                        $piece =~ s/\|/\\b/go;
                                    } else {
                                        $piece = '\0';
                                    }
                                }
                                push(@result, "$key|@value");
                            }
                        }
                    }
                    local $" = '|';
                    $scope->{$target} = "@result";
                } elsif ($node eq '{http://bugzilla.mozilla.org/coses}rounden') {
                    # the opposite of 'flat' is going to be 'round', ok...
                    my $source = $self->evaluateExpression($attributes->{'source'}, $scope);
                    my $target = $self->evaluateExpression($attributes->{'target'}, $scope);
                    $self->assert($target !~ /[\(\.\)]/o, 1,
                                  "variable '$target' contains one of '(', ')' or '.' and is therefore not valid to use as a variable name.");
                    if (defined($source)) {
                        $self->assert((not ref($source)), 1, "source variable is not a string and cannot be rounded."); # XXX I *really* need a better name for this
                        # expand it
                        my @hash = split(/\|/o, $source);
                        my $isValue = 0;
                        foreach my $piece (@hash) {
                            my @piece;
                            if ($isValue) {
                                $piece = [split(/\,/o, $piece)];
                            }
                            foreach my $smallPiece (ref($piece) eq 'ARRAY' ? @$piece : $piece) {
                                $smallPiece =~ s/\\0//go;
                                $smallPiece =~ s/\\b/\|/go;
                                $smallPiece =~ s/\\c/\,/go;
                                $smallPiece =~ s/\\s/\\/go;
                            }
                            $isValue = not $isValue;
                        }
                        $scope->{$target} = {@hash};
                    }
                } else {
                    my $serialisedAttributes = '';
                    foreach my $attribute (keys(%$attributes)) {
                        $serialisedAttributes .= ' '.$attribute.'="'.($xmlService->escape($attributes->{$attribute})).'"';
                    }
                    $self->dump(3, "Unexpected element <$node$serialisedAttributes> found during COSES expansion, ignoring...");
                }
                # fall through to default handling: push current
                # stack, scope and index, and set new index to move
                # past attributes
                push(@index, $index); $index = 1;
                push(@stack, $stack); $stack = $contents;
                push(@scope, $superscope);
            } elsif ($scope->{'coses: white space'}) {
                # raw text node which may or may not be included
                # xml:space="default" so only include text nodes with non-whitespace
                # and trim leading and closing newlines
                if ($contents =~ /\S/o) {
                    $contents =~ s/^\n//os;
                    $contents =~ s/\n$//os;
                    $result .= $self->escape($app, $contents, $scope);
                }
            } else {
                # raw text node
                $result .= $self->escape($app, $contents, $scope);
            }
        }
    }
}

sub getString {
    my $self = shift;
    my($app, $session, $protocol, $name) = @_;
    my($type, $string) = $app->getService('dataSource.strings')->get($app, $session, $protocol, $name);
    return $string;
}

sub evaluateVariable {
    my $self = shift;
    my($variable, $scope) = @_;
    my @parts = split(/\./o, $variable, -1); # split variable at dots ('.') (the negative number prevents null trailing fields from being stripped)
    # drill down through scope
    foreach my $part (@parts) {
        if (ref($scope) eq 'HASH') {
            $scope = $scope->{$part};
        } elsif (ref($scope) eq 'ARRAY') {
            if ($part =~ /^\d+$/o) {
                $scope = $scope->[$part];
            } elsif ($part eq 'length') {
                $scope = scalar(@$scope);
            } else {
                $self->assert(1, "Tried to drill into an array using a non-numeric key ('$part')");
            }
        } else {
            $self->error(1, "Could not resolve '$variable' (the part giving me trouble was '$part')");
        }
    }
    if (defined($scope)) {
        # fully dereference all scalar references
        while (ref($scope) eq 'SCALAR') {
            $scope = $$scope;
        }
        return $scope;
    } else {
        return '';
    }
}

sub evaluateNestedVariableSafely {
    my $self = shift;
    my($variable, $scope) = @_;
    $scope = $self->evaluateVariable($variable, $scope);
    if ($scope =~ /[\(\)]/o) {
        $self->error(1, "Evaluated nested variable '$variable' to '$scope' which contains one of '(' or ')' and is therefore not safe to use as a variable part");
    }
    return $scope;
}

sub evaluateExpression {
    my $self = shift;
    my($expression, $scope) = @_;
    if (defined($expression)) {
        if ($expression =~ /^\'(.*)$/os) {
            return $1; # bypass next bit if it's an explicit string
        } elsif ($expression =~ /^[^()]*$/o) {
            return $expression; # bypass next bit if there are no brackets at all
        } else {
            # expand *nested* variables safely
            while ($expression =~ s/^ # the start of the line
                                    ( # followed by a group of
                                 .*\( # anything up to an open bracket
                               [^()]* # then anything but brackets
                                    ) # followed by
                                   \( # an open bracket
                             ([^()]*) # our variable
                                   \) # a close bracket
                                    ( # followed by a group of
                                  (?: # as many instances as required
                               [^()]* # of first other-variable stuff
                           \([^()]*\) # and then of more embedded variabled
                                   )* # followed by
                           [^()]*\).* # anything but brackets, a close bracket then anything
                                    ) # which should be at the
                                    $ # end of the line
                                   /$1.$self->evaluateNestedVariableSafely($2, $scope).$3/sexo) {
                # this should cope with this smoketest (d=ab, g=fcde): (f.(c).((a).(b)).(e))
                # note that if b="x" and a="(b)" then "(a)" should be evaluated to "x"
            }
            # expand outer variable without safety checks, if there are any
            # first, check if the result would be a single variable
            if ($expression =~ /^\(([^()]*)\)$/o) {
                # we special case this -- doing it without using a
                # regexp s/// construct ensures we keep references as
                # live references in strict mode (i.e., we don't call
                # their "ToString" method or whatever...)
                $expression = $self->evaluateVariable($1, $scope);
            } else {
                # expand all remaining outer variables
                my $result = '';
                while ($expression =~ s/^(.*?)\(([^()]*)\)//o) {
                    # ok, let's deal with the next embedded variable
                    $result .= $1.$self->evaluateVariable($2, $scope);
                    # the bit we've dealt with so far will end up
                    # removed from the $expression string (so the
                    # current state is "$result$expression"). This is
                    # so that things that appear to be variables in
                    # the strings we are expanding don't themselves
                    # get expanded.
                }
                # put it back together again
                $expression = $result.$expression;
            }
            # and return the result
            return $expression;
        }
    } else {
        return '';
    }
}

sub evaluateCondition {
    my $self = shift;
    my($lvalue, $rvalue, $condition) = @_;
    if (defined($condition) and defined($lvalue) and defined($rvalue)) {
        if ($condition eq '=' or $condition eq '==') {
            return eval { $lvalue == $rvalue; }; # could fail with non numeric arguments
        } elsif ($condition eq '!=') {
            return eval { $lvalue != $rvalue; };
        } elsif ($condition eq '<') {
            return eval { $lvalue < $rvalue; };
        } elsif ($condition eq '>') {
            return eval { $lvalue > $rvalue; };
        } elsif ($condition eq '<=') {
            return eval { $lvalue <= $rvalue; };
        } elsif ($condition eq '>=') {
            return eval { $lvalue >= $rvalue; };
        } elsif ($condition eq 'eq') {
            return eval { $lvalue eq $rvalue; };
        } elsif ($condition eq 'ne') {
            return eval { $lvalue ne $rvalue; };
        } elsif ($condition eq '=~') {
            return eval { $lvalue =~ /$rvalue/; };
        } elsif ($condition eq '!~') {
            return eval { $lvalue !~ /$rvalue/; };
        } elsif ($condition eq 'is') {
            if (ref($lvalue)) {
                return $rvalue eq lc(ref($lvalue));
            } else {
                return $rvalue eq 'scalar';
            }
        } elsif ($condition eq 'is not') {
            if (ref($lvalue)) {
                return $rvalue ne lc(ref($lvalue));
            } else {
                return $rvalue ne 'scalar';
            }
        }
    } # else, well, they got it wrong, so it won't match now will it :-)
    return 0;
}

sub genericKeys {
    my $self = shift;
    my($value, $source) = @_;
    if (ref($value) eq 'HASH') {
        if (defined($source) and $source eq 'values') {
            return values(%$value);
        } else { # (not defined($source) or $source eq 'keys')
            $self->assert(not defined($source) or $source eq 'keys', 1, "Unknown source type '$source', valid values are 'keys' (the default) or 'values'");
            return keys(%$value);
        }
    } elsif (ref($value) eq 'ARRAY') {
        if (defined($source) and $source eq 'values') {
            return @$value;
        } else { # (not defined($source) or $source eq 'keys')
            $self->assert(not defined($source) or $source eq 'keys', 1, "Unknown source type '$source', valid values are 'keys' (the default) or 'values'");
            if ($#$value >= 0) {
                return (0..$#$value);
            } else {
                return ();
            }
        }
    } else {
        return ($value);
    }
}

sub genericSort {
    my $self = shift;
    my($order, @list) = @_;
    # sort the list (in reverse order!)
    if (defined($order) and scalar(@list)) {
        if ($order eq 'lexical') {
            return sort { $b cmp $a } @list;
        } elsif ($order eq 'reverse lexical') {
            return sort { $a cmp $b } @list;
        } elsif ($order eq 'case insensitive lexical') {
            return sort { lc($b) cmp lc($a) } @list;
        } elsif ($order eq 'reverse case insensitive lexical') {
            return sort { lc($a) cmp lc($b) } @list;
        } elsif ($order eq 'numerical') {
            return sort { $b <=> $a } @list;
        } elsif ($order eq 'reverse numerical') {
            return sort { $a <=> $b } @list;
        } elsif ($order eq 'length') {
            return sort { length($b) <=> length($b) } @list;
        } elsif ($order eq 'reverse length') {
            return sort { length($a) <=> length($a) } @list;
        } elsif ($order eq 'default') {
            return reverse @list; # keep in sync with default below
        } elsif ($order eq 'reverse default') {
            return @list;
        } else {
            $self->error(1, "Unknown sort order '$order', valid values are '[reverse] ([case insensitive] lexical | numerical | length | default)'");
        }
        # XXX we need to also support:
        #   Sorting by a particular subkey of a hash to sort an array of hashes
    }
    # else assume 'default':
    return reverse @list;
}

sub sanitiseScope {
    my $self = shift;
    my($data) = @_;
    my @stack = ($data);
    while (@stack) {
        my $value = pop(@stack);
        if (ref($value) eq 'HASH') {
            push(@stack, values(%$value));
            foreach my $key (keys(%$value)) {
                if ($key =~ /[\(\.\)]/) {
                    my $backup = $value->{$key};
                    delete($value->{$key});
                    my $oldKey = $key;
                    $key =~ tr/(.)/[:]/;
                    while (exists($value->{$key})) {
                        $key .= '_';
                    }
                    $value->{$key} = $backup;
                    if (not exists($value->{'coses: original keys'})) {
                        $value->{'coses: original keys'} = {};
                    }
                    $value->{'coses: original keys'}->{$key} = $oldKey;
                }
            }
        } elsif (ref($value) eq 'ARRAY') {
            push(@stack, @$value);
        }
    }
    return $data;
}

sub escape {
    my $self = shift;
    my($app, $string, $scope) = @_;
    foreach my $escape (@{$scope->{'coses: escapes'}}) {
        if ($escape eq 'html') {
            $string = encode_entities($string);
        } elsif ($escape eq 'xml') {
            $string = $app->getService('service.xml')->escape($string);
        } elsif ($escape eq 'uri') {
            $string =~ s/([^-A-Za-z0-9_.!~*'()])/sprintf("%%%02X", ord($1))/geos; # ' (unlock font-lock)
        } else {
            $self->error(1, "Unknown escape type '$escape'");
        }
    }
    return $string;
}
