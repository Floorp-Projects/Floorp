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

package PLIF::Exception;
use strict;
use vars qw(@ISA @EXPORT);
use overload '""' => 'stringify', 'cmp' => 'comparison';
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(try raise catch with fallthrough except otherwise finally);

# To use this package, you first have to define your own exceptions:
#
#     package MyException; @ISA = qw(PLIF::Exception);
#     package MemoryException; @ISA = qw(PLIF::Exception);
#     package IOException; @ISA = qw(PLIF::Exception);
#
# You can then use them as follows (the order must be as follows):
#
#     try {
#         # some code that might raise an exception:
#         raise MyException if $condition;
#         # ... more code ...
#     } catch MemoryException with {
#         my($exception) = @_;
#         raise $exception; # reraise
#     } catch IOException with {
#         my($exception) = @_;
#         return fallthrough; # fall through to the following handlers
#         fallthrough; # alternative form if the directive is the last one in the block
#     } catch DBError with {
#         my($exception) = @_;
#     } except {
#         my($exception) = @_;
#         # catch all, called if the exception wasn't handled
#     } otherwise {
#         # only executed if no exception was raised
#     } finally {
#         # always called after try block and any handlers
#     };


sub syntax($@) {
    my($message, $package, $filename, $line) = @_;
    die "$message at $filename line $line\n";
}

sub stacktrace($) {
    my($index) = @_;
    my @stacktrace;
    while (my @data = caller($index++)) {
        push(@stacktrace, {
            'package' => $data[0],
            'filename' => $data[1],
            'line' => $data[2],
            'subroutine' => $data[3], # set to '(eval)' for eval, require, and use statements
            'hasargs' => $data[4],
            'wantarray' => $data[5],
            'evaltext' => $data[6], # undef for eval {}, EXPR for eval EXPR
            'is_require' => $data[7], # true if eval was caused by require or use statement
        });
    }
    return \@stacktrace;
}

sub create {
    my $class = shift;
    return bless({@_}, $class);
}

sub raise {
    my($exception, @data) = @_;
    my($package, $filename, $line) = caller;
    my $stacktrace = stacktrace(2);
    PLIF->warn(7, "Exception raised: $exception");
    if (ref($exception) and $exception->isa('PLIF::Exception')) {
        # if the exception is an object, raise it
        # this is for people doing things like:
        #   raise IOException->create('message' => $!);
        # or:
        #   my $memoryException = MemoryException->create();
        #   # ...
        #   $memoryException->raise();
        $exception->{'filename'} = $filename;
        $exception->{'line'} = $line;
        $exception->{'stacktrace'} = $stacktrace;
        die $exception;
    } else {
        # otherwise, assume we were called as a constructor
        # this is for people doing things like:
        #   raise IOException;
        # or:
        #   raise IOException ('message' => $!);
        # or:
        #   IOException->raise('message' => $!);
        syntax "Syntax error in \"raise\": \"$exception\" is not a PLIF::Exception class", caller unless $exception->isa('PLIF::Exception');
        die $exception->create(
            'filename' => $filename,
            'line' => $line,
            'stacktrace' => $stacktrace,
            @data
        );
    }
}

sub try(&;$) {
    my($code, $continuation) = @_;
    if (defined($continuation) and
        (not ref($continuation) or not $continuation->isa('PLIF::Exception::Internal::Continuation'))) {
        syntax 'Syntax error in continuation of "try" clause', caller;
    }
    my $context = wantarray;
    my @result; # for array context
    my $result; # for scalar context
    eval {
        if ($context) {
            # array context
            @result = &$code;
        } elsif (defined($context)) {
            # scalar context
            $result = &$code;
        } else {
            # void context
            &$code;
        }
    };
    if (defined($continuation)) {
        $continuation->handle($@, caller, stacktrace(2));
    }
    return $context ? @result : $result;
}

sub catch($$) {
    my($class, $continuation, @more) = @_;
    syntax "Syntax error in \"catch ... with\" clause: \"$class\" is not a PLIF::Exception class", caller unless $class->isa('PLIF::Exception');
    if (not defined($continuation) or
        not ref($continuation) or
        not $continuation->isa('PLIF::Exception::Internal::With')) {
        syntax 'Syntax error: missing "with" operator in "catch" clause', caller;
    }
    syntax 'Syntax error after "catch ... with" clause', caller if (scalar(@more));
    $continuation->{'resolved'} = 1;
    my $handler = $continuation->{'handler'};
    $continuation = $continuation->{'continuation'};
    if (not defined($continuation)) {
        $continuation = PLIF::Exception::Internal::Continuation->create(caller);
    }
    unshift(@{$continuation->{'handlers'}}, [$class, $handler]);
    return $continuation;
}

