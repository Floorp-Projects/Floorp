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

# Table of tag names; it doesn't have to be sorted because code
# below will do it. However, for the sake of ease of additions, keep
# it sorted so that its easy to tell where to add a new tag and that
# the tag hasn't already been added.
$i = 0;
$tags[$i++] = "a";
$tags[$i++] = "abbr";
$tags[$i++] = "acronym";
$tags[$i++] = "address";
$tags[$i++] = "applet";
$tags[$i++] = "area";
$tags[$i++] = "b";
$tags[$i++] = "base";
$tags[$i++] = "basefont";
$tags[$i++] = "bdo";
$tags[$i++] = "bgsound";
$tags[$i++] = "big";
$tags[$i++] = "blink";
$tags[$i++] = "blockquote";
$tags[$i++] = "body";
$tags[$i++] = "br";
$tags[$i++] = "button";
$tags[$i++] = "caption";
$tags[$i++] = "center";
$tags[$i++] = "cite";
$tags[$i++] = "code";
$tags[$i++] = "col";
$tags[$i++] = "colgroup";
$tags[$i++] = "dd";
$tags[$i++] = "del";
$tags[$i++] = "dfn";
$tags[$i++] = "dir";
$tags[$i++] = "div";
$tags[$i++] = "dl";
$tags[$i++] = "dt";
$tags[$i++] = "em";
$tags[$i++] = "embed";
$tags[$i++] = "endnote";
$tags[$i++] = "fieldset";
$tags[$i++] = "font";
$tags[$i++] = "form";
$tags[$i++] = "frame";
$tags[$i++] = "frameset";
$tags[$i++] = "h1";
$tags[$i++] = "h2";
$tags[$i++] = "h3";
$tags[$i++] = "h4";
$tags[$i++] = "h5";
$tags[$i++] = "h6";
$tags[$i++] = "head";
$tags[$i++] = "hr";
$tags[$i++] = "html";
$tags[$i++] = "i";
$tags[$i++] = "iframe";
$tags[$i++] = "ilayer";
$tags[$i++] = "image";
$tags[$i++] = "img";
$tags[$i++] = "input";
$tags[$i++] = "ins";
$tags[$i++] = "isindex";
$tags[$i++] = "kbd";
$tags[$i++] = "keygen";
$tags[$i++] = "label";
$tags[$i++] = "layer";
$tags[$i++] = "legend";
$tags[$i++] = "li";
$tags[$i++] = "link";
$tags[$i++] = "listing";
$tags[$i++] = "map";
$tags[$i++] = "menu";
$tags[$i++] = "meta";
$tags[$i++] = "multicol";
$tags[$i++] = "nobr";
$tags[$i++] = "noembed";
$tags[$i++] = "noframes";
$tags[$i++] = "nolayer";
$tags[$i++] = "noscript";
$tags[$i++] = "object";
$tags[$i++] = "ol";
$tags[$i++] = "optgroup";
$tags[$i++] = "option";
$tags[$i++] = "p";
$tags[$i++] = "param";
$tags[$i++] = "parsererror";
$tags[$i++] = "plaintext";
$tags[$i++] = "pre";
$tags[$i++] = "q";
$tags[$i++] = "s";
$tags[$i++] = "samp";
$tags[$i++] = "script";
$tags[$i++] = "select";
$tags[$i++] = "server";
$tags[$i++] = "small";
$tags[$i++] = "sound";
$tags[$i++] = "sourcetext";
$tags[$i++] = "spacer";
$tags[$i++] = "span";
$tags[$i++] = "strike";
$tags[$i++] = "strong";
$tags[$i++] = "style";
$tags[$i++] = "sub";
$tags[$i++] = "sup";
$tags[$i++] = "table";
$tags[$i++] = "tbody";
$tags[$i++] = "td";
$tags[$i++] = "textarea";
$tags[$i++] = "tfoot";
$tags[$i++] = "th";
$tags[$i++] = "thead";
$tags[$i++] = "title";
$tags[$i++] = "tr";
$tags[$i++] = "tt";
$tags[$i++] = "u";
$tags[$i++] = "ul";
$tags[$i++] = "var";
$tags[$i++] = "wbr";
$tags[$i++] = "xmp";

######################################################################

# These are not tags; rather they are extra values to place into the
# tag enumeration after the normal tags. These do not need to be sorted
# and they do not go into the tag table, just into the tag enumeration.
$extra = 0;
$extra_tags[$extra++] = "text";
$extra_tags[$extra++] = "whitespace";
$extra_tags[$extra++] = "newline";
$extra_tags[$extra++] = "comment";
$extra_tags[$extra++] = "entity";
$extra_tags[$extra++] = "userdefined";
$extra_tags[$extra++] = "secret_h1style";
$extra_tags[$extra++] = "secret_h2style";
$extra_tags[$extra++] = "secret_h3style";
$extra_tags[$extra++] = "secret_h4style";
$extra_tags[$extra++] = "secret_h5style";
$extra_tags[$extra++] = "secret_h6style";

######################################################################

# Sort the tag table before using it
@tags = sort @tags;

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

