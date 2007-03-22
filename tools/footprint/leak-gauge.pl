#!/usr/bin/perl -w
# vim:sw=4:ts=4:et:
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is leak-gauge.pl
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# $Id: leak-gauge.pl,v 1.7 2006/01/14 00:27:41 dbaron%dbaron.org Exp $
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
#   NSPR_LOG_MODULES=DOMLeak:5,DocumentLeak:5,nsDocShellLeak:5
#   NSPR_LOG_FILE=nspr.log     (or any other filename of your choice)
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
            my $docs = ${$self}{docs};
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
                    ${${$docs}{$addr}}{$uri} = 1;
                }
            }
        },
        dump => sub ($) {
            my ($self) = @_;
            my $docs = ${$self}{docs};
            foreach my $addr (keys(%{$docs})) {
                print "Leaked document at address $addr.\n";
                foreach my $uri (keys(%{${$docs}{$addr}})) {
                    print " ... with URI \"$uri\".\n";
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
