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
# The Original Code is Mozilla Installer locale verification.
#
# The Initial Developer of the Original Code is Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Robert Strong <robert.bugzilla@gmail.com>
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

# Note: this was written using compare-locales.pl as a template.

$failure = 0;

sub readProperties
{
    my ($file) = @_;

    open PROPS, "<$file" || die ("Couldn't open file $file");

    local $/ = undef;
    my $contents = <PROPS>;
    close PROPS;

    $contents =~ s/\\$$//gm;

    return $contents =~ /^\s*([^#!\r\n]*)$/gm;
}

sub checkProperties
{
    my ($path) = @_;

    my @invalid;

    my @entities = readProperties("$gSourceDir/$path");

    foreach my $entity (@entities) {
        if ($entity =~ m|.*(\$\(\^Name\)).*| ||
            $entity =~ m|.*(\$\(\^NameDA\)).*| ||
            $entity =~ m|.*(\$\{BrandFullName\}).*| ||
            $entity =~ m|.*(\$\{BrandShortName\}).*|) {
            push @invalid, "$entity\n    *** incorrect variable usage please refer to en-US";
        }
    }

    if (@invalid) {
        $failure = 1;
        print "Properties in $path using invalid values:\n";
        if (@invalid) {
            print "  In $gSourceDir: (change these in your localization)\n";
            map { print "    $_\n"; } @invalid;
        }
    }
}

sub checkDir
{
    my ($path) = @_;

    my (@entries);

    opendir(DIR, "$gSourceDir/$path") ||
        die ("Couldn't list $gSourceDir/$path");
    @entries = grep(!(/^(\.|CVS)/ || /~$/), readdir(DIR));
    closedir(DIR);

    foreach my $file (@entries) {
        if (-d "$gSourceDir/$path/$file") {
            checkDir("$path/$file");
        } else {
            if ($file =~ /\.properties$/) {
               checkProperties("$path/$file");
            }
        }
    }
}

local ($gSourceDir) = @ARGV;
($gSourceDir) || die("Specify one directory to verify");

if (!-d $gSourceDir) {
    die("Not a directory");
}

checkDir(".");

exit $failure;
