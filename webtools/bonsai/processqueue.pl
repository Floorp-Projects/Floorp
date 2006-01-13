#!/usr/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

# This takes a bunch of files that have been put into the 'bonsai
# queue' (the 'queue' subdirectory) and processes them as checkins.
# This is to work around worlds where we can't send mail from inside
# the CVS loginfo file.
#
# Each file is expected to have the CVSROOT in the first line, any args in the 
# second line (currently unused), and the data fed to loginfo as the remaining
# lines.

$inprocess = "data/queue/processing-$$";

foreach $file (sort(glob("data/queue/*.q"))) {
    rename $file, $inprocess || die "Couldn't rename queue file.";
    system "./dolog.pl < $inprocess";
    rename $inprocess, "$file.done";
}
