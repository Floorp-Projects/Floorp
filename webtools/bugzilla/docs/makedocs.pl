#!/usr/bin/perl -w
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
# Contributor(s): Matthew Tuck <matty@chariot.net.au>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Colin Ogilvie <colin.ogilvie@gmail.com>

# This script compiles all the documentation.

use diagnostics;
use strict;

use File::Basename;
use lib("..");
use Bugzilla::Install::Requirements qw (REQUIRED_MODULES OPTIONAL_MODULES MOD_PERL_MODULES);
use Bugzilla::Constants qw (DB_MODULE);
chdir dirname($0);

###############################################################################
# Generate minimum version list
###############################################################################

my $modules = REQUIRED_MODULES;
my $opt_modules = OPTIONAL_MODULES;
my $mod_perl_modules = MOD_PERL_MODULES;

open(ENTITIES, '>', 'xml/bugzilla.ent') or die('Could not open xml/bugzilla.ent: ' . $!);
print ENTITIES '<?xml version="1.0"?>' ."\n\n";
print ENTITIES '<!-- Module Versions -->' . "\n";
foreach my $module (@$modules, @$opt_modules)
{
    my $name = $module->{'name'};
    $name =~ s/::/-/g;
    $name = lc($name);
    #This needs to be a string comparison, due to the modules having
    #version numbers like 0.9.4
    my $version = $module->{'version'} eq 0 ? 'any' : $module->{'version'};
    print ENTITIES '<!ENTITY min-' . $name . '-ver "'.$version.'">' . "\n";
}

print ENTITIES "\n <!-- mod_perl Versions --> \n";
foreach my $module (@$mod_perl_modules)
{
    my $name = $module->{'name'};
    $name =~ s/::/-/g;
    $name = lc($name);
    #This needs to be a string comparison, due to the modules having
    #version numbers like 0.9.4
    my $version = $module->{'version'} eq 0 ? 'any' : $module->{'version'};
    print ENTITIES '<!ENTITY min-mp-' . $name . '-ver "'.$version.'">' . "\n";
}

print ENTITIES "\n <!-- Database Versions --> \n";

my $db_modules = DB_MODULE;
foreach my $db (keys %$db_modules) {
    my $name = $db_modules->{$db}->{'dbd'};
    $name =~ s/::/-/g;
    $name = lc($name);
    my $version = $db_modules->{$db}->{'dbd_version'} eq 0 ? 'any' : $db_modules->{$db}->{'dbd_version'};
    my $db_version = $db_modules->{$db}->{'db_version'};
    print ENTITIES '<!ENTITY min-' . $name . '-ver "'.$version.'">' . "\n";
    print ENTITIES '<!ENTITY min-' . lc($db) . '-ver "'.$db_version.'">' . "\n";
}
close(ENTITIES);

###############################################################################
# Environment Variable Checking
###############################################################################

my ($JADE_PUB, $LDP_HOME);

if (defined $ENV{JADE_PUB} && $ENV{JADE_PUB} ne '') {
    $JADE_PUB = $ENV{JADE_PUB};
}
else {
    die "You need to set the JADE_PUB environment variable first.";
}

if (defined $ENV{LDP_HOME} && $ENV{LDP_HOME} ne '') {
    $LDP_HOME = $ENV{LDP_HOME};
}
else {
    die "You need to set the LDP_HOME environment variable first.";
}

###############################################################################
# Subs
###############################################################################

sub MakeDocs {

    my ($name, $cmdline) = @_;

    print "Creating $name documentation ...\n" if defined $name;
    print "$cmdline\n\n";
    system $cmdline;
    print "\n";

}

###############################################################################
# Make the docs ...
###############################################################################

if (!-d 'html') {
    unlink 'html';
    mkdir 'html', 0755;
}
if (!-d 'txt') {
    unlink 'txt';
    mkdir 'txt', 0755;
}
if (!-d 'pdf') {
    unlink 'pdf';
    mkdir 'pdf', 0755;
}

chdir 'html';

MakeDocs('separate HTML', "jade -t sgml -i html -d $LDP_HOME/ldp.dsl\#html " .
	 "$JADE_PUB/xml.dcl ../xml/Bugzilla-Guide.xml");
MakeDocs('big HTML', "jade -V nochunks -t sgml -i html -d " .
         "$LDP_HOME/ldp.dsl\#html $JADE_PUB/xml.dcl " .
	 "../xml/Bugzilla-Guide.xml > Bugzilla-Guide.html");
MakeDocs('big text', "lynx -dump -justify=off -nolist Bugzilla-Guide.html " .
	 "> ../txt/Bugzilla-Guide.txt");

if (! grep("--with-pdf", @ARGV)) {
    exit;
}

MakeDocs('PDF', "jade -t tex -d $LDP_HOME/ldp.dsl\#print $JADE_PUB/xml.dcl " .
         '../xml/Bugzilla-Guide.xml');
chdir '../pdf';
MakeDocs(undef, 'mv ../xml/Bugzilla-Guide.tex .');
MakeDocs(undef, 'pdfjadetex Bugzilla-Guide.tex');
MakeDocs(undef, 'pdfjadetex Bugzilla-Guide.tex');
MakeDocs(undef, 'pdfjadetex Bugzilla-Guide.tex');
MakeDocs(undef, 'rm Bugzilla-Guide.tex Bugzilla-Guide.log Bugzilla-Guide.aux Bugzilla-Guide.out');

