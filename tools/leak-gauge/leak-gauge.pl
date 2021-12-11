#!/usr/bin/perl -w
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# $Id: leak-gauge.pl,v 1.8 2008/02/08 19:55:03 dbaron%dbaron.org Exp $
# This script is designed to help testers isolate and simplify testcases
# for many classes of leaks (those that involve large graphs of core
# data structures) in Mozilla-based browsers.  It is designed to print
# information about what has leaked by processing a log taken while
# running the browser.  Such a log can be taken over a long session of
# normal browsing and then the log can be processed to find sites that
# leak.  Once a site is known to leak, the logging can then be repeated
# to figure out under what conditions the leak occurs.
#
# The way to create this log is to set the environment variables:
#   MOZ_LOG=DOMLeak:5,DocumentLeak:5,nsDocShellLeak:5,NodeInfoManagerLeak:5
#   MOZ_LOG_FILE=nspr.log     (or any other filename of your choice)
# in your shell and then run the program.
# * In a Windows command prompt, set environment variables with
#     set VAR=value
# * In an sh-based shell such as bash, set environment variables with
#     export VAR=value
# * In a csh-based shell such as tcsh, set environment variables with
#     setenv VAR value
#
# Then, after you have exited the browser, run this perl script over the
# log.  Either of the following commands should work:
#   perl ./path/to/leak-gauge.pl nspr.log
#   cat nspr.log | perl ./path/to/leak-gauge.pl
# and it will tell you which of certain core objects leaked and the URLs
# associated with those objects.


# Nobody said I'm not allowed to write my own object system in perl.  No
# classes here.  Just objects and methods.
sub call {
    my $func = shift;
    my $obj = shift;
    my $funcref = ${$obj}{$func};
    &$funcref($obj, @_);
}

