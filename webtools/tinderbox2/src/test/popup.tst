#!#perl# -w --

# generate static html pages for use in testing the popup libraries.
# Output is written to the directory $TinderConfig::TINDERBOX_HTML_DIR/popup


# $Revision: 1.4 $ 
# $Date: 2003/02/03 13:46:07 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/popup.tst,v $ 
# $Name:  $ 
#


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 


# Load the standard perl libraries
use File::Basename;



# Load the tinderbox specific libraries
use lib '#tinder_libdir#';

use TinderConfig;
use HTMLPopUp;
use Utils;

set_static_vars();
get_env();


# define some static tables which look like the kind of popups we
# normally produce.

$build_info_table =<<EOF;

endtime: 04/26&nbsp;09:56<br>
starttime: 04/26&nbsp;09:11<br>
runtime: 45.22 (minutes)<br>
buildstatus: success<br>
buildname: Linux comet Depend<br>
tree: SeaMonkey<br>
<br>
<a href=http://mozilla.com>test</a><br>

EOF
    ;


$checkin_table =<<EOF;	
Checkins by <b>pinkerton%netscape.com</b> 
<br> for module: MozillaTinderboxAll 
<br>branch: HEAD 
<br> 
<table border cellspacing=2> 
	<tr><th>Time</th><th>File</th><th>Log</th></tr> 
	<tr><td>04/25&nbsp;14:26</td><td>mozilla/embedding/config/basebrowser-mac-cfm</td><td>remove bugnumber accidentally inserted in file.</td></tr> 
	<tr>
		<td>04/25&nbsp;14:26</td>
		<td>mozilla/embedding/components/ui/helperAppDlg/nsHelperAppDlg.xul</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/basebrowser-mac-cfmDebug</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/basebrowser-win</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/GenerateManifest.pm</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/gen_mn.pl</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 


	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/basebrowser-mac-cfm</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/basebrowser-unix</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/embed-jar.mn</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/build/mac/build_scripts/MozillaBuildList.pm</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/config/basebrowser-mac-macho</td>
		<td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr> 

	<tr>
		<td>04/25&nbsp;14:25</td>
		<td>mozilla/embedding/browser/powerplant/source/CBrowserApp.cp</td><td>package download progress on mac for embedding. fix jar manifest to not pull en-unix on every platform. r=bryner/sr=ben/a=rjesup. bug#134523</td>
	</tr>

</table>

EOF

    ;


# create one popup page for each popup library we have.





my @popup_libs = glob "./build/lib/HTMLPopUp/*";
foreach $popup_lib (@popup_libs) {

    $lib = basename($popup_lib);
    $lib =~ s/\..*//;
    $libname= 'HTMLPopUp::'.$lib;
    @HTMLPopUp::ISA = ($libname);
    eval " use $libname ";

    my $outfile ="$TinderConfig::TINDERBOX_HTML_DIR/popup/$lib.html";
    
    my $links;
    
    $links.= "\t\t".
      HTMLPopUp::Link(
		      "linktxt"=>"L", 
		      "href"=>"http://lounge.mozilla.org/cgi-bin/cgiwrap/cgiwrap_exe/tbox/gunzip.cgi?tree=SeaMonkey&full-log=1019840173.26973",
		      "windowtxt"=>$build_info_table, 
		      "windowtitle" =>'Build Info Buildname: Linux comet Depend',
		      )."\n<br>\n\n\n\n";
    
    
    $links.= "\t\t".
      HTMLPopUp::Link(
		      "linktxt"=>"<tt>pinkerton</tt>", 
		      "href"=>"http://lounge.mozilla.org/cgi-bin/cgiwrap/cgiwrap_exe/tbox/gunzip.cgi?tree=SeaMonkey&full-log=1019840173.26973",
		      "windowtxt"=>$checkin_table, 
		      "windowtitle" =>'CVS Info Author: pinkerton%netscape.com 04/25&nbsp;14:30 to 04/25&nbsp;14:25 ',

		      "windowheight" => 825,
		      "windowwidth" => 825,

		      )."\n<br>\n\n\n\n\n";
    
    my $table = ("\t<table BORDER=0 CELLPADDING=12 CELLSPACING=0 WIDTH=\"100%\"\n".
		 "\t\t<tr><td align=left>".
		 $links.
		 "</td><td align=right>".
		 $links.
		 "</td></tr></thead>\n".
		 "\t</table>\n");

    my $out;
    
    $out .= HTMLPopUp::page_header('title'=>"Tinderbox Status Page tree: $tree", 
				   'refresh'=>$REFRESH_TIME);
    $out .= "\n\n";
    $out .= "$table\n\n";
    my (@structures) = HTMLPopUp::define_structures();
    $out .= "@structures";
    $out .= "<!-- /Page Footer --><p>\n\n";
    $out .= "</HTML>\n\n";

    overwrite_file($outfile, $out);

}

{

# simple tests to validate the escape routines.

my $string = ' <A  HREF="#1039471635" >12/09&nbsp;17:07</a> ';
my $urlescaped_string = HTMLPopUp::escapeURL($string);
my $htmlescaped_string = HTMLPopUp::escapeHTML($string);
my $url = HTMLPopUp::unescapeURL($urlescaped_string);
my $html = HTMLPopUp::unescapeHTML($htmlescaped_string);

($string eq $url) ||
	 die("escapeURL followed by unescapeURL did not give us original string\n");

($string eq $html) ||
	 die("escapeHTML followed by unescapeHTML did not give us original string\n");


($htmlescaped_string eq ' &lt;A  HREF=&quot;#1039471635&quot; &gt;12/09&amp;nbsp;17:07&lt;/a&gt; ') ||
	 die("escapeHTML did not return expected string\n");


($urlescaped_string eq
'%20%3CA%20%20HREF%3D%22%231039471635%22%20%3E12%2F09%26nbsp%3B17%3A07%3C%2Fa%3E%20')
||
	 die("escapeURL did not return expected string\n");

}

1;
