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

#include "nsAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5String.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsGkAtoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsHtml5Macros.h"
#include "nsIContentHandle.h"
#include "nsHtml5Portability.h"
#include "nsHtml5ContentCreatorFunction.h"

#include "nsHtml5AttributeName.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5ElementName.h"

nsHtml5ElementName::nsHtml5ElementName(
  nsAtom* name,
  nsAtom* camelCaseName,
  mozilla::dom::HTMLContentCreatorFunction htmlCreator,
  mozilla::dom::SVGContentCreatorFunction svgCreator,
  int32_t flags)
  : name(name)
  , camelCaseName(camelCaseName)
  , htmlCreator(htmlCreator)
  , svgCreator(svgCreator)
  , flags(flags)
{
  MOZ_COUNT_CTOR(nsHtml5ElementName);
}

nsHtml5ElementName::nsHtml5ElementName()
  : name(nullptr)
  , camelCaseName(nullptr)
  , htmlCreator(NS_NewHTMLUnknownElement)
  , svgCreator(NS_NewSVGUnknownElement)
  , flags(nsHtml5TreeBuilder::OTHER | NOT_INTERNED)
{
  MOZ_COUNT_CTOR(nsHtml5ElementName);
}


nsHtml5ElementName::~nsHtml5ElementName()
{
  MOZ_COUNT_DTOR(nsHtml5ElementName);
}

