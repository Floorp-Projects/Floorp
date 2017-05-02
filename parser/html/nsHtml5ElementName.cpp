/*
 * Copyright (c) 2008-2017 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Please edit ElementName.java instead and regenerate.
 */

#define nsHtml5ElementName_cpp__

#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5String.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsHtml5Macros.h"
#include "nsIContentHandle.h"

#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5ElementName.h"

int32_t 
nsHtml5ElementName::getGroup()
{
  return flags & NS_HTML5ELEMENT_NAME_GROUP_MASK;
}

bool
nsHtml5ElementName::isInterned()
{
  return !(flags & NS_HTML5ELEMENT_NAME_NOT_INTERNED);
}

nsHtml5ElementName* 
nsHtml5ElementName::elementNameByBuffer(char16_t* buf, int32_t offset, int32_t length, nsHtml5AtomTable* interner)
{
  uint32_t hash = nsHtml5ElementName::bufToHash(buf, length);
  int32_t index = nsHtml5ElementName::ELEMENT_HASHES.binarySearch(hash);
  if (index < 0) {
    return nullptr;
  } else {
    nsHtml5ElementName* elementName = nsHtml5ElementName::ELEMENT_NAMES[index];
    nsIAtom* name = elementName->name;
    if (!nsHtml5Portability::localEqualsBuffer(name, buf, offset, length)) {
      return nullptr;
    }
    return elementName;
  }
}


nsHtml5ElementName::nsHtml5ElementName(nsIAtom* name, nsIAtom* camelCaseName, int32_t flags)
  : name(name),
    camelCaseName(camelCaseName),
    flags(flags)
{
  MOZ_COUNT_CTOR(nsHtml5ElementName);
}

nsHtml5ElementName::nsHtml5ElementName()
  : name(nullptr)
  , camelCaseName(nullptr)
  , flags(NS_HTML5TREE_BUILDER_OTHER | NS_HTML5ELEMENT_NAME_NOT_INTERNED)
{
  MOZ_COUNT_CTOR(nsHtml5ElementName);
}


nsHtml5ElementName::~nsHtml5ElementName()
{
  MOZ_COUNT_DTOR(nsHtml5ElementName);
}

void
nsHtml5ElementName::setNameForNonInterned(nsIAtom* name)
{
  this->name = name;
  this->camelCaseName = name;
  MOZ_ASSERT(this->flags ==
             (NS_HTML5TREE_BUILDER_OTHER | NS_HTML5ELEMENT_NAME_NOT_INTERNED));
}

