#!/usr/bin/perl
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
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#


use CGI qw(:standard);



# Replace this will the full path to the sslstrength executable.
$sslstrength = "./sslstrength";


# Replace this with the name of this CGI.

$sslcgi = "sslstr.cgi";


$query = new CGI;

print header;

print "<HTML><HEAD>
<SCRIPT language='javascript'>

function doexport(form) {
    form.ssl2ciphers.options[0].selected=0;
    form.ssl2ciphers.options[1].selected=0;
    form.ssl2ciphers.options[2].selected=0;
    form.ssl2ciphers.options[3].selected=0;
    form.ssl2ciphers.options[4].selected=1;
    form.ssl2ciphers.options[5].selected=1;

    form.ssl3ciphers.options[0].selected=1;
    form.ssl3ciphers.options[1].selected=1;
    form.ssl3ciphers.options[2].selected=0;
    form.ssl3ciphers.options[3].selected=1;
    form.ssl3ciphers.options[4].selected=1;
    form.ssl3ciphers.options[5].selected=1;
    form.ssl3ciphers.options[6].selected=0;
    form.ssl3ciphers.options[7].selected=0;


}

function dodomestic(form) {
    form.ssl2ciphers.options[0].selected=1;
    form.ssl2ciphers.options[1].selected=1;
    form.ssl2ciphers.options[2].selected=1;
    form.ssl2ciphers.options[3].selected=1;
    form.ssl2ciphers.options[4].selected=1;
    form.ssl2ciphers.options[5].selected=1;

    form.ssl3ciphers.options[0].selected=1;
    form.ssl3ciphers.options[1].selected=1;
    form.ssl3ciphers.options[2].selected=1;
    form.ssl3ciphers.options[3].selected=1;
    form.ssl3ciphers.options[4].selected=1;
    form.ssl3ciphers.options[5].selected=1;
    form.ssl3ciphers.options[6].selected=1;
    form.ssl3ciphers.options[7].selected=1;

}

function doclearssl2(form) {
    form.ssl2ciphers.options[0].selected=0;
    form.ssl2ciphers.options[1].selected=0;
    form.ssl2ciphers.options[2].selected=0;
    form.ssl2ciphers.options[3].selected=0;
    form.ssl2ciphers.options[4].selected=0;
    form.ssl2ciphers.options[5].selected=0;
}


function doclearssl3(form) {
    form.ssl3ciphers.options[0].selected=0;
    form.ssl3ciphers.options[1].selected=0;
    form.ssl3ciphers.options[2].selected=0;
    form.ssl3ciphers.options[3].selected=0;
    form.ssl3ciphers.options[4].selected=0;
    form.ssl3ciphers.options[5].selected=0;
    form.ssl3ciphers.options[6].selected=0;
    form.ssl3ciphers.options[7].selected=0;

}

function dohost(form,hostname) {
    form.host.value=hostname;
    }



</SCRIPT>
<TITLE>\n";
print "SSLStrength\n";
print "</TITLE></HEAD>\n";

print "<h1>SSLStrength</h1>\n";

if ($query->param('dotest')) {
    print "Output from sslstrength: \n";
    print "<pre>\n";

    $cs = "";
    
    @ssl2ciphers = $query->param('ssl2ciphers');
    for $cipher (@ssl2ciphers) {
	if ($cipher eq "SSL_EN_RC2_128_WITH_MD5")              { $cs .= "a"; }
	if ($cipher eq "SSL_EN_RC2_128_CBC_WITH_MD5")          { $cs .= "b"; }
	if ($cipher eq "SSL_EN_DES_192_EDE3_CBC_WITH_MD5")     { $cs .= "c"; }
	if ($cipher eq "SSL_EN_DES_64_CBC_WITH_MD5")           { $cs .= "d"; }
	if ($cipher eq "SSL_EN_RC4_128_EXPORT40_WITH_MD5")     { $cs .= "e"; }
	if ($cipher eq "SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5") { $cs .= "f"; }
    }

    @ssl3ciphers = $query->param('ssl3ciphers');
    for $cipher (@ssl3ciphers) {
	if ($cipher eq "SSL_RSA_WITH_RC4_128_MD5")           { $cs .= "i"; }
	if ($cipher eq "SSL_RSA_WITH_3DES_EDE_CBC_SHA")      { $cs .= "j"; }
	if ($cipher eq "SSL_RSA_WITH_DES_CBC_SHA")           { $cs .= "k"; }
	if ($cipher eq "SSL_RSA_EXPORT_WITH_RC4_40_MD5")     { $cs .= "l"; }
	if ($cipher eq "SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5") { $cs .= "m"; }
	if ($cipher eq "SSL_RSA_WITH_NULL_MD5")              { $cs .= "o"; }
	if ($cipher eq "SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA") { $cs .= "p"; }
	if ($cipher eq "SSL_RSA_FIPS_WITH_DES_CBC_SHA")      { $cs .= "q"; }
    }

    $hs = $query->param('host');
    if ($hs eq "") {
	print "</pre>You must specify a host to connect to.<br><br>\n";
	exit(0);
    }

    $ps = $query->param('policy');
    
    $cmdstring = "$sslstrength $hs policy=$ps ciphers=$cs";

    print "running sslstrength:\n";
    print "$cmdstring\n";

    $r = open(SSLS, "$cmdstring |");
    if ($r == 0) {
	print "<pre>There was a problem starting $cmdstring<br><br>\n";
	exit(0);
    }
    while (<SSLS>) {
	print "$_";
    }
    close(SSLS);


    print "</pre>\n";
   
}