sub with(&;$) {
    my($handler, $continuation) = @_;
    if (not defined($continuation) or
        not ref($continuation) or
        not $continuation->isa('PLIF::Exception::Internal::Continuation')) {
        syntax 'Syntax error after "catch ... with" clause', caller;
    }
    return PLIF::Exception::Internal::With->create($handler, $continuation, caller);
}

sub except(&;$) {
    my($handler, $continuation) = @_;
    if (defined($continuation) and
        (not ref($continuation) or
         not $continuation->isa('PLIF::Exception::Internal::Continuation') or
         defined($continuation->{'except'}) or
         scalar(@{$continuation->{'handlers'}}))) {
        syntax 'Syntax error after "except" clause', caller;
    }
    if (not defined($continuation)) {
        $continuation = PLIF::Exception::Internal::Continuation->create(caller);
    }
    $continuation->{'except'} = $handler;
    return $continuation;
}

sub otherwise(&;$) {
    my($handler, $continuation) = @_;
    if (defined($continuation) and
        (not ref($continuation) or
         not $continuation->isa('PLIF::Exception::Internal::Continuation') or
         defined($continuation->{'otherwise'}) or
         defined($continuation->{'except'}) or
         scalar(@{$continuation->{'handlers'}}))) {
        syntax 'Syntax error after "otherwise" clause', caller;
    }
    if (not defined($continuation)) {
        $continuation = PLIF::Exception::Internal::Continuation->create(caller);
    }
    $continuation->{'otherwise'} = $handler;
    return $continuation;
}

sub finally(&;@) {
    my($handler, @continuation) = @_;
    syntax 'Missing semicolon after "finally" clause', caller if (scalar(@continuation));
    my $continuation = PLIF::Exception::Internal::Continuation->create(caller);
    $continuation->{'finally'} = $handler;
    return $continuation;
}

sub fallthrough() {
    return PLIF::Exception::Internal::Fallthrough->create(caller);
}

sub stringify {
    my $self = shift;
    my $stacktrace = '';
    foreach my $frame (@{$self->{'stacktrace'}}) {
        # XXX this should be made better
        if ($frame->{'subroutine'} eq '(eval)') {
            if ($frame->{'is_require'}) {
                $stacktrace .= "  require in $frame->{'filename'} line $frame->{'line'}\n";
            } elsif (defined($frame->{'evaltext'})) {
                $stacktrace .= "  eval '$frame->{'evaltext'}' in $frame->{'filename'} line $frame->{'line'}\n";
            } else {
                $stacktrace .= "  eval {...} in $frame->{'filename'} line $frame->{'line'}\n";
            }
        } elsif ($frame->{'hasargs'}) {
            $stacktrace .= "  $frame->{'subroutine'}(...) called from $frame->{'filename'} line $frame->{'line'}\n";
        } else {
            $stacktrace .= "  $frame->{'subroutine'}() called from $frame->{'filename'} line $frame->{'line'}\n";
        }
    }
    if (defined($self->{'message'})) {
        return "$self->{'message'} at $self->{'filename'} line $self->{'line'}\n$stacktrace";
    } else {
        return ref($self) . " exception at $self->{'filename'} line $self->{'line'}\n$stacktrace";
    }
}

sub comparison {
    my($a, $b, $reverse) = @_;
    my $result = ((defined($a) and defined($b)) ? ("$a" cmp "$b") :
                  defined($a) ? 1 :
                  defined($b) ? -1 :
                  0);
    return $reverse ? !$result : $result;
}


package PLIF::Exception::Internal::Continuation;

sub wrap($$$$) {
    my($exception, $filename, $line, $stacktrace) = @_;
    if ($exception ne '') {
        if (not ref($exception) or
            not $exception->isa('PLIF::Exception')) {
            # an unexpected exception
            $exception = PLIF::Exception->create('message' => $exception);
        }
        if (not exists $exception->{'stacktrace'}) {
            $exception->{'filename'} = $filename;
            $exception->{'line'} = $line;
            $exception->{'stacktrace'} = $stacktrace;
        }
    } else {
        $exception = undef;
    }
    return $exception;
}