nsHtml5ElementName* nsHtml5ElementName::ELT_ANNOTATION_XML = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ISINDEX = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BIG = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BDO = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_COL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DEL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DFN = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DIR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DIV = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_IMG = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_INS = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_KBD = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MAP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_NAV = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_PRE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_A = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_B = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_RTC = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SUB = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SVG = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SUP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SET = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_USE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_VAR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_G = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_WBR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_XMP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_I = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_P = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_Q = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_S = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_U = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_H1 = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_H2 = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_H3 = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_H4 = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_H5 = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_H6 = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_AREA = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEFUNCA = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_METADATA = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_META = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TEXTAREA = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEFUNCB = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_RB = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DESC = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DD = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BGSOUND = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_EMBED = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEBLEND = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEFLOOD = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_HEAD = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_LEGEND = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_NOEMBED = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TD = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_THEAD = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ASIDE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ARTICLE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ANIMATE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BASE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BLOCKQUOTE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_CODE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_CIRCLE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_CITE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ELLIPSE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FETURBULENCE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEMERGENODE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEIMAGE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEMERGE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FETILE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FRAME = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FIGURE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FECOMPOSITE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_IMAGE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_IFRAME = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_LINE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MARQUEE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_POLYLINE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_PICTURE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SOURCE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_STRIKE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_STYLE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TABLE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TITLE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TIME = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TEMPLATE = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ALTGLYPHDEF = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_GLYPHREF = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DIALOG = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEFUNCG = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEDIFFUSELIGHTING = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FESPECULARLIGHTING = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_LISTING = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_STRONG = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ALTGLYPH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_CLIPPATH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MGLYPH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MATH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MPATH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_PATH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SWITCH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TEXTPATH = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_LI = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MI = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_LINK = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MARK = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MALIGNMARK = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MASK = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TRACK = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_HTML = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_OL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_LABEL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_UL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SMALL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SYMBOL = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ALTGLYPHITEM = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ANIMATETRANSFORM = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ACRONYM = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_EM = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FORM = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MENUITEM = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_PARAM = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ANIMATEMOTION = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BUTTON = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_CAPTION = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FIGCAPTION = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MN = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_KEYGEN = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MAIN = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_OPTION = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_POLYGON = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_PATTERN = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SPAN = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SECTION = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TSPAN = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_AUDIO = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MO = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_VIDEO = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_COLGROUP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEDISPLACEMENTMAP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_HGROUP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_RP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_OPTGROUP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SAMP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_STOP = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ABBR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ANIMATECOLOR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_CENTER = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_HR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEFUNCR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FECOMPONENTTRANSFER = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FILTER = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FOOTER = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEGAUSSIANBLUR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_HEADER = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MARKER = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_METER = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_NOBR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TR = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_ADDRESS = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_CANVAS = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DEFS = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DETAILS = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MS = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_NOFRAMES = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_PROGRESS = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_DT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_APPLET = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BASEFONT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FOREIGNOBJECT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FIELDSET = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FRAMESET = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEOFFSET = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FESPOTLIGHT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEPOINTLIGHT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEDISTANTLIGHT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FONT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_INPUT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_LINEARGRADIENT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MTEXT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_NOSCRIPT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_RT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_OBJECT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_OUTPUT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_PLAINTEXT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_RECT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_RADIALGRADIENT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SELECT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SCRIPT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TFOOT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TEXT = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_MENU = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEDROPSHADOW = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_VIEW = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FECOLORMATRIX = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FECONVOLVEMATRIX = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_BODY = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_FEMORPHOLOGY = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_RUBY = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_SUMMARY = nullptr;
nsHtml5ElementName* nsHtml5ElementName::ELT_TBODY = nullptr;
nsHtml5ElementName** nsHtml5ElementName::ELEMENT_NAMES = 0;
static int32_t const ELEMENT_HASHES_DATA[] = {
  51434643,   51438659,   51961587,   52485715,   52486755,   52488851,
  52490899,   55104723,   55110883,   56151587,   57206291,   57733651,
  58773795,   59244545,   59768833,   59821379,   60345171,   60347747,
  60352339,   60354131,   61395251,   61925907,   62390273,   62450211,
  62973651,   63438849,   67108865,   67633153,   68681729,   69730305,
  876609538,  893386754,  910163970,  926941186,  943718402,  960495618,
  1679960596, 1682547543, 1686489160, 1686491348, 1689922072, 1699324759,
  1703936002, 1715310660, 1730150402, 1730965751, 1732381397, 1733054663,
  1733076167, 1733890180, 1736200310, 1737099991, 1738539010, 1740181637,
  1747048757, 1747176599, 1747306711, 1747814436, 1747838298, 1748100148,
  1748225318, 1748359220, 1749395095, 1749656156, 1749673195, 1749715159,
  1749723735, 1749801286, 1749813541, 1749905526, 1749932347, 1751288021,
  1751386406, 1752979652, 1753362711, 1755076808, 1755148615, 1756474198,
  1756600614, 1756625221, 1757137429, 1757146773, 1757157700, 1757268168,
  1763839627, 1766992520, 1782357526, 1783210839, 1783388497, 1783388498,
  1786534215, 1790207270, 1797585096, 1798686984, 1803876550, 1803929812,
  1803929861, 1805502724, 1805647874, 1806806678, 1807599880, 1818230786,
  1818755074, 1853642948, 1854228692, 1854228698, 1854245076, 1857653029,
  1864368130, 1868312196, 1870135298, 1870268949, 1873281026, 1874053333,
  1874102998, 1881288348, 1881498736, 1881613047, 1881669634, 1884120164,
  1887579800, 1889085973, 1898223949, 1898753862, 1899272519, 1900845386,
  1902641154, 1903302038, 1904412884, 1905563974, 1906087319, 1906135367,
  1907435316, 1907661127, 1907959605, 1914900309, 1919418370, 1925844629,
  1932928296, 1934172497, 1935549734, 1938817026, 1939219752, 1941178676,
  1941221172, 1963982850, 1965115924, 1965334268, 1966223078, 1967128578,
  1967760215, 1967788867, 1967795910, 1967795958, 1968053806, 1968836118,
  1971461414, 1971465813, 1971938532, 1973420034, 1982173479, 1982935782,
  1983533124, 1983633431, 1986527234, 1988763672, 1990037800, 1998585858,
  1998724870, 1999397992, 2001309869, 2001349704, 2001349720, 2001349736,
  2001392795, 2001392796, 2001392798, 2001495140, 2003183333, 2004635806,
  2005324101, 2005719336, 2005925890, 2006028454, 2006329158, 2006896969,
  2006974466, 2007601444, 2007781534, 2008125638, 2008340774, 2008851557,
  2008994116, 2021937364, 2051837468, 2060065124, 2068523853, 2068523856,
  2070023911, 2083120164, 2085266636, 2091479332, 2092255447, 2092557349
};
staticJArray<int32_t,int32_t> nsHtml5ElementName::ELEMENT_HASHES = { ELEMENT_HASHES_DATA, MOZ_ARRAY_LENGTH(ELEMENT_HASHES_DATA) };
void
nsHtml5ElementName::initializeStatics()
{
  ELT_ANNOTATION_XML =
    new nsHtml5ElementName(nsHtml5Atoms::annotation_xml,
                           nsHtml5Atoms::annotation_xml,
                           NS_HTML5TREE_BUILDER_ANNOTATION_XML |
                             NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML);
  ELT_ISINDEX = new nsHtml5ElementName(nsHtml5Atoms::isindex,
                                       nsHtml5Atoms::isindex,
                                       NS_HTML5TREE_BUILDER_ISINDEX |
                                         NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_BIG = new nsHtml5ElementName(nsHtml5Atoms::big, nsHtml5Atoms::big, NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_BDO = new nsHtml5ElementName(nsHtml5Atoms::bdo, nsHtml5Atoms::bdo, NS_HTML5TREE_BUILDER_OTHER);
  ELT_COL = new nsHtml5ElementName(nsHtml5Atoms::col, nsHtml5Atoms::col, NS_HTML5TREE_BUILDER_COL | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_DEL = new nsHtml5ElementName(nsHtml5Atoms::del, nsHtml5Atoms::del, NS_HTML5TREE_BUILDER_OTHER);
  ELT_DFN = new nsHtml5ElementName(nsHtml5Atoms::dfn, nsHtml5Atoms::dfn, NS_HTML5TREE_BUILDER_OTHER);
  ELT_DIR = new nsHtml5ElementName(nsHtml5Atoms::dir, nsHtml5Atoms::dir, NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_DIV = new nsHtml5ElementName(nsHtml5Atoms::div, nsHtml5Atoms::div, NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_IMG = new nsHtml5ElementName(nsHtml5Atoms::img, nsHtml5Atoms::img, NS_HTML5TREE_BUILDER_IMG | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_INS = new nsHtml5ElementName(nsHtml5Atoms::ins, nsHtml5Atoms::ins, NS_HTML5TREE_BUILDER_OTHER);
  ELT_KBD = new nsHtml5ElementName(nsHtml5Atoms::kbd, nsHtml5Atoms::kbd, NS_HTML5TREE_BUILDER_OTHER);
  ELT_MAP = new nsHtml5ElementName(nsHtml5Atoms::map, nsHtml5Atoms::map, NS_HTML5TREE_BUILDER_OTHER);
  ELT_NAV = new nsHtml5ElementName(nsHtml5Atoms::nav, nsHtml5Atoms::nav, NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_PRE = new nsHtml5ElementName(nsHtml5Atoms::pre, nsHtml5Atoms::pre, NS_HTML5TREE_BUILDER_PRE_OR_LISTING | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_A = new nsHtml5ElementName(
    nsHtml5Atoms::a, nsHtml5Atoms::a, NS_HTML5TREE_BUILDER_A);
  ELT_B = new nsHtml5ElementName(
    nsHtml5Atoms::b,
    nsHtml5Atoms::b,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_RTC = new nsHtml5ElementName(nsHtml5Atoms::rtc, nsHtml5Atoms::rtc, NS_HTML5TREE_BUILDER_RB_OR_RTC | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_SUB = new nsHtml5ElementName(nsHtml5Atoms::sub, nsHtml5Atoms::sub, NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SVG = new nsHtml5ElementName(nsHtml5Atoms::svg, nsHtml5Atoms::svg, NS_HTML5TREE_BUILDER_SVG);
  ELT_SUP = new nsHtml5ElementName(nsHtml5Atoms::sup, nsHtml5Atoms::sup, NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SET = new nsHtml5ElementName(nsHtml5Atoms::set, nsHtml5Atoms::set, NS_HTML5TREE_BUILDER_OTHER);
  ELT_USE = new nsHtml5ElementName(nsHtml5Atoms::use, nsHtml5Atoms::use, NS_HTML5TREE_BUILDER_OTHER);
  ELT_VAR = new nsHtml5ElementName(nsHtml5Atoms::var, nsHtml5Atoms::var, NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_G = new nsHtml5ElementName(
    nsHtml5Atoms::g, nsHtml5Atoms::g, NS_HTML5TREE_BUILDER_OTHER);
  ELT_WBR = new nsHtml5ElementName(nsHtml5Atoms::wbr, nsHtml5Atoms::wbr, NS_HTML5TREE_BUILDER_AREA_OR_WBR | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_XMP = new nsHtml5ElementName(nsHtml5Atoms::xmp, nsHtml5Atoms::xmp, NS_HTML5TREE_BUILDER_XMP | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_I = new nsHtml5ElementName(
    nsHtml5Atoms::i,
    nsHtml5Atoms::i,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_P = new nsHtml5ElementName(nsHtml5Atoms::p,
                                 nsHtml5Atoms::p,
                                 NS_HTML5TREE_BUILDER_P |
                                   NS_HTML5ELEMENT_NAME_SPECIAL |
                                   NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_Q = new nsHtml5ElementName(
    nsHtml5Atoms::q, nsHtml5Atoms::q, NS_HTML5TREE_BUILDER_OTHER);
  ELT_S = new nsHtml5ElementName(
    nsHtml5Atoms::s,
    nsHtml5Atoms::s,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_U = new nsHtml5ElementName(
    nsHtml5Atoms::u,
    nsHtml5Atoms::u,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_H1 = new nsHtml5ElementName(
    nsHtml5Atoms::h1,
    nsHtml5Atoms::h1,
    NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_H2 = new nsHtml5ElementName(
    nsHtml5Atoms::h2,
    nsHtml5Atoms::h2,
    NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_H3 = new nsHtml5ElementName(
    nsHtml5Atoms::h3,
    nsHtml5Atoms::h3,
    NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_H4 = new nsHtml5ElementName(
    nsHtml5Atoms::h4,
    nsHtml5Atoms::h4,
    NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_H5 = new nsHtml5ElementName(
    nsHtml5Atoms::h5,
    nsHtml5Atoms::h5,
    NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_H6 = new nsHtml5ElementName(
    nsHtml5Atoms::h6,
    nsHtml5Atoms::h6,
    NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_AREA = new nsHtml5ElementName(nsHtml5Atoms::area, nsHtml5Atoms::area, NS_HTML5TREE_BUILDER_AREA_OR_WBR | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEFUNCA = new nsHtml5ElementName(
    nsHtml5Atoms::fefunca, nsHtml5Atoms::feFuncA, NS_HTML5TREE_BUILDER_OTHER);
  ELT_METADATA = new nsHtml5ElementName(
    nsHtml5Atoms::metadata, nsHtml5Atoms::metadata, NS_HTML5TREE_BUILDER_OTHER);
  ELT_META = new nsHtml5ElementName(nsHtml5Atoms::meta, nsHtml5Atoms::meta, NS_HTML5TREE_BUILDER_META | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_TEXTAREA = new nsHtml5ElementName(nsHtml5Atoms::textarea,
                                        nsHtml5Atoms::textarea,
                                        NS_HTML5TREE_BUILDER_TEXTAREA |
                                          NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEFUNCB = new nsHtml5ElementName(
    nsHtml5Atoms::fefuncb, nsHtml5Atoms::feFuncB, NS_HTML5TREE_BUILDER_OTHER);
  ELT_RB = new nsHtml5ElementName(nsHtml5Atoms::rb,
                                  nsHtml5Atoms::rb,
                                  NS_HTML5TREE_BUILDER_RB_OR_RTC |
                                    NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_DESC = new nsHtml5ElementName(nsHtml5Atoms::desc,
                                    nsHtml5Atoms::desc,
                                    NS_HTML5TREE_BUILDER_FOREIGNOBJECT_OR_DESC |
                                      NS_HTML5ELEMENT_NAME_SCOPING_AS_SVG);
  ELT_DD = new nsHtml5ElementName(nsHtml5Atoms::dd,
                                  nsHtml5Atoms::dd,
                                  NS_HTML5TREE_BUILDER_DD_OR_DT |
                                    NS_HTML5ELEMENT_NAME_SPECIAL |
                                    NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_BGSOUND =
    new nsHtml5ElementName(nsHtml5Atoms::bgsound,
                           nsHtml5Atoms::bgsound,
                           NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND |
                             NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_EMBED = new nsHtml5ElementName(nsHtml5Atoms::embed, nsHtml5Atoms::embed, NS_HTML5TREE_BUILDER_EMBED | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEBLEND = new nsHtml5ElementName(
    nsHtml5Atoms::feblend, nsHtml5Atoms::feBlend, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FEFLOOD = new nsHtml5ElementName(
    nsHtml5Atoms::feflood, nsHtml5Atoms::feFlood, NS_HTML5TREE_BUILDER_OTHER);
  ELT_HEAD = new nsHtml5ElementName(nsHtml5Atoms::head,
                                    nsHtml5Atoms::head,
                                    NS_HTML5TREE_BUILDER_HEAD |
                                      NS_HTML5ELEMENT_NAME_SPECIAL |
                                      NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_LEGEND = new nsHtml5ElementName(
    nsHtml5Atoms::legend, nsHtml5Atoms::legend, NS_HTML5TREE_BUILDER_OTHER);
  ELT_NOEMBED = new nsHtml5ElementName(nsHtml5Atoms::noembed,
                                       nsHtml5Atoms::noembed,
                                       NS_HTML5TREE_BUILDER_NOEMBED |
                                         NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_TD = new nsHtml5ElementName(
    nsHtml5Atoms::td,
    nsHtml5Atoms::td,
    NS_HTML5TREE_BUILDER_TD_OR_TH | NS_HTML5ELEMENT_NAME_SPECIAL |
      NS_HTML5ELEMENT_NAME_SCOPING | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_THEAD = new nsHtml5ElementName(nsHtml5Atoms::thead, nsHtml5Atoms::thead, NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT | NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_FOSTER_PARENTING | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_ASIDE = new nsHtml5ElementName(
    nsHtml5Atoms::aside,
    nsHtml5Atoms::aside,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_ARTICLE = new nsHtml5ElementName(
    nsHtml5Atoms::article,
    nsHtml5Atoms::article,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_ANIMATE = new nsHtml5ElementName(
    nsHtml5Atoms::animate, nsHtml5Atoms::animate, NS_HTML5TREE_BUILDER_OTHER);
  ELT_BASE = new nsHtml5ElementName(nsHtml5Atoms::base,
                                    nsHtml5Atoms::base,
                                    NS_HTML5TREE_BUILDER_BASE |
                                      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_BLOCKQUOTE = new nsHtml5ElementName(
    nsHtml5Atoms::blockquote,
    nsHtml5Atoms::blockquote,
    NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_CODE = new nsHtml5ElementName(
    nsHtml5Atoms::code,
    nsHtml5Atoms::code,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_CIRCLE = new nsHtml5ElementName(nsHtml5Atoms::circle, nsHtml5Atoms::circle, NS_HTML5TREE_BUILDER_OTHER);
  ELT_CITE = new nsHtml5ElementName(
    nsHtml5Atoms::cite, nsHtml5Atoms::cite, NS_HTML5TREE_BUILDER_OTHER);
  ELT_ELLIPSE = new nsHtml5ElementName(
    nsHtml5Atoms::ellipse, nsHtml5Atoms::ellipse, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FETURBULENCE = new nsHtml5ElementName(nsHtml5Atoms::feturbulence,
                                            nsHtml5Atoms::feTurbulence,
                                            NS_HTML5TREE_BUILDER_OTHER);
  ELT_FEMERGENODE = new nsHtml5ElementName(nsHtml5Atoms::femergenode,
                                           nsHtml5Atoms::feMergeNode,
                                           NS_HTML5TREE_BUILDER_OTHER);
  ELT_FEIMAGE = new nsHtml5ElementName(
    nsHtml5Atoms::feimage, nsHtml5Atoms::feImage, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FEMERGE = new nsHtml5ElementName(
    nsHtml5Atoms::femerge, nsHtml5Atoms::feMerge, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FETILE = new nsHtml5ElementName(nsHtml5Atoms::fetile, nsHtml5Atoms::feTile, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FRAME = new nsHtml5ElementName(nsHtml5Atoms::frame,
                                     nsHtml5Atoms::frame,
                                     NS_HTML5TREE_BUILDER_FRAME |
                                       NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FIGURE = new nsHtml5ElementName(nsHtml5Atoms::figure, nsHtml5Atoms::figure, NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FECOMPOSITE = new nsHtml5ElementName(nsHtml5Atoms::fecomposite,
                                           nsHtml5Atoms::feComposite,
                                           NS_HTML5TREE_BUILDER_OTHER);
  ELT_IMAGE = new nsHtml5ElementName(
    nsHtml5Atoms::image, nsHtml5Atoms::image, NS_HTML5TREE_BUILDER_IMAGE);
  ELT_IFRAME = new nsHtml5ElementName(nsHtml5Atoms::iframe, nsHtml5Atoms::iframe, NS_HTML5TREE_BUILDER_IFRAME | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_LINE = new nsHtml5ElementName(
    nsHtml5Atoms::line, nsHtml5Atoms::line, NS_HTML5TREE_BUILDER_OTHER);
  ELT_MARQUEE = new nsHtml5ElementName(nsHtml5Atoms::marquee,
                                       nsHtml5Atoms::marquee,
                                       NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET |
                                         NS_HTML5ELEMENT_NAME_SPECIAL |
                                         NS_HTML5ELEMENT_NAME_SCOPING);
  ELT_POLYLINE = new nsHtml5ElementName(
    nsHtml5Atoms::polyline, nsHtml5Atoms::polyline, NS_HTML5TREE_BUILDER_OTHER);
  ELT_PICTURE = new nsHtml5ElementName(
    nsHtml5Atoms::picture, nsHtml5Atoms::picture, NS_HTML5TREE_BUILDER_OTHER);
  ELT_SOURCE = new nsHtml5ElementName(nsHtml5Atoms::source, nsHtml5Atoms::source, NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK);
  ELT_STRIKE = new nsHtml5ElementName(nsHtml5Atoms::strike, nsHtml5Atoms::strike, NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_STYLE = new nsHtml5ElementName(nsHtml5Atoms::style,
                                     nsHtml5Atoms::style,
                                     NS_HTML5TREE_BUILDER_STYLE |
                                       NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_TABLE = new nsHtml5ElementName(
    nsHtml5Atoms::table,
    nsHtml5Atoms::table,
    NS_HTML5TREE_BUILDER_TABLE | NS_HTML5ELEMENT_NAME_SPECIAL |
      NS_HTML5ELEMENT_NAME_FOSTER_PARENTING | NS_HTML5ELEMENT_NAME_SCOPING);
  ELT_TITLE = new nsHtml5ElementName(nsHtml5Atoms::title,
                                     nsHtml5Atoms::title,
                                     NS_HTML5TREE_BUILDER_TITLE |
                                       NS_HTML5ELEMENT_NAME_SPECIAL |
                                       NS_HTML5ELEMENT_NAME_SCOPING_AS_SVG);
  ELT_TIME = new nsHtml5ElementName(
    nsHtml5Atoms::time, nsHtml5Atoms::time, NS_HTML5TREE_BUILDER_OTHER);
  ELT_TEMPLATE = new nsHtml5ElementName(nsHtml5Atoms::template_,
                                        nsHtml5Atoms::template_,
                                        NS_HTML5TREE_BUILDER_TEMPLATE |
                                          NS_HTML5ELEMENT_NAME_SPECIAL |
                                          NS_HTML5ELEMENT_NAME_SCOPING);
  ELT_ALTGLYPHDEF = new nsHtml5ElementName(nsHtml5Atoms::altglyphdef,
                                           nsHtml5Atoms::altGlyphDef,
                                           NS_HTML5TREE_BUILDER_OTHER);
  ELT_GLYPHREF = new nsHtml5ElementName(
    nsHtml5Atoms::glyphref, nsHtml5Atoms::glyphRef, NS_HTML5TREE_BUILDER_OTHER);
  ELT_DIALOG = new nsHtml5ElementName(
    nsHtml5Atoms::dialog,
    nsHtml5Atoms::dialog,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEFUNCG = new nsHtml5ElementName(
    nsHtml5Atoms::fefuncg, nsHtml5Atoms::feFuncG, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FEDIFFUSELIGHTING =
    new nsHtml5ElementName(nsHtml5Atoms::fediffuselighting,
                           nsHtml5Atoms::feDiffuseLighting,
                           NS_HTML5TREE_BUILDER_OTHER);
  ELT_FESPECULARLIGHTING =
    new nsHtml5ElementName(nsHtml5Atoms::fespecularlighting,
                           nsHtml5Atoms::feSpecularLighting,
                           NS_HTML5TREE_BUILDER_OTHER);
  ELT_LISTING = new nsHtml5ElementName(nsHtml5Atoms::listing,
                                       nsHtml5Atoms::listing,
                                       NS_HTML5TREE_BUILDER_PRE_OR_LISTING |
                                         NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_STRONG = new nsHtml5ElementName(nsHtml5Atoms::strong, nsHtml5Atoms::strong, NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_ALTGLYPH = new nsHtml5ElementName(
    nsHtml5Atoms::altglyph, nsHtml5Atoms::altGlyph, NS_HTML5TREE_BUILDER_OTHER);
  ELT_CLIPPATH = new nsHtml5ElementName(
    nsHtml5Atoms::clippath, nsHtml5Atoms::clipPath, NS_HTML5TREE_BUILDER_OTHER);
  ELT_MGLYPH =
    new nsHtml5ElementName(nsHtml5Atoms::mglyph,
                           nsHtml5Atoms::mglyph,
                           NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK);
  ELT_MATH = new nsHtml5ElementName(
    nsHtml5Atoms::math, nsHtml5Atoms::math, NS_HTML5TREE_BUILDER_MATH);
  ELT_MPATH = new nsHtml5ElementName(
    nsHtml5Atoms::mpath, nsHtml5Atoms::mpath, NS_HTML5TREE_BUILDER_OTHER);
  ELT_PATH = new nsHtml5ElementName(
    nsHtml5Atoms::path, nsHtml5Atoms::path, NS_HTML5TREE_BUILDER_OTHER);
  ELT_TH = new nsHtml5ElementName(
    nsHtml5Atoms::th,
    nsHtml5Atoms::th,
    NS_HTML5TREE_BUILDER_TD_OR_TH | NS_HTML5ELEMENT_NAME_SPECIAL |
      NS_HTML5ELEMENT_NAME_SCOPING | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_SWITCH = new nsHtml5ElementName(nsHtml5Atoms::switch_, nsHtml5Atoms::switch_, NS_HTML5TREE_BUILDER_OTHER);
  ELT_TEXTPATH = new nsHtml5ElementName(
    nsHtml5Atoms::textpath, nsHtml5Atoms::textPath, NS_HTML5TREE_BUILDER_OTHER);
  ELT_LI = new nsHtml5ElementName(nsHtml5Atoms::li,
                                  nsHtml5Atoms::li,
                                  NS_HTML5TREE_BUILDER_LI |
                                    NS_HTML5ELEMENT_NAME_SPECIAL |
                                    NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_MI = new nsHtml5ElementName(nsHtml5Atoms::mi,
                                  nsHtml5Atoms::mi,
                                  NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT |
                                    NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML);
  ELT_LINK =
    new nsHtml5ElementName(nsHtml5Atoms::link,
                           nsHtml5Atoms::link,
                           NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND |
                             NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_MARK = new nsHtml5ElementName(
    nsHtml5Atoms::mark, nsHtml5Atoms::mark, NS_HTML5TREE_BUILDER_OTHER);
  ELT_MALIGNMARK =
    new nsHtml5ElementName(nsHtml5Atoms::malignmark,
                           nsHtml5Atoms::malignmark,
                           NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK);
  ELT_MASK = new nsHtml5ElementName(
    nsHtml5Atoms::mask, nsHtml5Atoms::mask, NS_HTML5TREE_BUILDER_OTHER);
  ELT_TRACK =
    new nsHtml5ElementName(nsHtml5Atoms::track,
                           nsHtml5Atoms::track,
                           NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK |
                             NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_DL = new nsHtml5ElementName(nsHtml5Atoms::dl,
                                  nsHtml5Atoms::dl,
                                  NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL |
                                    NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_HTML = new nsHtml5ElementName(
    nsHtml5Atoms::html,
    nsHtml5Atoms::html,
    NS_HTML5TREE_BUILDER_HTML | NS_HTML5ELEMENT_NAME_SPECIAL |
      NS_HTML5ELEMENT_NAME_SCOPING | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_OL = new nsHtml5ElementName(nsHtml5Atoms::ol,
                                  nsHtml5Atoms::ol,
                                  NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL |
                                    NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_LABEL = new nsHtml5ElementName(
    nsHtml5Atoms::label, nsHtml5Atoms::label, NS_HTML5TREE_BUILDER_OTHER);
  ELT_UL = new nsHtml5ElementName(nsHtml5Atoms::ul,
                                  nsHtml5Atoms::ul,
                                  NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL |
                                    NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_SMALL = new nsHtml5ElementName(
    nsHtml5Atoms::small_,
    nsHtml5Atoms::small_,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_SYMBOL = new nsHtml5ElementName(nsHtml5Atoms::symbol, nsHtml5Atoms::symbol, NS_HTML5TREE_BUILDER_OTHER);
  ELT_ALTGLYPHITEM = new nsHtml5ElementName(nsHtml5Atoms::altglyphitem,
                                            nsHtml5Atoms::altGlyphItem,
                                            NS_HTML5TREE_BUILDER_OTHER);
  ELT_ANIMATETRANSFORM = new nsHtml5ElementName(nsHtml5Atoms::animatetransform,
                                                nsHtml5Atoms::animateTransform,
                                                NS_HTML5TREE_BUILDER_OTHER);
  ELT_ACRONYM = new nsHtml5ElementName(nsHtml5Atoms::acronym, nsHtml5Atoms::acronym, NS_HTML5TREE_BUILDER_OTHER);
  ELT_EM = new nsHtml5ElementName(
    nsHtml5Atoms::em,
    nsHtml5Atoms::em,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_FORM = new nsHtml5ElementName(nsHtml5Atoms::form,
                                    nsHtml5Atoms::form,
                                    NS_HTML5TREE_BUILDER_FORM |
                                      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_MENUITEM = new nsHtml5ElementName(nsHtml5Atoms::menuitem,
                                        nsHtml5Atoms::menuitem,
                                        NS_HTML5TREE_BUILDER_MENUITEM);
  ELT_PARAM =
    new nsHtml5ElementName(nsHtml5Atoms::param,
                           nsHtml5Atoms::param,
                           NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK |
                             NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_ANIMATEMOTION = new nsHtml5ElementName(nsHtml5Atoms::animatemotion,
                                             nsHtml5Atoms::animateMotion,
                                             NS_HTML5TREE_BUILDER_OTHER);
  ELT_BUTTON = new nsHtml5ElementName(nsHtml5Atoms::button,
                                      nsHtml5Atoms::button,
                                      NS_HTML5TREE_BUILDER_BUTTON |
                                        NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_CAPTION = new nsHtml5ElementName(nsHtml5Atoms::caption, nsHtml5Atoms::caption, NS_HTML5TREE_BUILDER_CAPTION | NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_SCOPING);
  ELT_FIGCAPTION = new nsHtml5ElementName(
    nsHtml5Atoms::figcaption,
    nsHtml5Atoms::figcaption,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_MN = new nsHtml5ElementName(nsHtml5Atoms::mn,
                                  nsHtml5Atoms::mn,
                                  NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT |
                                    NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML);
  ELT_KEYGEN = new nsHtml5ElementName(
    nsHtml5Atoms::keygen, nsHtml5Atoms::keygen, NS_HTML5TREE_BUILDER_KEYGEN);
  ELT_MAIN = new nsHtml5ElementName(
    nsHtml5Atoms::main,
    nsHtml5Atoms::main,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_OPTION = new nsHtml5ElementName(nsHtml5Atoms::option,
                                      nsHtml5Atoms::option,
                                      NS_HTML5TREE_BUILDER_OPTION |
                                        NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_POLYGON = new nsHtml5ElementName(nsHtml5Atoms::polygon, nsHtml5Atoms::polygon, NS_HTML5TREE_BUILDER_OTHER);
  ELT_PATTERN = new nsHtml5ElementName(nsHtml5Atoms::pattern, nsHtml5Atoms::pattern, NS_HTML5TREE_BUILDER_OTHER);
  ELT_SPAN = new nsHtml5ElementName(
    nsHtml5Atoms::span,
    nsHtml5Atoms::span,
    NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SECTION = new nsHtml5ElementName(nsHtml5Atoms::section, nsHtml5Atoms::section, NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_TSPAN = new nsHtml5ElementName(
    nsHtml5Atoms::tspan, nsHtml5Atoms::tspan, NS_HTML5TREE_BUILDER_OTHER);
  ELT_AUDIO = new nsHtml5ElementName(
    nsHtml5Atoms::audio, nsHtml5Atoms::audio, NS_HTML5TREE_BUILDER_OTHER);
  ELT_MO = new nsHtml5ElementName(nsHtml5Atoms::mo,
                                  nsHtml5Atoms::mo,
                                  NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT |
                                    NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML);
  ELT_VIDEO = new nsHtml5ElementName(
    nsHtml5Atoms::video, nsHtml5Atoms::video, NS_HTML5TREE_BUILDER_OTHER);
  ELT_COLGROUP = new nsHtml5ElementName(nsHtml5Atoms::colgroup, nsHtml5Atoms::colgroup, NS_HTML5TREE_BUILDER_COLGROUP | NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_FEDISPLACEMENTMAP =
    new nsHtml5ElementName(nsHtml5Atoms::fedisplacementmap,
                           nsHtml5Atoms::feDisplacementMap,
                           NS_HTML5TREE_BUILDER_OTHER);
  ELT_HGROUP = new nsHtml5ElementName(
    nsHtml5Atoms::hgroup,
    nsHtml5Atoms::hgroup,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_RP = new nsHtml5ElementName(nsHtml5Atoms::rp,
                                  nsHtml5Atoms::rp,
                                  NS_HTML5TREE_BUILDER_RT_OR_RP |
                                    NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_OPTGROUP = new nsHtml5ElementName(
    nsHtml5Atoms::optgroup,
    nsHtml5Atoms::optgroup,
    NS_HTML5TREE_BUILDER_OPTGROUP | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_SAMP = new nsHtml5ElementName(
    nsHtml5Atoms::samp, nsHtml5Atoms::samp, NS_HTML5TREE_BUILDER_OTHER);
  ELT_STOP = new nsHtml5ElementName(
    nsHtml5Atoms::stop, nsHtml5Atoms::stop, NS_HTML5TREE_BUILDER_OTHER);
  ELT_BR = new nsHtml5ElementName(nsHtml5Atoms::br,
                                  nsHtml5Atoms::br,
                                  NS_HTML5TREE_BUILDER_BR |
                                    NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_ABBR = new nsHtml5ElementName(
    nsHtml5Atoms::abbr, nsHtml5Atoms::abbr, NS_HTML5TREE_BUILDER_OTHER);
  ELT_ANIMATECOLOR = new nsHtml5ElementName(nsHtml5Atoms::animatecolor,
                                            nsHtml5Atoms::animateColor,
                                            NS_HTML5TREE_BUILDER_OTHER);
  ELT_CENTER = new nsHtml5ElementName(
    nsHtml5Atoms::center,
    nsHtml5Atoms::center,
    NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_HR = new nsHtml5ElementName(nsHtml5Atoms::hr,
                                  nsHtml5Atoms::hr,
                                  NS_HTML5TREE_BUILDER_HR |
                                    NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEFUNCR = new nsHtml5ElementName(
    nsHtml5Atoms::fefuncr, nsHtml5Atoms::feFuncR, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FECOMPONENTTRANSFER =
    new nsHtml5ElementName(nsHtml5Atoms::fecomponenttransfer,
                           nsHtml5Atoms::feComponentTransfer,
                           NS_HTML5TREE_BUILDER_OTHER);
  ELT_FILTER = new nsHtml5ElementName(
    nsHtml5Atoms::filter, nsHtml5Atoms::filter, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FOOTER = new nsHtml5ElementName(
    nsHtml5Atoms::footer,
    nsHtml5Atoms::footer,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEGAUSSIANBLUR = new nsHtml5ElementName(nsHtml5Atoms::fegaussianblur,
                                              nsHtml5Atoms::feGaussianBlur,
                                              NS_HTML5TREE_BUILDER_OTHER);
  ELT_HEADER = new nsHtml5ElementName(
    nsHtml5Atoms::header,
    nsHtml5Atoms::header,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_MARKER = new nsHtml5ElementName(
    nsHtml5Atoms::marker, nsHtml5Atoms::marker, NS_HTML5TREE_BUILDER_OTHER);
  ELT_METER = new nsHtml5ElementName(
    nsHtml5Atoms::meter, nsHtml5Atoms::meter, NS_HTML5TREE_BUILDER_OTHER);
  ELT_NOBR = new nsHtml5ElementName(
    nsHtml5Atoms::nobr, nsHtml5Atoms::nobr, NS_HTML5TREE_BUILDER_NOBR);
  ELT_TR = new nsHtml5ElementName(nsHtml5Atoms::tr,
                                  nsHtml5Atoms::tr,
                                  NS_HTML5TREE_BUILDER_TR |
                                    NS_HTML5ELEMENT_NAME_SPECIAL |
                                    NS_HTML5ELEMENT_NAME_FOSTER_PARENTING |
                                    NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_ADDRESS = new nsHtml5ElementName(
    nsHtml5Atoms::address,
    nsHtml5Atoms::address,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_CANVAS = new nsHtml5ElementName(
    nsHtml5Atoms::canvas, nsHtml5Atoms::canvas, NS_HTML5TREE_BUILDER_OTHER);
  ELT_DEFS = new nsHtml5ElementName(
    nsHtml5Atoms::defs, nsHtml5Atoms::defs, NS_HTML5TREE_BUILDER_OTHER);
  ELT_DETAILS = new nsHtml5ElementName(
    nsHtml5Atoms::details,
    nsHtml5Atoms::details,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_MS = new nsHtml5ElementName(nsHtml5Atoms::ms,
                                  nsHtml5Atoms::ms,
                                  NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT |
                                    NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML);
  ELT_NOFRAMES = new nsHtml5ElementName(nsHtml5Atoms::noframes,
                                        nsHtml5Atoms::noframes,
                                        NS_HTML5TREE_BUILDER_NOFRAMES |
                                          NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_PROGRESS = new nsHtml5ElementName(
    nsHtml5Atoms::progress, nsHtml5Atoms::progress, NS_HTML5TREE_BUILDER_OTHER);
  ELT_DT = new nsHtml5ElementName(nsHtml5Atoms::dt,
                                  nsHtml5Atoms::dt,
                                  NS_HTML5TREE_BUILDER_DD_OR_DT |
                                    NS_HTML5ELEMENT_NAME_SPECIAL |
                                    NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_APPLET = new nsHtml5ElementName(nsHtml5Atoms::applet,
                                      nsHtml5Atoms::applet,
                                      NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET |
                                        NS_HTML5ELEMENT_NAME_SPECIAL |
                                        NS_HTML5ELEMENT_NAME_SCOPING);
  ELT_BASEFONT =
    new nsHtml5ElementName(nsHtml5Atoms::basefont,
                           nsHtml5Atoms::basefont,
                           NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND |
                             NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FOREIGNOBJECT =
    new nsHtml5ElementName(nsHtml5Atoms::foreignobject,
                           nsHtml5Atoms::foreignObject,
                           NS_HTML5TREE_BUILDER_FOREIGNOBJECT_OR_DESC |
                             NS_HTML5ELEMENT_NAME_SCOPING_AS_SVG);
  ELT_FIELDSET = new nsHtml5ElementName(nsHtml5Atoms::fieldset, nsHtml5Atoms::fieldset, NS_HTML5TREE_BUILDER_FIELDSET | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FRAMESET = new nsHtml5ElementName(nsHtml5Atoms::frameset, nsHtml5Atoms::frameset, NS_HTML5TREE_BUILDER_FRAMESET | NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEOFFSET = new nsHtml5ElementName(nsHtml5Atoms::feoffset, nsHtml5Atoms::feOffset, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FESPOTLIGHT = new nsHtml5ElementName(nsHtml5Atoms::fespotlight, nsHtml5Atoms::feSpotLight, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FEPOINTLIGHT = new nsHtml5ElementName(nsHtml5Atoms::fepointlight, nsHtml5Atoms::fePointLight, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FEDISTANTLIGHT = new nsHtml5ElementName(nsHtml5Atoms::fedistantlight, nsHtml5Atoms::feDistantLight, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FONT = new nsHtml5ElementName(
    nsHtml5Atoms::font, nsHtml5Atoms::font, NS_HTML5TREE_BUILDER_FONT);
  ELT_INPUT = new nsHtml5ElementName(nsHtml5Atoms::input,
                                     nsHtml5Atoms::input,
                                     NS_HTML5TREE_BUILDER_INPUT |
                                       NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_LINEARGRADIENT = new nsHtml5ElementName(nsHtml5Atoms::lineargradient, nsHtml5Atoms::linearGradient, NS_HTML5TREE_BUILDER_OTHER);
  ELT_MTEXT = new nsHtml5ElementName(nsHtml5Atoms::mtext,
                                     nsHtml5Atoms::mtext,
                                     NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT |
                                       NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML);
  ELT_NOSCRIPT = new nsHtml5ElementName(nsHtml5Atoms::noscript,
                                        nsHtml5Atoms::noscript,
                                        NS_HTML5TREE_BUILDER_NOSCRIPT |
                                          NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_RT = new nsHtml5ElementName(nsHtml5Atoms::rt,
                                  nsHtml5Atoms::rt,
                                  NS_HTML5TREE_BUILDER_RT_OR_RP |
                                    NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_OBJECT = new nsHtml5ElementName(nsHtml5Atoms::object,
                                      nsHtml5Atoms::object,
                                      NS_HTML5TREE_BUILDER_OBJECT |
                                        NS_HTML5ELEMENT_NAME_SPECIAL |
                                        NS_HTML5ELEMENT_NAME_SCOPING);
  ELT_OUTPUT = new nsHtml5ElementName(
    nsHtml5Atoms::output, nsHtml5Atoms::output, NS_HTML5TREE_BUILDER_OUTPUT);
  ELT_PLAINTEXT = new nsHtml5ElementName(nsHtml5Atoms::plaintext,
                                         nsHtml5Atoms::plaintext,
                                         NS_HTML5TREE_BUILDER_PLAINTEXT |
                                           NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_TT = new nsHtml5ElementName(
    nsHtml5Atoms::tt,
    nsHtml5Atoms::tt,
    NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_RECT = new nsHtml5ElementName(
    nsHtml5Atoms::rect, nsHtml5Atoms::rect, NS_HTML5TREE_BUILDER_OTHER);
  ELT_RADIALGRADIENT = new nsHtml5ElementName(nsHtml5Atoms::radialgradient, nsHtml5Atoms::radialGradient, NS_HTML5TREE_BUILDER_OTHER);
  ELT_SELECT = new nsHtml5ElementName(nsHtml5Atoms::select,
                                      nsHtml5Atoms::select,
                                      NS_HTML5TREE_BUILDER_SELECT |
                                        NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_SCRIPT = new nsHtml5ElementName(nsHtml5Atoms::script,
                                      nsHtml5Atoms::script,
                                      NS_HTML5TREE_BUILDER_SCRIPT |
                                        NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_TFOOT = new nsHtml5ElementName(
    nsHtml5Atoms::tfoot,
    nsHtml5Atoms::tfoot,
    NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT |
      NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_FOSTER_PARENTING |
      NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_TEXT = new nsHtml5ElementName(
    nsHtml5Atoms::text, nsHtml5Atoms::text, NS_HTML5TREE_BUILDER_OTHER);
  ELT_MENU = new nsHtml5ElementName(
    nsHtml5Atoms::menu,
    nsHtml5Atoms::menu,
    NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_FEDROPSHADOW = new nsHtml5ElementName(nsHtml5Atoms::fedropshadow,
                                            nsHtml5Atoms::feDropShadow,
                                            NS_HTML5TREE_BUILDER_OTHER);
  ELT_VIEW = new nsHtml5ElementName(
    nsHtml5Atoms::view, nsHtml5Atoms::view, NS_HTML5TREE_BUILDER_OTHER);
  ELT_FECOLORMATRIX = new nsHtml5ElementName(nsHtml5Atoms::fecolormatrix,
                                             nsHtml5Atoms::feColorMatrix,
                                             NS_HTML5TREE_BUILDER_OTHER);
  ELT_FECONVOLVEMATRIX = new nsHtml5ElementName(nsHtml5Atoms::feconvolvematrix, nsHtml5Atoms::feConvolveMatrix, NS_HTML5TREE_BUILDER_OTHER);
  ELT_BODY = new nsHtml5ElementName(nsHtml5Atoms::body,
                                    nsHtml5Atoms::body,
                                    NS_HTML5TREE_BUILDER_BODY |
                                      NS_HTML5ELEMENT_NAME_SPECIAL |
                                      NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELT_FEMORPHOLOGY = new nsHtml5ElementName(nsHtml5Atoms::femorphology,
                                            nsHtml5Atoms::feMorphology,
                                            NS_HTML5TREE_BUILDER_OTHER);
  ELT_RUBY = new nsHtml5ElementName(
    nsHtml5Atoms::ruby,
    nsHtml5Atoms::ruby,
    NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SUMMARY = new nsHtml5ElementName(
    nsHtml5Atoms::summary,
    nsHtml5Atoms::summary,
    NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      NS_HTML5ELEMENT_NAME_SPECIAL);
  ELT_TBODY = new nsHtml5ElementName(
    nsHtml5Atoms::tbody,
    nsHtml5Atoms::tbody,
    NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT |
      NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_FOSTER_PARENTING |
      NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  ELEMENT_NAMES = new nsHtml5ElementName*[204];
  ELEMENT_NAMES[0] = ELT_BIG;
  ELEMENT_NAMES[1] = ELT_BDO;
  ELEMENT_NAMES[2] = ELT_COL;
  ELEMENT_NAMES[3] = ELT_DEL;
  ELEMENT_NAMES[4] = ELT_DFN;
  ELEMENT_NAMES[5] = ELT_DIR;
  ELEMENT_NAMES[6] = ELT_DIV;
  ELEMENT_NAMES[7] = ELT_IMG;
  ELEMENT_NAMES[8] = ELT_INS;
  ELEMENT_NAMES[9] = ELT_KBD;
  ELEMENT_NAMES[10] = ELT_MAP;
  ELEMENT_NAMES[11] = ELT_NAV;
  ELEMENT_NAMES[12] = ELT_PRE;
  ELEMENT_NAMES[13] = ELT_A;
  ELEMENT_NAMES[14] = ELT_B;
  ELEMENT_NAMES[15] = ELT_RTC;
  ELEMENT_NAMES[16] = ELT_SUB;
  ELEMENT_NAMES[17] = ELT_SVG;
  ELEMENT_NAMES[18] = ELT_SUP;
  ELEMENT_NAMES[19] = ELT_SET;
  ELEMENT_NAMES[20] = ELT_USE;
  ELEMENT_NAMES[21] = ELT_VAR;
  ELEMENT_NAMES[22] = ELT_G;
  ELEMENT_NAMES[23] = ELT_WBR;
  ELEMENT_NAMES[24] = ELT_XMP;
  ELEMENT_NAMES[25] = ELT_I;
  ELEMENT_NAMES[26] = ELT_P;
  ELEMENT_NAMES[27] = ELT_Q;
  ELEMENT_NAMES[28] = ELT_S;
  ELEMENT_NAMES[29] = ELT_U;
  ELEMENT_NAMES[30] = ELT_H1;
  ELEMENT_NAMES[31] = ELT_H2;
  ELEMENT_NAMES[32] = ELT_H3;
  ELEMENT_NAMES[33] = ELT_H4;
  ELEMENT_NAMES[34] = ELT_H5;
  ELEMENT_NAMES[35] = ELT_H6;
  ELEMENT_NAMES[36] = ELT_AREA;
  ELEMENT_NAMES[37] = ELT_FEFUNCA;
  ELEMENT_NAMES[38] = ELT_METADATA;
  ELEMENT_NAMES[39] = ELT_META;
  ELEMENT_NAMES[40] = ELT_TEXTAREA;
  ELEMENT_NAMES[41] = ELT_FEFUNCB;
  ELEMENT_NAMES[42] = ELT_RB;
  ELEMENT_NAMES[43] = ELT_DESC;
  ELEMENT_NAMES[44] = ELT_DD;
  ELEMENT_NAMES[45] = ELT_BGSOUND;
  ELEMENT_NAMES[46] = ELT_EMBED;
  ELEMENT_NAMES[47] = ELT_FEBLEND;
  ELEMENT_NAMES[48] = ELT_FEFLOOD;
  ELEMENT_NAMES[49] = ELT_HEAD;
  ELEMENT_NAMES[50] = ELT_LEGEND;
  ELEMENT_NAMES[51] = ELT_NOEMBED;
  ELEMENT_NAMES[52] = ELT_TD;
  ELEMENT_NAMES[53] = ELT_THEAD;
  ELEMENT_NAMES[54] = ELT_ASIDE;
  ELEMENT_NAMES[55] = ELT_ARTICLE;
  ELEMENT_NAMES[56] = ELT_ANIMATE;
  ELEMENT_NAMES[57] = ELT_BASE;
  ELEMENT_NAMES[58] = ELT_BLOCKQUOTE;
  ELEMENT_NAMES[59] = ELT_CODE;
  ELEMENT_NAMES[60] = ELT_CIRCLE;
  ELEMENT_NAMES[61] = ELT_CITE;
  ELEMENT_NAMES[62] = ELT_ELLIPSE;
  ELEMENT_NAMES[63] = ELT_FETURBULENCE;
  ELEMENT_NAMES[64] = ELT_FEMERGENODE;
  ELEMENT_NAMES[65] = ELT_FEIMAGE;
  ELEMENT_NAMES[66] = ELT_FEMERGE;
  ELEMENT_NAMES[67] = ELT_FETILE;
  ELEMENT_NAMES[68] = ELT_FRAME;
  ELEMENT_NAMES[69] = ELT_FIGURE;
  ELEMENT_NAMES[70] = ELT_FECOMPOSITE;
  ELEMENT_NAMES[71] = ELT_IMAGE;
  ELEMENT_NAMES[72] = ELT_IFRAME;
  ELEMENT_NAMES[73] = ELT_LINE;
  ELEMENT_NAMES[74] = ELT_MARQUEE;
  ELEMENT_NAMES[75] = ELT_POLYLINE;
  ELEMENT_NAMES[76] = ELT_PICTURE;
  ELEMENT_NAMES[77] = ELT_SOURCE;
  ELEMENT_NAMES[78] = ELT_STRIKE;
  ELEMENT_NAMES[79] = ELT_STYLE;
  ELEMENT_NAMES[80] = ELT_TABLE;
  ELEMENT_NAMES[81] = ELT_TITLE;
  ELEMENT_NAMES[82] = ELT_TIME;
  ELEMENT_NAMES[83] = ELT_TEMPLATE;
  ELEMENT_NAMES[84] = ELT_ALTGLYPHDEF;
  ELEMENT_NAMES[85] = ELT_GLYPHREF;
  ELEMENT_NAMES[86] = ELT_DIALOG;
  ELEMENT_NAMES[87] = ELT_FEFUNCG;
  ELEMENT_NAMES[88] = ELT_FEDIFFUSELIGHTING;
  ELEMENT_NAMES[89] = ELT_FESPECULARLIGHTING;
  ELEMENT_NAMES[90] = ELT_LISTING;
  ELEMENT_NAMES[91] = ELT_STRONG;
  ELEMENT_NAMES[92] = ELT_ALTGLYPH;
  ELEMENT_NAMES[93] = ELT_CLIPPATH;
  ELEMENT_NAMES[94] = ELT_MGLYPH;
  ELEMENT_NAMES[95] = ELT_MATH;
  ELEMENT_NAMES[96] = ELT_MPATH;
  ELEMENT_NAMES[97] = ELT_PATH;
  ELEMENT_NAMES[98] = ELT_TH;
  ELEMENT_NAMES[99] = ELT_SWITCH;
  ELEMENT_NAMES[100] = ELT_TEXTPATH;
  ELEMENT_NAMES[101] = ELT_LI;
  ELEMENT_NAMES[102] = ELT_MI;
  ELEMENT_NAMES[103] = ELT_LINK;
  ELEMENT_NAMES[104] = ELT_MARK;
  ELEMENT_NAMES[105] = ELT_MALIGNMARK;
  ELEMENT_NAMES[106] = ELT_MASK;
  ELEMENT_NAMES[107] = ELT_TRACK;
  ELEMENT_NAMES[108] = ELT_DL;
  ELEMENT_NAMES[109] = ELT_HTML;
  ELEMENT_NAMES[110] = ELT_OL;
  ELEMENT_NAMES[111] = ELT_LABEL;
  ELEMENT_NAMES[112] = ELT_UL;
  ELEMENT_NAMES[113] = ELT_SMALL;
  ELEMENT_NAMES[114] = ELT_SYMBOL;
  ELEMENT_NAMES[115] = ELT_ALTGLYPHITEM;
  ELEMENT_NAMES[116] = ELT_ANIMATETRANSFORM;
  ELEMENT_NAMES[117] = ELT_ACRONYM;
  ELEMENT_NAMES[118] = ELT_EM;
  ELEMENT_NAMES[119] = ELT_FORM;
  ELEMENT_NAMES[120] = ELT_MENUITEM;
  ELEMENT_NAMES[121] = ELT_PARAM;
  ELEMENT_NAMES[122] = ELT_ANIMATEMOTION;
  ELEMENT_NAMES[123] = ELT_BUTTON;
  ELEMENT_NAMES[124] = ELT_CAPTION;
  ELEMENT_NAMES[125] = ELT_FIGCAPTION;
  ELEMENT_NAMES[126] = ELT_MN;
  ELEMENT_NAMES[127] = ELT_KEYGEN;
  ELEMENT_NAMES[128] = ELT_MAIN;
  ELEMENT_NAMES[129] = ELT_OPTION;
  ELEMENT_NAMES[130] = ELT_POLYGON;
  ELEMENT_NAMES[131] = ELT_PATTERN;
  ELEMENT_NAMES[132] = ELT_SPAN;
  ELEMENT_NAMES[133] = ELT_SECTION;
  ELEMENT_NAMES[134] = ELT_TSPAN;
  ELEMENT_NAMES[135] = ELT_AUDIO;
  ELEMENT_NAMES[136] = ELT_MO;
  ELEMENT_NAMES[137] = ELT_VIDEO;
  ELEMENT_NAMES[138] = ELT_COLGROUP;
  ELEMENT_NAMES[139] = ELT_FEDISPLACEMENTMAP;
  ELEMENT_NAMES[140] = ELT_HGROUP;
  ELEMENT_NAMES[141] = ELT_RP;
  ELEMENT_NAMES[142] = ELT_OPTGROUP;
  ELEMENT_NAMES[143] = ELT_SAMP;
  ELEMENT_NAMES[144] = ELT_STOP;
  ELEMENT_NAMES[145] = ELT_BR;
  ELEMENT_NAMES[146] = ELT_ABBR;
  ELEMENT_NAMES[147] = ELT_ANIMATECOLOR;
  ELEMENT_NAMES[148] = ELT_CENTER;
  ELEMENT_NAMES[149] = ELT_HR;
  ELEMENT_NAMES[150] = ELT_FEFUNCR;
  ELEMENT_NAMES[151] = ELT_FECOMPONENTTRANSFER;
  ELEMENT_NAMES[152] = ELT_FILTER;
  ELEMENT_NAMES[153] = ELT_FOOTER;
  ELEMENT_NAMES[154] = ELT_FEGAUSSIANBLUR;
  ELEMENT_NAMES[155] = ELT_HEADER;
  ELEMENT_NAMES[156] = ELT_MARKER;
  ELEMENT_NAMES[157] = ELT_METER;
  ELEMENT_NAMES[158] = ELT_NOBR;
  ELEMENT_NAMES[159] = ELT_TR;
  ELEMENT_NAMES[160] = ELT_ADDRESS;
  ELEMENT_NAMES[161] = ELT_CANVAS;
  ELEMENT_NAMES[162] = ELT_DEFS;
  ELEMENT_NAMES[163] = ELT_DETAILS;
  ELEMENT_NAMES[164] = ELT_MS;
  ELEMENT_NAMES[165] = ELT_NOFRAMES;
  ELEMENT_NAMES[166] = ELT_PROGRESS;
  ELEMENT_NAMES[167] = ELT_DT;
  ELEMENT_NAMES[168] = ELT_APPLET;
  ELEMENT_NAMES[169] = ELT_BASEFONT;
  ELEMENT_NAMES[170] = ELT_FOREIGNOBJECT;
  ELEMENT_NAMES[171] = ELT_FIELDSET;
  ELEMENT_NAMES[172] = ELT_FRAMESET;
  ELEMENT_NAMES[173] = ELT_FEOFFSET;
  ELEMENT_NAMES[174] = ELT_FESPOTLIGHT;
  ELEMENT_NAMES[175] = ELT_FEPOINTLIGHT;
  ELEMENT_NAMES[176] = ELT_FEDISTANTLIGHT;
  ELEMENT_NAMES[177] = ELT_FONT;
  ELEMENT_NAMES[178] = ELT_INPUT;
  ELEMENT_NAMES[179] = ELT_LINEARGRADIENT;
  ELEMENT_NAMES[180] = ELT_MTEXT;
  ELEMENT_NAMES[181] = ELT_NOSCRIPT;
  ELEMENT_NAMES[182] = ELT_RT;
  ELEMENT_NAMES[183] = ELT_OBJECT;
  ELEMENT_NAMES[184] = ELT_OUTPUT;
  ELEMENT_NAMES[185] = ELT_PLAINTEXT;
  ELEMENT_NAMES[186] = ELT_TT;
  ELEMENT_NAMES[187] = ELT_RECT;
  ELEMENT_NAMES[188] = ELT_RADIALGRADIENT;
  ELEMENT_NAMES[189] = ELT_SELECT;
  ELEMENT_NAMES[190] = ELT_SCRIPT;
  ELEMENT_NAMES[191] = ELT_TFOOT;
  ELEMENT_NAMES[192] = ELT_TEXT;
  ELEMENT_NAMES[193] = ELT_MENU;
  ELEMENT_NAMES[194] = ELT_FEDROPSHADOW;
  ELEMENT_NAMES[195] = ELT_VIEW;
  ELEMENT_NAMES[196] = ELT_FECOLORMATRIX;
  ELEMENT_NAMES[197] = ELT_FECONVOLVEMATRIX;
  ELEMENT_NAMES[198] = ELT_ISINDEX;
  ELEMENT_NAMES[199] = ELT_BODY;
  ELEMENT_NAMES[200] = ELT_FEMORPHOLOGY;
  ELEMENT_NAMES[201] = ELT_RUBY;
  ELEMENT_NAMES[202] = ELT_SUMMARY;
  ELEMENT_NAMES[203] = ELT_TBODY;
}

void
nsHtml5ElementName::releaseStatics()
{
  delete ELT_ANNOTATION_XML;
  delete ELT_ISINDEX;
  delete ELT_BIG;
  delete ELT_BDO;
  delete ELT_COL;
  delete ELT_DEL;
  delete ELT_DFN;
  delete ELT_DIR;
  delete ELT_DIV;
  delete ELT_IMG;
  delete ELT_INS;
  delete ELT_KBD;
  delete ELT_MAP;
  delete ELT_NAV;
  delete ELT_PRE;
  delete ELT_A;
  delete ELT_B;
  delete ELT_RTC;
  delete ELT_SUB;
  delete ELT_SVG;
  delete ELT_SUP;
  delete ELT_SET;
  delete ELT_USE;
  delete ELT_VAR;
  delete ELT_G;
  delete ELT_WBR;
  delete ELT_XMP;
  delete ELT_I;
  delete ELT_P;
  delete ELT_Q;
  delete ELT_S;
  delete ELT_U;
  delete ELT_H1;
  delete ELT_H2;
  delete ELT_H3;
  delete ELT_H4;
  delete ELT_H5;
  delete ELT_H6;
  delete ELT_AREA;
  delete ELT_FEFUNCA;
  delete ELT_METADATA;
  delete ELT_META;
  delete ELT_TEXTAREA;
  delete ELT_FEFUNCB;
  delete ELT_RB;
  delete ELT_DESC;
  delete ELT_DD;
  delete ELT_BGSOUND;
  delete ELT_EMBED;
  delete ELT_FEBLEND;
  delete ELT_FEFLOOD;
  delete ELT_HEAD;
  delete ELT_LEGEND;
  delete ELT_NOEMBED;
  delete ELT_TD;
  delete ELT_THEAD;
  delete ELT_ASIDE;
  delete ELT_ARTICLE;
  delete ELT_ANIMATE;
  delete ELT_BASE;
  delete ELT_BLOCKQUOTE;
  delete ELT_CODE;
  delete ELT_CIRCLE;
  delete ELT_CITE;
  delete ELT_ELLIPSE;
  delete ELT_FETURBULENCE;
  delete ELT_FEMERGENODE;
  delete ELT_FEIMAGE;
  delete ELT_FEMERGE;
  delete ELT_FETILE;
  delete ELT_FRAME;
  delete ELT_FIGURE;
  delete ELT_FECOMPOSITE;
  delete ELT_IMAGE;
  delete ELT_IFRAME;
  delete ELT_LINE;
  delete ELT_MARQUEE;
  delete ELT_POLYLINE;
  delete ELT_PICTURE;
  delete ELT_SOURCE;
  delete ELT_STRIKE;
  delete ELT_STYLE;
  delete ELT_TABLE;
  delete ELT_TITLE;
  delete ELT_TIME;
  delete ELT_TEMPLATE;
  delete ELT_ALTGLYPHDEF;
  delete ELT_GLYPHREF;
  delete ELT_DIALOG;
  delete ELT_FEFUNCG;
  delete ELT_FEDIFFUSELIGHTING;
  delete ELT_FESPECULARLIGHTING;
  delete ELT_LISTING;
  delete ELT_STRONG;
  delete ELT_ALTGLYPH;
  delete ELT_CLIPPATH;
  delete ELT_MGLYPH;
  delete ELT_MATH;
  delete ELT_MPATH;
  delete ELT_PATH;
  delete ELT_TH;
  delete ELT_SWITCH;
  delete ELT_TEXTPATH;
  delete ELT_LI;
  delete ELT_MI;
  delete ELT_LINK;
  delete ELT_MARK;
  delete ELT_MALIGNMARK;
  delete ELT_MASK;
  delete ELT_TRACK;
  delete ELT_DL;
  delete ELT_HTML;
  delete ELT_OL;
  delete ELT_LABEL;
  delete ELT_UL;
  delete ELT_SMALL;
  delete ELT_SYMBOL;
  delete ELT_ALTGLYPHITEM;
  delete ELT_ANIMATETRANSFORM;
  delete ELT_ACRONYM;
  delete ELT_EM;
  delete ELT_FORM;
  delete ELT_MENUITEM;
  delete ELT_PARAM;
  delete ELT_ANIMATEMOTION;
  delete ELT_BUTTON;
  delete ELT_CAPTION;
  delete ELT_FIGCAPTION;
  delete ELT_MN;
  delete ELT_KEYGEN;
  delete ELT_MAIN;
  delete ELT_OPTION;
  delete ELT_POLYGON;
  delete ELT_PATTERN;
  delete ELT_SPAN;
  delete ELT_SECTION;
  delete ELT_TSPAN;
  delete ELT_AUDIO;
  delete ELT_MO;
  delete ELT_VIDEO;
  delete ELT_COLGROUP;
  delete ELT_FEDISPLACEMENTMAP;
  delete ELT_HGROUP;
  delete ELT_RP;
  delete ELT_OPTGROUP;
  delete ELT_SAMP;
  delete ELT_STOP;
  delete ELT_BR;
  delete ELT_ABBR;
  delete ELT_ANIMATECOLOR;
  delete ELT_CENTER;
  delete ELT_HR;
  delete ELT_FEFUNCR;
  delete ELT_FECOMPONENTTRANSFER;
  delete ELT_FILTER;
  delete ELT_FOOTER;
  delete ELT_FEGAUSSIANBLUR;
  delete ELT_HEADER;
  delete ELT_MARKER;
  delete ELT_METER;
  delete ELT_NOBR;
  delete ELT_TR;
  delete ELT_ADDRESS;
  delete ELT_CANVAS;
  delete ELT_DEFS;
  delete ELT_DETAILS;
  delete ELT_MS;
  delete ELT_NOFRAMES;
  delete ELT_PROGRESS;
  delete ELT_DT;
  delete ELT_APPLET;
  delete ELT_BASEFONT;
  delete ELT_FOREIGNOBJECT;
  delete ELT_FIELDSET;
  delete ELT_FRAMESET;
  delete ELT_FEOFFSET;
  delete ELT_FESPOTLIGHT;
  delete ELT_FEPOINTLIGHT;
  delete ELT_FEDISTANTLIGHT;
  delete ELT_FONT;
  delete ELT_INPUT;
  delete ELT_LINEARGRADIENT;
  delete ELT_MTEXT;
  delete ELT_NOSCRIPT;
  delete ELT_RT;
  delete ELT_OBJECT;
  delete ELT_OUTPUT;
  delete ELT_PLAINTEXT;
  delete ELT_TT;
  delete ELT_RECT;
  delete ELT_RADIALGRADIENT;
  delete ELT_SELECT;
  delete ELT_SCRIPT;
  delete ELT_TFOOT;
  delete ELT_TEXT;
  delete ELT_MENU;
  delete ELT_FEDROPSHADOW;
  delete ELT_VIEW;
  delete ELT_FECOLORMATRIX;
  delete ELT_FECONVOLVEMATRIX;
  delete ELT_BODY;
  delete ELT_FEMORPHOLOGY;
  delete ELT_RUBY;
  delete ELT_SUMMARY;
  delete ELT_TBODY;
  delete[] ELEMENT_NAMES;
}


