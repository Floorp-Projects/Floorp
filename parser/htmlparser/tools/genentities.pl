#! /usr/local/bin/perl

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
# The Original Code is Mozilla Communicator client code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
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

######################################################################

# ISO 8859-1 entities.
# See the HTML4.0 spec for this list in it's DTD form
$i = 0;
$entity[$i++] = "nbsp"; $value{"nbsp"} = "160";
$entity[$i++] = "iexcl"; $value{"iexcl"} = "161";
$entity[$i++] = "cent"; $value{"cent"} = "162";
$entity[$i++] = "pound"; $value{"pound"} = "163";
$entity[$i++] = "curren"; $value{"curren"} = "164";
$entity[$i++] = "yen"; $value{"yen"} = "165";
$entity[$i++] = "brvbar"; $value{"brvbar"} = "166";
$entity[$i++] = "sect"; $value{"sect"} = "167";
$entity[$i++] = "uml"; $value{"uml"} = "168";
$entity[$i++] = "copy"; $value{"copy"} = "169";
$entity[$i++] = "ordf"; $value{"ordf"} = "170";
$entity[$i++] = "laquo"; $value{"laquo"} = "171";
$entity[$i++] = "not"; $value{"not"} = "172";
$entity[$i++] = "shy"; $value{"shy"} = "173";
$entity[$i++] = "reg"; $value{"reg"} = "174";
$entity[$i++] = "macr"; $value{"macr"} = "175";
$entity[$i++] = "deg"; $value{"deg"} = "176";
$entity[$i++] = "plusmn"; $value{"plusmn"} = "177";
$entity[$i++] = "sup2"; $value{"sup2"} = "178";
$entity[$i++] = "sup3"; $value{"sup3"} = "179";
$entity[$i++] = "acute"; $value{"acute"} = "180";
$entity[$i++] = "micro"; $value{"micro"} = "181";
$entity[$i++] = "para"; $value{"para"} = "182";
$entity[$i++] = "middot"; $value{"middot"} = "183";
$entity[$i++] = "cedil"; $value{"cedil"} = "184";
$entity[$i++] = "sup1"; $value{"sup1"} = "185";
$entity[$i++] = "ordm"; $value{"ordm"} = "186";
$entity[$i++] = "raquo"; $value{"raquo"} = "187";
$entity[$i++] = "frac14"; $value{"frac14"} = "188";
$entity[$i++] = "frac12"; $value{"frac12"} = "189";
$entity[$i++] = "frac34"; $value{"frac34"} = "190";
$entity[$i++] = "iquest"; $value{"iquest"} = "191";
$entity[$i++] = "Agrave"; $value{"Agrave"} = "192";
$entity[$i++] = "Aacute"; $value{"Aacute"} = "193";
$entity[$i++] = "Acirc"; $value{"Acirc"} = "194";
$entity[$i++] = "Atilde"; $value{"Atilde"} = "195";
$entity[$i++] = "Auml"; $value{"Auml"} = "196";
$entity[$i++] = "Aring"; $value{"Aring"} = "197";
$entity[$i++] = "AElig"; $value{"AElig"} = "198";
$entity[$i++] = "Ccedil"; $value{"Ccedil"} = "199";
$entity[$i++] = "Egrave"; $value{"Egrave"} = "200";
$entity[$i++] = "Eacute"; $value{"Eacute"} = "201";
$entity[$i++] = "Ecirc"; $value{"Ecirc"} = "202";
$entity[$i++] = "Euml"; $value{"Euml"} = "203";
$entity[$i++] = "Igrave"; $value{"Igrave"} = "204";
$entity[$i++] = "Iacute"; $value{"Iacute"} = "205";
$entity[$i++] = "Icirc"; $value{"Icirc"} = "206";
$entity[$i++] = "Iuml"; $value{"Iuml"} = "207";
$entity[$i++] = "ETH"; $value{"ETH"} = "208";
$entity[$i++] = "Ntilde"; $value{"Ntilde"} = "209";
$entity[$i++] = "Ograve"; $value{"Ograve"} = "210";
$entity[$i++] = "Oacute"; $value{"Oacute"} = "211";
$entity[$i++] = "Ocirc"; $value{"Ocirc"} = "212";
$entity[$i++] = "Otilde"; $value{"Otilde"} = "213";
$entity[$i++] = "Ouml"; $value{"Ouml"} = "214";
$entity[$i++] = "times"; $value{"times"} = "215";
$entity[$i++] = "Oslash"; $value{"Oslash"} = "216";
$entity[$i++] = "Ugrave"; $value{"Ugrave"} = "217";
$entity[$i++] = "Uacute"; $value{"Uacute"} = "218";
$entity[$i++] = "Ucirc"; $value{"Ucirc"} = "219";
$entity[$i++] = "Uuml"; $value{"Uuml"} = "220";
$entity[$i++] = "Yacute"; $value{"Yacute"} = "221";
$entity[$i++] = "THORN"; $value{"THORN"} = "222";
$entity[$i++] = "szlig"; $value{"szlig"} = "223";
$entity[$i++] = "agrave"; $value{"agrave"} = "224";
$entity[$i++] = "aacute"; $value{"aacute"} = "225";
$entity[$i++] = "acirc"; $value{"acirc"} = "226";
$entity[$i++] = "atilde"; $value{"atilde"} = "227";
$entity[$i++] = "auml"; $value{"auml"} = "228";
$entity[$i++] = "aring"; $value{"aring"} = "229";
$entity[$i++] = "aelig"; $value{"aelig"} = "230";
$entity[$i++] = "ccedil"; $value{"ccedil"} = "231";
$entity[$i++] = "egrave"; $value{"egrave"} = "232";
$entity[$i++] = "eacute"; $value{"eacute"} = "233";
$entity[$i++] = "ecirc"; $value{"ecirc"} = "234";
$entity[$i++] = "euml"; $value{"euml"} = "235";
$entity[$i++] = "igrave"; $value{"igrave"} = "236";
$entity[$i++] = "iacute"; $value{"iacute"} = "237";
$entity[$i++] = "icirc"; $value{"icirc"} = "238";
$entity[$i++] = "iuml"; $value{"iuml"} = "239";
$entity[$i++] = "eth"; $value{"eth"} = "240";
$entity[$i++] = "ntilde"; $value{"ntilde"} = "241";
$entity[$i++] = "ograve"; $value{"ograve"} = "242";
$entity[$i++] = "oacute"; $value{"oacute"} = "243";
$entity[$i++] = "ocirc"; $value{"ocirc"} = "244";
$entity[$i++] = "otilde"; $value{"otilde"} = "245";
$entity[$i++] = "ouml"; $value{"ouml"} = "246";
$entity[$i++] = "divide"; $value{"divide"} = "247";
$entity[$i++] = "oslash"; $value{"oslash"} = "248";
$entity[$i++] = "ugrave"; $value{"ugrave"} = "249";
$entity[$i++] = "uacute"; $value{"uacute"} = "250";
$entity[$i++] = "ucirc"; $value{"ucirc"} = "251";
$entity[$i++] = "uuml"; $value{"uuml"} = "252";
$entity[$i++] = "yacute"; $value{"yacute"} = "253";
$entity[$i++] = "thorn"; $value{"thorn"} = "254";
$entity[$i++] = "yuml"; $value{"yuml"} = "255";