else {
print "<FORM method=post action=$sslcgi>\n";
print "<hr>
<h2>Host Name</h2>
<TABLE BORDER=0 CELLPADDING=20>
<TR>
<TD>
Type hostname here:<br>
<input type=text name=host size=30>&nbsp;<br><br>
<TD>
 <b>Or click these buttons to test some well-known servers</b><br>
 <TABLE BORDER=0>
 <TR>
 <TD>
 Export servers:
 <TD>
 <input type=button value='F-Tech' onclick=dohost(this.form,'strongbox.ftech.net')>
 </TR>
 <TR>
 <TD>
 Domestic servers:
 <TD>
 <input type=button value='Wells Fargo' onclick=dohost(this.form,'banking.wellsfargo.com')>
 </TR>
 <TR>
 <TD>
 Step-Up Servers
 <TD>
 <input type=button value='Barclaycard' onclick=dohost(this.form,'enigma.barclaycard.co.uk')>
 <input type=button value='BBVnet' onclick=dohost(this.form,'www.bbvnet.com')>&nbsp;
 <input type=button value='BHIF' onclick=dohost(this.form,'empresas.bhif.cl')>&nbsp;
 </TR>
 </TABLE>
</TR>
</TABLE>
<br>
<hr>
<br>
<h2>Encryption policy</h2>
<input type=radio name=policy VALUE=export            onclick=doexport(this.form)>&nbsp;
Export<br>
<input type=radio name=policy VALUE=domestic CHECKED  onclick=dodomestic(this.form)>&nbsp;
Domestic<br>
<br>
<hr>
<br>
<h2>Cipher Selection</h2>
(use ctrl to multi-select)<br>
<table>
<tr>
<td>SSL 2 Ciphers
<td>
<SELECT NAME=ssl2ciphers SIZE=6 MULTIPLE align=bottom>
<OPTION SELECTED>SSL_EN_RC4_128_WITH_MD5
<OPTION SELECTED>SSL_EN_RC2_128_CBC_WITH_MD5
<OPTION SELECTED>SSL_EN_DES_192_EDE3_CBC_WITH_MD5
<OPTION SELECTED>SSL_EN_DES_64_CBC_WITH_MD5
<OPTION SELECTED>SSL_EN_RC4_128_EXPORT40_WITH_MD5
<OPTION SELECTED>SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5
</SELECT>
<input type=button Value='Clear all' onclick = 'doclearssl2(this.form)'>
</tr>
<tr>
<td>SSL3 Ciphers
<td>
<SELECT NAME=ssl3ciphers SIZE=8 MULTIPLE>
<OPTION SELECTED>SSL_RSA_WITH_RC4_128_MD5
<OPTION SELECTED>SSL_RSA_WITH_3DES_EDE_CBC_SHA
<OPTION SELECTED>SSL_RSA_WITH_DES_CBC_SHA
<OPTION SELECTED>SSL_RSA_EXPORT_WITH_RC4_40_MD5
<OPTION SELECTED>SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5
<OPTION SELECTED>SSL_RSA_WITH_NULL_MD5
<OPTION SELECTED>SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA
<OPTION SELECTED>SSL_RSA_FIPS_WITH_DES_CBC_SHA
</SELECT>
<input type=button value='Clear all' onclick = 'doclearssl3(this.form)'>

<TD>
<input type=submit name=dotest value='Run SSLStrength'>
</tr>
</table>
<input type=hidden name=dotest>
<br>
<br>
</form>
\n";

}


exit(0);


__END__

 id    CipherName                                     Domestic     Export      
 a     SSL_EN_RC4_128_WITH_MD5              (ssl2)    Yes          No          
 b     SSL_EN_RC2_128_CBC_WITH_MD5          (ssl2)    Yes          No          
 c     SSL_EN_DES_192_EDE3_CBC_WITH_MD5     (ssl2)    Yes          No          
 d     SSL_EN_DES_64_CBC_WITH_MD5           (ssl2)    Yes          No          
 e     SSL_EN_RC4_128_EXPORT40_WITH_MD5     (ssl2)    Yes          Yes         
 f     SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5 (ssl2)    Yes          Yes         
 i     SSL_RSA_WITH_RC4_128_MD5             (ssl3)    Yes          Step-up only
 j     SSL_RSA_WITH_3DES_EDE_CBC_SHA        (ssl3)    Yes          Step-up only
 k     SSL_RSA_WITH_DES_CBC_SHA             (ssl3)    Yes          No          
 l     SSL_RSA_EXPORT_WITH_RC4_40_MD5       (ssl3)    Yes          Yes         
 m     SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5   (ssl3)    Yes          Yes         
 o     SSL_RSA_WITH_NULL_MD5                (ssl3)    Yes          Yes         



