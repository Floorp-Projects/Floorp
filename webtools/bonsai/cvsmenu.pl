# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

1;

require 'utils.pl';

sub cvsmenu {
my($extra) = @_;
loadConfigData();
print "
<table border=1 bgcolor=#ffffcc $extra><tr><th>Menu</tr><tr><td>
<p><dl>";

my $pass;
my $i;
foreach $pass ("cvsqueryform|Query",
               "rview|Browse",
               "moduleanalyse|Examine Modules") {
    ($page, $title) = split(/\|/, $pass);
    print "<b>$title</b><br><ul>\n";
    foreach $i (@treelist) {
        my $branch = $treeinfo{$i}->{'branch'};
        if ($branch ne "") {
            $branch = "&branch=" . $branch;
        }
        $desc = $treeinfo{$i}->{'shortdesc'};
        if ($desc eq "") {
            $desc = $treeinfo{$i}->{'description'};
        }
        print "<li><a href=$page.cgi?cvsroot=$treeinfo{$i}->{'repository'}&module=$treeinfo{$i}->{'module'}$branch>$desc</a>\n";
    };
    print "</ul>\n";
};

if (open(EXTRA, "<data/cvsmenuextra")) {
    while (<EXTRA>) {
        print $_;
    }
    close EXTRA;
}

print "</dl>
<p></tr><tr><td><font size=-1> Questions, Comments, Feature requests? mail <a href=mailto:terry\@netscape.com>terry</a>
</tr></table>
";

}