# A hash of objects (keyed by the first word of the line in the log)
# that have two public methods, handle_line and dump (to be called using
# call, above), along with any private data they need.
my $handlers = {
    "DOMWINDOW" => {
        count => 0,
        windows => {},
        handle_line => sub($$) {
            my ($self, $line) = @_;
            my $windows = ${$self}{windows};
            if ($line =~ /^([0-9a-f]*) (\S*)/) {
                my ($addr, $verb, $rest) = ($1, $2, $');
                if ($verb eq "created") {
                    $rest =~ / outer=([0-9a-f]*)$/ || die "outer expected";
                    my $outer = $1;
                    ${$windows}{$addr} = { outer => $1 };
                    ++${$self}{count};
                } elsif ($verb eq "destroyed") {
                    delete ${$windows}{$addr};
                } elsif ($verb eq "SetNewDocument") {
                    $rest =~ /^ (.*)$/ || die "URI expected";
                    my $uri = ($1);
                    ${${$windows}{$addr}}{$uri} = 1;
                }
            }
        },
        dump => sub ($) {
            my ($self) = @_;
            my $windows = ${$self}{windows};
            foreach my $addr (keys(%{$windows})) {
                my $winobj = ${$windows}{$addr};
                my $outer = delete ${$winobj}{outer};
                print "Leaked " . ($outer eq "0" ? "outer" : "inner") .
                      " window $addr " .
                      ($outer eq "0" ? "" : "(outer $outer) ") .
                      "at address $addr.\n";
                foreach my $uri (keys(%{$winobj})) {
                    print " ... with URI \"$uri\".\n";
                }
            }
        },
        summary => sub($) {
            my ($self) = @_;
            my $windows = ${$self}{windows};
            print 'Leaked ' . keys(%{$windows}) . ' out of ' .
                  ${$self}{count} . " DOM Windows\n";
        }
    },
    "DOCUMENT" => {
        count => 0,
        docs => {},
        handle_line => sub($$) {
            my ($self, $line) = @_;
            # This doesn't work; I don't have time to figure out why not.
            # my $docs = ${$self}{docs};
            my $docs = ${$handlers}{"DOCUMENT"}{docs};
            if ($line =~ /^([0-9a-f]*) (\S*)/) {
                my ($addr, $verb, $rest) = ($1, $2, $');
                if ($verb eq "created") {
                    ${$docs}{$addr} = {};
                    ++${$self}{count};
                } elsif ($verb eq "destroyed") {
                    delete ${$docs}{$addr};
                } elsif ($verb eq "ResetToURI" ||
                         $verb eq "StartDocumentLoad") {
                    $rest =~ /^ (.*)$/ || die "URI expected";
                    my $uri = $1;
                    my $doc_info = ${$docs}{$addr};
                    ${$doc_info}{$uri} = 1;
                    if (exists(${$doc_info}{"nim"})) {
                        ${$doc_info}{"nim"}{$uri} = 1;
                    }
                }
            }
        },
        dump => sub ($) {
            my ($self) = @_;
            my $docs = ${$self}{docs};
            foreach my $addr (keys(%{$docs})) {
                print "Leaked document at address $addr.\n";
                foreach my $uri (keys(%{${$docs}{$addr}})) {
                    print " ... with URI \"$uri\".\n" unless $uri eq "nim";
                }
            }
        },
        summary => sub($) {
            my ($self) = @_;
            my $docs = ${$self}{docs};
            print 'Leaked ' . keys(%{$docs}) . ' out of ' .
                  ${$self}{count} . " documents\n";
        }
    },
    "DOCSHELL" => {
        count => 0,
        shells => {},
        handle_line => sub($$) {
            my ($self, $line) = @_;
            my $shells = ${$self}{shells};
            if ($line =~ /^([0-9a-f]*) (\S*)/) {
                my ($addr, $verb, $rest) = ($1, $2, $');
                if ($verb eq "created") {
                    ${$shells}{$addr} = {};
                    ++${$self}{count};
                } elsif ($verb eq "destroyed") {
                    delete ${$shells}{$addr};
                } elsif ($verb eq "InternalLoad" ||
                         $verb eq "SetCurrentURI") {
                    $rest =~ /^ (.*)$/ || die "URI expected";
                    my $uri = $1;
                    ${${$shells}{$addr}}{$uri} = 1;
                }
            }
        },
        dump => sub ($) {
            my ($self) = @_;
            my $shells = ${$self}{shells};
            foreach my $addr (keys(%{$shells})) {
                print "Leaked docshell at address $addr.\n";
                foreach my $uri (keys(%{${$shells}{$addr}})) {
                    print " ... which loaded URI \"$uri\".\n";
                }
            }
        },
        summary => sub($) {
            my ($self) = @_;
            my $shells = ${$self}{shells};
            print 'Leaked ' . keys(%{$shells}) . ' out of ' .
                  ${$self}{count} . " docshells\n";
        }
    },
    "NODEINFOMANAGER" => {
        count => 0,
        nims => {},
        handle_line => sub($$) {
            my ($self, $line) = @_;
            my $nims = ${$self}{nims};
            if ($line =~ /^([0-9a-f]*) (\S*)/) {
                my ($addr, $verb, $rest) = ($1, $2, $');
                if ($verb eq "created") {
                    ${$nims}{$addr} = {};
                    ++${$self}{count};
                } elsif ($verb eq "destroyed") {
                    delete ${$nims}{$addr};
                } elsif ($verb eq "Init") {
                    $rest =~ /^ document=(.*)$/ ||
                        die "document pointer expected";
                    my $doc = $1;
                    if ($doc ne "0") {
                        my $nim_info = ${$nims}{$addr};
                        my $doc_info = ${$handlers}{"DOCUMENT"}{docs}{$doc};
                        foreach my $uri (keys(%{$doc_info})) {
                            ${$nim_info}{$uri} = 1;
                        }
                        ${$doc_info}{"nim"} = $nim_info;
                    }
                }
            }
        },
        dump => sub ($) {
            my ($self) = @_;
            my $nims = ${$self}{nims};
            foreach my $addr (keys(%{$nims})) {
                print "Leaked content nodes associated with node info manager at address $addr.\n";
                foreach my $uri (keys(%{${$nims}{$addr}})) {
                    print " ... with document URI \"$uri\".\n";
                }
            }
        },
        summary => sub($) {
            my ($self) = @_;
            my $nims = ${$self}{nims};
            print 'Leaked content nodes within ' . keys(%{$nims}) . ' out of ' .
                  ${$self}{count} . " documents\n";
        }
    }
};

while (<>) {
    # strip off initial "-", thread id, and thread pointer; separate
    # first word and rest
    if (/^\-?[0-9]*\[[0-9a-f]*\]: (\S*) ([^\n\r]*)[\n\r]*$/) {
        my ($handler, $data) = ($1, $2);
        if (defined(${$handlers}{$handler})) {
            call("handle_line", ${$handlers}{$handler}, $data);
        }
    }
}

foreach my $key (keys(%{$handlers})) {
    call("dump", ${$handlers}{$key});
}
print "Summary:\n";
foreach my $key (keys(%{$handlers})) {
    call("summary", ${$handlers}{$key});
}
