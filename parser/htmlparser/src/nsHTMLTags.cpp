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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsHTMLTags.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "plhash.h"
#include "nsString.h"
#include "nsStaticAtom.h"

// C++ sucks! There's no way to do this with a macro, at least not
// that I know, if you know how to do this with a macro then please do
// so...
static const PRUnichar sHTMLTagUnicodeName_a[] =
  {'a', '\0'};
static const PRUnichar sHTMLTagUnicodeName_abbr[] =
  {'a', 'b', 'b', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_acronym[] =
  {'a', 'c', 'r', 'o', 'n', 'y', 'm', '\0'};
static const PRUnichar sHTMLTagUnicodeName_address[] =
  {'a', 'd', 'd', 'r', 'e', 's', 's', '\0'};
static const PRUnichar sHTMLTagUnicodeName_applet[] =
  {'a', 'p', 'p', 'l', 'e', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_area[] =
  {'a', 'r', 'e', 'a', '\0'};
static const PRUnichar sHTMLTagUnicodeName_b[] =
  {'b', '\0'};
static const PRUnichar sHTMLTagUnicodeName_base[] =
  {'b', 'a', 's', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_basefont[] =
  {'b', 'a', 's', 'e', 'f', 'o', 'n', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_bdo[] =
  {'b', 'd', 'o', '\0'};
static const PRUnichar sHTMLTagUnicodeName_bgsound[] =
  {'b', 'g', 's', 'o', 'u', 'n', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_big[] =
  {'b', 'i', 'g', '\0'};
static const PRUnichar sHTMLTagUnicodeName_blink[] =
  {'b', 'l', 'i', 'n', 'k', '\0'};
static const PRUnichar sHTMLTagUnicodeName_blockquote[] =
  {'b', 'l', 'o', 'c', 'k', 'q', 'u', 'o', 't', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_body[] =
  {'b', 'o', 'd', 'y', '\0'};
static const PRUnichar sHTMLTagUnicodeName_br[] =
  {'b', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_button[] =
  {'b', 'u', 't', 't', 'o', 'n', '\0'};
static const PRUnichar sHTMLTagUnicodeName_caption[] =
  {'c', 'a', 'p', 't', 'i', 'o', 'n', '\0'};
static const PRUnichar sHTMLTagUnicodeName_center[] =
  {'c', 'e', 'n', 't', 'e', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_cite[] =
  {'c', 'i', 't', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_code[] =
  {'c', 'o', 'd', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_col[] =
  {'c', 'o', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_colgroup[] =
  {'c', 'o', 'l', 'g', 'r', 'o', 'u', 'p', '\0'};
static const PRUnichar sHTMLTagUnicodeName_counter[] =
  {'c', 'o', 'u', 'n', 't', 'e', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_dd[] =
  {'d', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_del[] =
  {'d', 'e', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_dfn[] =
  {'d', 'f', 'n', '\0'};
static const PRUnichar sHTMLTagUnicodeName_dir[] =
  {'d', 'i', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_div[] =
  {'d', 'i', 'v', '\0'};
static const PRUnichar sHTMLTagUnicodeName_dl[] =
  {'d', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_dt[] =
  {'d', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_em[] =
  {'e', 'm', '\0'};
static const PRUnichar sHTMLTagUnicodeName_embed[] =
  {'e', 'm', 'b', 'e', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_endnote[] =
  {'e', 'n', 'd', 'n', 'o', 't', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_fieldset[] =
  {'f', 'i', 'e', 'l', 'd', 's', 'e', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_font[] =
  {'f', 'o', 'n', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_form[] =
  {'f', 'o', 'r', 'm', '\0'};
static const PRUnichar sHTMLTagUnicodeName_frame[] =
  {'f', 'r', 'a', 'm', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_frameset[] =
  {'f', 'r', 'a', 'm', 'e', 's', 'e', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_h1[] =
  {'h', '1', '\0'};
static const PRUnichar sHTMLTagUnicodeName_h2[] =
  {'h', '2', '\0'};
static const PRUnichar sHTMLTagUnicodeName_h3[] =
  {'h', '3', '\0'};
static const PRUnichar sHTMLTagUnicodeName_h4[] =
  {'h', '4', '\0'};
static const PRUnichar sHTMLTagUnicodeName_h5[] =
  {'h', '5', '\0'};
static const PRUnichar sHTMLTagUnicodeName_h6[] =
  {'h', '6', '\0'};
static const PRUnichar sHTMLTagUnicodeName_head[] =
  {'h', 'e', 'a', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_hr[] =
  {'h', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_html[] =
  {'h', 't', 'm', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_i[] =
  {'i', '\0'};
static const PRUnichar sHTMLTagUnicodeName_iframe[] =
  {'i', 'f', 'r', 'a', 'm', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_image[] =
  {'i', 'm', 'a', 'g', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_img[] =
  {'i', 'm', 'g', '\0'};
static const PRUnichar sHTMLTagUnicodeName_input[] =
  {'i', 'n', 'p', 'u', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_ins[] =
  {'i', 'n', 's', '\0'};
static const PRUnichar sHTMLTagUnicodeName_isindex[] =
  {'i', 's', 'i', 'n', 'd', 'e', 'x', '\0'};
static const PRUnichar sHTMLTagUnicodeName_kbd[] =
  {'k', 'b', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_keygen[] =
  {'k', 'e', 'y', 'g', 'e', 'n', '\0'};
static const PRUnichar sHTMLTagUnicodeName_label[] =
  {'l', 'a', 'b', 'e', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_legend[] =
  {'l', 'e', 'g', 'e', 'n', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_li[] =
  {'l', 'i', '\0'};
static const PRUnichar sHTMLTagUnicodeName_link[] =
  {'l', 'i', 'n', 'k', '\0'};
static const PRUnichar sHTMLTagUnicodeName_listing[] =
  {'l', 'i', 's', 't', 'i', 'n', 'g', '\0'};
static const PRUnichar sHTMLTagUnicodeName_map[] =
  {'m', 'a', 'p', '\0'};
static const PRUnichar sHTMLTagUnicodeName_marquee[] =
  {'m', 'a', 'r', 'q', 'u', 'e', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_menu[] =
  {'m', 'e', 'n', 'u', '\0'};
static const PRUnichar sHTMLTagUnicodeName_meta[] =
  {'m', 'e', 't', 'a', '\0'};
static const PRUnichar sHTMLTagUnicodeName_multicol[] =
  {'m', 'u', 'l', 't', 'i', 'c', 'o', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_nobr[] =
  {'n', 'o', 'b', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_noembed[] =
  {'n', 'o', 'e', 'm', 'b', 'e', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_noframes[] =
  {'n', 'o', 'f', 'r', 'a', 'm', 'e', 's', '\0'};
static const PRUnichar sHTMLTagUnicodeName_noscript[] =
  {'n', 'o', 's', 'c', 'r', 'i', 'p', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_object[] =
  {'o', 'b', 'j', 'e', 'c', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_ol[] =
  {'o', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_optgroup[] =
  {'o', 'p', 't', 'g', 'r', 'o', 'u', 'p', '\0'};
static const PRUnichar sHTMLTagUnicodeName_option[] =
  {'o', 'p', 't', 'i', 'o', 'n', '\0'};
static const PRUnichar sHTMLTagUnicodeName_p[] =
  {'p', '\0'};
static const PRUnichar sHTMLTagUnicodeName_param[] =
  {'p', 'a', 'r', 'a', 'm', '\0'};
static const PRUnichar sHTMLTagUnicodeName_pre[] =
  {'p', 'r', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_q[] =
  {'q', '\0'};
static const PRUnichar sHTMLTagUnicodeName_s[] =
  {'s', '\0'};
static const PRUnichar sHTMLTagUnicodeName_samp[] =
  {'s', 'a', 'm', 'p', '\0'};
static const PRUnichar sHTMLTagUnicodeName_script[] =
  {'s', 'c', 'r', 'i', 'p', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_select[] =
  {'s', 'e', 'l', 'e', 'c', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_server[] =
  {'s', 'e', 'r', 'v', 'e', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_small[] =
  {'s', 'm', 'a', 'l', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_sound[] =
  {'s', 'o', 'u', 'n', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_spacer[] =
  {'s', 'p', 'a', 'c', 'e', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_span[] =
  {'s', 'p', 'a', 'n', '\0'};
static const PRUnichar sHTMLTagUnicodeName_strike[] =
  {'s', 't', 'r', 'i', 'k', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_strong[] =
  {'s', 't', 'r', 'o', 'n', 'g', '\0'};
static const PRUnichar sHTMLTagUnicodeName_style[] =
  {'s', 't', 'y', 'l', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_sub[] =
  {'s', 'u', 'b', '\0'};
static const PRUnichar sHTMLTagUnicodeName_sup[] =
  {'s', 'u', 'p', '\0'};
static const PRUnichar sHTMLTagUnicodeName_table[] =
  {'t', 'a', 'b', 'l', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_tbody[] =
  {'t', 'b', 'o', 'd', 'y', '\0'};
static const PRUnichar sHTMLTagUnicodeName_td[] =
  {'t', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_textarea[] =
  {'t', 'e', 'x', 't', 'a', 'r', 'e', 'a', '\0'};
static const PRUnichar sHTMLTagUnicodeName_tfoot[] =
  {'t', 'f', 'o', 'o', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_th[] =
  {'t', 'h', '\0'};
static const PRUnichar sHTMLTagUnicodeName_thead[] =
  {'t', 'h', 'e', 'a', 'd', '\0'};
static const PRUnichar sHTMLTagUnicodeName_title[] =
  {'t', 'i', 't', 'l', 'e', '\0'};
static const PRUnichar sHTMLTagUnicodeName_tr[] =
  {'t', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_tt[] =
  {'t', 't', '\0'};
static const PRUnichar sHTMLTagUnicodeName_u[] =
  {'u', '\0'};
static const PRUnichar sHTMLTagUnicodeName_ul[] =
  {'u', 'l', '\0'};
static const PRUnichar sHTMLTagUnicodeName_var[] =
  {'v', 'a', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_wbr[] =
  {'w', 'b', 'r', '\0'};
static const PRUnichar sHTMLTagUnicodeName_xmp[] =
  {'x', 'm', 'p', '\0'};

// static array of unicode tag names
#define HTML_TAG(_tag, _classname) sHTMLTagUnicodeName_##_tag,
#define HTML_OTHER(_tag, _classname)
static const PRUnichar* const kTagUnicodeTable[] = {
#include "nsHTMLTagList.h"
};
#undef HTML_TAG

// static array of tag atoms
static nsIAtom* kTagAtomTable[eHTMLTag_userdefined - 1];

// static array of tag StaticAtom structs
#define HTML_TAG(_tag, _classname) { #_tag, &kTagAtomTable[eHTMLTag_##_tag - 1] },
static const nsStaticAtom kTagAtoms_info[] = {
#include "nsHTMLTagList.h"
};
#undef HTML_TAG
#undef HTML_OTHER

static PRInt32 gTableRefCount;
static PLHashTable* gTagTable;


PR_STATIC_CALLBACK(PLHashNumber)
HTMLTagsHashCodeUCPtr(const void *key)
{
  const PRUnichar *str = (const PRUnichar *)key;

  return nsCRT::HashCode(str);
}

PR_STATIC_CALLBACK(PRIntn)
HTMLTagsKeyCompareUCPtr(const void *key1, const void *key2)
{
  const PRUnichar *str1 = (const PRUnichar *)key1;
  const PRUnichar *str2 = (const PRUnichar *)key2;

  return nsCRT::strcmp(str1, str2) == 0;
}


static PRUint32 sMaxTagNameLength;
#define NS_HTMLTAG_NAME_MAX_LENGTH 10

// static
nsresult
nsHTMLTags::AddRefTable(void)
{
  if (gTableRefCount++ == 0) {
    NS_ASSERTION(!gTagTable, "pre existing hash!");

    gTagTable = PL_NewHashTable(64, HTMLTagsHashCodeUCPtr,
                                HTMLTagsKeyCompareUCPtr, PL_CompareValues,
                                nsnull, nsnull);
    NS_ENSURE_TRUE(gTagTable, NS_ERROR_OUT_OF_MEMORY);

    // Fill in gTagTable with the above static PRUnichar strings as
    // keys and the value of the corresponding enum as the value in
    // the table.

    PRInt32 i;
    for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
      PRUint32 len = nsCRT::strlen(kTagUnicodeTable[i]);

      PL_HashTableAdd(gTagTable, kTagUnicodeTable[i],
                      NS_INT32_TO_PTR(i + 1));

      if (len > sMaxTagNameLength) {
        sMaxTagNameLength = len;
      }
    }

    NS_ASSERTION(sMaxTagNameLength == NS_HTMLTAG_NAME_MAX_LENGTH,
                 "NS_HTMLTAG_NAME_MAX_LENGTH not set correctly!");

    // Fill in our static atom pointers
    NS_RegisterStaticAtoms(kTagAtoms_info, NS_ARRAY_LENGTH(kTagAtoms_info));

#ifdef DEBUG
    {
      // let's verify that all names in the the table are lowercase...
      for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
        nsCAutoString temp1(kTagAtoms_info[i].mString);
        nsCAutoString temp2(kTagAtoms_info[i].mString);
        ToLowerCase(temp1);
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
      }

      // let's verify that all names in the unicode strings above are
      // correct.
      for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
        nsAutoString temp1(kTagUnicodeTable[i]);
        nsAutoString temp2; temp2.AssignWithConversion(kTagAtoms_info[i].mString);
        NS_ASSERTION(temp1.Equals(temp2), "Bad unicode tag name!");
      }
    }
#endif
  }

  return NS_OK;
}

// static
void
nsHTMLTags::ReleaseTable(void)
{
  if (0 == --gTableRefCount) {
    if (gTagTable) {
      // Nothing to delete/free in this table, just destroy the table.

      PL_HashTableDestroy(gTagTable);

      gTagTable = nsnull;
    }
  }
}

// static
nsHTMLTag
nsHTMLTags::CaseSensitiveLookupTag(const PRUnichar* aTagName)
{
  NS_ASSERTION(gTagTable, "no lookup table, needs addref");
  NS_ASSERTION(aTagName, "null tagname!");

  PRUint32 tag = NS_PTR_TO_INT32(PL_HashTableLookupConst(gTagTable, aTagName));

  return (nsHTMLTag)tag;
}

// static
nsHTMLTag
nsHTMLTags::LookupTag(const nsAString& aTagName)
{
  PRUint32 length = aTagName.Length();

  if (length > sMaxTagNameLength) {
    return eHTMLTag_userdefined;
  }

  static PRUnichar buf[NS_HTMLTAG_NAME_MAX_LENGTH + 1];

  nsAString::const_iterator iter;
  PRUint32 i = 0;
  PRUnichar c;

  aTagName.BeginReading(iter);

  // Fast lowercasing-while-copying of ASCII characters into a
  // PRUnichar buffer

  while (i < length) {
    c = *iter;

    if (c <= 'Z' && c >= 'A') {
      c |= 0x20; // Lowercase the ASCII character.
    }

    buf[i] = c; // Copy ASCII character.

    ++i;
    ++iter;
  }

  buf[i] = 0;

  nsHTMLTag tag = CaseSensitiveLookupTag(buf);

  // hack: this can come out when rickg provides a way for the editor to ask
  // CanContain() questions without having to first fetch the parsers
  // internal enum values for a tag name.

  // Hmm, this hack would be faster if we'd put these strings in the
  // hash table. But maybe it's not worth it...

  if (tag == eHTMLTag_unknown) {
    // "__moz_text"
    static const PRUnichar moz_text[] =
      {'_', '_', 'm', 'o', 'z', '_', 't', 'e', 'x', 't', PRUnichar(0) };

    // "#text"
    static const PRUnichar text[] =
      {'#', 't', 'e', 'x', 't', PRUnichar(0) };

    if (nsCRT::strcmp(buf, moz_text) == 0) {
      tag = eHTMLTag_text;
    } else if (nsCRT::strcmp(buf, text) == 0) {
      tag = eHTMLTag_text;
    } else {
      tag = eHTMLTag_userdefined;
    }
  }

  return tag;
}

// static
const PRUnichar *
nsHTMLTags::GetStringValue(nsHTMLTag aEnum)
{
  if (aEnum <= eHTMLTag_unknown || aEnum > NS_HTML_TAG_MAX) {
    return nsnull;
  }

  return kTagUnicodeTable[aEnum - 1];
}

// static
nsIAtom *
nsHTMLTags::GetAtom(nsHTMLTag aEnum)
{
  if (aEnum <= eHTMLTag_unknown || aEnum > NS_HTML_TAG_MAX) {
    return nsnull;
  }

  return kTagAtomTable[aEnum - 1];
}


#ifdef NS_DEBUG

// tag table verification class.

class nsTestTagTable {
public:
   nsTestTagTable() {
     const PRUnichar *tag;
     nsHTMLTag id;

     nsHTMLTags::AddRefTable();
     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_TAG_MAX; ++i) {
       tag = kTagUnicodeTable[i];
       id = nsHTMLTags::LookupTag(nsDependentString(tag));
       NS_ASSERTION(id != eHTMLTag_userdefined, "can't find tag id");
       const PRUnichar* check = nsHTMLTags::GetStringValue(id);
       NS_ASSERTION(0 == nsCRT::strcmp(check, tag), "can't map id back to tag");
     }

     // Make sure we don't find things that aren't there
     id = nsHTMLTags::LookupTag(NS_LITERAL_STRING("@"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
     id = nsHTMLTags::LookupTag(NS_LITERAL_STRING("zzzzz"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

     tag = nsHTMLTags::GetStringValue((nsHTMLTag) 0);
     NS_ASSERTION(!tag, "found enum 0");
     tag = nsHTMLTags::GetStringValue((nsHTMLTag) -1);
     NS_ASSERTION(!tag, "found enum -1");
     tag = nsHTMLTags::GetStringValue((nsHTMLTag) (NS_HTML_TAG_MAX + 1));
     NS_ASSERTION(!tag, "found past max enum");

     nsHTMLTags::ReleaseTable();
   }
};

static const nsTestTagTable validateTagTable;

#endif
