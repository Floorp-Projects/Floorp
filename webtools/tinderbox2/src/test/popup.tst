#!#perl# --

# generate static html pages for use in testing the popup libraries.


# $Revision: 1.1 $ 
# $Date: 2002/05/01 01:51:51 $ 
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

# define some static tables which look like the kind of popups we
# normally produce.

$build_info_table =<<EOF;

endtime: 04/26&nbsp;09:56<br>
starttime: 04/26&nbsp;09:11<br>
runtime: 45.22 (minutes)<br>
buildstatus: success<br>
buildname: Linux comet Depend<br>
tree: SeaMonkey<br>

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
