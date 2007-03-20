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
# The Initial Developer of the Original Code is Everything Solved.
# Portions created by Everything Solved are Copyright (C) 2007
# Everything Solved. All Rights Reserved.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

# This file contains a single hash named %strings, which is used by the
# installation code to display strings before Template-Toolkit can safely
# be loaded.
#
# Each string supports a very simple substitution system, where you can
# have variables named like ##this## and they'll be replaced by the string
# variable with that name.
#
# Please keep the strings in alphabetical order by their name.

%strings = (
    any  => 'any',
    blacklisted => '(blacklisted)',
    checking_for => 'Checking for',
    checking_dbd      => 'Checking available perl DBD modules...',
    checking_optional => 'The following Perl modules are optional:',
    checking_modules  => 'Checking perl modules...',
    header => "* This is Bugzilla ##bz_ver## on perl ##perl_ver##\n"
            . "* Running on ##os_name## ##os_ver##",
    module_found => "found v##ver##",
    module_not_found => "not found",
    module_ok => 'ok',
    module_unknown_version => "found unknown version",
);

1;