# Symbols, mathematical symbols and Greek letters
# See the HTML4.0 spec for this list in it's DTD form
$entity[$i++] = "fnof"; $value{"fnof"} = "402";
$entity[$i++] = "Alpha"; $value{"Alpha"} = "913";
$entity[$i++] = "Beta"; $value{"Beta"} = "914";
$entity[$i++] = "Gamma"; $value{"Gamma"} = "915";
$entity[$i++] = "Delta"; $value{"Delta"} = "916";
$entity[$i++] = "Epsilon"; $value{"Epsilon"} = "917";
$entity[$i++] = "Zeta"; $value{"Zeta"} = "918";
$entity[$i++] = "Eta"; $value{"Eta"} = "919";
$entity[$i++] = "Theta"; $value{"Theta"} = "920";
$entity[$i++] = "Iota"; $value{"Iota"} = "921";
$entity[$i++] = "Kappa"; $value{"Kappa"} = "922";
$entity[$i++] = "Lambda"; $value{"Lambda"} = "923";
$entity[$i++] = "Mu"; $value{"Mu"} = "924";
$entity[$i++] = "Nu"; $value{"Nu"} = "925";
$entity[$i++] = "Xi"; $value{"Xi"} = "926";
$entity[$i++] = "Omicron"; $value{"Omicron"} = "927";
$entity[$i++] = "Pi"; $value{"Pi"} = "928";
$entity[$i++] = "Rho"; $value{"Rho"} = "929";
$entity[$i++] = "Sigma"; $value{"Sigma"} = "931";
$entity[$i++] = "Tau"; $value{"Tau"} = "932";
$entity[$i++] = "Upsilon"; $value{"Upsilon"} = "933";
$entity[$i++] = "Phi"; $value{"Phi"} = "934";
$entity[$i++] = "Chi"; $value{"Chi"} = "935";
$entity[$i++] = "Psi"; $value{"Psi"} = "936";
$entity[$i++] = "Omega"; $value{"Omega"} = "937";
$entity[$i++] = "alpha"; $value{"alpha"} = "945";
$entity[$i++] = "beta"; $value{"beta"} = "946";
$entity[$i++] = "gamma"; $value{"gamma"} = "947";
$entity[$i++] = "delta"; $value{"delta"} = "948";
$entity[$i++] = "epsilon"; $value{"epsilon"} = "949";
$entity[$i++] = "zeta"; $value{"zeta"} = "950";
$entity[$i++] = "eta"; $value{"eta"} = "951";
$entity[$i++] = "theta"; $value{"theta"} = "952";
$entity[$i++] = "iota"; $value{"iota"} = "953";
$entity[$i++] = "kappa"; $value{"kappa"} = "954";
$entity[$i++] = "lambda"; $value{"lambda"} = "955";
$entity[$i++] = "mu"; $value{"mu"} = "956";
$entity[$i++] = "nu"; $value{"nu"} = "957";
$entity[$i++] = "xi"; $value{"xi"} = "958";
$entity[$i++] = "omicron"; $value{"omicron"} = "959";
$entity[$i++] = "pi"; $value{"pi"} = "960";
$entity[$i++] = "rho"; $value{"rho"} = "961";
$entity[$i++] = "sigmaf"; $value{"sigmaf"} = "962";
$entity[$i++] = "sigma"; $value{"sigma"} = "963";
$entity[$i++] = "tau"; $value{"tau"} = "964";
$entity[$i++] = "upsilon"; $value{"upsilon"} = "965";
$entity[$i++] = "phi"; $value{"phi"} = "966";
$entity[$i++] = "chi"; $value{"chi"} = "967";
$entity[$i++] = "psi"; $value{"psi"} = "968";
$entity[$i++] = "omega"; $value{"omega"} = "969";
$entity[$i++] = "thetasym"; $value{"thetasym"} = "977";
$entity[$i++] = "upsih"; $value{"upsih"} = "978";
$entity[$i++] = "piv"; $value{"piv"} = "982";
$entity[$i++] = "bull"; $value{"bull"} = "8226";
$entity[$i++] = "hellip"; $value{"hellip"} = "8230";
$entity[$i++] = "prime"; $value{"prime"} = "8242";
$entity[$i++] = "Prime"; $value{"Prime"} = "8243";
$entity[$i++] = "oline"; $value{"oline"} = "8254";
$entity[$i++] = "frasl"; $value{"frasl"} = "8260";
$entity[$i++] = "weierp"; $value{"weierp"} = "8472";
$entity[$i++] = "image"; $value{"image"} = "8465";
$entity[$i++] = "real"; $value{"real"} = "8476";
$entity[$i++] = "trade"; $value{"trade"} = "8482";
$entity[$i++] = "alefsym"; $value{"alefsym"} = "8501";
$entity[$i++] = "larr"; $value{"larr"} = "8592";
$entity[$i++] = "uarr"; $value{"uarr"} = "8593";
$entity[$i++] = "rarr"; $value{"rarr"} = "8594";
$entity[$i++] = "darr"; $value{"darr"} = "8595";
$entity[$i++] = "harr"; $value{"harr"} = "8596";
$entity[$i++] = "crarr"; $value{"crarr"} = "8629";
$entity[$i++] = "lArr"; $value{"lArr"} = "8656";
$entity[$i++] = "uArr"; $value{"uArr"} = "8657";
$entity[$i++] = "rArr"; $value{"rArr"} = "8658";
$entity[$i++] = "dArr"; $value{"dArr"} = "8659";
$entity[$i++] = "hArr"; $value{"hArr"} = "8660";
$entity[$i++] = "forall"; $value{"forall"} = "8704";
$entity[$i++] = "part"; $value{"part"} = "8706";
$entity[$i++] = "exist"; $value{"exist"} = "8707";
$entity[$i++] = "empty"; $value{"empty"} = "8709";
$entity[$i++] = "nabla"; $value{"nabla"} = "8711";
$entity[$i++] = "isin"; $value{"isin"} = "8712";
$entity[$i++] = "notin"; $value{"notin"} = "8713";
$entity[$i++] = "ni"; $value{"ni"} = "8715";
$entity[$i++] = "prod"; $value{"prod"} = "8719";
$entity[$i++] = "sum"; $value{"sum"} = "8721";
$entity[$i++] = "minus"; $value{"minus"} = "8722";
$entity[$i++] = "lowast"; $value{"lowast"} = "8727";
$entity[$i++] = "radic"; $value{"radic"} = "8730";
$entity[$i++] = "prop"; $value{"prop"} = "8733";
$entity[$i++] = "infin"; $value{"infin"} = "8734";
$entity[$i++] = "ang"; $value{"ang"} = "8736";
$entity[$i++] = "and"; $value{"and"} = "8743";
$entity[$i++] = "or"; $value{"or"} = "8744";
$entity[$i++] = "cap"; $value{"cap"} = "8745";
$entity[$i++] = "cup"; $value{"cup"} = "8746";
$entity[$i++] = "int"; $value{"int"} = "8747";
$entity[$i++] = "there4"; $value{"there4"} = "8756";
$entity[$i++] = "sim"; $value{"sim"} = "8764";
$entity[$i++] = "cong"; $value{"cong"} = "8773";
$entity[$i++] = "asymp"; $value{"asymp"} = "8776";
$entity[$i++] = "ne"; $value{"ne"} = "8800";
$entity[$i++] = "equiv"; $value{"equiv"} = "8801";
$entity[$i++] = "le"; $value{"le"} = "8804";
$entity[$i++] = "ge"; $value{"ge"} = "8805";
$entity[$i++] = "sub"; $value{"sub"} = "8834";
$entity[$i++] = "sup"; $value{"sup"} = "8835";
$entity[$i++] = "nsub"; $value{"nsub"} = "8836";
$entity[$i++] = "sube"; $value{"sube"} = "8838";
$entity[$i++] = "supe"; $value{"supe"} = "8839";
$entity[$i++] = "oplus"; $value{"oplus"} = "8853";
$entity[$i++] = "otimes"; $value{"otimes"} = "8855";
$entity[$i++] = "perp"; $value{"perp"} = "8869";
$entity[$i++] = "sdot"; $value{"sdot"} = "8901";
$entity[$i++] = "lceil"; $value{"lceil"} = "8968";
$entity[$i++] = "rceil"; $value{"rceil"} = "8969";
$entity[$i++] = "lfloor"; $value{"lfloor"} = "8970";
$entity[$i++] = "rfloor"; $value{"rfloor"} = "8971";
$entity[$i++] = "lang"; $value{"lang"} = "9001";
$entity[$i++] = "rang"; $value{"rang"} = "9002";
$entity[$i++] = "loz"; $value{"loz"} = "9674";
$entity[$i++] = "spades"; $value{"spades"} = "9824";
$entity[$i++] = "clubs"; $value{"clubs"} = "9827";
$entity[$i++] = "hearts"; $value{"hearts"} = "9829";
$entity[$i++] = "diams"; $value{"diams"} = "9830";

