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
# The Original Code are the Bugzilla tests.
#
# The Initial Developer of the Original Code is Jacob Steenhagen.
# Portions created by Jacob Steenhagen are
# Copyright (C) 2001 Jacob Steenhagen. All
# Rights Reserved.
#
# Contributor(s): Jacob Steenhagen <jake@bugzilla.org>
#                 Zach Lipton <zach@zachlipton.com>
#                 David D. Kilzer <ddkilzer@kilzer.net>
#

#################
#Bugzilla Test 4#
####Templates####

use strict;

use lib 't';

use Support::Templates;

# Bug 137589 - Disable command-line input of CGI.pm when testing
use CGI qw(-no_debug);

use File::Spec 0.82;
use Template;
use Test::More tests => (  scalar(@Support::Templates::referenced_files)
                         + scalar(@Support::Templates::actual_files) * 2);

# Capture the TESTOUT from Test::More or Test::Builder for printing errors.
# This will handle verbosity for us automatically.
my $fh;
{
    local $^W = 0;  # Don't complain about non-existent filehandles
    if (-e \*Test::More::TESTOUT) {
        $fh = \*Test::More::TESTOUT;
    } elsif (-e \*Test::Builder::TESTOUT) {
        $fh = \*Test::Builder::TESTOUT;
    } else {
        $fh = \*STDOUT;
    }
}

my $include_path = $Support::Templates::include_path;

# Check to make sure all templates that are referenced in
# Bugzilla exist in the proper place.

foreach my $file(@Support::Templates::referenced_files) {
    my $path = File::Spec->catfile($include_path, $file);
    if (-e $path) {
        ok(1, "$path exists");
    } else {
        ok(0, "$path does not exist --ERROR");
    }
}

# Processes all the templates to make sure they have good syntax
my $provider = Template::Provider->new(
{
    INCLUDE_PATH => $include_path ,
    # Need to define filters used in the codebase, they don't
    # actually have to function in this test, just be defined.
    # See globals.pl for the actual codebase definitions.
    FILTERS =>
    {
        html_linebreak => sub { return $_; },
        js        => sub { return $_ } ,
        strike    => sub { return $_ } ,
        url_quote => sub { return $_ } ,
        quoteUrls => sub { return $_ } ,
        bug_link => [ sub { return sub { return $_; } }, 1] ,
        csv       => sub { return $_ } ,
        time      => sub { return $_ } ,
    },
}
);

foreach my $file(@Support::Templates::actual_files) {
    my $path = File::Spec->catfile($include_path, $file);
    if (-e $path) {
        my ($data, $err) = $provider->fetch($file);

        if (!$err) {
            ok(1, "$file syntax ok");
        }
        else {
            ok(0, "$file has bad syntax --ERROR");
            print $fh $data . "\n";
        }
    }
    else {
        ok(1, "$path doesn't exist, skipping test");
    }
}

# check to see that all templates have a version string:

foreach my $file(@Support::Templates::actual_files) {
    my $path = File::Spec->catfile($include_path, $file);
    open(TMPL, $path);
    my $firstline = <TMPL>;
    if ($firstline =~ /\d+\.\d+\@[\w\.-]+/) {
        ok(1,"$file has a version string");
    } else {
        ok(0,"$file does not have a version string --ERROR");
    }
    close(TMPL);
}

exit 0;
