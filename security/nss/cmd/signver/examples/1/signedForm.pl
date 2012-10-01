#! /usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.



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