nsHtml5ElementName* nsHtml5ElementName::ELT_ANNOTATION_XML = nullptr;
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
nsHtml5ElementName* nsHtml5ElementName::ELT_DATA = nullptr;
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
nsHtml5ElementName* nsHtml5ElementName::ELT_DATALIST = nullptr;
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
nsHtml5ElementName* nsHtml5ElementName::ELT_SLOT = nullptr;
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
  1902641154, 1749395095, 2001349720, 893386754,  1803876550, 1971465813,
  2007781534, 59821379,   1732381397, 1756600614, 1870135298, 1939219752,
  1988763672, 2005324101, 2060065124, 55104723,   62450211,   1686489160,
  1747048757, 1749932347, 1782357526, 1818755074, 1881669634, 1907959605,
  1967760215, 1982935782, 1999397992, 2001392798, 2006329158, 2008851557,
  2085266636, 52485715,   57733651,   60354131,   67633153,   960495618,
  1703936002, 1736200310, 1747838298, 1749723735, 1753362711, 1757157700,
  1786534215, 1805647874, 1854245076, 1874102998, 1898223949, 1906087319,
  1932928296, 1965115924, 1968053806, 1973420034, 1983633431, 1998585858,
  2001309869, 2001392795, 2003183333, 2005925890, 2006974466, 2008325940,
  2021937364, 2068523856, 2092255447, 51438659,   52488851,   56151587,
  59244545,   60347747,   61925907,   63438849,   69730305,   926941186,
  1681770564, 1689922072, 1730150402, 1733076167, 1738539010, 1747306711,
  1748225318, 1749673195, 1749813541, 1751386406, 1755148615, 1757137429,
  1763839627, 1783388497, 1797585096, 1803929861, 1807599880, 1854228692,
  1864368130, 1873281026, 1881498736, 1887579800, 1899272519, 1904412884,
  1907435316, 1919418370, 1935549734, 1941221172, 1966223078, 1967795910,
  1971461414, 1971938532, 1982173479, 1983533124, 1986527234, 1990037800,
  1998724870, 2000525512, 2001349704, 2001349736, 2001392796, 2001495140,
  2004635806, 2005719336, 2006028454, 2006896969, 2007601444, 2008125638,
  2008340774, 2008994116, 2051837468, 2068523853, 2083120164, 2091479332,
  2092557349, 51434643,   51961587,   52486755,   52490899,   55110883,
  57206291,   58773795,   59768833,   60345171,   60352339,   61395251,
  62390273,   62973651,   67108865,   68681729,   876609538,  910163970,
  943718402,  1679960596, 1682547543, 1686491348, 1699324759, 1715310660,
  1730965751, 1733054663, 1733890180, 1737099991, 1740181637, 1747176599,
  1747814436, 1748100148, 1748359220, 1749656156, 1749715159, 1749801286,
  1749905526, 1751288021, 1752979652, 1755076808, 1756474198, 1756625221,
  1757146773, 1757268168, 1766992520, 1783210839, 1783388498, 1790207270,
  1798686984, 1803929812, 1805502724, 1806806678, 1818230786, 1853642948,
  1854228698, 1857653029, 1868312196, 1870268949, 1874053333, 1881288348,
  1881613047, 1884120164, 1889085973, 1898753862, 1900845386, 1903302038,
  1905563974, 1906135367, 1907661127, 1914900309, 1925844629, 1934172497,
  1938817026, 1941178676, 1963982850, 1965334268, 1967128578, 1967788867,
  1967795958, 1968836118
};
staticJArray<int32_t,int32_t> nsHtml5ElementName::ELEMENT_HASHES = { ELEMENT_HASHES_DATA, MOZ_ARRAY_LENGTH(ELEMENT_HASHES_DATA) };
void
nsHtml5ElementName::initializeStatics()
{
  ELT_ANNOTATION_XML = new nsHtml5ElementName(
    nsGkAtoms::annotation_xml_,
    nsGkAtoms::annotation_xml_,
    NS_NewHTMLUnknownElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::ANNOTATION_XML | SCOPING_AS_MATHML);
  ELT_BIG = new nsHtml5ElementName(
    nsGkAtoms::big,
    nsGkAtoms::big,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_BDO = new nsHtml5ElementName(nsGkAtoms::bdo,
                                   nsGkAtoms::bdo,
                                   NS_NewHTMLElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_COL = new nsHtml5ElementName(nsGkAtoms::col,
                                   nsGkAtoms::col,
                                   NS_NewHTMLTableColElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::COL | SPECIAL);
  ELT_DEL = new nsHtml5ElementName(nsGkAtoms::del,
                                   nsGkAtoms::del,
                                   NS_NewHTMLModElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_DFN = new nsHtml5ElementName(nsGkAtoms::dfn,
                                   nsGkAtoms::dfn,
                                   NS_NewHTMLElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_DIR = new nsHtml5ElementName(
    nsGkAtoms::dir,
    nsGkAtoms::dir,
    NS_NewHTMLSharedElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_DIV = new nsHtml5ElementName(
    nsGkAtoms::div,
    nsGkAtoms::div,
    NS_NewHTMLDivElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
  ELT_IMG = new nsHtml5ElementName(nsGkAtoms::img,
                                   nsGkAtoms::img,
                                   NS_NewHTMLImageElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::IMG | SPECIAL);
  ELT_INS = new nsHtml5ElementName(nsGkAtoms::ins,
                                   nsGkAtoms::ins,
                                   NS_NewHTMLModElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_KBD = new nsHtml5ElementName(nsGkAtoms::kbd,
                                   nsGkAtoms::kbd,
                                   NS_NewHTMLElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_MAP = new nsHtml5ElementName(nsGkAtoms::map,
                                   nsGkAtoms::map,
                                   NS_NewHTMLMapElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_NAV = new nsHtml5ElementName(
    nsGkAtoms::nav,
    nsGkAtoms::nav,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_PRE =
    new nsHtml5ElementName(nsGkAtoms::pre,
                           nsGkAtoms::pre,
                           NS_NewHTMLPreElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::PRE_OR_LISTING | SPECIAL);
  ELT_A = new nsHtml5ElementName(nsGkAtoms::a,
                                 nsGkAtoms::a,
                                 NS_NewHTMLAnchorElement,
                                 NS_NewSVGAElement,
                                 nsHtml5TreeBuilder::A);
  ELT_B = new nsHtml5ElementName(
    nsGkAtoms::b,
    nsGkAtoms::b,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_RTC =
    new nsHtml5ElementName(nsGkAtoms::rtc,
                           nsGkAtoms::rtc,
                           NS_NewHTMLElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::RB_OR_RTC | OPTIONAL_END_TAG);
  ELT_SUB = new nsHtml5ElementName(
    nsGkAtoms::sub,
    nsGkAtoms::sub,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SVG = new nsHtml5ElementName(nsGkAtoms::svg,
                                   nsGkAtoms::svg,
                                   NS_NewHTMLUnknownElement,
                                   NS_NewSVGSVGElement,
                                   nsHtml5TreeBuilder::SVG);
  ELT_SUP = new nsHtml5ElementName(
    nsGkAtoms::sup,
    nsGkAtoms::sup,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SET = new nsHtml5ElementName(nsGkAtoms::set_,
                                   nsGkAtoms::set_,
                                   NS_NewHTMLUnknownElement,
                                   NS_NewSVGSetElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_USE = new nsHtml5ElementName(nsGkAtoms::use,
                                   nsGkAtoms::use,
                                   NS_NewHTMLUnknownElement,
                                   NS_NewSVGUseElement,
                                   nsHtml5TreeBuilder::OTHER);
  ELT_VAR = new nsHtml5ElementName(
    nsGkAtoms::var,
    nsGkAtoms::var,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_G = new nsHtml5ElementName(nsGkAtoms::g,
                                 nsGkAtoms::g,
                                 NS_NewHTMLUnknownElement,
                                 NS_NewSVGGElement,
                                 nsHtml5TreeBuilder::OTHER);
  ELT_WBR = new nsHtml5ElementName(nsGkAtoms::wbr,
                                   nsGkAtoms::wbr,
                                   NS_NewHTMLElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::AREA_OR_WBR | SPECIAL);
  ELT_XMP = new nsHtml5ElementName(nsGkAtoms::xmp,
                                   nsGkAtoms::xmp,
                                   NS_NewHTMLPreElement,
                                   NS_NewSVGUnknownElement,
                                   nsHtml5TreeBuilder::XMP | SPECIAL);
  ELT_I = new nsHtml5ElementName(
    nsGkAtoms::i,
    nsGkAtoms::i,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_P =
    new nsHtml5ElementName(nsGkAtoms::p,
                           nsGkAtoms::p,
                           NS_NewHTMLParagraphElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::P | SPECIAL | OPTIONAL_END_TAG);
  ELT_Q = new nsHtml5ElementName(nsGkAtoms::q,
                                 nsGkAtoms::q,
                                 NS_NewHTMLSharedElement,
                                 NS_NewSVGUnknownElement,
                                 nsHtml5TreeBuilder::OTHER);
  ELT_S = new nsHtml5ElementName(
    nsGkAtoms::s,
    nsGkAtoms::s,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_U = new nsHtml5ElementName(
    nsGkAtoms::u,
    nsGkAtoms::u,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_H1 = new nsHtml5ElementName(
    nsGkAtoms::h1,
    nsGkAtoms::h1,
    NS_NewHTMLHeadingElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
  ELT_H2 = new nsHtml5ElementName(
    nsGkAtoms::h2,
    nsGkAtoms::h2,
    NS_NewHTMLHeadingElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
  ELT_H3 = new nsHtml5ElementName(
    nsGkAtoms::h3,
    nsGkAtoms::h3,
    NS_NewHTMLHeadingElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
  ELT_H4 = new nsHtml5ElementName(
    nsGkAtoms::h4,
    nsGkAtoms::h4,
    NS_NewHTMLHeadingElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
  ELT_H5 = new nsHtml5ElementName(
    nsGkAtoms::h5,
    nsGkAtoms::h5,
    NS_NewHTMLHeadingElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
  ELT_H6 = new nsHtml5ElementName(
    nsGkAtoms::h6,
    nsGkAtoms::h6,
    NS_NewHTMLHeadingElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
  ELT_AREA = new nsHtml5ElementName(nsGkAtoms::area,
                                    nsGkAtoms::area,
                                    NS_NewHTMLAreaElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::AREA_OR_WBR | SPECIAL);
  ELT_DATA = new nsHtml5ElementName(nsGkAtoms::data,
                                    nsGkAtoms::data,
                                    NS_NewHTMLDataElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_FEFUNCA = new nsHtml5ElementName(nsGkAtoms::fefunca,
                                       nsGkAtoms::feFuncA,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEFuncAElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_METADATA = new nsHtml5ElementName(nsGkAtoms::metadata,
                                        nsGkAtoms::metadata,
                                        NS_NewHTMLUnknownElement,
                                        NS_NewSVGMetadataElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_META = new nsHtml5ElementName(nsGkAtoms::meta,
                                    nsGkAtoms::meta,
                                    NS_NewHTMLMetaElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::META | SPECIAL);
  ELT_TEXTAREA = new nsHtml5ElementName(nsGkAtoms::textarea,
                                        nsGkAtoms::textarea,
                                        NS_NewHTMLTextAreaElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::TEXTAREA | SPECIAL);
  ELT_FEFUNCB = new nsHtml5ElementName(nsGkAtoms::fefuncb,
                                       nsGkAtoms::feFuncB,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEFuncBElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_RB =
    new nsHtml5ElementName(nsGkAtoms::rb,
                           nsGkAtoms::rb,
                           NS_NewHTMLElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::RB_OR_RTC | OPTIONAL_END_TAG);
  ELT_DESC = new nsHtml5ElementName(nsGkAtoms::desc,
                                    nsGkAtoms::desc,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGDescElement,
                                    nsHtml5TreeBuilder::FOREIGNOBJECT_OR_DESC |
                                      SCOPING_AS_SVG);
  ELT_DD = new nsHtml5ElementName(nsGkAtoms::dd,
                                  nsGkAtoms::dd,
                                  NS_NewHTMLElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::DD_OR_DT | SPECIAL |
                                    OPTIONAL_END_TAG);
  ELT_BGSOUND = new nsHtml5ElementName(
    nsGkAtoms::bgsound,
    nsGkAtoms::bgsound,
    NS_NewHTMLUnknownElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
  ELT_EMBED = new nsHtml5ElementName(nsGkAtoms::embed,
                                     nsGkAtoms::embed,
                                     NS_NewHTMLEmbedElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::EMBED | SPECIAL);
  ELT_FEBLEND = new nsHtml5ElementName(nsGkAtoms::feblend,
                                       nsGkAtoms::feBlend,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEBlendElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_FEFLOOD = new nsHtml5ElementName(nsGkAtoms::feflood,
                                       nsGkAtoms::feFlood,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEFloodElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_HEAD = new nsHtml5ElementName(nsGkAtoms::head,
                                    nsGkAtoms::head,
                                    NS_NewHTMLSharedElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::HEAD | SPECIAL |
                                      OPTIONAL_END_TAG);
  ELT_LEGEND = new nsHtml5ElementName(nsGkAtoms::legend,
                                      nsGkAtoms::legend,
                                      NS_NewHTMLLegendElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_NOEMBED = new nsHtml5ElementName(nsGkAtoms::noembed,
                                       nsGkAtoms::noembed,
                                       NS_NewHTMLElement,
                                       NS_NewSVGUnknownElement,
                                       nsHtml5TreeBuilder::NOEMBED | SPECIAL);
  ELT_TD = new nsHtml5ElementName(nsGkAtoms::td,
                                  nsGkAtoms::td,
                                  NS_NewHTMLTableCellElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::TD_OR_TH | SPECIAL |
                                    SCOPING | OPTIONAL_END_TAG);
  ELT_THEAD =
    new nsHtml5ElementName(nsGkAtoms::thead,
                           nsGkAtoms::thead,
                           NS_NewHTMLTableSectionElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::TBODY_OR_THEAD_OR_TFOOT |
                             SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
  ELT_ASIDE = new nsHtml5ElementName(
    nsGkAtoms::aside,
    nsGkAtoms::aside,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_ARTICLE = new nsHtml5ElementName(
    nsGkAtoms::article,
    nsGkAtoms::article,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_ANIMATE = new nsHtml5ElementName(nsGkAtoms::animate,
                                       nsGkAtoms::animate,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGAnimateElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_BASE = new nsHtml5ElementName(nsGkAtoms::base,
                                    nsGkAtoms::base,
                                    NS_NewHTMLSharedElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::BASE | SPECIAL);
  ELT_BLOCKQUOTE = new nsHtml5ElementName(
    nsGkAtoms::blockquote,
    nsGkAtoms::blockquote,
    NS_NewHTMLSharedElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
  ELT_CODE = new nsHtml5ElementName(
    nsGkAtoms::code,
    nsGkAtoms::code,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_CIRCLE = new nsHtml5ElementName(nsGkAtoms::circle,
                                      nsGkAtoms::circle,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGCircleElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_CITE = new nsHtml5ElementName(nsGkAtoms::cite,
                                    nsGkAtoms::cite,
                                    NS_NewHTMLElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_ELLIPSE = new nsHtml5ElementName(nsGkAtoms::ellipse,
                                       nsGkAtoms::ellipse,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGEllipseElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_FETURBULENCE = new nsHtml5ElementName(nsGkAtoms::feturbulence,
                                            nsGkAtoms::feTurbulence,
                                            NS_NewHTMLUnknownElement,
                                            NS_NewSVGFETurbulenceElement,
                                            nsHtml5TreeBuilder::OTHER);
  ELT_FEMERGENODE = new nsHtml5ElementName(nsGkAtoms::femergenode,
                                           nsGkAtoms::feMergeNode,
                                           NS_NewHTMLUnknownElement,
                                           NS_NewSVGFEMergeNodeElement,
                                           nsHtml5TreeBuilder::OTHER);
  ELT_FEIMAGE = new nsHtml5ElementName(nsGkAtoms::feimage,
                                       nsGkAtoms::feImage,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEImageElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_FEMERGE = new nsHtml5ElementName(nsGkAtoms::femerge,
                                       nsGkAtoms::feMerge,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEMergeElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_FETILE = new nsHtml5ElementName(nsGkAtoms::fetile,
                                      nsGkAtoms::feTile,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGFETileElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_FRAME = new nsHtml5ElementName(nsGkAtoms::frame,
                                     nsGkAtoms::frame,
                                     NS_NewHTMLFrameElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::FRAME | SPECIAL);
  ELT_FIGURE = new nsHtml5ElementName(
    nsGkAtoms::figure,
    nsGkAtoms::figure,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_FECOMPOSITE = new nsHtml5ElementName(nsGkAtoms::fecomposite,
                                           nsGkAtoms::feComposite,
                                           NS_NewHTMLUnknownElement,
                                           NS_NewSVGFECompositeElement,
                                           nsHtml5TreeBuilder::OTHER);
  ELT_IMAGE = new nsHtml5ElementName(nsGkAtoms::image,
                                     nsGkAtoms::image,
                                     NS_NewHTMLElement,
                                     NS_NewSVGImageElement,
                                     nsHtml5TreeBuilder::IMAGE);
  ELT_IFRAME = new nsHtml5ElementName(nsGkAtoms::iframe,
                                      nsGkAtoms::iframe,
                                      NS_NewHTMLIFrameElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::IFRAME | SPECIAL);
  ELT_LINE = new nsHtml5ElementName(nsGkAtoms::line,
                                    nsGkAtoms::line,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGLineElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_MARQUEE = new nsHtml5ElementName(nsGkAtoms::marquee,
                                       nsGkAtoms::marquee,
                                       NS_NewHTMLDivElement,
                                       NS_NewSVGUnknownElement,
                                       nsHtml5TreeBuilder::MARQUEE_OR_APPLET |
                                         SPECIAL | SCOPING);
  ELT_POLYLINE = new nsHtml5ElementName(nsGkAtoms::polyline,
                                        nsGkAtoms::polyline,
                                        NS_NewHTMLUnknownElement,
                                        NS_NewSVGPolylineElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_PICTURE = new nsHtml5ElementName(nsGkAtoms::picture,
                                       nsGkAtoms::picture,
                                       NS_NewHTMLPictureElement,
                                       NS_NewSVGUnknownElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_SOURCE =
    new nsHtml5ElementName(nsGkAtoms::source,
                           nsGkAtoms::source,
                           NS_NewHTMLSourceElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::PARAM_OR_SOURCE_OR_TRACK);
  ELT_STRIKE = new nsHtml5ElementName(
    nsGkAtoms::strike,
    nsGkAtoms::strike,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_STYLE = new nsHtml5ElementName(nsGkAtoms::style,
                                     nsGkAtoms::style,
                                     NS_NewHTMLStyleElement,
                                     NS_NewSVGStyleElement,
                                     nsHtml5TreeBuilder::STYLE | SPECIAL);
  ELT_TABLE = new nsHtml5ElementName(nsGkAtoms::table,
                                     nsGkAtoms::table,
                                     NS_NewHTMLTableElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::TABLE | SPECIAL |
                                       FOSTER_PARENTING | SCOPING);
  ELT_TITLE = new nsHtml5ElementName(nsGkAtoms::title,
                                     nsGkAtoms::title,
                                     NS_NewHTMLTitleElement,
                                     NS_NewSVGTitleElement,
                                     nsHtml5TreeBuilder::TITLE | SPECIAL |
                                       SCOPING_AS_SVG);
  ELT_TIME = new nsHtml5ElementName(nsGkAtoms::time,
                                    nsGkAtoms::time,
                                    NS_NewHTMLTimeElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_TEMPLATE =
    new nsHtml5ElementName(nsGkAtoms::_template,
                           nsGkAtoms::_template,
                           NS_NewHTMLTemplateElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::TEMPLATE | SPECIAL | SCOPING);
  ELT_ALTGLYPHDEF = new nsHtml5ElementName(nsGkAtoms::altglyphdef,
                                           nsGkAtoms::altGlyphDef,
                                           NS_NewHTMLUnknownElement,
                                           NS_NewSVGUnknownElement,
                                           nsHtml5TreeBuilder::OTHER);
  ELT_GLYPHREF = new nsHtml5ElementName(nsGkAtoms::glyphref,
                                        nsGkAtoms::glyphRef,
                                        NS_NewHTMLUnknownElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_DIALOG = new nsHtml5ElementName(
    nsGkAtoms::dialog,
    nsGkAtoms::dialog,
    NS_NewHTMLDialogElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_FEFUNCG = new nsHtml5ElementName(nsGkAtoms::fefuncg,
                                       nsGkAtoms::feFuncG,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEFuncGElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_FEDIFFUSELIGHTING =
    new nsHtml5ElementName(nsGkAtoms::fediffuselighting,
                           nsGkAtoms::feDiffuseLighting,
                           NS_NewHTMLUnknownElement,
                           NS_NewSVGFEDiffuseLightingElement,
                           nsHtml5TreeBuilder::OTHER);
  ELT_FESPECULARLIGHTING =
    new nsHtml5ElementName(nsGkAtoms::fespecularlighting,
                           nsGkAtoms::feSpecularLighting,
                           NS_NewHTMLUnknownElement,
                           NS_NewSVGFESpecularLightingElement,
                           nsHtml5TreeBuilder::OTHER);
  ELT_LISTING =
    new nsHtml5ElementName(nsGkAtoms::listing,
                           nsGkAtoms::listing,
                           NS_NewHTMLPreElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::PRE_OR_LISTING | SPECIAL);
  ELT_STRONG = new nsHtml5ElementName(
    nsGkAtoms::strong,
    nsGkAtoms::strong,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_ALTGLYPH = new nsHtml5ElementName(nsGkAtoms::altglyph,
                                        nsGkAtoms::altGlyph,
                                        NS_NewHTMLUnknownElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_CLIPPATH = new nsHtml5ElementName(nsGkAtoms::clippath,
                                        nsGkAtoms::clipPath,
                                        NS_NewHTMLUnknownElement,
                                        NS_NewSVGClipPathElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_MGLYPH = new nsHtml5ElementName(nsGkAtoms::mglyph_,
                                      nsGkAtoms::mglyph_,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::MGLYPH_OR_MALIGNMARK);
  ELT_MATH = new nsHtml5ElementName(nsGkAtoms::math,
                                    nsGkAtoms::math,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::MATH);
  ELT_MPATH = new nsHtml5ElementName(nsGkAtoms::mpath,
                                     nsGkAtoms::mpath,
                                     NS_NewHTMLUnknownElement,
                                     NS_NewSVGMPathElement,
                                     nsHtml5TreeBuilder::OTHER);
  ELT_PATH = new nsHtml5ElementName(nsGkAtoms::path,
                                    nsGkAtoms::path,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGPathElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_TH = new nsHtml5ElementName(nsGkAtoms::th,
                                  nsGkAtoms::th,
                                  NS_NewHTMLTableCellElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::TD_OR_TH | SPECIAL |
                                    SCOPING | OPTIONAL_END_TAG);
  ELT_SWITCH = new nsHtml5ElementName(nsGkAtoms::svgSwitch,
                                      nsGkAtoms::svgSwitch,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGSwitchElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_TEXTPATH = new nsHtml5ElementName(nsGkAtoms::textpath,
                                        nsGkAtoms::textPath,
                                        NS_NewHTMLUnknownElement,
                                        NS_NewSVGTextPathElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_LI =
    new nsHtml5ElementName(nsGkAtoms::li,
                           nsGkAtoms::li,
                           NS_NewHTMLLIElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::LI | SPECIAL | OPTIONAL_END_TAG);
  ELT_MI = new nsHtml5ElementName(nsGkAtoms::mi_,
                                  nsGkAtoms::mi_,
                                  NS_NewHTMLUnknownElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::MI_MO_MN_MS_MTEXT |
                                    SCOPING_AS_MATHML);
  ELT_LINK = new nsHtml5ElementName(
    nsGkAtoms::link,
    nsGkAtoms::link,
    NS_NewHTMLLinkElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
  ELT_MARK = new nsHtml5ElementName(nsGkAtoms::mark,
                                    nsGkAtoms::mark,
                                    NS_NewHTMLElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_MALIGNMARK =
    new nsHtml5ElementName(nsGkAtoms::malignmark_,
                           nsGkAtoms::malignmark_,
                           NS_NewHTMLUnknownElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::MGLYPH_OR_MALIGNMARK);
  ELT_MASK = new nsHtml5ElementName(nsGkAtoms::mask,
                                    nsGkAtoms::mask,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGMaskElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_TRACK = new nsHtml5ElementName(
    nsGkAtoms::track,
    nsGkAtoms::track,
    NS_NewHTMLTrackElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
  ELT_DL = new nsHtml5ElementName(nsGkAtoms::dl,
                                  nsGkAtoms::dl,
                                  NS_NewHTMLSharedListElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::UL_OR_OL_OR_DL | SPECIAL);
  ELT_HTML = new nsHtml5ElementName(nsGkAtoms::html,
                                    nsGkAtoms::html,
                                    NS_NewHTMLSharedElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::HTML | SPECIAL |
                                      SCOPING | OPTIONAL_END_TAG);
  ELT_OL = new nsHtml5ElementName(nsGkAtoms::ol,
                                  nsGkAtoms::ol,
                                  NS_NewHTMLSharedListElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::UL_OR_OL_OR_DL | SPECIAL);
  ELT_LABEL = new nsHtml5ElementName(nsGkAtoms::label,
                                     nsGkAtoms::label,
                                     NS_NewHTMLLabelElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::OTHER);
  ELT_UL = new nsHtml5ElementName(nsGkAtoms::ul,
                                  nsGkAtoms::ul,
                                  NS_NewHTMLSharedListElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::UL_OR_OL_OR_DL | SPECIAL);
  ELT_SMALL = new nsHtml5ElementName(
    nsGkAtoms::small,
    nsGkAtoms::small,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_SYMBOL = new nsHtml5ElementName(nsGkAtoms::symbol,
                                      nsGkAtoms::symbol,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGSymbolElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_ALTGLYPHITEM = new nsHtml5ElementName(nsGkAtoms::altglyphitem,
                                            nsGkAtoms::altGlyphItem,
                                            NS_NewHTMLUnknownElement,
                                            NS_NewSVGUnknownElement,
                                            nsHtml5TreeBuilder::OTHER);
  ELT_ANIMATETRANSFORM =
    new nsHtml5ElementName(nsGkAtoms::animatetransform,
                           nsGkAtoms::animateTransform,
                           NS_NewHTMLUnknownElement,
                           NS_NewSVGAnimateTransformElement,
                           nsHtml5TreeBuilder::OTHER);
  ELT_ACRONYM = new nsHtml5ElementName(nsGkAtoms::acronym,
                                       nsGkAtoms::acronym,
                                       NS_NewHTMLElement,
                                       NS_NewSVGUnknownElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_EM = new nsHtml5ElementName(
    nsGkAtoms::em,
    nsGkAtoms::em,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_FORM = new nsHtml5ElementName(nsGkAtoms::form,
                                    nsGkAtoms::form,
                                    NS_NewHTMLFormElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::FORM | SPECIAL);
  ELT_MENUITEM = new nsHtml5ElementName(nsGkAtoms::menuitem,
                                        nsGkAtoms::menuitem,
                                        NS_NewHTMLMenuItemElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::MENUITEM);
  ELT_PARAM = new nsHtml5ElementName(
    nsGkAtoms::param,
    nsGkAtoms::param,
    NS_NewHTMLSharedElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
  ELT_ANIMATEMOTION = new nsHtml5ElementName(nsGkAtoms::animatemotion,
                                             nsGkAtoms::animateMotion,
                                             NS_NewHTMLUnknownElement,
                                             NS_NewSVGAnimateMotionElement,
                                             nsHtml5TreeBuilder::OTHER);
  ELT_BUTTON = new nsHtml5ElementName(nsGkAtoms::button,
                                      nsGkAtoms::button,
                                      NS_NewHTMLButtonElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::BUTTON | SPECIAL);
  ELT_CAPTION =
    new nsHtml5ElementName(nsGkAtoms::caption,
                           nsGkAtoms::caption,
                           NS_NewHTMLTableCaptionElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::CAPTION | SPECIAL | SCOPING);
  ELT_FIGCAPTION = new nsHtml5ElementName(
    nsGkAtoms::figcaption,
    nsGkAtoms::figcaption,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_MN = new nsHtml5ElementName(nsGkAtoms::mn_,
                                  nsGkAtoms::mn_,
                                  NS_NewHTMLUnknownElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::MI_MO_MN_MS_MTEXT |
                                    SCOPING_AS_MATHML);
  ELT_KEYGEN = new nsHtml5ElementName(nsGkAtoms::keygen,
                                      nsGkAtoms::keygen,
                                      NS_NewHTMLSpanElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::KEYGEN);
  ELT_MAIN = new nsHtml5ElementName(
    nsGkAtoms::main,
    nsGkAtoms::main,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_OPTION =
    new nsHtml5ElementName(nsGkAtoms::option,
                           nsGkAtoms::option,
                           NS_NewHTMLOptionElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::OPTION | OPTIONAL_END_TAG);
  ELT_POLYGON = new nsHtml5ElementName(nsGkAtoms::polygon,
                                       nsGkAtoms::polygon,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGPolygonElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_PATTERN = new nsHtml5ElementName(nsGkAtoms::pattern,
                                       nsGkAtoms::pattern,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGPatternElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_SPAN = new nsHtml5ElementName(
    nsGkAtoms::span,
    nsGkAtoms::span,
    NS_NewHTMLSpanElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SECTION = new nsHtml5ElementName(
    nsGkAtoms::section,
    nsGkAtoms::section,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_TSPAN = new nsHtml5ElementName(nsGkAtoms::tspan,
                                     nsGkAtoms::tspan,
                                     NS_NewHTMLUnknownElement,
                                     NS_NewSVGTSpanElement,
                                     nsHtml5TreeBuilder::OTHER);
  ELT_AUDIO = new nsHtml5ElementName(nsGkAtoms::audio,
                                     nsGkAtoms::audio,
                                     NS_NewHTMLAudioElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::OTHER);
  ELT_MO = new nsHtml5ElementName(nsGkAtoms::mo_,
                                  nsGkAtoms::mo_,
                                  NS_NewHTMLUnknownElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::MI_MO_MN_MS_MTEXT |
                                    SCOPING_AS_MATHML);
  ELT_VIDEO = new nsHtml5ElementName(nsGkAtoms::video,
                                     nsGkAtoms::video,
                                     NS_NewHTMLVideoElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::OTHER);
  ELT_COLGROUP = new nsHtml5ElementName(nsGkAtoms::colgroup,
                                        nsGkAtoms::colgroup,
                                        NS_NewHTMLTableColElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::COLGROUP | SPECIAL |
                                          OPTIONAL_END_TAG);
  ELT_FEDISPLACEMENTMAP =
    new nsHtml5ElementName(nsGkAtoms::fedisplacementmap,
                           nsGkAtoms::feDisplacementMap,
                           NS_NewHTMLUnknownElement,
                           NS_NewSVGFEDisplacementMapElement,
                           nsHtml5TreeBuilder::OTHER);
  ELT_HGROUP = new nsHtml5ElementName(
    nsGkAtoms::hgroup,
    nsGkAtoms::hgroup,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_RP =
    new nsHtml5ElementName(nsGkAtoms::rp,
                           nsGkAtoms::rp,
                           NS_NewHTMLElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::RT_OR_RP | OPTIONAL_END_TAG);
  ELT_OPTGROUP =
    new nsHtml5ElementName(nsGkAtoms::optgroup,
                           nsGkAtoms::optgroup,
                           NS_NewHTMLOptGroupElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::OPTGROUP | OPTIONAL_END_TAG);
  ELT_SAMP = new nsHtml5ElementName(nsGkAtoms::samp,
                                    nsGkAtoms::samp,
                                    NS_NewHTMLElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_STOP = new nsHtml5ElementName(nsGkAtoms::stop,
                                    nsGkAtoms::stop,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGStopElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_BR = new nsHtml5ElementName(nsGkAtoms::br,
                                  nsGkAtoms::br,
                                  NS_NewHTMLBRElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::BR | SPECIAL);
  ELT_ABBR = new nsHtml5ElementName(nsGkAtoms::abbr,
                                    nsGkAtoms::abbr,
                                    NS_NewHTMLElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_ANIMATECOLOR = new nsHtml5ElementName(nsGkAtoms::animatecolor,
                                            nsGkAtoms::animateColor,
                                            NS_NewHTMLUnknownElement,
                                            NS_NewSVGUnknownElement,
                                            nsHtml5TreeBuilder::OTHER);
  ELT_CENTER = new nsHtml5ElementName(
    nsGkAtoms::center,
    nsGkAtoms::center,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
  ELT_HR = new nsHtml5ElementName(nsGkAtoms::hr,
                                  nsGkAtoms::hr,
                                  NS_NewHTMLHRElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::HR | SPECIAL);
  ELT_FEFUNCR = new nsHtml5ElementName(nsGkAtoms::fefuncr,
                                       nsGkAtoms::feFuncR,
                                       NS_NewHTMLUnknownElement,
                                       NS_NewSVGFEFuncRElement,
                                       nsHtml5TreeBuilder::OTHER);
  ELT_FECOMPONENTTRANSFER =
    new nsHtml5ElementName(nsGkAtoms::fecomponenttransfer,
                           nsGkAtoms::feComponentTransfer,
                           NS_NewHTMLUnknownElement,
                           NS_NewSVGFEComponentTransferElement,
                           nsHtml5TreeBuilder::OTHER);
  ELT_FILTER = new nsHtml5ElementName(nsGkAtoms::filter,
                                      nsGkAtoms::filter,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGFilterElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_FOOTER = new nsHtml5ElementName(
    nsGkAtoms::footer,
    nsGkAtoms::footer,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_FEGAUSSIANBLUR = new nsHtml5ElementName(nsGkAtoms::fegaussianblur,
                                              nsGkAtoms::feGaussianBlur,
                                              NS_NewHTMLUnknownElement,
                                              NS_NewSVGFEGaussianBlurElement,
                                              nsHtml5TreeBuilder::OTHER);
  ELT_HEADER = new nsHtml5ElementName(
    nsGkAtoms::header,
    nsGkAtoms::header,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_MARKER = new nsHtml5ElementName(nsGkAtoms::marker,
                                      nsGkAtoms::marker,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGMarkerElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_METER = new nsHtml5ElementName(nsGkAtoms::meter,
                                     nsGkAtoms::meter,
                                     NS_NewHTMLMeterElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::OTHER);
  ELT_NOBR = new nsHtml5ElementName(nsGkAtoms::nobr,
                                    nsGkAtoms::nobr,
                                    NS_NewHTMLElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::NOBR);
  ELT_TR = new nsHtml5ElementName(nsGkAtoms::tr,
                                  nsGkAtoms::tr,
                                  NS_NewHTMLTableRowElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::TR | SPECIAL |
                                    FOSTER_PARENTING | OPTIONAL_END_TAG);
  ELT_ADDRESS = new nsHtml5ElementName(
    nsGkAtoms::address,
    nsGkAtoms::address,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_CANVAS = new nsHtml5ElementName(nsGkAtoms::canvas,
                                      nsGkAtoms::canvas,
                                      NS_NewHTMLCanvasElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::OTHER);
  ELT_DEFS = new nsHtml5ElementName(nsGkAtoms::defs,
                                    nsGkAtoms::defs,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGDefsElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_DETAILS = new nsHtml5ElementName(
    nsGkAtoms::details,
    nsGkAtoms::details,
    NS_NewHTMLDetailsElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_MS = new nsHtml5ElementName(nsGkAtoms::ms_,
                                  nsGkAtoms::ms_,
                                  NS_NewHTMLUnknownElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::MI_MO_MN_MS_MTEXT |
                                    SCOPING_AS_MATHML);
  ELT_NOFRAMES = new nsHtml5ElementName(nsGkAtoms::noframes,
                                        nsGkAtoms::noframes,
                                        NS_NewHTMLElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::NOFRAMES | SPECIAL);
  ELT_PROGRESS = new nsHtml5ElementName(nsGkAtoms::progress,
                                        nsGkAtoms::progress,
                                        NS_NewHTMLProgressElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_DT = new nsHtml5ElementName(nsGkAtoms::dt,
                                  nsGkAtoms::dt,
                                  NS_NewHTMLElement,
                                  NS_NewSVGUnknownElement,
                                  nsHtml5TreeBuilder::DD_OR_DT | SPECIAL |
                                    OPTIONAL_END_TAG);
  ELT_APPLET = new nsHtml5ElementName(nsGkAtoms::applet,
                                      nsGkAtoms::applet,
                                      NS_NewHTMLUnknownElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::MARQUEE_OR_APPLET |
                                        SPECIAL | SCOPING);
  ELT_BASEFONT = new nsHtml5ElementName(
    nsGkAtoms::basefont,
    nsGkAtoms::basefont,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
  ELT_DATALIST = new nsHtml5ElementName(nsGkAtoms::datalist,
                                        nsGkAtoms::datalist,
                                        NS_NewHTMLDataListElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_FOREIGNOBJECT = new nsHtml5ElementName(
    nsGkAtoms::foreignobject,
    nsGkAtoms::foreignObject,
    NS_NewHTMLUnknownElement,
    NS_NewSVGForeignObjectElement,
    nsHtml5TreeBuilder::FOREIGNOBJECT_OR_DESC | SCOPING_AS_SVG);
  ELT_FIELDSET = new nsHtml5ElementName(nsGkAtoms::fieldset,
                                        nsGkAtoms::fieldset,
                                        NS_NewHTMLFieldSetElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::FIELDSET | SPECIAL);
  ELT_FRAMESET = new nsHtml5ElementName(nsGkAtoms::frameset,
                                        nsGkAtoms::frameset,
                                        NS_NewHTMLFrameSetElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::FRAMESET | SPECIAL);
  ELT_FEOFFSET = new nsHtml5ElementName(nsGkAtoms::feoffset,
                                        nsGkAtoms::feOffset,
                                        NS_NewHTMLUnknownElement,
                                        NS_NewSVGFEOffsetElement,
                                        nsHtml5TreeBuilder::OTHER);
  ELT_FESPOTLIGHT = new nsHtml5ElementName(nsGkAtoms::fespotlight,
                                           nsGkAtoms::feSpotLight,
                                           NS_NewHTMLUnknownElement,
                                           NS_NewSVGFESpotLightElement,
                                           nsHtml5TreeBuilder::OTHER);
  ELT_FEPOINTLIGHT = new nsHtml5ElementName(nsGkAtoms::fepointlight,
                                            nsGkAtoms::fePointLight,
                                            NS_NewHTMLUnknownElement,
                                            NS_NewSVGFEPointLightElement,
                                            nsHtml5TreeBuilder::OTHER);
  ELT_FEDISTANTLIGHT = new nsHtml5ElementName(nsGkAtoms::fedistantlight,
                                              nsGkAtoms::feDistantLight,
                                              NS_NewHTMLUnknownElement,
                                              NS_NewSVGFEDistantLightElement,
                                              nsHtml5TreeBuilder::OTHER);
  ELT_FONT = new nsHtml5ElementName(nsGkAtoms::font,
                                    nsGkAtoms::font,
                                    NS_NewHTMLFontElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::FONT);
  ELT_INPUT = new nsHtml5ElementName(nsGkAtoms::input,
                                     nsGkAtoms::input,
                                     NS_NewHTMLInputElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::INPUT | SPECIAL);
  ELT_LINEARGRADIENT = new nsHtml5ElementName(nsGkAtoms::lineargradient,
                                              nsGkAtoms::linearGradient,
                                              NS_NewHTMLUnknownElement,
                                              NS_NewSVGLinearGradientElement,
                                              nsHtml5TreeBuilder::OTHER);
  ELT_MTEXT = new nsHtml5ElementName(nsGkAtoms::mtext_,
                                     nsGkAtoms::mtext_,
                                     NS_NewHTMLUnknownElement,
                                     NS_NewSVGUnknownElement,
                                     nsHtml5TreeBuilder::MI_MO_MN_MS_MTEXT |
                                       SCOPING_AS_MATHML);
  ELT_NOSCRIPT = new nsHtml5ElementName(nsGkAtoms::noscript,
                                        nsGkAtoms::noscript,
                                        NS_NewHTMLElement,
                                        NS_NewSVGUnknownElement,
                                        nsHtml5TreeBuilder::NOSCRIPT | SPECIAL);
  ELT_RT =
    new nsHtml5ElementName(nsGkAtoms::rt,
                           nsGkAtoms::rt,
                           NS_NewHTMLElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::RT_OR_RP | OPTIONAL_END_TAG);
  ELT_OBJECT =
    new nsHtml5ElementName(nsGkAtoms::object,
                           nsGkAtoms::object,
                           NS_NewHTMLObjectElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::OBJECT | SPECIAL | SCOPING);
  ELT_OUTPUT = new nsHtml5ElementName(nsGkAtoms::output,
                                      nsGkAtoms::output,
                                      NS_NewHTMLOutputElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::OUTPUT);
  ELT_PLAINTEXT =
    new nsHtml5ElementName(nsGkAtoms::plaintext,
                           nsGkAtoms::plaintext,
                           NS_NewHTMLElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::PLAINTEXT | SPECIAL);
  ELT_TT = new nsHtml5ElementName(
    nsGkAtoms::tt,
    nsGkAtoms::tt,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
      B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
  ELT_RECT = new nsHtml5ElementName(nsGkAtoms::rect,
                                    nsGkAtoms::rect,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGRectElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_RADIALGRADIENT = new nsHtml5ElementName(nsGkAtoms::radialgradient,
                                              nsGkAtoms::radialGradient,
                                              NS_NewHTMLUnknownElement,
                                              NS_NewSVGRadialGradientElement,
                                              nsHtml5TreeBuilder::OTHER);
  ELT_SELECT = new nsHtml5ElementName(nsGkAtoms::select,
                                      nsGkAtoms::select,
                                      NS_NewHTMLSelectElement,
                                      NS_NewSVGUnknownElement,
                                      nsHtml5TreeBuilder::SELECT | SPECIAL);
  ELT_SLOT = new nsHtml5ElementName(nsGkAtoms::slot,
                                    nsGkAtoms::slot,
                                    NS_NewHTMLSlotElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_SCRIPT = new nsHtml5ElementName(nsGkAtoms::script,
                                      nsGkAtoms::script,
                                      NS_NewHTMLScriptElement,
                                      NS_NewSVGScriptElement,
                                      nsHtml5TreeBuilder::SCRIPT | SPECIAL);
  ELT_TFOOT =
    new nsHtml5ElementName(nsGkAtoms::tfoot,
                           nsGkAtoms::tfoot,
                           NS_NewHTMLTableSectionElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::TBODY_OR_THEAD_OR_TFOOT |
                             SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
  ELT_TEXT = new nsHtml5ElementName(nsGkAtoms::text,
                                    nsGkAtoms::text,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGTextElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_MENU = new nsHtml5ElementName(
    nsGkAtoms::menu,
    nsGkAtoms::menu,
    NS_NewHTMLMenuElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
  ELT_FEDROPSHADOW = new nsHtml5ElementName(nsGkAtoms::fedropshadow,
                                            nsGkAtoms::feDropShadow,
                                            NS_NewHTMLUnknownElement,
                                            NS_NewSVGFEDropShadowElement,
                                            nsHtml5TreeBuilder::OTHER);
  ELT_VIEW = new nsHtml5ElementName(nsGkAtoms::view,
                                    nsGkAtoms::view,
                                    NS_NewHTMLUnknownElement,
                                    NS_NewSVGViewElement,
                                    nsHtml5TreeBuilder::OTHER);
  ELT_FECOLORMATRIX = new nsHtml5ElementName(nsGkAtoms::fecolormatrix,
                                             nsGkAtoms::feColorMatrix,
                                             NS_NewHTMLUnknownElement,
                                             NS_NewSVGFEColorMatrixElement,
                                             nsHtml5TreeBuilder::OTHER);
  ELT_FECONVOLVEMATRIX =
    new nsHtml5ElementName(nsGkAtoms::feconvolvematrix,
                           nsGkAtoms::feConvolveMatrix,
                           NS_NewHTMLUnknownElement,
                           NS_NewSVGFEConvolveMatrixElement,
                           nsHtml5TreeBuilder::OTHER);
  ELT_BODY = new nsHtml5ElementName(nsGkAtoms::body,
                                    nsGkAtoms::body,
                                    NS_NewHTMLBodyElement,
                                    NS_NewSVGUnknownElement,
                                    nsHtml5TreeBuilder::BODY | SPECIAL |
                                      OPTIONAL_END_TAG);
  ELT_FEMORPHOLOGY = new nsHtml5ElementName(nsGkAtoms::femorphology,
                                            nsGkAtoms::feMorphology,
                                            NS_NewHTMLUnknownElement,
                                            NS_NewSVGFEMorphologyElement,
                                            nsHtml5TreeBuilder::OTHER);
  ELT_RUBY = new nsHtml5ElementName(
    nsGkAtoms::ruby,
    nsGkAtoms::ruby,
    NS_NewHTMLElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
  ELT_SUMMARY = new nsHtml5ElementName(
    nsGkAtoms::summary,
    nsGkAtoms::summary,
    NS_NewHTMLSummaryElement,
    NS_NewSVGUnknownElement,
    nsHtml5TreeBuilder::
        ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY |
      SPECIAL);
  ELT_TBODY =
    new nsHtml5ElementName(nsGkAtoms::tbody,
                           nsGkAtoms::tbody,
                           NS_NewHTMLTableSectionElement,
                           NS_NewSVGUnknownElement,
                           nsHtml5TreeBuilder::TBODY_OR_THEAD_OR_TFOOT |
                             SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
  ELEMENT_NAMES = new nsHtml5ElementName*[206];
  ELEMENT_NAMES[0] = ELT_MN;
  ELEMENT_NAMES[1] = ELT_ELLIPSE;
  ELEMENT_NAMES[2] = ELT_FRAMESET;
  ELEMENT_NAMES[3] = ELT_H2;
  ELEMENT_NAMES[4] = ELT_MGLYPH;
  ELEMENT_NAMES[5] = ELT_METER;
  ELEMENT_NAMES[6] = ELT_RADIALGRADIENT;
  ELEMENT_NAMES[7] = ELT_RTC;
  ELEMENT_NAMES[8] = ELT_EMBED;
  ELEMENT_NAMES[9] = ELT_STRIKE;
  ELEMENT_NAMES[10] = ELT_OL;
  ELEMENT_NAMES[11] = ELT_OPTGROUP;
  ELEMENT_NAMES[12] = ELT_NOFRAMES;
  ELEMENT_NAMES[13] = ELT_MTEXT;
  ELEMENT_NAMES[14] = ELT_VIEW;
  ELEMENT_NAMES[15] = ELT_IMG;
  ELEMENT_NAMES[16] = ELT_WBR;
  ELEMENT_NAMES[17] = ELT_METADATA;
  ELEMENT_NAMES[18] = ELT_ASIDE;
  ELEMENT_NAMES[19] = ELT_FECOMPOSITE;
  ELEMENT_NAMES[20] = ELT_DIALOG;
  ELEMENT_NAMES[21] = ELT_MI;
  ELEMENT_NAMES[22] = ELT_EM;
  ELEMENT_NAMES[23] = ELT_TSPAN;
  ELEMENT_NAMES[24] = ELT_FEFUNCR;
  ELEMENT_NAMES[25] = ELT_CANVAS;
  ELEMENT_NAMES[26] = ELT_BASEFONT;
  ELEMENT_NAMES[27] = ELT_FEDISTANTLIGHT;
  ELEMENT_NAMES[28] = ELT_OUTPUT;
  ELEMENT_NAMES[29] = ELT_TFOOT;
  ELEMENT_NAMES[30] = ELT_FEMORPHOLOGY;
  ELEMENT_NAMES[31] = ELT_DEL;
  ELEMENT_NAMES[32] = ELT_NAV;
  ELEMENT_NAMES[33] = ELT_SET;
  ELEMENT_NAMES[34] = ELT_Q;
  ELEMENT_NAMES[35] = ELT_H6;
  ELEMENT_NAMES[36] = ELT_RB;
  ELEMENT_NAMES[37] = ELT_LEGEND;
  ELEMENT_NAMES[38] = ELT_BLOCKQUOTE;
  ELEMENT_NAMES[39] = ELT_FEMERGE;
  ELEMENT_NAMES[40] = ELT_MARQUEE;
  ELEMENT_NAMES[41] = ELT_TIME;
  ELEMENT_NAMES[42] = ELT_LISTING;
  ELEMENT_NAMES[43] = ELT_TH;
  ELEMENT_NAMES[44] = ELT_MASK;
  ELEMENT_NAMES[45] = ELT_SYMBOL;
  ELEMENT_NAMES[46] = ELT_ANIMATEMOTION;
  ELEMENT_NAMES[47] = ELT_POLYGON;
  ELEMENT_NAMES[48] = ELT_COLGROUP;
  ELEMENT_NAMES[49] = ELT_ABBR;
  ELEMENT_NAMES[50] = ELT_FEGAUSSIANBLUR;
  ELEMENT_NAMES[51] = ELT_TR;
  ELEMENT_NAMES[52] = ELT_DETAILS;
  ELEMENT_NAMES[53] = ELT_DT;
  ELEMENT_NAMES[54] = ELT_FOREIGNOBJECT;
  ELEMENT_NAMES[55] = ELT_FESPOTLIGHT;
  ELEMENT_NAMES[56] = ELT_INPUT;
  ELEMENT_NAMES[57] = ELT_RT;
  ELEMENT_NAMES[58] = ELT_TT;
  ELEMENT_NAMES[59] = ELT_SLOT;
  ELEMENT_NAMES[60] = ELT_MENU;
  ELEMENT_NAMES[61] = ELT_FECONVOLVEMATRIX;
  ELEMENT_NAMES[62] = ELT_SUMMARY;
  ELEMENT_NAMES[63] = ELT_BDO;
  ELEMENT_NAMES[64] = ELT_DIR;
  ELEMENT_NAMES[65] = ELT_KBD;
  ELEMENT_NAMES[66] = ELT_A;
  ELEMENT_NAMES[67] = ELT_SVG;
  ELEMENT_NAMES[68] = ELT_VAR;
  ELEMENT_NAMES[69] = ELT_I;
  ELEMENT_NAMES[70] = ELT_U;
  ELEMENT_NAMES[71] = ELT_H4;
  ELEMENT_NAMES[72] = ELT_DATA;
  ELEMENT_NAMES[73] = ELT_TEXTAREA;
  ELEMENT_NAMES[74] = ELT_DD;
  ELEMENT_NAMES[75] = ELT_FEFLOOD;
  ELEMENT_NAMES[76] = ELT_TD;
  ELEMENT_NAMES[77] = ELT_ANIMATE;
  ELEMENT_NAMES[78] = ELT_CIRCLE;
  ELEMENT_NAMES[79] = ELT_FEMERGENODE;
  ELEMENT_NAMES[80] = ELT_FRAME;
  ELEMENT_NAMES[81] = ELT_IFRAME;
  ELEMENT_NAMES[82] = ELT_PICTURE;
  ELEMENT_NAMES[83] = ELT_TABLE;
  ELEMENT_NAMES[84] = ELT_ALTGLYPHDEF;
  ELEMENT_NAMES[85] = ELT_FEDIFFUSELIGHTING;
  ELEMENT_NAMES[86] = ELT_ALTGLYPH;
  ELEMENT_NAMES[87] = ELT_MPATH;
  ELEMENT_NAMES[88] = ELT_TEXTPATH;
  ELEMENT_NAMES[89] = ELT_MARK;
  ELEMENT_NAMES[90] = ELT_DL;
  ELEMENT_NAMES[91] = ELT_UL;
  ELEMENT_NAMES[92] = ELT_ANIMATETRANSFORM;
  ELEMENT_NAMES[93] = ELT_MENUITEM;
  ELEMENT_NAMES[94] = ELT_CAPTION;
  ELEMENT_NAMES[95] = ELT_MAIN;
  ELEMENT_NAMES[96] = ELT_SPAN;
  ELEMENT_NAMES[97] = ELT_MO;
  ELEMENT_NAMES[98] = ELT_HGROUP;
  ELEMENT_NAMES[99] = ELT_STOP;
  ELEMENT_NAMES[100] = ELT_CENTER;
  ELEMENT_NAMES[101] = ELT_FILTER;
  ELEMENT_NAMES[102] = ELT_MARKER;
  ELEMENT_NAMES[103] = ELT_NOBR;
  ELEMENT_NAMES[104] = ELT_ADDRESS;
  ELEMENT_NAMES[105] = ELT_DEFS;
  ELEMENT_NAMES[106] = ELT_MS;
  ELEMENT_NAMES[107] = ELT_PROGRESS;
  ELEMENT_NAMES[108] = ELT_APPLET;
  ELEMENT_NAMES[109] = ELT_DATALIST;
  ELEMENT_NAMES[110] = ELT_FIELDSET;
  ELEMENT_NAMES[111] = ELT_FEOFFSET;
  ELEMENT_NAMES[112] = ELT_FEPOINTLIGHT;
  ELEMENT_NAMES[113] = ELT_FONT;
  ELEMENT_NAMES[114] = ELT_LINEARGRADIENT;
  ELEMENT_NAMES[115] = ELT_NOSCRIPT;
  ELEMENT_NAMES[116] = ELT_OBJECT;
  ELEMENT_NAMES[117] = ELT_PLAINTEXT;
  ELEMENT_NAMES[118] = ELT_RECT;
  ELEMENT_NAMES[119] = ELT_SELECT;
  ELEMENT_NAMES[120] = ELT_SCRIPT;
  ELEMENT_NAMES[121] = ELT_TEXT;
  ELEMENT_NAMES[122] = ELT_FEDROPSHADOW;
  ELEMENT_NAMES[123] = ELT_FECOLORMATRIX;
  ELEMENT_NAMES[124] = ELT_BODY;
  ELEMENT_NAMES[125] = ELT_RUBY;
  ELEMENT_NAMES[126] = ELT_TBODY;
  ELEMENT_NAMES[127] = ELT_BIG;
  ELEMENT_NAMES[128] = ELT_COL;
  ELEMENT_NAMES[129] = ELT_DFN;
  ELEMENT_NAMES[130] = ELT_DIV;
  ELEMENT_NAMES[131] = ELT_INS;
  ELEMENT_NAMES[132] = ELT_MAP;
  ELEMENT_NAMES[133] = ELT_PRE;
  ELEMENT_NAMES[134] = ELT_B;
  ELEMENT_NAMES[135] = ELT_SUB;
  ELEMENT_NAMES[136] = ELT_SUP;
  ELEMENT_NAMES[137] = ELT_USE;
  ELEMENT_NAMES[138] = ELT_G;
  ELEMENT_NAMES[139] = ELT_XMP;
  ELEMENT_NAMES[140] = ELT_P;
  ELEMENT_NAMES[141] = ELT_S;
  ELEMENT_NAMES[142] = ELT_H1;
  ELEMENT_NAMES[143] = ELT_H3;
  ELEMENT_NAMES[144] = ELT_H5;
  ELEMENT_NAMES[145] = ELT_AREA;
  ELEMENT_NAMES[146] = ELT_FEFUNCA;
  ELEMENT_NAMES[147] = ELT_META;
  ELEMENT_NAMES[148] = ELT_FEFUNCB;
  ELEMENT_NAMES[149] = ELT_DESC;
  ELEMENT_NAMES[150] = ELT_BGSOUND;
  ELEMENT_NAMES[151] = ELT_FEBLEND;
  ELEMENT_NAMES[152] = ELT_HEAD;
  ELEMENT_NAMES[153] = ELT_NOEMBED;
  ELEMENT_NAMES[154] = ELT_THEAD;
  ELEMENT_NAMES[155] = ELT_ARTICLE;
  ELEMENT_NAMES[156] = ELT_BASE;
  ELEMENT_NAMES[157] = ELT_CODE;
  ELEMENT_NAMES[158] = ELT_CITE;
  ELEMENT_NAMES[159] = ELT_FETURBULENCE;
  ELEMENT_NAMES[160] = ELT_FEIMAGE;
  ELEMENT_NAMES[161] = ELT_FETILE;
  ELEMENT_NAMES[162] = ELT_FIGURE;
  ELEMENT_NAMES[163] = ELT_IMAGE;
  ELEMENT_NAMES[164] = ELT_LINE;
  ELEMENT_NAMES[165] = ELT_POLYLINE;
  ELEMENT_NAMES[166] = ELT_SOURCE;
  ELEMENT_NAMES[167] = ELT_STYLE;
  ELEMENT_NAMES[168] = ELT_TITLE;
  ELEMENT_NAMES[169] = ELT_TEMPLATE;
  ELEMENT_NAMES[170] = ELT_GLYPHREF;
  ELEMENT_NAMES[171] = ELT_FEFUNCG;
  ELEMENT_NAMES[172] = ELT_FESPECULARLIGHTING;
  ELEMENT_NAMES[173] = ELT_STRONG;
  ELEMENT_NAMES[174] = ELT_CLIPPATH;
  ELEMENT_NAMES[175] = ELT_MATH;
  ELEMENT_NAMES[176] = ELT_PATH;
  ELEMENT_NAMES[177] = ELT_SWITCH;
  ELEMENT_NAMES[178] = ELT_LI;
  ELEMENT_NAMES[179] = ELT_LINK;
  ELEMENT_NAMES[180] = ELT_MALIGNMARK;
  ELEMENT_NAMES[181] = ELT_TRACK;
  ELEMENT_NAMES[182] = ELT_HTML;
  ELEMENT_NAMES[183] = ELT_LABEL;
  ELEMENT_NAMES[184] = ELT_SMALL;
  ELEMENT_NAMES[185] = ELT_ALTGLYPHITEM;
  ELEMENT_NAMES[186] = ELT_ACRONYM;
  ELEMENT_NAMES[187] = ELT_FORM;
  ELEMENT_NAMES[188] = ELT_PARAM;
  ELEMENT_NAMES[189] = ELT_BUTTON;
  ELEMENT_NAMES[190] = ELT_FIGCAPTION;
  ELEMENT_NAMES[191] = ELT_KEYGEN;
  ELEMENT_NAMES[192] = ELT_OPTION;
  ELEMENT_NAMES[193] = ELT_PATTERN;
  ELEMENT_NAMES[194] = ELT_SECTION;
  ELEMENT_NAMES[195] = ELT_AUDIO;
  ELEMENT_NAMES[196] = ELT_VIDEO;
  ELEMENT_NAMES[197] = ELT_FEDISPLACEMENTMAP;
  ELEMENT_NAMES[198] = ELT_RP;
  ELEMENT_NAMES[199] = ELT_SAMP;
  ELEMENT_NAMES[200] = ELT_BR;
  ELEMENT_NAMES[201] = ELT_ANIMATECOLOR;
  ELEMENT_NAMES[202] = ELT_HR;
  ELEMENT_NAMES[203] = ELT_FECOMPONENTTRANSFER;
  ELEMENT_NAMES[204] = ELT_FOOTER;
  ELEMENT_NAMES[205] = ELT_HEADER;
}

void
nsHtml5ElementName::releaseStatics()
{
  delete ELT_ANNOTATION_XML;
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
  delete ELT_DATA;
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
  delete ELT_DATALIST;
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
  delete ELT_SLOT;
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