# Markup-significant and internationalization characters
# See the HTML4.0 spec for this list in it's DTD form
$entity[$i++] = "quot"; $value{"quot"} = "34";
$entity[$i++] = "amp"; $value{"amp"} = "38";
$entity[$i++] = "lt"; $value{"lt"} = "60";
$entity[$i++] = "gt"; $value{"gt"} = "62";
$entity[$i++] = "OElig"; $value{"OElig"} = "338";
$entity[$i++] = "oelig"; $value{"oelig"} = "339";
$entity[$i++] = "Scaron"; $value{"Scaron"} = "352";
$entity[$i++] = "scaron"; $value{"scaron"} = "353";
$entity[$i++] = "Yuml"; $value{"Yuml"} = "376";
$entity[$i++] = "circ"; $value{"circ"} = "710";
$entity[$i++] = "tilde"; $value{"tilde"} = "732";
$entity[$i++] = "ensp"; $value{"ensp"} = "8194";
$entity[$i++] = "emsp"; $value{"emsp"} = "8195";
$entity[$i++] = "thinsp"; $value{"thinsp"} = "8201";
$entity[$i++] = "zwnj"; $value{"zwnj"} = "8204";
$entity[$i++] = "zwj"; $value{"zwj"} = "8205";
$entity[$i++] = "lrm"; $value{"lrm"} = "8206";
$entity[$i++] = "rlm"; $value{"rlm"} = "8207";
$entity[$i++] = "ndash"; $value{"ndash"} = "8211";
$entity[$i++] = "mdash"; $value{"mdash"} = "8212";
$entity[$i++] = "lsquo"; $value{"lsquo"} = "8216";
$entity[$i++] = "rsquo"; $value{"rsquo"} = "8217";
$entity[$i++] = "sbquo"; $value{"sbquo"} = "8218";
$entity[$i++] = "ldquo"; $value{"ldquo"} = "8220";
$entity[$i++] = "rdquo"; $value{"rdquo"} = "8221";
$entity[$i++] = "bdquo"; $value{"bdquo"} = "8222";
$entity[$i++] = "dagger"; $value{"dagger"} = "8224";
$entity[$i++] = "Dagger"; $value{"Dagger"} = "8225";
$entity[$i++] = "permil"; $value{"permil"} = "8240";
$entity[$i++] = "lsaquo"; $value{"lsaquo"} = "8249";
$entity[$i++] = "rsaquo"; $value{"rsaquo"} = "8250";
$entity[$i++] = "euro"; $value{"euro"} = "8364";