sub create {
    return bless {
        'handlers' => [],
        'except' => undef,
        'otherwise' => undef,
        'finally' => undef,
        'filename' => $_[2],
        'line' => $_[3],
        'resolved' => 0,
    }, $_[0];
}

sub handle {
    my $self = shift;
    my($exception, $package, $filename, $line, $stacktrace) = @_;
    $self->{'resolved'} = 1;
    $exception = wrap($exception, $filename, $line, $stacktrace);
    my $reraise = undef;
    handler: while (1) {
        if (defined($exception)) {
            foreach my $handler (@{$self->{'handlers'}}) {
                if ($exception->isa($handler->[0])) {
                    my $result = eval { &{$handler->[1]}($exception) };
                    $reraise = wrap($@, $filename, $line, $stacktrace);
                    if (not defined($result) or # $result is not defined if $reraise is now defined
                        not ref($result) or
                        not $result->isa('PLIF::Exception::Internal::Fallthrough')) {
                        last handler;
                    }
                    # else, it's the result of an "fallthrough" function call
                    $result->{'resolved'} = 1;
                }
            }
            if (defined($self->{'except'})) {
                my $result = eval { &{$self->{'except'}}($exception) };
                $reraise = wrap($@, $filename, $line, $stacktrace);
            } else {
                # fallthrough exception
                $reraise = $exception;
            }
        } else {
            if (defined($self->{'otherwise'})) {
                my $result = eval { &{$self->{'otherwise'}}($exception) };
                $reraise = wrap($@, $filename, $line, $stacktrace);
            }
        }
        last;
    }
    if (defined($self->{'finally'})) {
        &{$self->{'finally'}}();
    }
    if (defined($reraise)) {
        # note that if the finally block raises an exception, we drop this one
        $reraise->raise();
    }
}

sub DESTROY {
    my $self = shift;
    return $self->SUPER::DESTROY(@_) if $self->{'resolved'};
    my($package, $filename, $line) = caller;
    my $parts = 0x00;
    $parts |= 0x01 if scalar(@{$self->{'handlers'}});
    $parts |= 0x02 if defined($self->{'except'});
    $parts |= 0x04 if defined($self->{'finally'});
    my $messages = ["Incorrectly used PLIF::Exception::Internal::Continuation object at $self->{'filename'} line $self->{'line'}\n",
                    "Incorrectly used \"catch ... with\" clause at $self->{'filename'} line $self->{'line'}\n",
                    "Incorrectly used \"except\" clause at $self->{'filename'} line $self->{'line'}\n",
                    "Incorrectly used \"catch ... with\" and \"except\" clauses at $self->{'filename'} line $self->{'line'}\n",
                    "Incorrectly used \"finally\" clause at $self->{'filename'} line $self->{'line'}\n",
                    "Incorrectly used \"catch ... with\" and \"finally\" clauses at $self->{'filename'} line $self->{'line'}\n",
                    "Incorrectly used \"except\" and \"finally\" clauses at $self->{'filename'} line $self->{'line'}\n",
                    "Incorrectly used \"catch ... with\", \"except\", and \"finally\" clauses at $self->{'filename'} line $self->{'line'}\n",];
    warn $messages->[$parts]; # XXX can't raise an exception in a destructor
}


package PLIF::Exception::Internal::With;

sub create {
    return bless {
        'handler' => $_[1],
        'continuation' => $_[2],
        'filename' => $_[4],
        'line' => $_[5],
        'resolved' => 0,
    }, $_[0];
}

sub DESTROY {
    my $self = shift;
    return $self->SUPER::DESTROY(@_) if $self->{'resolved'};
    warn "Incorrectly used \"with\" operator at $self->{'filename'} line $self->{'line'}\n"; # XXX can't raise an exception in a destructor
}


package PLIF::Exception::Internal::Fallthrough;

sub create {
    my($package, $filename, $line) = @_;
    return bless {
        'filename' => $_[2],
        'line' => $_[3],
        'resolved' => 0,
    }, $_[0];
}

sub DESTROY {
    my $self = shift;
    return $self->SUPER::DESTROY(@_) if $self->{'resolved'};
    warn "Incorrectly used \"fallthrough\" function at $self->{'filename'} line $self->{'line'}\n"; # XXX can't raise an exception in a destructor
}


package PLIF::Exception::Alarm; use vars qw(@ISA); @ISA = qw(PLIF::Exception);
