#!/usr/bin/perl -w
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
# The Original Code is libxul build automation.
#
# The Initial Developer of the Original Code is
# Benjamin Smedberg <benjamin@smedbergs.us>
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#	Soeren Munk Skroeder (sskroeder) @ 2005-08-24 - 
#	  added support for inc files (defines)
#	  added to-do description with "add/remove these keys from your locale"
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

$failure = 0;

sub unJAR
{
    my ($file, $dir) = @_;

    -d $dir && system("rm -rf $dir");
    system("unzip -q -d $dir $file") && die("Could not unZIP $file");
}

sub readDTD
{
    my ($file) = @_;

    open DTD, "<$file" || die ("Couldn't open file $file");

    local $/ = undef;
    my $contents = <DTD>;
    close DTD;

    $contents =~ s/<!--.*?-->//gs; # strip SGML comments

    return $contents =~ /<!ENTITY\s+([\w\.]+)\s+(?:\"[^\"]*\"|\'[^\']*\')\s*>/g;
}

sub compareDTD
{
    my ($path) = @_;

    my @entities1 = readDTD("$gSourceDir1/$path");
    my %entities2 = map { $_ => 1 } readDTD("$gSourceDir2/$path");

    my @extra1;

    foreach my $entity (@entities1) {
        if (exists $entities2{$entity}) {
            delete $entities2{$entity};
        } else {
            push @extra1, $entity;
        }
    }

    if (@extra1 or keys %entities2) {
        $failure = 1;
        print "Entities in $path don't match:\n";
        if (@extra1) {
            print "  In $gSource1: (add these keys to your localization)\n";
            map { print "    $_\n"; } @extra1;
        }

        if (keys %entities2) {
            print "  In $gSource2: (remove these keys from your localization)\n";
            map {print "    $_\n"; } keys %entities2;
        }
        print "\n";
    }
}

sub readProperties
{
    my ($file) = @_;

    open PROPS, "<$file" || die ("Couldn't open file $file");

    local $/ = undef;
    my $contents = <PROPS>;
    close PROPS;

    $contents =~ s/\\$$//gm;

    return $contents =~ /^\s*([^#!\s\r\n][^=:\r\n]*?)\s*[=:]/gm;
}

sub compareProperties
{
    my ($path) = @_;

    my @entities1 = readProperties("$gSourceDir1/$path");
    my %entities2 = map { $_ => 1 } readProperties("$gSourceDir2/$path");

    my @extra1;

    foreach my $entity (@entities1) {
        if (exists $entities2{$entity}) {
            delete $entities2{$entity};
        } else {
# hack to ignore non-fatal region.properties differences
            if ($path !~ /chrome\/browser-region\/region\.properties$/ or
                ($entity !~ /browser\.search\.order\.[1-9]/ and
                 $entity !~ /browser\.contentHandlers\.types\.[0-5]/ and
                 $entity !~ /gecko\.handlerService\.schemes\./ and
                 $entity !~ /gecko\.handlerService\.defaultHandlersVersion/)) {
                push @extra1, $entity;
            }
        }
    }
# hack to ignore non-fatal region.properties differences
    if ($path =~ /chrome\/browser-region\/region\.properties$/) {
        foreach $entity (keys(%entities2)) {
            if ($entity =~ /browser\.search\.order\.[1-9]/ ||
                $entity =~ /browser\.contentHandlers\.types\.[0-5]/ ||
                $entity =~ /gecko\.handlerService\.schemes\./ ||
                $entity =~ /gecko\.handlerService\.defaultHandlersVersion/) {
                delete $entities2{$entity};
            }
        }
    }

    if (@extra1 or keys %entities2) {
        $failure = 1;
        print "Properties in $path don't match:\n";
        if (@extra1) {
            print "  In $gSource1: (add these to your localization)\n";
            map { print "    $_\n"; } @extra1;
        }

        if (keys %entities2) {
            print "  In $gSource2: (remove these from your localization)\n";
            map {print "    $_\n"; } keys %entities2;
        }
        print "\n";
    }
}

sub readDefines
{
    my ($file) = @_;

    open DEFS, "<$file" || die ("Couldn't open file $file");

    local $/ = undef;
    my $contents = <DEFS>;
    close DEFS;

    return $contents =~ /#define\s+(\w+)/gm;
}

sub compareDefines
{
    my ($path) = @_;

    my @entities1 = readDefines("$gSourceDir1/$path");
    my %entities2 = map { $_ => 1 } readDefines("$gSourceDir2/$path");

    my @extra1;

    foreach my $entity (@entities1) {
        if (exists $entities2{$entity}) {
            delete $entities2{$entity};
        } else {
            push @extra1, $entity;
        }
    }

    if (@extra1 or keys %entities2) {
        $failure = 1;
        print "Defines in $path don't match:\n";
        if (@extra1) {
            print "  In $gSource1: (add these to your localization)\n";
            map { print "    $_\n"; } @extra1;
        }

        if (keys %entities2) {
            print "  In $gSource2: (remove these from your localization)\n";
            map {print "    $_\n"; } keys %entities2;
        }
        print "\n";
    }
}

sub compareDir
{
    my ($path) = @_;

    my (@entries1, %entries2);

    opendir(DIR1, "$gSourceDir1/$path") ||
        die ("Couldn't list $gSourceDir1/$path");
    @entries1 = grep(!(/^(\.|CVS)/ || /~$/), readdir(DIR1));
    closedir(DIR1);

    opendir(DIR2, "$gSourceDir2/$path") ||
        die ("Couldn't list $gSourceDir2/$path");
    %entries2 = map { $_ => 1 } grep(!(/^(\.|CVS)/ || /~$/), readdir(DIR2));
    closedir(DIR2);

    foreach my $file (@entries1) {
        if (exists($entries2{$file})) {
            delete $entries2{$file};

            if (-d "$gSourceDir1/$path/$file") {
                compareDir("$path/$file");
            } else {
                if ($file =~ /\.dtd$/) {
                    compareDTD("$path/$file");
                } elsif ($file =~ /\.inc$/) {
                    compareDefines("$path/$file");
                } elsif ($file =~ /\.properties$/) {
                    compareProperties("$path/$file");
                } else {
                    print "no comparison for $path/$file\n";
                }
            }
        } else {
            push @gSource1Extra, "$path/$file";
        }
    }

    foreach my $file (keys %entries2) {
        push @gSource2Extra, "$path/$file";
    }
}

local ($gSource1, $gSource2) = @ARGV;
($gSource1 && $gSource2) || die("Specify two directories or ZIP files");

my ($gSource1IsZIP, $gSource2IsZIP);
local ($gSourceDir1, $gSourceDir2);
local (@gSource1Extra, @gSource2Extra);

if (-d $gSource1) {
    $gSource1IsZIP = 0;
    $gSourceDir1 = $gSource1;
} else {
    $gSource1IsZIP = 1;
    $gSourceDir1 = "temp1";
    unJAR($gSource1, $gSourceDir1);
}

if (-d $gSource2) {
    $gSource2IsZIP = 0;
    $gSourceDir2 = $gSource2;
} else {
    $gSource2IsZIP = 1;
    $gSourceDir2 = "temp2";
    unJAR($gSource2, $gSourceDir2);
}

compareDir(".");

if (@gSource1Extra) {
    print "Files in $gSource1 not in $gSource2:\n";
    map { print "  $_\n"; } @gSource1Extra;
    print "\n";
}

if (@gSource2Extra) {
    print "Files in $gSource2 not in $gSource1:\n";
    map { print "  $_\n"; } @gSource2Extra;
    print "\n";
}

$gSource1IsZIP && system("rm -rf $gSourceDir1");
$gSource2IsZIP && system("rm -rf $gSourceDir2");

exit $failure;