# Navigator entity extensions
$entity[$i++] = "AMP"; $value{"AMP"} = "38";
$entity[$i++] = "COPY"; $value{"COPY"} = "169";
$entity[$i++] = "GT"; $value{"GT"} = "62";
$entity[$i++] = "LT"; $value{"LT"} = "60";
$entity[$i++] = "QUOT"; $value{"QUOT"} = "34";
$entity[$i++] = "REG"; $value{"REG"} = "174";

######################################################################

# Sort the entity table before using it
@entity = sort @entity;

$copyright = <<END_COPYRIGHT;
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Do not edit - generated by genentities.pl */
END_COPYRIGHT

######################################################################

$file_base = @ARGV[0];

# Generate the source file
open(CPP_FILE, ">$file_base.cpp");
print CPP_FILE $copyright;
print CPP_FILE "#include \"nsCRT.h\"\n";
print CPP_FILE "#include \"" . $file_base . ".h\"\n\n";

# Print out table of tag names
print CPP_FILE "static struct { char* mEntity; PRInt32 mValue; } entityTable[$i] = {\n  ";
$width = 2;
for ($j = 0; $j < $i; $j++) {
    $key = $entity[$j];
    $val = $value{$key};
    $str = "{ \"" . $key . "\", " . $val . " }";
    if ($j < $i - 1) {
	$str = $str . ", ";
    }
    $len = length($str);
    if ($width + $len > 78) {
	print CPP_FILE "\n  ";
	$width = 2;
    } 
    print CPP_FILE $str;
    $width = $width + $len;
}
print CPP_FILE "\n};\n";
print CPP_FILE "#define NS_HTML_ENTITY_MAX " . $i . "\n";

