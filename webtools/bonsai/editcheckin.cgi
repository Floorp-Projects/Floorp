#!/usr/bonsaitools/bin/perl -w
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

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::TreeID;
}

require 'CGI.pl';

LoadCheckins();

my $info = eval("\\%" . $::FORM{'id'});

print "Content-type: text/html

<HTML>
<TITLE>Say the magic word.</TITLE>
<H1>Edit a checkin.</H1>

Congratulations, you have found the hidden edit-a-checkin feature.  Of course,
you need to know the magic word to do anything from here.

<P>

<FORM method=get action=\"doeditcheckin.cgi\">
<TABLE>
<tr>
<td align=right><B>Password:</B></td>
<td><INPUT NAME=password TYPE=password></td>
</tr><tr>
<td align=right><B>When:</B></td>
<td><INPUT NAME=datestring VALUE=\"" .
value_quote(MyFmtClock($info->{'date'})) . "\">
</td></tr>
";

if (!exists $info->{'notes'}) {
    $info->{'notes'} = "";
}

foreach my $i ('person', 'dir', 'files', 'notes') {
    print "<tr><td align=right><B>$i:</B></td>";
    print "<td><INPUT NAME=$i VALUE=\"" . value_quote($info->{$i}) .
    "\"></td></tr>";
}

sub CheckString {
    my ($value) = (@_);
    if ($value) {
        return "CHECKED";
    } else {
        return "";
    }
}

my $isopen = CheckString($info->{'treeopen'});
my $isclosed = CheckString(!$info->{'treeopen'});

print qq{
<tr><td align=right><b>Tree state:</b></td>
<td><INPUT TYPE=radio NAME=treeopen VALUE=1 $isopen>Open
</td></tr><tr><td></td>
<td><INPUT TYPE=radio NAME=treeopen VALUE=0 $isclosed>Closed
</td></tr><tr>
<td align=right valign=top><B>Log message:</B></td>
<td><TEXTAREA NAME=log ROWS=10 COLS=80>$info->{'log'}</TEXTAREA></td></tr>
</table>
<INPUT TYPE=CHECKBOX NAME=nukeit>Check this box to blow away this checkin entirely.<br>

<INPUT TYPE=SUBMIT VALUE=Submit>
};

foreach my $i (sort(keys(%$info))) {
    my $q = value_quote($info->{$i});
    print qq{<INPUT TYPE=HIDDEN NAME=orig$i VALUE="$q">\n};
}

print "<INPUT TYPE=HIDDEN NAME=id VALUE=\"$::FORM{'id'}\">";

print "<INPUT TYPE=HIDDEN NAME=treeid VALUE=\"" . value_quote($::TreeID) . "\">";



print "</TABLE></FORM>";

PutsTrailer();

