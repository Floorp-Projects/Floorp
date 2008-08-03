#! /usr/bin/perl
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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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



sub decode {
    read (STDIN, $buffer, $ENV{'CONTENT_LENGTH'});
    @pairs = split(/&/, $buffer);
    foreach $pair (@pairs)
    {
        ($name, $value) = split(/=/, $pair);
        $value =~tr/+/ /;
        $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
        $FORM{$name} = $value;
#        print "name=$name  value=$value<BR>\n";
    }
}

print "Content-type: text/html\n\n";

&decode();

$dataSignature = $FORM{'dataSignature'};
$dataToSign = $FORM{'dataToSign'};

unlink("signature");
open(FILE1,">signature") || die("Cannot open file for writing\n");

print FILE1 "$dataSignature";

close(FILE1);


unlink("data");
open(FILE2,">data") || die("Cannot open file for writing\n");

print FILE2 "$dataToSign";

close(FILE2);


print "<BR><B>Signed Data:</B><BR>", "$dataToSign", "<BR>";
 
print "<BR><b>Verification Info:</b><BR>";
 
$verInfo = `./signver -D . -s signature -d data -v`;
print "<font color=red><b>$verInfo</b></font><BR>";

print "<BR><B>Signature Data:</B><BR>", "$dataSignature", "<BR>";

print "<BR><b>Signature Info:</b><BR>";

foreach $line (`./signver -s signature -A`) {
     print "$line<BR>\n";
}

print "<b>End of Info</b><BR>";