# Finally, dump out the search routine that takes a char* and finds it
# in the table.
print CPP_FILE "
PRInt32 NS_EntityToUnicode(const char* aEntity) {
  int low = 0;
  int high = NS_HTML_ENTITY_MAX - 1;
  while (low <= high) {
    int middle = (low + high) >> 1;
    int result = nsCRT::strcmp(aEntity, entityTable[middle].mEntity);
    if (result == 0)
      return entityTable[middle].mValue;
    if (result < 0)
      high = middle - 1; 
    else
      low = middle + 1; 
  }
  return -1;
}

// XXX - WARNING, slow, we should have
// a much faster routine instead of scanning
// the entire list
const char* NS_UnicodeToEntity(PRInt32 aCode)
{
  for (PRInt32 i = 0; i < NS_HTML_ENTITY_MAX; i++)
  {
    if (entityTable[i].mValue == aCode)
      return entityTable[i].mEntity;
  }
  return nsnull;
}


#ifdef NS_DEBUG
#include <stdio.h>

class nsTestEntityTable {
public:
   nsTestEntityTable() {
     const char *entity;
     PRInt32 value;

     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_ENTITY_MAX; i++) {
       entity = entityTable[i].mEntity;
       value = NS_EntityToUnicode(entity);
       NS_ASSERTION(value != -1, \"can't find entity\");
     }

     // Make sure we don't find things that aren't there
     value = NS_EntityToUnicode(\"@\");
     NS_ASSERTION(value == -1, \"found @\");
     value = NS_EntityToUnicode(\"zzzzz\");
     NS_ASSERTION(value == -1, \"found zzzzz\");
   }
};
nsTestEntityTable validateEntityTable;
#endif

";

close(CPP_FILE);
