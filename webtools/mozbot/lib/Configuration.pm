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
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>
#                 Ian Hickson <py8ieh=mozbot@bath.ac.uk>

package Configuration;
use strict;
use Carp;

sub Get {
    my ($file, $config) = @_;
    my %seen;
    open FILE, "<$file" or return 0;
    my $line = 0;
    while (<FILE>) {
        $line++; chomp;
        if (/^ *([^#;][^=\n\r]*)(?:=(.*))?$/os) {
            my $value = $$config{$1};
            if (defined($value)) {
                $value = $$value while ref($value) eq 'REF';
                if (ref($value) eq 'SCALAR') {
                    $$value = $2;
                } elsif (ref($value) eq 'ARRAY') {
                    unless ($seen{$1}) {
                        @$value = ();
                    }
                    if (defined($2)) {
                        push(@$value, $2);
                    }
                } elsif (ref($value) eq 'HASH') {
                    unless ($seen{$1}) {
                        %$value = ();
                    }
                    if (defined($2)) {
                        $2 =~ /^(.)(.*?)\1=>(.*)$/so;
                        $$value{$2} = $3;
                    }
                }
            } # else unknown variable, ignore
            $seen{$1} = 1;
        } # else ignore (probably comment)
    }
    close FILE;
    return $line;
}

sub Save {
    my ($file, $config) = @_;
    local $_;

    # Try to keep file structure if possible
    my @lines;
    if (open FILE, "<$file") {
        while (<FILE>) {
           push @lines, $_;
        }
        close FILE;
    }

    # but make sure we put in all the data (dups are dealt with)
    foreach (sort keys %$config) {
        push @lines, "$_=";
    }

    # Open file to which we are saving
    open FILE, ">$file.~$$~" or confess("Could not save configuration: $!");

    # ok, save file back again
    # make sure we only write parameters once by
    # keeping a log of those done
    my %seen;
    foreach (@lines) {
        chomp;
        if (/^ *([^#;][^=\n\r]*)(?:=(.*))?$/os) {
            my $variable = $1;
            my $value = $2;
            if (defined($$config{$variable})) {
                unless ($seen{$variable}) {
                    $value = $$config{$variable};
                    $value = $$value while ref($value) eq 'REF';
                    if (ref($value) eq 'SCALAR') {
                        if (defined($$value)) {
                            print FILE $variable.'='.$$value."\n" or confess("Could not save configuration: $!");
                        }
                    } elsif (ref($value) eq 'HASH') {
                        my @keys = keys %$value;
                        if (@keys > 0) {
                            foreach my $item (@keys) {
                                my $data = $$value{$item};
                                $item = '' unless defined $item;
                                $data = '' unless defined $data;
                                my $delimiter;
                                foreach ('"','\'','|',':','#','*','<','>','/','[',']','{','}',
                                         '(',')','\\','=','-','@','!','\$','%','&',' ','\`','~') {
                                    if ($item !~ /\Q$_\E=>/os) {
                                        $delimiter = $_;
                                        last;
                                    }
                                }
                                if (defined($delimiter)) {
                                  print FILE "$variable=$delimiter$item$delimiter=>$data\n"
                                    or confess("Could not save configuration: $!");
                                }
                                # else, silent data loss... XXX
                            }
                        } else {
                            print FILE "$variable\n" or confess("Could not save configuration: $!");
                        }
                    } elsif (ref($value) eq 'ARRAY') {
                        if (@$value > 0) {
                            foreach my $item (@$value) {
                                if (defined($item)) {
                                    print FILE "$variable=$item\n" or confess("Could not save configuration: $!");
                                } else {
                                    print FILE "$variable=\n" or confess("Could not save configuration: $!");
                                }
                            }
                        } else {
                            print FILE "$variable\n" or confess("Could not save configuration: $!");
                        }
                    } else {
                        confess("Unsupported data type '".ref($value)."' writing $variable (".$$config{$variable}.')');
                    }
                    $seen{$variable} = 1;
                } # else seen it already
            } else { # unknown
                if (defined($value)) {
                    print FILE "$variable=$value\n" or confess("Could not save configuration: $!");
                } else {
                    print FILE "$variable\n" or confess("Could not save configuration: $!");
                }
            }
        } else {
            # might be a comment
            print FILE $_."\n" or confess("Could not save configuration: $!");
        }
    }
    # actually do make a change to the real file    
    close FILE or confess("Could not save configuration: $!");

    # -- #mozwebtools was here --
    # * Hixie is sad as his bot crashes.
    # * Hixie adds in a check to make sure that the file he tries
    #   to delete actually exists first.
    # <timeless>   delete??

    unlink $file or confess("Could not delete $file: $!") if (-e $file);
    rename("$file.~$$~", $file) or confess("Could not rename to $file: $!");
}

sub Ensure {
    my ($config) = @_;
    my $changed;
    foreach (@$config) {
        if (ref($$_[1]) eq 'SCALAR') {
            unless (defined(${$$_[1]})) {
                if (-t) {
                    print $$_[0]. ' ';
                    <> =~ /^(.*)$/os;
                    ${$$_[1]} = $1;
                    ${$$_[1]} = '' unless defined ${$$_[1]};
                    chomp(${$$_[1]});
                    $changed++;
                } else {
                    confess("Terminal is not interactive, so could not ask '$$_[0]'. Gave up");
                }
            }
        } elsif (ref($$_[1]) eq 'ARRAY') {
            unless (defined(@{$$_[1]})) {
                if (-t) {
                    print $$_[0]. " (enter a blank line to finish)\n";
                    my $input;
                    do {
                        $input = <>;
                        $input = '' unless defined $input;
                        chomp($input);
                        push @{$$_[1]}, $input if $input;
                        $changed++;
                    } while $input;
                } else {
                    confess("Terminal is not interactive, so could not ask '$$_[0]'. Gave up");
                }
            }
        } else {
            confess("Unsupported data type expected for question '$$_[0]'");
        }
    }
    return $changed;
}

1; # end