/* Do not edit - generated by gentags.pl */
END_COPYRIGHT

######################################################################

$file_base = @ARGV[0];

# Generate the header file first
open(HEADER_FILE, ">$file_base.h");

# Print out copyright and do not edit notice
print HEADER_FILE $copyright;
print HEADER_FILE "#ifndef " . $file_base . "_h___\n";
print HEADER_FILE "#define " . $file_base . "_h___\n";

# Print out enum's for the tag symbols
print HEADER_FILE "enum nsHTMLTag {\n";
print HEADER_FILE "  /* this enum must be first and must be zero */\n";
print HEADER_FILE "  eHTMLTag_unknown=0,\n\n";
print HEADER_FILE "  /* begin tag enums */\n  ";
$width = 2;
print HEADER_FILE $str;
for ($j = 0; $j < $i; $j++) {
    $lower = $tags[$j];
    $lower =~ tr/A-Z/a-z/;
    $str = "eHTMLTag_" . $lower . "=" . ($j + 1);
    $str = $str . ", ";
    $len = length($str);
    if ($width + $len > 78) {
	print HEADER_FILE "\n  ";
	$width = 2;
    }
    print HEADER_FILE $str;
    $width = $width + $len;
}
print HEADER_FILE "\n\n  /* The remaining enums are not for tags */\n  ";

# Print out extra enum's that are not in the tag table
$width = 2;
for ($k = 0; $k < $extra; $k++) {
    $lower = $extra_tags[$k];
    $lower =~ tr/A-Z/a-z/;
    $str = "eHTMLTag_" . $lower . "=" . ($j + $k + 1);
    if ($k < $extra - 1) {
	$str = $str . ", ";
    }
    $len = length($str);
    if ($width + $len > 78) {
	print HEADER_FILE "\n  ";
	$width = 2;
    }
    print HEADER_FILE $str;
    $width = $width + $len;
}

print HEADER_FILE "\n};\n#define NS_HTML_TAG_MAX " . $j . "\n\n";
print HEADER_FILE
    "extern nsHTMLTag NS_TagToEnum(const char* aTag);\n";
print HEADER_FILE
    "extern const char* NS_EnumToTag(nsHTMLTag aEnum);\n\n";
print HEADER_FILE "#endif /* " . $file_base . "_h___ */\n";
close(HEADER_FILE);

######################################################################

# Generate the source file
open(CPP_FILE, ">$file_base.cpp");
print CPP_FILE $copyright;
print CPP_FILE "#include \"nsCRT.h\"\n";
print CPP_FILE "#include \"$file_base.h\"\n\n";

# Print out table of tag names
print CPP_FILE "static char* tagTable[] = {\n  ";
$width = 2;
for ($j = 0; $j < $i; $j++) {
    $lower = $tags[$j];
    $lower =~ tr/A-Z/a-z/;
    $str = "\"" . $lower . "\"";
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

# Finally, dump out the search routine that takes a char* and finds it
# in the table.
print CPP_FILE "
nsHTMLTag NS_TagToEnum(const char* aTagName) {
  int low = 0;
  int high = NS_HTML_TAG_MAX - 1;
  while (low <= high) {
    int middle = (low + high) >> 1;
    int result = nsCRT::strcasecmp(aTagName, tagTable[middle]);
    if (result == 0)
      return (nsHTMLTag) (middle + 1);
    if (result < 0)
      high = middle - 1; 
    else
      low = middle + 1; 
  }
  return eHTMLTag_userdefined;
}

const char* NS_EnumToTag(nsHTMLTag aTagID) {
  if ((int(aTagID) <= 0) || (int(aTagID) > NS_HTML_TAG_MAX)) {
    return 0;
  }
  return tagTable[int(aTagID) - 1];
}

#ifdef NS_DEBUG
#include <stdio.h>

class nsTestTagTable {
public:
   nsTestTagTable() {
     const char *tag;
     nsHTMLTag id;

     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_TAG_MAX; i++) {
       tag = tagTable[i];
       id = NS_TagToEnum(tag);
       NS_ASSERTION(id != eHTMLTag_userdefined, \"can't find tag id\");
       const char* check = NS_EnumToTag(id);
       NS_ASSERTION(check == tag, \"can't map id back to tag\");
     }

     // Make sure we don't find things that aren't there
     id = NS_TagToEnum(\"@\");
     NS_ASSERTION(id == eHTMLTag_userdefined, \"found @\");
     id = NS_TagToEnum(\"zzzzz\");
     NS_ASSERTION(id == eHTMLTag_userdefined, \"found zzzzz\");

     tag = NS_EnumToTag((nsHTMLTag) 0);
     NS_ASSERTION(0 == tag, \"found enum 0\");
     tag = NS_EnumToTag((nsHTMLTag) -1);
     NS_ASSERTION(0 == tag, \"found enum -1\");
     tag = NS_EnumToTag((nsHTMLTag) (NS_HTML_TAG_MAX + 1));
     NS_ASSERTION(0 == tag, \"found past max enum\");
   }
};
nsTestTagTable validateTagTable;
#endif

";

close(CPP_FILE);
