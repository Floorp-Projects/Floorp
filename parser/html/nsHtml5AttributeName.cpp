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
 * Please edit AttributeName.java instead and regenerate.
 */

#define nsHtml5AttributeName_cpp__

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
#include "nsHtml5ElementName.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5AttributeName.h"

int32_t* nsHtml5AttributeName::ALL_NO_NS = 0;
int32_t* nsHtml5AttributeName::XMLNS_NS = 0;
int32_t* nsHtml5AttributeName::XML_NS = 0;
int32_t* nsHtml5AttributeName::XLINK_NS = 0;
nsIAtom** nsHtml5AttributeName::ALL_NO_PREFIX = 0;
nsIAtom** nsHtml5AttributeName::XMLNS_PREFIX = 0;
nsIAtom** nsHtml5AttributeName::XLINK_PREFIX = 0;
nsIAtom** nsHtml5AttributeName::XML_PREFIX = 0;
nsIAtom** 
nsHtml5AttributeName::SVG_DIFFERENT(nsIAtom* name, nsIAtom* camel)
{
  nsIAtom** arr = new nsIAtom*[4];
  arr[0] = name;
  arr[1] = name;
  arr[2] = camel;
  return arr;
}

nsIAtom** 
nsHtml5AttributeName::MATH_DIFFERENT(nsIAtom* name, nsIAtom* camel)
{
  nsIAtom** arr = new nsIAtom*[4];
  arr[0] = name;
  arr[1] = camel;
  arr[2] = name;
  return arr;
}

nsIAtom** 
nsHtml5AttributeName::COLONIFIED_LOCAL(nsIAtom* name, nsIAtom* suffix)
{
  nsIAtom** arr = new nsIAtom*[4];
  arr[0] = name;
  arr[1] = suffix;
  arr[2] = suffix;
  return arr;
}

nsIAtom** 
nsHtml5AttributeName::SAME_LOCAL(nsIAtom* name)
{
  nsIAtom** arr = new nsIAtom*[4];
  arr[0] = name;
  arr[1] = name;
  arr[2] = name;
  return arr;
}

nsHtml5AttributeName* 
nsHtml5AttributeName::nameByBuffer(char16_t* buf, int32_t offset, int32_t length, nsHtml5AtomTable* interner)
{
  uint32_t hash = nsHtml5AttributeName::bufToHash(buf, length);
  int32_t index = nsHtml5AttributeName::ATTRIBUTE_HASHES.binarySearch(hash);
  if (index < 0) {
    return nullptr;
  }
  nsHtml5AttributeName* attributeName =
    nsHtml5AttributeName::ATTRIBUTE_NAMES[index];
  nsIAtom* name = attributeName->getLocal(NS_HTML5ATTRIBUTE_NAME_HTML);
  if (!nsHtml5Portability::localEqualsBuffer(name, buf, offset, length)) {
    return nullptr;
  }
  return attributeName;
}

nsHtml5AttributeName::nsHtml5AttributeName(int32_t* uri,
                                           nsIAtom** local,
                                           nsIAtom** prefix)
  : uri(uri)
  , local(local)
  , prefix(prefix)
  , custom(false)
{
  MOZ_COUNT_CTOR(nsHtml5AttributeName);
}

nsHtml5AttributeName::nsHtml5AttributeName()
  : uri(nsHtml5AttributeName::ALL_NO_NS)
  , local(nsHtml5AttributeName::SAME_LOCAL(nullptr))
  , prefix(ALL_NO_PREFIX)
  , custom(true)
{
  MOZ_COUNT_CTOR(nsHtml5AttributeName);
}

bool
nsHtml5AttributeName::isInterned()
{
  return !custom;
}

void
nsHtml5AttributeName::setNameForNonInterned(nsIAtom* name)
{
  MOZ_ASSERT(custom);
  local[0] = name;
  local[1] = name;
  local[2] = name;
}

nsHtml5AttributeName*
nsHtml5AttributeName::createAttributeName(nsIAtom* name)
{
  return new nsHtml5AttributeName(nsHtml5AttributeName::ALL_NO_NS,
                                  nsHtml5AttributeName::SAME_LOCAL(name),
                                  ALL_NO_PREFIX);
}


nsHtml5AttributeName::~nsHtml5AttributeName()
{
  MOZ_COUNT_DTOR(nsHtml5AttributeName);
  delete[] local;
}

int32_t 
nsHtml5AttributeName::getUri(int32_t mode)
{
  return uri[mode];
}

nsIAtom* 
nsHtml5AttributeName::getLocal(int32_t mode)
{
  return local[mode];
}

nsIAtom* 
nsHtml5AttributeName::getPrefix(int32_t mode)
{
  return prefix[mode];
}

bool 
nsHtml5AttributeName::equalsAnother(nsHtml5AttributeName* another)
{
  return this->getLocal(NS_HTML5ATTRIBUTE_NAME_HTML) == another->getLocal(NS_HTML5ATTRIBUTE_NAME_HTML);
}

nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DIR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DUR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_END = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IN2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LOW = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MAX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REV = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SRC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_D = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_R = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_X = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Y = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Z = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CAP_HEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_G1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_U1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_X1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Y1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_G2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_U2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_X2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Y2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K3 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K4 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XML_SPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XML_LANG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XML_BASE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_GRAB = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_VALUEMAX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_LABELLEDBY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_DESCRIBEDBY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_DISABLED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_CHECKED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_SELECTED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_DROPEFFECT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_REQUIRED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_EXPANDED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_PRESSED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_LEVEL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_CHANNEL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_HIDDEN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_SECRET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_POSINSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_ATOMIC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_INVALID = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_TEMPLATEID = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_VALUEMIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_MULTISELECTABLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_CONTROLS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_MULTILINE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_READONLY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_OWNS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_ACTIVEDESCENDANT =
  nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_RELEVANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_DATATYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_VALUENOW = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_SORT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_AUTOCOMPLETE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_FLOWTO = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_BUSY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_LIVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_HASPOPUP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARIA_SETSIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLEAR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DATAFORMATAS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DISABLED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DATAFLD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DEFAULT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DATASRC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DATA = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_EQUALCOLUMNS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_EQUALROWS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ISMAP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LOCAL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MOVABLELIMITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOTATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDATASETCHANGED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDATAAVAILABLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONPASTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDATASETCOMPLETE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROWALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROTATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SEPARATOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SEPARATORS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_V_MATHEMATICAL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_V_HANGING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XCHANNELSELECTOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_YCHANNELSELECTOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARABIC_FORM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ENABLE_BACKGROUND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDBLCLICK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONABORT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CALCMODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CHECKED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DESCENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FENCE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSCROLL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONACTIVATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OPACITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPECULAREXPONENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPECULARCONSTANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPECIFICATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_THICKMATHSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_UNICODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_UNICODE_BIDI = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_UNICODE_RANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BORDER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ID = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GRADIENTTRANSFORM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GRADIENTUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HIDDEN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HEADERS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_READONLY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RENDERING_INTENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SEED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SRCDOC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STDDEVIATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SANDBOX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_V_IDEOGRAPHIC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WORD_SPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCENTUNDER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCEPT_CHARSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCESSKEY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCENT_HEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ASCENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCEPT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BEVELLED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BASEFREQUENCY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BASELINE_SHIFT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BASEPROFILE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BASELINE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BASE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CODETYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CODEBASE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CITE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DEFER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DATETIME = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DIRECTION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_EDGEMODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_EDGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HIDEFOCUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_INDEX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IRRELEVANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_INTERCEPT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_INTEGRITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LINEBREAK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LABEL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LINETHICKNESS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NAME = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NORESIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREUNLOAD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONREPEAT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OBJECT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSELECT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ORDER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OTHER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONRESET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCELLCHANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONREADYSTATECHANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMESSAGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEGIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONHELP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREPRINT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ORIENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ORIENTATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFORECOPY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSELECTSTART = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREPASTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREUPDATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDEACTIVATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREACTIVATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFORDEACTIVATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONKEYPRESS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONKEYUP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREEDITFOCUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFORECUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONKEYDOWN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONRESIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEAT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEAT_MAX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REFERRERPOLICY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RULES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEAT_MIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEATCOUNT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEAT_START = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEAT_TEMPLATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEATDUR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SELECTED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPEED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SIZES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SUPERSCRIPTSHIFT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STRETCHY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCHEME = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPREADMETHOD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SELECTION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_UNSELECTABLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_UNDERLINE_POSITION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_UNDERLINE_THICKNESS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_X_HEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DIFFUSECONSTANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HREFLANG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONAFTERPRINT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONAFTERUPDATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PROFILE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SURFACESCALE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALIGNMENT_BASELINE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALIGNMENTSCOPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DRAGGABLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HANGING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IMAGE_RENDERING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LANGUAGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LANG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LARGEOP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LONGDESC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LENGTHADJUST = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARGINHEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARGINWIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NARGS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ORIGIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TARGET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TARGETX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TARGETY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALPHABETIC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARCHIVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HIGH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LIGHTING_COLOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MATHEMATICAL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MATHBACKGROUND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_METHOD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MATHVARIANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MATHCOLOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MATHSIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOSHADE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCHANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PATHLENGTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PATH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALTIMG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACTIONTYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACTION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACTIVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ADDITIVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BEGIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DOMINANT_BASELINE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DIVISOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DEFINITIONURL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HORIZ_ADV_X = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HORIZ_ORIGIN_X = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HORIZ_ORIGIN_Y = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LIMITINGCONEANGLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MEDIUMMATHSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MEDIA = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MANIFEST = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFILTERCHANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFINISH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OPTIMUM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RADIOGROUP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RADIUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCRIPTLEVEL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCRIPTSIZEMULTIPLIER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STRING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STRIKETHROUGH_POSITION =
  nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STRIKETHROUGH_THICKNESS =
  nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCRIPTMINSIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TABINDEX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VISIBILITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BACKGROUND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LINK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARKER_MID = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARKERHEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARKER_END = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MASK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARKER_START = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARKERWIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MASKUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARKERUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MASKCONTENTUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AMPLITUDE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CELLSPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CELLPADDING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DECLARE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FILL_RULE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FILL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FILL_OPACITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MAXLENGTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCLICK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBLUR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPLACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROWLINES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCALE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STYLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TABLEVALUES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TITLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_V_ALPHABETIC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AZIMUTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FORMAT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FRAMEBORDER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FRAME = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FRAMESPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FROM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FORM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PROMPT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PRIMITIVEUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SYMMETRIC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STEMH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STEMV = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SEAMLESS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SUMMARY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_USEMAP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ZOOMANDPAN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ASYNC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALINK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ICON = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_KERNELMATRIX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_KERNING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_KERNELUNITLENGTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONUNLOAD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OPEN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONINVALID = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONEND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONINPUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_POINTER_EVENTS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_POINTS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_POINTSATX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_POINTSATY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_POINTSATZ = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPAN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STANDBY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_THINMATHSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TRANSFORM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VLINK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WHEN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XLINK_HREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XLINK_TITLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XLINK_ROLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XLINK_ARCROLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XMLNS_XLINK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XMLNS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XLINK_TYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XLINK_SHOW = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XLINK_ACTUATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AUTOPLAY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AUTOSUBMIT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AUTOCOMPLETE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AUTOFOCUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BGCOLOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLOR_PROFILE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLOR_RENDERING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLOR_INTERPOLATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLOR_INTERPOLATION_FILTERS =
  nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ENCODING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_EXPONENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FLOOD_COLOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FLOOD_OPACITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IDEOGRAPHIC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LQUOTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PANOSE_1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NUMOCTAVES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONLOAD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBOUNCE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCONTROLSELECT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONROWSINSERTED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEWHEEL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONROWENTER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEENTER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEOVER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFORMCHANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFOCUSIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONROWEXIT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOVEEND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCONTEXTMENU = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONZOOM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONLOSECAPTURE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCOPY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOVESTART = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONROWSDELETE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSELEAVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEMOVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEUP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFOCUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEOUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFORMINPUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFOCUSOUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEDOWN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TO = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RQUOTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_LINECAP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCROLLDELAY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_DASHARRAY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_DASHOFFSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_LINEJOIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_MITERLIMIT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCROLLING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_WIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_OPACITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COMPACT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLIP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLIP_RULE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLIP_PATH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLIPPATHUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DISPLAY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DISPLAYSTYLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GLYPH_ORIENTATION_VERTICAL =
  nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GLYPH_ORIENTATION_HORIZONTAL =
  nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GLYPHREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GLYPH_NAME = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HTTP_EQUIV = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_KEYPOINTS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LOOP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PROPERTY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCOPED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STEP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SHAPE_RENDERING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCOPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SHAPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SLOPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STOP_COLOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STOP_OPACITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEMPLATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WRAP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ABBR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ATTRIBUTENAME = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ATTRIBUTETYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CHAR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COORDS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CHAROFF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CHARSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MACROS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOWRAP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOHREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGENTER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGOVER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONPROPERTYCHANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGEND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDROP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGDROP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OVERLINE_POSITION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONERROR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OPERATOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OVERFLOW = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGSTART = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONERRORUPDATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OVERLINE_THICKNESS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGLEAVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STARTOFFSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_START = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AXIS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BIAS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLSPAN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLASSID = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CROSSORIGIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CURSOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLOSURE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLOSE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CLASS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_KEYSYSTEM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_KEYSPLINES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LOWSRC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MAXSIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MINSIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OFFSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PRESERVEALPHA = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PRESERVEASPECTRATIO = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROWSPAN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROWSPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROWS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SRCSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SUBSCRIPTSHIFT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERSION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALTTEXT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CONTENTEDITABLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CONTROLS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CONTENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CONTEXTMENU = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DEPTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ENCTYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONT_STRETCH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FILTER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONTWEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONT_WEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONTSTYLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONT_STYLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONTFAMILY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONT_FAMILY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONT_VARIANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONT_SIZE_ADJUST = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FILTERUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONTSIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FONT_SIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_KEYTIMES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LETTER_SPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LIST = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MULTIPLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSTOP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSTART = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_POSTER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PATTERNTRANSFORM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PATTERN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PATTERNUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PATTERNCONTENTUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RESTART = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STITCHTILES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SYSTEMLANGUAGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXT_RENDERING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERT_ORIGIN_X = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERT_ADV_Y = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERT_ORIGIN_Y = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXT_DECORATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXT_ANCHOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXTLENGTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_UNITS_PER_EM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WRITING_MODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WIDTHS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCUMULATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNSPAN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNLINES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNSPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNWIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GROUPALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_INPUTMODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OCCURRENCE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSUBMIT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REQUIRED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REQUIREDFEATURES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RESULT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REQUIREDEXTENSIONS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VALUES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VALUETYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VALUE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ELEVATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VIEWTARGET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VIEWBOX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BBOX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REFX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REFY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERYTHINMATHSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERYTHICKMATHSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERYVERYTHINMATHSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VERYVERYTHICKMATHSPACE = nullptr;
nsHtml5AttributeName** nsHtml5AttributeName::ATTRIBUTE_NAMES = 0;
static int32_t const ATTRIBUTE_HASHES_DATA[] = {
  50917059,   52488851,   52489043,   53006051,   53537523,   55077603,
  56685811,   57205395,   57210387,   59825747,   59830867,   60345635,
  60817409,   64487425,   68157441,   71303169,   71827457,   72351745,
  808872090,  876085250,  878182402,  883425282,  884998146,  885522434,
  892862466,  894959618,  900202498,  901775362,  902299650,  911736834,
  928514050,  1037879561, 1038063816, 1038141480, 1680095865, 1680140893,
  1680159327, 1680159328, 1680165421, 1680165436, 1680165437, 1680165487,
  1680165533, 1680165613, 1680165692, 1680181850, 1680181996, 1680185931,
  1680198203, 1680198381, 1680229115, 1680230940, 1680231247, 1680251485,
  1680282148, 1680311085, 1680315086, 1680323325, 1680343801, 1680345685,
  1680345965, 1680347981, 1680368221, 1680411449, 1680413393, 1680433915,
  1680437801, 1680446153, 1680452349, 1680511804, 1681174213, 1681694748,
  1681733672, 1681844247, 1681879063, 1681940503, 1681969220, 1682440540,
  1682587945, 1683805446, 1684319541, 1685882101, 1685902598, 1686731997,
  1687164232, 1687503600, 1687620127, 1687751191, 1687751377, 1689048326,
  1689130184, 1689324870, 1689788441, 1689839946, 1691091102, 1691145478,
  1691293817, 1692408896, 1692933184, 1697174123, 1699185409, 1704262346,
  1704526375, 1714745560, 1714763319, 1715466295, 1716303957, 1721189160,
  1721305962, 1721347639, 1723309623, 1723336432, 1723336528, 1723340621,
  1723645710, 1724189239, 1724197420, 1724238365, 1731048742, 1732771842,
  1733874289, 1733919469, 1734182982, 1734404167, 1739561208, 1739583824,
  1739927860, 1740096054, 1740119884, 1740130375, 1741535501, 1742183484,
  1747295467, 1747299630, 1747309881, 1747348637, 1747446838, 1747455030,
  1747479606, 1747792072, 1747800157, 1747839118, 1747906667, 1747939528,
  1748021284, 1748306996, 1748503880, 1748552744, 1748566068, 1748869205,
  1748971848, 1749027145, 1749350104, 1749399124, 1749856356, 1751232761,
  1751507685, 1751649130, 1751679545, 1751755561, 1752985897, 1753049109,
  1753297133, 1753550036, 1754214628, 1754434872, 1754546894, 1754579720,
  1754606246, 1754612424, 1754643237, 1754644293, 1754645079, 1754647068,
  1754647074, 1754647353, 1754698327, 1754751622, 1754792749, 1754794646,
  1754798923, 1754835516, 1754858317, 1754860061, 1754860110, 1754860396,
  1754860400, 1754860401, 1754872618, 1754899031, 1754905345, 1754907227,
  1754927689, 1754958648, 1756147974, 1756155098, 1756190926, 1756219733,
  1756265690, 1756302628, 1756360955, 1756426572, 1756428495, 1756471625,
  1756704824, 1756710661, 1756737685, 1756762256, 1756804936, 1756836998,
  1756874572, 1756889417, 1757053236, 1757421892, 1757874716, 1757942610,
  1758018291, 1759379608, 1765800271, 1767725700, 1767875272, 1771569964,
  1771637325, 1772032615, 1773606972, 1776114564, 1780879045, 1780975314,
  1781007934, 1782518297, 1784574102, 1784643703, 1785174319, 1786622296,
  1786740932, 1786775671, 1786821704, 1786851500, 1787193500, 1787365531,
  1787699221, 1788254870, 1788842244, 1790814502, 1791068279, 1791070327,
  1797666394, 1797886599, 1801312388, 1803561214, 1803839644, 1804036350,
  1804054854, 1804069019, 1804081401, 1804235064, 1804405895, 1804978712,
  1805715690, 1805715716, 1814517574, 1814558026, 1814560070, 1814656326,
  1814656840, 1814986837, 1816104145, 1816144023, 1816178925, 1817175115,
  1817175198, 1817177246, 1820262641, 1820637455, 1820727381, 1820928104,
  1821755934, 1821958888, 1822002839, 1823574314, 1823580230, 1823829083,
  1823841492, 1823975206, 1824005974, 1824081655, 1824159037, 1824377064,
  1825437894, 1825677514, 1848600826, 1853862084, 1854285018, 1854302364,
  1854366938, 1854464212, 1854466380, 1854474395, 1854497001, 1854497003,
  1854497008, 1864698185, 1865910331, 1865910347, 1866496199, 1867448617,
  1867462756, 1867620412, 1871251689, 1872034503, 1872343590, 1873590471,
  1873656984, 1874261045, 1874270021, 1874698443, 1874788501, 1875753052,
  1881750231, 1884079398, 1884142379, 1884246821, 1884267068, 1884295780,
  1884343396, 1889569526, 1889633006, 1890996553, 1891069765, 1891098437,
  1891182792, 1891186903, 1891937366, 1894552650, 1898415413, 1898428101,
  1900544002, 1902640276, 1903612236, 1903659239, 1903759600, 1905541832,
  1905628916, 1905672729, 1905754853, 1905902311, 1906408542, 1906408598,
  1906419001, 1906421049, 1906423097, 1907660596, 1907701479, 1908195085,
  1908462185, 1909438149, 1909819252, 1910328970, 1910441627, 1910441770,
  1910441773, 1910487243, 1910503637, 1910507338, 1910527802, 1910572893,
  1915025672, 1915146282, 1915295948, 1915341049, 1915757815, 1916210285,
  1916247343, 1916278099, 1916286197, 1916337499, 1917295176, 1917327080,
  1917857531, 1917953597, 1919297291, 1921061206, 1921880376, 1921894426,
  1922319046, 1922354008, 1922384591, 1922384686, 1922400908, 1922413290,
  1922413292, 1922413307, 1922419228, 1922470745, 1922482777, 1922531929,
  1922566877, 1922567078, 1922599757, 1922607670, 1922630475, 1922632396,
  1922665052, 1922665174, 1922665179, 1922671417, 1922677495, 1922679386,
  1922679531, 1922679610, 1922699851, 1923088386, 1924206934, 1924443742,
  1924453467, 1924462384, 1924517489, 1924570799, 1924583073, 1924585254,
  1924629705, 1924738716, 1924773438, 1932870919, 1932959284, 1932986153,
  1933123337, 1933145837, 1933369607, 1933508940, 1934917290, 1934917372,
  1934970504, 1935099626, 1935597338, 1937336473, 1937777860, 1939976792,
  1941253366, 1941286708, 1941409583, 1941435445, 1941438085, 1941440197,
  1941454586, 1941550652, 1942026440, 1943317364, 1965349396, 1965512429,
  1965561677, 1966384692, 1966439670, 1966442279, 1966454567, 1971855414,
  1972151670, 1972196486, 1972656710, 1972744939, 1972744954, 1972750880,
  1972863609, 1972904518, 1972904522, 1972904785, 1972908839, 1972909592,
  1972922984, 1972962123, 1972963917, 1972980466, 1972996699, 1974849131,
  1975062341, 1982254612, 1982640164, 1983157559, 1983266615, 1983290011,
  1983347764, 1983398182, 1983416119, 1983432389, 1983461061, 1987410233,
  1987422362, 1988132214, 1988784439, 1988788535, 1989522022, 1990062797,
  1990107683, 1991021879, 1991220282, 1991392548, 1991625270, 1991643278,
  1993343287, 1999273799, 2000096287, 2000125224, 2000160071, 2000162011,
  2000752725, 2001210183, 2001527900, 2001578182, 2001634458, 2001634459,
  2001669449, 2001669450, 2001710298, 2001710299, 2001732764, 2001814704,
  2001826027, 2001898808, 2001898809, 2004199576, 2004846654, 2004957380,
  2005342360, 2005925890, 2006459190, 2006516551, 2006824246, 2007019632,
  2007021895, 2007064812, 2007064819, 2008084807, 2008401563, 2008408414,
  2009041198, 2009059485, 2009061450, 2009061533, 2009071951, 2009079867,
  2009141482, 2009231684, 2009434924, 2010452700, 2010542150, 2010716309,
  2015950026, 2016711994, 2016787611, 2016810187, 2016910397, 2017010843,
  2018908874, 2019887833, 2023011418, 2023146024, 2023342821, 2024616088,
  2024647008, 2024763702, 2024794274, 2026741958, 2026893641, 2026975253,
  2034765641, 2060302634, 2060474743, 2065170434, 2065694722, 2066743298,
  2066762276, 2073034754, 2075005220, 2081423362, 2081947650, 2082471938,
  2083520514, 2089811970, 2091784484, 2093791505, 2093791506, 2093791509,
  2093791510
};
staticJArray<int32_t,int32_t> nsHtml5AttributeName::ATTRIBUTE_HASHES = { ATTRIBUTE_HASHES_DATA, MOZ_ARRAY_LENGTH(ATTRIBUTE_HASHES_DATA) };
void
nsHtml5AttributeName::initializeStatics()
{
  ALL_NO_NS = new int32_t[3];
  ALL_NO_NS[0] = kNameSpaceID_None;
  ALL_NO_NS[1] = kNameSpaceID_None;
  ALL_NO_NS[2] = kNameSpaceID_None;
  XMLNS_NS = new int32_t[3];
  XMLNS_NS[0] = kNameSpaceID_None;
  XMLNS_NS[1] = kNameSpaceID_XMLNS;
  XMLNS_NS[2] = kNameSpaceID_XMLNS;
  XML_NS = new int32_t[3];
  XML_NS[0] = kNameSpaceID_None;
  XML_NS[1] = kNameSpaceID_XML;
  XML_NS[2] = kNameSpaceID_XML;
  XLINK_NS = new int32_t[3];
  XLINK_NS[0] = kNameSpaceID_None;
  XLINK_NS[1] = kNameSpaceID_XLink;
  XLINK_NS[2] = kNameSpaceID_XLink;
  ALL_NO_PREFIX = new nsIAtom*[3];
  ALL_NO_PREFIX[0] = nullptr;
  ALL_NO_PREFIX[1] = nullptr;
  ALL_NO_PREFIX[2] = nullptr;
  XMLNS_PREFIX = new nsIAtom*[3];
  XMLNS_PREFIX[0] = nullptr;
  XMLNS_PREFIX[1] = nsHtml5Atoms::xmlns;
  XMLNS_PREFIX[2] = nsHtml5Atoms::xmlns;
  XLINK_PREFIX = new nsIAtom*[3];
  XLINK_PREFIX[0] = nullptr;
  XLINK_PREFIX[1] = nsHtml5Atoms::xlink;
  XLINK_PREFIX[2] = nsHtml5Atoms::xlink;
  XML_PREFIX = new nsIAtom*[3];
  XML_PREFIX[0] = nullptr;
  XML_PREFIX[1] = nsHtml5Atoms::xml;
  XML_PREFIX[2] = nsHtml5Atoms::xml;
  ATTR_ALT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::alt), ALL_NO_PREFIX);
  ATTR_DIR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::dir), ALL_NO_PREFIX);
  ATTR_DUR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::dur), ALL_NO_PREFIX);
  ATTR_END = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::end), ALL_NO_PREFIX);
  ATTR_FOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::for_), ALL_NO_PREFIX);
  ATTR_IN2 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::in2), ALL_NO_PREFIX);
  ATTR_LOW = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::low), ALL_NO_PREFIX);
  ATTR_MIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::min), ALL_NO_PREFIX);
  ATTR_MAX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::max), ALL_NO_PREFIX);
  ATTR_REL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rel), ALL_NO_PREFIX);
  ATTR_REV = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rev), ALL_NO_PREFIX);
  ATTR_SRC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::src), ALL_NO_PREFIX);
  ATTR_D = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::d), ALL_NO_PREFIX);
  ATTR_K = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::k), ALL_NO_PREFIX);
  ATTR_R = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::r), ALL_NO_PREFIX);
  ATTR_X = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::x), ALL_NO_PREFIX);
  ATTR_Y = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::y), ALL_NO_PREFIX);
  ATTR_Z = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::z), ALL_NO_PREFIX);
  ATTR_CAP_HEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cap_height), ALL_NO_PREFIX);
  ATTR_G1 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::g1), ALL_NO_PREFIX);
  ATTR_K1 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::k1), ALL_NO_PREFIX);
  ATTR_U1 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::u1), ALL_NO_PREFIX);
  ATTR_X1 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::x1), ALL_NO_PREFIX);
  ATTR_Y1 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::y1), ALL_NO_PREFIX);
  ATTR_G2 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::g2), ALL_NO_PREFIX);
  ATTR_K2 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::k2), ALL_NO_PREFIX);
  ATTR_U2 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::u2), ALL_NO_PREFIX);
  ATTR_X2 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::x2), ALL_NO_PREFIX);
  ATTR_Y2 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::y2), ALL_NO_PREFIX);
  ATTR_K3 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::k3), ALL_NO_PREFIX);
  ATTR_K4 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::k4), ALL_NO_PREFIX);
  ATTR_XML_SPACE = new nsHtml5AttributeName(
    XML_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xml_space, nsHtml5Atoms::space),
    XML_PREFIX);
  ATTR_XML_LANG = new nsHtml5AttributeName(
    XML_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xml_lang, nsHtml5Atoms::lang),
    XML_PREFIX);
  ATTR_XML_BASE = new nsHtml5AttributeName(
    XML_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xml_base, nsHtml5Atoms::base),
    XML_PREFIX);
  ATTR_ARIA_GRAB = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_grab), ALL_NO_PREFIX);
  ATTR_ARIA_VALUEMAX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_valuemax), ALL_NO_PREFIX);
  ATTR_ARIA_LABELLEDBY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_labelledby), ALL_NO_PREFIX);
  ATTR_ARIA_DESCRIBEDBY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_describedby), ALL_NO_PREFIX);
  ATTR_ARIA_DISABLED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_disabled), ALL_NO_PREFIX);
  ATTR_ARIA_CHECKED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_checked), ALL_NO_PREFIX);
  ATTR_ARIA_SELECTED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_selected), ALL_NO_PREFIX);
  ATTR_ARIA_DROPEFFECT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_dropeffect), ALL_NO_PREFIX);
  ATTR_ARIA_REQUIRED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_required), ALL_NO_PREFIX);
  ATTR_ARIA_EXPANDED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_expanded), ALL_NO_PREFIX);
  ATTR_ARIA_PRESSED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_pressed), ALL_NO_PREFIX);
  ATTR_ARIA_LEVEL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_level), ALL_NO_PREFIX);
  ATTR_ARIA_CHANNEL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_channel), ALL_NO_PREFIX);
  ATTR_ARIA_HIDDEN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_hidden), ALL_NO_PREFIX);
  ATTR_ARIA_SECRET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_secret), ALL_NO_PREFIX);
  ATTR_ARIA_POSINSET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_posinset), ALL_NO_PREFIX);
  ATTR_ARIA_ATOMIC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_atomic), ALL_NO_PREFIX);
  ATTR_ARIA_INVALID = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_invalid), ALL_NO_PREFIX);
  ATTR_ARIA_TEMPLATEID = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_templateid), ALL_NO_PREFIX);
  ATTR_ARIA_VALUEMIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_valuemin), ALL_NO_PREFIX);
  ATTR_ARIA_MULTISELECTABLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_multiselectable), ALL_NO_PREFIX);
  ATTR_ARIA_CONTROLS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_controls), ALL_NO_PREFIX);
  ATTR_ARIA_MULTILINE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_multiline), ALL_NO_PREFIX);
  ATTR_ARIA_READONLY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_readonly), ALL_NO_PREFIX);
  ATTR_ARIA_OWNS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_owns), ALL_NO_PREFIX);
  ATTR_ARIA_ACTIVEDESCENDANT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_activedescendant), ALL_NO_PREFIX);
  ATTR_ARIA_RELEVANT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_relevant), ALL_NO_PREFIX);
  ATTR_ARIA_DATATYPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_datatype), ALL_NO_PREFIX);
  ATTR_ARIA_VALUENOW = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_valuenow), ALL_NO_PREFIX);
  ATTR_ARIA_SORT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_sort), ALL_NO_PREFIX);
  ATTR_ARIA_AUTOCOMPLETE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_autocomplete), ALL_NO_PREFIX);
  ATTR_ARIA_FLOWTO = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_flowto), ALL_NO_PREFIX);
  ATTR_ARIA_BUSY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_busy), ALL_NO_PREFIX);
  ATTR_ARIA_LIVE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_live), ALL_NO_PREFIX);
  ATTR_ARIA_HASPOPUP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_haspopup), ALL_NO_PREFIX);
  ATTR_ARIA_SETSIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::aria_setsize), ALL_NO_PREFIX);
  ATTR_CLEAR = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::clear), ALL_NO_PREFIX);
  ATTR_DATAFORMATAS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::dataformatas), ALL_NO_PREFIX);
  ATTR_DISABLED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::disabled), ALL_NO_PREFIX);
  ATTR_DATAFLD = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::datafld), ALL_NO_PREFIX);
  ATTR_DEFAULT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::default_), ALL_NO_PREFIX);
  ATTR_DATASRC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::datasrc), ALL_NO_PREFIX);
  ATTR_DATA = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::data), ALL_NO_PREFIX);
  ATTR_EQUALCOLUMNS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::equalcolumns), ALL_NO_PREFIX);
  ATTR_EQUALROWS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::equalrows), ALL_NO_PREFIX);
  ATTR_HSPACE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::hspace), ALL_NO_PREFIX);
  ATTR_ISMAP = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ismap), ALL_NO_PREFIX);
  ATTR_LOCAL = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::local), ALL_NO_PREFIX);
  ATTR_LSPACE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::lspace), ALL_NO_PREFIX);
  ATTR_MOVABLELIMITS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::movablelimits), ALL_NO_PREFIX);
  ATTR_NOTATION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::notation), ALL_NO_PREFIX);
  ATTR_ONDATASETCHANGED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondatasetchanged), ALL_NO_PREFIX);
  ATTR_ONDATAAVAILABLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondataavailable), ALL_NO_PREFIX);
  ATTR_ONPASTE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onpaste), ALL_NO_PREFIX);
  ATTR_ONDATASETCOMPLETE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondatasetcomplete), ALL_NO_PREFIX);
  ATTR_RSPACE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rspace), ALL_NO_PREFIX);
  ATTR_ROWALIGN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rowalign), ALL_NO_PREFIX);
  ATTR_ROTATE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rotate), ALL_NO_PREFIX);
  ATTR_SEPARATOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::separator), ALL_NO_PREFIX);
  ATTR_SEPARATORS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::separators), ALL_NO_PREFIX);
  ATTR_V_MATHEMATICAL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::v_mathematical), ALL_NO_PREFIX);
  ATTR_VSPACE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::vspace), ALL_NO_PREFIX);
  ATTR_V_HANGING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::v_hanging), ALL_NO_PREFIX);
  ATTR_XCHANNELSELECTOR =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::xchannelselector,
                                           nsHtml5Atoms::xChannelSelector),
                             ALL_NO_PREFIX);
  ATTR_YCHANNELSELECTOR =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::ychannelselector,
                                           nsHtml5Atoms::yChannelSelector),
                             ALL_NO_PREFIX);
  ATTR_ARABIC_FORM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::arabic_form), ALL_NO_PREFIX);
  ATTR_ENABLE_BACKGROUND = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::enable_background), ALL_NO_PREFIX);
  ATTR_ONDBLCLICK = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondblclick), ALL_NO_PREFIX);
  ATTR_ONABORT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onabort), ALL_NO_PREFIX);
  ATTR_CALCMODE = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::calcmode, nsHtml5Atoms::calcMode),
    ALL_NO_PREFIX);
  ATTR_CHECKED = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::checked), ALL_NO_PREFIX);
  ATTR_DESCENT = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::descent), ALL_NO_PREFIX);
  ATTR_FENCE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fence), ALL_NO_PREFIX);
  ATTR_ONSCROLL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onscroll), ALL_NO_PREFIX);
  ATTR_ONACTIVATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onactivate), ALL_NO_PREFIX);
  ATTR_OPACITY = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::opacity), ALL_NO_PREFIX);
  ATTR_SPACING = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::spacing), ALL_NO_PREFIX);
  ATTR_SPECULAREXPONENT =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::specularexponent,
                                           nsHtml5Atoms::specularExponent),
                             ALL_NO_PREFIX);
  ATTR_SPECULARCONSTANT =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::specularconstant,
                                           nsHtml5Atoms::specularConstant),
                             ALL_NO_PREFIX);
  ATTR_SPECIFICATION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::specification), ALL_NO_PREFIX);
  ATTR_THICKMATHSPACE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::thickmathspace), ALL_NO_PREFIX);
  ATTR_UNICODE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::unicode_), ALL_NO_PREFIX);
  ATTR_UNICODE_BIDI = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::unicode_bidi), ALL_NO_PREFIX);
  ATTR_UNICODE_RANGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::unicode_range), ALL_NO_PREFIX);
  ATTR_BORDER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::border), ALL_NO_PREFIX);
  ATTR_ID = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::id), ALL_NO_PREFIX);
  ATTR_GRADIENTTRANSFORM =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::gradienttransform,
                                           nsHtml5Atoms::gradientTransform),
                             ALL_NO_PREFIX);
  ATTR_GRADIENTUNITS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::gradientunits, nsHtml5Atoms::gradientUnits),
    ALL_NO_PREFIX);
  ATTR_HIDDEN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::hidden), ALL_NO_PREFIX);
  ATTR_HEADERS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::headers), ALL_NO_PREFIX);
  ATTR_READONLY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::readonly), ALL_NO_PREFIX);
  ATTR_RENDERING_INTENT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rendering_intent), ALL_NO_PREFIX);
  ATTR_SEED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::seed), ALL_NO_PREFIX);
  ATTR_SRCDOC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::srcdoc), ALL_NO_PREFIX);
  ATTR_STDDEVIATION = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::stddeviation, nsHtml5Atoms::stdDeviation),
    ALL_NO_PREFIX);
  ATTR_SANDBOX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::sandbox), ALL_NO_PREFIX);
  ATTR_V_IDEOGRAPHIC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::v_ideographic), ALL_NO_PREFIX);
  ATTR_WORD_SPACING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::word_spacing), ALL_NO_PREFIX);
  ATTR_ACCENTUNDER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::accentunder), ALL_NO_PREFIX);
  ATTR_ACCEPT_CHARSET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::accept_charset), ALL_NO_PREFIX);
  ATTR_ACCESSKEY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::accesskey), ALL_NO_PREFIX);
  ATTR_ACCENT_HEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::accent_height), ALL_NO_PREFIX);
  ATTR_ACCENT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::accent), ALL_NO_PREFIX);
  ATTR_ASCENT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ascent), ALL_NO_PREFIX);
  ATTR_ACCEPT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::accept), ALL_NO_PREFIX);
  ATTR_BEVELLED = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::bevelled), ALL_NO_PREFIX);
  ATTR_BASEFREQUENCY = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::basefrequency, nsHtml5Atoms::baseFrequency),
    ALL_NO_PREFIX);
  ATTR_BASELINE_SHIFT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::baseline_shift), ALL_NO_PREFIX);
  ATTR_BASEPROFILE = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::baseprofile, nsHtml5Atoms::baseProfile),
    ALL_NO_PREFIX);
  ATTR_BASELINE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::baseline), ALL_NO_PREFIX);
  ATTR_BASE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::base), ALL_NO_PREFIX);
  ATTR_CODE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::code), ALL_NO_PREFIX);
  ATTR_CODETYPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::codetype), ALL_NO_PREFIX);
  ATTR_CODEBASE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::codebase), ALL_NO_PREFIX);
  ATTR_CITE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cite), ALL_NO_PREFIX);
  ATTR_DEFER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::defer), ALL_NO_PREFIX);
  ATTR_DATETIME = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::datetime), ALL_NO_PREFIX);
  ATTR_DIRECTION = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::direction), ALL_NO_PREFIX);
  ATTR_EDGEMODE = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::edgemode, nsHtml5Atoms::edgeMode),
    ALL_NO_PREFIX);
  ATTR_EDGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::edge), ALL_NO_PREFIX);
  ATTR_FACE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::face), ALL_NO_PREFIX);
  ATTR_HIDEFOCUS = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::hidefocus), ALL_NO_PREFIX);
  ATTR_INDEX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::index), ALL_NO_PREFIX);
  ATTR_IRRELEVANT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::irrelevant), ALL_NO_PREFIX);
  ATTR_INTERCEPT = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::intercept), ALL_NO_PREFIX);
  ATTR_INTEGRITY = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::integrity), ALL_NO_PREFIX);
  ATTR_LINEBREAK = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::linebreak), ALL_NO_PREFIX);
  ATTR_LABEL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::label), ALL_NO_PREFIX);
  ATTR_LINETHICKNESS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::linethickness), ALL_NO_PREFIX);
  ATTR_MODE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mode), ALL_NO_PREFIX);
  ATTR_NAME = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::name), ALL_NO_PREFIX);
  ATTR_NORESIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::noresize), ALL_NO_PREFIX);
  ATTR_ONBEFOREUNLOAD = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforeunload), ALL_NO_PREFIX);
  ATTR_ONREPEAT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onrepeat), ALL_NO_PREFIX);
  ATTR_OBJECT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::object), ALL_NO_PREFIX);
  ATTR_ONSELECT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onselect), ALL_NO_PREFIX);
  ATTR_ORDER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::order), ALL_NO_PREFIX);
  ATTR_OTHER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::other), ALL_NO_PREFIX);
  ATTR_ONRESET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onreset), ALL_NO_PREFIX);
  ATTR_ONCELLCHANGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::oncellchange), ALL_NO_PREFIX);
  ATTR_ONREADYSTATECHANGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onreadystatechange), ALL_NO_PREFIX);
  ATTR_ONMESSAGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmessage), ALL_NO_PREFIX);
  ATTR_ONBEGIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbegin), ALL_NO_PREFIX);
  ATTR_ONHELP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onhelp), ALL_NO_PREFIX);
  ATTR_ONBEFOREPRINT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforeprint), ALL_NO_PREFIX);
  ATTR_ORIENT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::orient), ALL_NO_PREFIX);
  ATTR_ORIENTATION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::orientation), ALL_NO_PREFIX);
  ATTR_ONBEFORECOPY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforecopy), ALL_NO_PREFIX);
  ATTR_ONSELECTSTART = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onselectstart), ALL_NO_PREFIX);
  ATTR_ONBEFOREPASTE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforepaste), ALL_NO_PREFIX);
  ATTR_ONBEFOREUPDATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforeupdate), ALL_NO_PREFIX);
  ATTR_ONDEACTIVATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondeactivate), ALL_NO_PREFIX);
  ATTR_ONBEFOREACTIVATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforeactivate), ALL_NO_PREFIX);
  ATTR_ONBEFORDEACTIVATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbefordeactivate), ALL_NO_PREFIX);
  ATTR_ONKEYPRESS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onkeypress), ALL_NO_PREFIX);
  ATTR_ONKEYUP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onkeyup), ALL_NO_PREFIX);
  ATTR_ONBEFOREEDITFOCUS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforeeditfocus), ALL_NO_PREFIX);
  ATTR_ONBEFORECUT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbeforecut), ALL_NO_PREFIX);
  ATTR_ONKEYDOWN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onkeydown), ALL_NO_PREFIX);
  ATTR_ONRESIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onresize), ALL_NO_PREFIX);
  ATTR_REPEAT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::repeat), ALL_NO_PREFIX);
  ATTR_REPEAT_MAX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::repeat_max), ALL_NO_PREFIX);
  ATTR_REFERRERPOLICY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::referrerpolicy), ALL_NO_PREFIX);
  ATTR_RULES = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rules), ALL_NO_PREFIX);
  ATTR_REPEAT_MIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::repeat_min), ALL_NO_PREFIX);
  ATTR_ROLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::role), ALL_NO_PREFIX);
  ATTR_REPEATCOUNT = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::repeatcount, nsHtml5Atoms::repeatCount),
    ALL_NO_PREFIX);
  ATTR_REPEAT_START = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::repeat_start), ALL_NO_PREFIX);
  ATTR_REPEAT_TEMPLATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::repeat_template), ALL_NO_PREFIX);
  ATTR_REPEATDUR = new nsHtml5AttributeName(ALL_NO_NS, SVG_DIFFERENT(nsHtml5Atoms::repeatdur, nsHtml5Atoms::repeatDur), ALL_NO_PREFIX);
  ATTR_SELECTED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::selected), ALL_NO_PREFIX);
  ATTR_SPEED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::speed), ALL_NO_PREFIX);
  ATTR_SIZES = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::sizes), ALL_NO_PREFIX);
  ATTR_SUPERSCRIPTSHIFT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::superscriptshift), ALL_NO_PREFIX);
  ATTR_STRETCHY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stretchy), ALL_NO_PREFIX);
  ATTR_SCHEME = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scheme), ALL_NO_PREFIX);
  ATTR_SPREADMETHOD = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::spreadmethod, nsHtml5Atoms::spreadMethod),
    ALL_NO_PREFIX);
  ATTR_SELECTION = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::selection), ALL_NO_PREFIX);
  ATTR_SIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::size), ALL_NO_PREFIX);
  ATTR_TYPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::type), ALL_NO_PREFIX);
  ATTR_UNSELECTABLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::unselectable), ALL_NO_PREFIX);
  ATTR_UNDERLINE_POSITION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::underline_position), ALL_NO_PREFIX);
  ATTR_UNDERLINE_THICKNESS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::underline_thickness), ALL_NO_PREFIX);
  ATTR_X_HEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::x_height), ALL_NO_PREFIX);
  ATTR_DIFFUSECONSTANT = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::diffuseconstant, nsHtml5Atoms::diffuseConstant),
    ALL_NO_PREFIX);
  ATTR_HREF = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::href), ALL_NO_PREFIX);
  ATTR_HREFLANG = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::hreflang), ALL_NO_PREFIX);
  ATTR_ONAFTERPRINT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onafterprint), ALL_NO_PREFIX);
  ATTR_ONAFTERUPDATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onafterupdate), ALL_NO_PREFIX);
  ATTR_PROFILE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::profile), ALL_NO_PREFIX);
  ATTR_SURFACESCALE = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::surfacescale, nsHtml5Atoms::surfaceScale),
    ALL_NO_PREFIX);
  ATTR_XREF = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::xref), ALL_NO_PREFIX);
  ATTR_ALIGN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::align), ALL_NO_PREFIX);
  ATTR_ALIGNMENT_BASELINE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::alignment_baseline), ALL_NO_PREFIX);
  ATTR_ALIGNMENTSCOPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::alignmentscope), ALL_NO_PREFIX);
  ATTR_DRAGGABLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::draggable), ALL_NO_PREFIX);
  ATTR_HEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::height), ALL_NO_PREFIX);
  ATTR_HANGING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::hanging), ALL_NO_PREFIX);
  ATTR_IMAGE_RENDERING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::image_rendering), ALL_NO_PREFIX);
  ATTR_LANGUAGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::language), ALL_NO_PREFIX);
  ATTR_LANG = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::lang), ALL_NO_PREFIX);
  ATTR_LARGEOP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::largeop), ALL_NO_PREFIX);
  ATTR_LONGDESC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::longdesc), ALL_NO_PREFIX);
  ATTR_LENGTHADJUST = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::lengthadjust, nsHtml5Atoms::lengthAdjust),
    ALL_NO_PREFIX);
  ATTR_MARGINHEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::marginheight), ALL_NO_PREFIX);
  ATTR_MARGINWIDTH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::marginwidth), ALL_NO_PREFIX);
  ATTR_NARGS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::nargs), ALL_NO_PREFIX);
  ATTR_ORIGIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::origin), ALL_NO_PREFIX);
  ATTR_PING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ping), ALL_NO_PREFIX);
  ATTR_TARGET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::target), ALL_NO_PREFIX);
  ATTR_TARGETX = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::targetx, nsHtml5Atoms::targetX),
    ALL_NO_PREFIX);
  ATTR_TARGETY = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::targety, nsHtml5Atoms::targetY),
    ALL_NO_PREFIX);
  ATTR_ALPHABETIC = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::alphabetic), ALL_NO_PREFIX);
  ATTR_ARCHIVE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::archive), ALL_NO_PREFIX);
  ATTR_HIGH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::high), ALL_NO_PREFIX);
  ATTR_LIGHTING_COLOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::lighting_color), ALL_NO_PREFIX);
  ATTR_MATHEMATICAL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mathematical), ALL_NO_PREFIX);
  ATTR_MATHBACKGROUND = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mathbackground), ALL_NO_PREFIX);
  ATTR_METHOD = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::method), ALL_NO_PREFIX);
  ATTR_MATHVARIANT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mathvariant), ALL_NO_PREFIX);
  ATTR_MATHCOLOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mathcolor), ALL_NO_PREFIX);
  ATTR_MATHSIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mathsize), ALL_NO_PREFIX);
  ATTR_NOSHADE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::noshade), ALL_NO_PREFIX);
  ATTR_ONCHANGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onchange), ALL_NO_PREFIX);
  ATTR_PATHLENGTH = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::pathlength, nsHtml5Atoms::pathLength),
    ALL_NO_PREFIX);
  ATTR_PATH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::path), ALL_NO_PREFIX);
  ATTR_ALTIMG = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::altimg), ALL_NO_PREFIX);
  ATTR_ACTIONTYPE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::actiontype), ALL_NO_PREFIX);
  ATTR_ACTION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::action), ALL_NO_PREFIX);
  ATTR_ACTIVE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::active), ALL_NO_PREFIX);
  ATTR_ADDITIVE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::additive), ALL_NO_PREFIX);
  ATTR_BEGIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::begin), ALL_NO_PREFIX);
  ATTR_DOMINANT_BASELINE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::dominant_baseline), ALL_NO_PREFIX);
  ATTR_DIVISOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::divisor), ALL_NO_PREFIX);
  ATTR_DEFINITIONURL = new nsHtml5AttributeName(
    ALL_NO_NS,
    MATH_DIFFERENT(nsHtml5Atoms::definitionurl, nsHtml5Atoms::definitionURL),
    ALL_NO_PREFIX);
  ATTR_HORIZ_ADV_X = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::horiz_adv_x), ALL_NO_PREFIX);
  ATTR_HORIZ_ORIGIN_X = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::horiz_origin_x), ALL_NO_PREFIX);
  ATTR_HORIZ_ORIGIN_Y = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::horiz_origin_y), ALL_NO_PREFIX);
  ATTR_LIMITINGCONEANGLE =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::limitingconeangle,
                                           nsHtml5Atoms::limitingConeAngle),
                             ALL_NO_PREFIX);
  ATTR_MEDIUMMATHSPACE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mediummathspace), ALL_NO_PREFIX);
  ATTR_MEDIA = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::media), ALL_NO_PREFIX);
  ATTR_MANIFEST = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::manifest), ALL_NO_PREFIX);
  ATTR_ONFILTERCHANGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onfilterchange), ALL_NO_PREFIX);
  ATTR_ONFINISH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onfinish), ALL_NO_PREFIX);
  ATTR_OPTIMUM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::optimum), ALL_NO_PREFIX);
  ATTR_RADIOGROUP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::radiogroup), ALL_NO_PREFIX);
  ATTR_RADIUS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::radius), ALL_NO_PREFIX);
  ATTR_SCRIPTLEVEL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scriptlevel), ALL_NO_PREFIX);
  ATTR_SCRIPTSIZEMULTIPLIER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scriptsizemultiplier), ALL_NO_PREFIX);
  ATTR_STRING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::string), ALL_NO_PREFIX);
  ATTR_STRIKETHROUGH_POSITION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::strikethrough_position), ALL_NO_PREFIX);
  ATTR_STRIKETHROUGH_THICKNESS =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SAME_LOCAL(nsHtml5Atoms::strikethrough_thickness),
                             ALL_NO_PREFIX);
  ATTR_SCRIPTMINSIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scriptminsize), ALL_NO_PREFIX);
  ATTR_TABINDEX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::tabindex), ALL_NO_PREFIX);
  ATTR_VALIGN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::valign), ALL_NO_PREFIX);
  ATTR_VISIBILITY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::visibility), ALL_NO_PREFIX);
  ATTR_BACKGROUND = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::background), ALL_NO_PREFIX);
  ATTR_LINK = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::link), ALL_NO_PREFIX);
  ATTR_MARKER_MID = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::marker_mid), ALL_NO_PREFIX);
  ATTR_MARKERHEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::markerheight, nsHtml5Atoms::markerHeight),
    ALL_NO_PREFIX);
  ATTR_MARKER_END = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::marker_end), ALL_NO_PREFIX);
  ATTR_MASK = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::mask), ALL_NO_PREFIX);
  ATTR_MARKER_START = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::marker_start), ALL_NO_PREFIX);
  ATTR_MARKERWIDTH = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::markerwidth, nsHtml5Atoms::markerWidth),
    ALL_NO_PREFIX);
  ATTR_MASKUNITS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::maskunits, nsHtml5Atoms::maskUnits),
    ALL_NO_PREFIX);
  ATTR_MARKERUNITS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::markerunits, nsHtml5Atoms::markerUnits),
    ALL_NO_PREFIX);
  ATTR_MASKCONTENTUNITS =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::maskcontentunits,
                                           nsHtml5Atoms::maskContentUnits),
                             ALL_NO_PREFIX);
  ATTR_AMPLITUDE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::amplitude), ALL_NO_PREFIX);
  ATTR_CELLSPACING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cellspacing), ALL_NO_PREFIX);
  ATTR_CELLPADDING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cellpadding), ALL_NO_PREFIX);
  ATTR_DECLARE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::declare), ALL_NO_PREFIX);
  ATTR_FILL_RULE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fill_rule), ALL_NO_PREFIX);
  ATTR_FILL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fill), ALL_NO_PREFIX);
  ATTR_FILL_OPACITY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fill_opacity), ALL_NO_PREFIX);
  ATTR_MAXLENGTH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::maxlength), ALL_NO_PREFIX);
  ATTR_ONCLICK = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onclick), ALL_NO_PREFIX);
  ATTR_ONBLUR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onblur), ALL_NO_PREFIX);
  ATTR_REPLACE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::replace), ALL_NO_PREFIX);
  ATTR_ROWLINES = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rowlines), ALL_NO_PREFIX);
  ATTR_SCALE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scale), ALL_NO_PREFIX);
  ATTR_STYLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::style), ALL_NO_PREFIX);
  ATTR_TABLEVALUES = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::tablevalues, nsHtml5Atoms::tableValues),
    ALL_NO_PREFIX);
  ATTR_TITLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::title), ALL_NO_PREFIX);
  ATTR_V_ALPHABETIC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::v_alphabetic), ALL_NO_PREFIX);
  ATTR_AZIMUTH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::azimuth), ALL_NO_PREFIX);
  ATTR_FORMAT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::format), ALL_NO_PREFIX);
  ATTR_FRAMEBORDER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::frameborder), ALL_NO_PREFIX);
  ATTR_FRAME = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::frame), ALL_NO_PREFIX);
  ATTR_FRAMESPACING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::framespacing), ALL_NO_PREFIX);
  ATTR_FROM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::from), ALL_NO_PREFIX);
  ATTR_FORM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::form), ALL_NO_PREFIX);
  ATTR_PROMPT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::prompt), ALL_NO_PREFIX);
  ATTR_PRIMITIVEUNITS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::primitiveunits, nsHtml5Atoms::primitiveUnits),
    ALL_NO_PREFIX);
  ATTR_SYMMETRIC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::symmetric), ALL_NO_PREFIX);
  ATTR_STEMH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stemh), ALL_NO_PREFIX);
  ATTR_STEMV = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stemv), ALL_NO_PREFIX);
  ATTR_SEAMLESS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::seamless), ALL_NO_PREFIX);
  ATTR_SUMMARY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::summary), ALL_NO_PREFIX);
  ATTR_USEMAP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::usemap), ALL_NO_PREFIX);
  ATTR_ZOOMANDPAN = new nsHtml5AttributeName(ALL_NO_NS, SVG_DIFFERENT(nsHtml5Atoms::zoomandpan, nsHtml5Atoms::zoomAndPan), ALL_NO_PREFIX);
  ATTR_ASYNC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::async), ALL_NO_PREFIX);
  ATTR_ALINK = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::alink), ALL_NO_PREFIX);
  ATTR_IN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::in), ALL_NO_PREFIX);
  ATTR_ICON = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::icon), ALL_NO_PREFIX);
  ATTR_KERNELMATRIX = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::kernelmatrix, nsHtml5Atoms::kernelMatrix),
    ALL_NO_PREFIX);
  ATTR_KERNING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::kerning), ALL_NO_PREFIX);
  ATTR_KERNELUNITLENGTH =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::kernelunitlength,
                                           nsHtml5Atoms::kernelUnitLength),
                             ALL_NO_PREFIX);
  ATTR_ONUNLOAD = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onunload), ALL_NO_PREFIX);
  ATTR_OPEN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::open), ALL_NO_PREFIX);
  ATTR_ONINVALID = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::oninvalid), ALL_NO_PREFIX);
  ATTR_ONEND = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onend), ALL_NO_PREFIX);
  ATTR_ONINPUT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::oninput), ALL_NO_PREFIX);
  ATTR_POINTER_EVENTS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::pointer_events), ALL_NO_PREFIX);
  ATTR_POINTS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::points), ALL_NO_PREFIX);
  ATTR_POINTSATX = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::pointsatx, nsHtml5Atoms::pointsAtX),
    ALL_NO_PREFIX);
  ATTR_POINTSATY = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::pointsaty, nsHtml5Atoms::pointsAtY),
    ALL_NO_PREFIX);
  ATTR_POINTSATZ = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::pointsatz, nsHtml5Atoms::pointsAtZ),
    ALL_NO_PREFIX);
  ATTR_SPAN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::span), ALL_NO_PREFIX);
  ATTR_STANDBY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::standby), ALL_NO_PREFIX);
  ATTR_THINMATHSPACE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::thinmathspace), ALL_NO_PREFIX);
  ATTR_TRANSFORM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::transform), ALL_NO_PREFIX);
  ATTR_VLINK = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::vlink), ALL_NO_PREFIX);
  ATTR_WHEN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::when), ALL_NO_PREFIX);
  ATTR_XLINK_HREF = new nsHtml5AttributeName(XLINK_NS, COLONIFIED_LOCAL(nsHtml5Atoms::xlink_href, nsHtml5Atoms::href), XLINK_PREFIX);
  ATTR_XLINK_TITLE = new nsHtml5AttributeName(
    XLINK_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xlink_title, nsHtml5Atoms::title),
    XLINK_PREFIX);
  ATTR_XLINK_ROLE = new nsHtml5AttributeName(
    XLINK_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xlink_role, nsHtml5Atoms::role),
    XLINK_PREFIX);
  ATTR_XLINK_ARCROLE = new nsHtml5AttributeName(
    XLINK_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xlink_arcrole, nsHtml5Atoms::arcrole),
    XLINK_PREFIX);
  ATTR_XMLNS_XLINK = new nsHtml5AttributeName(
    XMLNS_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xmlns_xlink, nsHtml5Atoms::xlink),
    XMLNS_PREFIX);
  ATTR_XMLNS = new nsHtml5AttributeName(
    XMLNS_NS, SAME_LOCAL(nsHtml5Atoms::xmlns), ALL_NO_PREFIX);
  ATTR_XLINK_TYPE = new nsHtml5AttributeName(
    XLINK_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xlink_type, nsHtml5Atoms::type),
    XLINK_PREFIX);
  ATTR_XLINK_SHOW = new nsHtml5AttributeName(XLINK_NS, COLONIFIED_LOCAL(nsHtml5Atoms::xlink_show, nsHtml5Atoms::show), XLINK_PREFIX);
  ATTR_XLINK_ACTUATE = new nsHtml5AttributeName(
    XLINK_NS,
    COLONIFIED_LOCAL(nsHtml5Atoms::xlink_actuate, nsHtml5Atoms::actuate),
    XLINK_PREFIX);
  ATTR_AUTOPLAY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::autoplay), ALL_NO_PREFIX);
  ATTR_AUTOSUBMIT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::autosubmit), ALL_NO_PREFIX);
  ATTR_AUTOCOMPLETE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::autocomplete), ALL_NO_PREFIX);
  ATTR_AUTOFOCUS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::autofocus), ALL_NO_PREFIX);
  ATTR_BGCOLOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::bgcolor), ALL_NO_PREFIX);
  ATTR_COLOR_PROFILE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::color_profile), ALL_NO_PREFIX);
  ATTR_COLOR_RENDERING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::color_rendering), ALL_NO_PREFIX);
  ATTR_COLOR_INTERPOLATION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::color_interpolation), ALL_NO_PREFIX);
  ATTR_COLOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::color), ALL_NO_PREFIX);
  ATTR_COLOR_INTERPOLATION_FILTERS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SAME_LOCAL(nsHtml5Atoms::color_interpolation_filters),
    ALL_NO_PREFIX);
  ATTR_ENCODING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::encoding), ALL_NO_PREFIX);
  ATTR_EXPONENT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::exponent), ALL_NO_PREFIX);
  ATTR_FLOOD_COLOR = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::flood_color), ALL_NO_PREFIX);
  ATTR_FLOOD_OPACITY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::flood_opacity), ALL_NO_PREFIX);
  ATTR_IDEOGRAPHIC = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ideographic), ALL_NO_PREFIX);
  ATTR_LQUOTE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::lquote), ALL_NO_PREFIX);
  ATTR_PANOSE_1 = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::panose_1), ALL_NO_PREFIX);
  ATTR_NUMOCTAVES = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::numoctaves, nsHtml5Atoms::numOctaves),
    ALL_NO_PREFIX);
  ATTR_ONLOAD = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onload), ALL_NO_PREFIX);
  ATTR_ONBOUNCE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onbounce), ALL_NO_PREFIX);
  ATTR_ONCONTROLSELECT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::oncontrolselect), ALL_NO_PREFIX);
  ATTR_ONROWSINSERTED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onrowsinserted), ALL_NO_PREFIX);
  ATTR_ONMOUSEWHEEL = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmousewheel), ALL_NO_PREFIX);
  ATTR_ONROWENTER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onrowenter), ALL_NO_PREFIX);
  ATTR_ONMOUSEENTER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmouseenter), ALL_NO_PREFIX);
  ATTR_ONMOUSEOVER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmouseover), ALL_NO_PREFIX);
  ATTR_ONFORMCHANGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onformchange), ALL_NO_PREFIX);
  ATTR_ONFOCUSIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onfocusin), ALL_NO_PREFIX);
  ATTR_ONROWEXIT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onrowexit), ALL_NO_PREFIX);
  ATTR_ONMOVEEND = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmoveend), ALL_NO_PREFIX);
  ATTR_ONCONTEXTMENU = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::oncontextmenu), ALL_NO_PREFIX);
  ATTR_ONZOOM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onzoom), ALL_NO_PREFIX);
  ATTR_ONLOSECAPTURE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onlosecapture), ALL_NO_PREFIX);
  ATTR_ONCOPY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::oncopy), ALL_NO_PREFIX);
  ATTR_ONMOVESTART = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmovestart), ALL_NO_PREFIX);
  ATTR_ONROWSDELETE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onrowsdelete), ALL_NO_PREFIX);
  ATTR_ONMOUSELEAVE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmouseleave), ALL_NO_PREFIX);
  ATTR_ONMOVE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmove), ALL_NO_PREFIX);
  ATTR_ONMOUSEMOVE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmousemove), ALL_NO_PREFIX);
  ATTR_ONMOUSEUP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmouseup), ALL_NO_PREFIX);
  ATTR_ONFOCUS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onfocus), ALL_NO_PREFIX);
  ATTR_ONMOUSEOUT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmouseout), ALL_NO_PREFIX);
  ATTR_ONFORMINPUT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onforminput), ALL_NO_PREFIX);
  ATTR_ONFOCUSOUT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onfocusout), ALL_NO_PREFIX);
  ATTR_ONMOUSEDOWN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onmousedown), ALL_NO_PREFIX);
  ATTR_TO = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::to), ALL_NO_PREFIX);
  ATTR_RQUOTE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rquote), ALL_NO_PREFIX);
  ATTR_STROKE_LINECAP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke_linecap), ALL_NO_PREFIX);
  ATTR_SCROLLDELAY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scrolldelay), ALL_NO_PREFIX);
  ATTR_STROKE_DASHARRAY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke_dasharray), ALL_NO_PREFIX);
  ATTR_STROKE_DASHOFFSET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke_dashoffset), ALL_NO_PREFIX);
  ATTR_STROKE_LINEJOIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke_linejoin), ALL_NO_PREFIX);
  ATTR_STROKE_MITERLIMIT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke_miterlimit), ALL_NO_PREFIX);
  ATTR_STROKE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke), ALL_NO_PREFIX);
  ATTR_SCROLLING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scrolling), ALL_NO_PREFIX);
  ATTR_STROKE_WIDTH = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke_width), ALL_NO_PREFIX);
  ATTR_STROKE_OPACITY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stroke_opacity), ALL_NO_PREFIX);
  ATTR_COMPACT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::compact), ALL_NO_PREFIX);
  ATTR_CLIP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::clip), ALL_NO_PREFIX);
  ATTR_CLIP_RULE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::clip_rule), ALL_NO_PREFIX);
  ATTR_CLIP_PATH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::clip_path), ALL_NO_PREFIX);
  ATTR_CLIPPATHUNITS = new nsHtml5AttributeName(ALL_NO_NS, SVG_DIFFERENT(nsHtml5Atoms::clippathunits, nsHtml5Atoms::clipPathUnits), ALL_NO_PREFIX);
  ATTR_DISPLAY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::display), ALL_NO_PREFIX);
  ATTR_DISPLAYSTYLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::displaystyle), ALL_NO_PREFIX);
  ATTR_GLYPH_ORIENTATION_VERTICAL = new nsHtml5AttributeName(
    ALL_NO_NS,
    SAME_LOCAL(nsHtml5Atoms::glyph_orientation_vertical),
    ALL_NO_PREFIX);
  ATTR_GLYPH_ORIENTATION_HORIZONTAL = new nsHtml5AttributeName(
    ALL_NO_NS,
    SAME_LOCAL(nsHtml5Atoms::glyph_orientation_horizontal),
    ALL_NO_PREFIX);
  ATTR_GLYPHREF = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::glyphref, nsHtml5Atoms::glyphRef),
    ALL_NO_PREFIX);
  ATTR_GLYPH_NAME = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::glyph_name), ALL_NO_PREFIX);
  ATTR_HTTP_EQUIV = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::http_equiv), ALL_NO_PREFIX);
  ATTR_KEYPOINTS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::keypoints, nsHtml5Atoms::keyPoints),
    ALL_NO_PREFIX);
  ATTR_LOOP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::loop), ALL_NO_PREFIX);
  ATTR_PROPERTY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::property), ALL_NO_PREFIX);
  ATTR_SCOPED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scoped), ALL_NO_PREFIX);
  ATTR_STEP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::step), ALL_NO_PREFIX);
  ATTR_SHAPE_RENDERING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::shape_rendering), ALL_NO_PREFIX);
  ATTR_SCOPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::scope), ALL_NO_PREFIX);
  ATTR_SHAPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::shape), ALL_NO_PREFIX);
  ATTR_SLOPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::slope), ALL_NO_PREFIX);
  ATTR_STOP_COLOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stop_color), ALL_NO_PREFIX);
  ATTR_STOP_OPACITY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::stop_opacity), ALL_NO_PREFIX);
  ATTR_TEMPLATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::template_), ALL_NO_PREFIX);
  ATTR_WRAP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::wrap), ALL_NO_PREFIX);
  ATTR_ABBR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::abbr), ALL_NO_PREFIX);
  ATTR_ATTRIBUTENAME = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::attributename, nsHtml5Atoms::attributeName),
    ALL_NO_PREFIX);
  ATTR_ATTRIBUTETYPE = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::attributetype, nsHtml5Atoms::attributeType),
    ALL_NO_PREFIX);
  ATTR_CHAR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::char_), ALL_NO_PREFIX);
  ATTR_COORDS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::coords), ALL_NO_PREFIX);
  ATTR_CHAROFF = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::charoff), ALL_NO_PREFIX);
  ATTR_CHARSET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::charset), ALL_NO_PREFIX);
  ATTR_MACROS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::macros), ALL_NO_PREFIX);
  ATTR_NOWRAP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::nowrap), ALL_NO_PREFIX);
  ATTR_NOHREF = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::nohref), ALL_NO_PREFIX);
  ATTR_ONDRAG = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondrag), ALL_NO_PREFIX);
  ATTR_ONDRAGENTER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondragenter), ALL_NO_PREFIX);
  ATTR_ONDRAGOVER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondragover), ALL_NO_PREFIX);
  ATTR_ONPROPERTYCHANGE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onpropertychange), ALL_NO_PREFIX);
  ATTR_ONDRAGEND = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondragend), ALL_NO_PREFIX);
  ATTR_ONDROP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondrop), ALL_NO_PREFIX);
  ATTR_ONDRAGDROP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondragdrop), ALL_NO_PREFIX);
  ATTR_OVERLINE_POSITION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::overline_position), ALL_NO_PREFIX);
  ATTR_ONERROR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onerror), ALL_NO_PREFIX);
  ATTR_OPERATOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::operator_), ALL_NO_PREFIX);
  ATTR_OVERFLOW = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::overflow), ALL_NO_PREFIX);
  ATTR_ONDRAGSTART = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondragstart), ALL_NO_PREFIX);
  ATTR_ONERRORUPDATE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onerrorupdate), ALL_NO_PREFIX);
  ATTR_OVERLINE_THICKNESS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::overline_thickness), ALL_NO_PREFIX);
  ATTR_ONDRAGLEAVE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ondragleave), ALL_NO_PREFIX);
  ATTR_STARTOFFSET = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::startoffset, nsHtml5Atoms::startOffset),
    ALL_NO_PREFIX);
  ATTR_START = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::start), ALL_NO_PREFIX);
  ATTR_AXIS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::axis), ALL_NO_PREFIX);
  ATTR_BIAS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::bias), ALL_NO_PREFIX);
  ATTR_COLSPAN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::colspan), ALL_NO_PREFIX);
  ATTR_CLASSID = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::classid), ALL_NO_PREFIX);
  ATTR_CROSSORIGIN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::crossorigin), ALL_NO_PREFIX);
  ATTR_COLS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cols), ALL_NO_PREFIX);
  ATTR_CURSOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cursor), ALL_NO_PREFIX);
  ATTR_CLOSURE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::closure), ALL_NO_PREFIX);
  ATTR_CLOSE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::close), ALL_NO_PREFIX);
  ATTR_CLASS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::class_), ALL_NO_PREFIX);
  ATTR_KEYSYSTEM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::keysystem), ALL_NO_PREFIX);
  ATTR_KEYSPLINES = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::keysplines, nsHtml5Atoms::keySplines),
    ALL_NO_PREFIX);
  ATTR_LOWSRC = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::lowsrc), ALL_NO_PREFIX);
  ATTR_MAXSIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::maxsize), ALL_NO_PREFIX);
  ATTR_MINSIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::minsize), ALL_NO_PREFIX);
  ATTR_OFFSET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::offset), ALL_NO_PREFIX);
  ATTR_PRESERVEALPHA = new nsHtml5AttributeName(ALL_NO_NS, SVG_DIFFERENT(nsHtml5Atoms::preservealpha, nsHtml5Atoms::preserveAlpha), ALL_NO_PREFIX);
  ATTR_PRESERVEASPECTRATIO =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::preserveaspectratio,
                                           nsHtml5Atoms::preserveAspectRatio),
                             ALL_NO_PREFIX);
  ATTR_ROWSPAN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rowspan), ALL_NO_PREFIX);
  ATTR_ROWSPACING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rowspacing), ALL_NO_PREFIX);
  ATTR_ROWS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rows), ALL_NO_PREFIX);
  ATTR_SRCSET = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::srcset), ALL_NO_PREFIX);
  ATTR_SUBSCRIPTSHIFT = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::subscriptshift), ALL_NO_PREFIX);
  ATTR_VERSION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::version), ALL_NO_PREFIX);
  ATTR_ALTTEXT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::alttext), ALL_NO_PREFIX);
  ATTR_CONTENTEDITABLE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::contenteditable), ALL_NO_PREFIX);
  ATTR_CONTROLS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::controls), ALL_NO_PREFIX);
  ATTR_CONTENT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::content), ALL_NO_PREFIX);
  ATTR_CONTEXTMENU = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::contextmenu), ALL_NO_PREFIX);
  ATTR_DEPTH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::depth), ALL_NO_PREFIX);
  ATTR_ENCTYPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::enctype), ALL_NO_PREFIX);
  ATTR_FONT_STRETCH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::font_stretch), ALL_NO_PREFIX);
  ATTR_FILTER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::filter), ALL_NO_PREFIX);
  ATTR_FONTWEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fontweight), ALL_NO_PREFIX);
  ATTR_FONT_WEIGHT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::font_weight), ALL_NO_PREFIX);
  ATTR_FONTSTYLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fontstyle), ALL_NO_PREFIX);
  ATTR_FONT_STYLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::font_style), ALL_NO_PREFIX);
  ATTR_FONTFAMILY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fontfamily), ALL_NO_PREFIX);
  ATTR_FONT_FAMILY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::font_family), ALL_NO_PREFIX);
  ATTR_FONT_VARIANT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::font_variant), ALL_NO_PREFIX);
  ATTR_FONT_SIZE_ADJUST = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::font_size_adjust), ALL_NO_PREFIX);
  ATTR_FILTERUNITS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::filterunits, nsHtml5Atoms::filterUnits),
    ALL_NO_PREFIX);
  ATTR_FONTSIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fontsize), ALL_NO_PREFIX);
  ATTR_FONT_SIZE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::font_size), ALL_NO_PREFIX);
  ATTR_KEYTIMES = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::keytimes, nsHtml5Atoms::keyTimes),
    ALL_NO_PREFIX);
  ATTR_LETTER_SPACING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::letter_spacing), ALL_NO_PREFIX);
  ATTR_LIST = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::list), ALL_NO_PREFIX);
  ATTR_MULTIPLE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::multiple), ALL_NO_PREFIX);
  ATTR_RT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rt), ALL_NO_PREFIX);
  ATTR_ONSTOP = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onstop), ALL_NO_PREFIX);
  ATTR_ONSTART = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onstart), ALL_NO_PREFIX);
  ATTR_POSTER = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::poster), ALL_NO_PREFIX);
  ATTR_PATTERNTRANSFORM = new nsHtml5AttributeName(ALL_NO_NS, SVG_DIFFERENT(nsHtml5Atoms::patterntransform, nsHtml5Atoms::patternTransform), ALL_NO_PREFIX);
  ATTR_PATTERN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::pattern), ALL_NO_PREFIX);
  ATTR_PATTERNUNITS = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::patternunits, nsHtml5Atoms::patternUnits),
    ALL_NO_PREFIX);
  ATTR_PATTERNCONTENTUNITS =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::patterncontentunits,
                                           nsHtml5Atoms::patternContentUnits),
                             ALL_NO_PREFIX);
  ATTR_RESTART = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::restart), ALL_NO_PREFIX);
  ATTR_STITCHTILES = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::stitchtiles, nsHtml5Atoms::stitchTiles),
    ALL_NO_PREFIX);
  ATTR_SYSTEMLANGUAGE = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::systemlanguage, nsHtml5Atoms::systemLanguage),
    ALL_NO_PREFIX);
  ATTR_TEXT_RENDERING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::text_rendering), ALL_NO_PREFIX);
  ATTR_VERT_ORIGIN_X = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::vert_origin_x), ALL_NO_PREFIX);
  ATTR_VERT_ADV_Y = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::vert_adv_y), ALL_NO_PREFIX);
  ATTR_VERT_ORIGIN_Y = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::vert_origin_y), ALL_NO_PREFIX);
  ATTR_TEXT_DECORATION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::text_decoration), ALL_NO_PREFIX);
  ATTR_TEXT_ANCHOR = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::text_anchor), ALL_NO_PREFIX);
  ATTR_TEXTLENGTH = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::textlength, nsHtml5Atoms::textLength),
    ALL_NO_PREFIX);
  ATTR_TEXT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::text), ALL_NO_PREFIX);
  ATTR_UNITS_PER_EM = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::units_per_em), ALL_NO_PREFIX);
  ATTR_WRITING_MODE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::writing_mode), ALL_NO_PREFIX);
  ATTR_WIDTHS = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::widths), ALL_NO_PREFIX);
  ATTR_WIDTH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::width), ALL_NO_PREFIX);
  ATTR_ACCUMULATE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::accumulate), ALL_NO_PREFIX);
  ATTR_COLUMNSPAN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::columnspan), ALL_NO_PREFIX);
  ATTR_COLUMNLINES = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::columnlines), ALL_NO_PREFIX);
  ATTR_COLUMNALIGN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::columnalign), ALL_NO_PREFIX);
  ATTR_COLUMNSPACING = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::columnspacing), ALL_NO_PREFIX);
  ATTR_COLUMNWIDTH = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::columnwidth), ALL_NO_PREFIX);
  ATTR_GROUPALIGN = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::groupalign), ALL_NO_PREFIX);
  ATTR_INPUTMODE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::inputmode), ALL_NO_PREFIX);
  ATTR_OCCURRENCE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::occurrence), ALL_NO_PREFIX);
  ATTR_ONSUBMIT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::onsubmit), ALL_NO_PREFIX);
  ATTR_ONCUT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::oncut), ALL_NO_PREFIX);
  ATTR_REQUIRED = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::required), ALL_NO_PREFIX);
  ATTR_REQUIREDFEATURES = new nsHtml5AttributeName(ALL_NO_NS, SVG_DIFFERENT(nsHtml5Atoms::requiredfeatures, nsHtml5Atoms::requiredFeatures), ALL_NO_PREFIX);
  ATTR_RESULT = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::result), ALL_NO_PREFIX);
  ATTR_REQUIREDEXTENSIONS =
    new nsHtml5AttributeName(ALL_NO_NS,
                             SVG_DIFFERENT(nsHtml5Atoms::requiredextensions,
                                           nsHtml5Atoms::requiredExtensions),
                             ALL_NO_PREFIX);
  ATTR_VALUES = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::values), ALL_NO_PREFIX);
  ATTR_VALUETYPE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::valuetype), ALL_NO_PREFIX);
  ATTR_VALUE = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::value), ALL_NO_PREFIX);
  ATTR_ELEVATION = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::elevation), ALL_NO_PREFIX);
  ATTR_VIEWTARGET = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::viewtarget, nsHtml5Atoms::viewTarget),
    ALL_NO_PREFIX);
  ATTR_VIEWBOX = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::viewbox, nsHtml5Atoms::viewBox),
    ALL_NO_PREFIX);
  ATTR_CX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cx), ALL_NO_PREFIX);
  ATTR_DX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::dx), ALL_NO_PREFIX);
  ATTR_FX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fx), ALL_NO_PREFIX);
  ATTR_BBOX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::bbox), ALL_NO_PREFIX);
  ATTR_RX = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::rx), ALL_NO_PREFIX);
  ATTR_REFX = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::refx, nsHtml5Atoms::refX),
    ALL_NO_PREFIX);
  ATTR_BY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::by), ALL_NO_PREFIX);
  ATTR_CY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::cy), ALL_NO_PREFIX);
  ATTR_DY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::dy), ALL_NO_PREFIX);
  ATTR_FY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::fy), ALL_NO_PREFIX);
  ATTR_RY = new nsHtml5AttributeName(
    ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::ry), ALL_NO_PREFIX);
  ATTR_REFY = new nsHtml5AttributeName(
    ALL_NO_NS,
    SVG_DIFFERENT(nsHtml5Atoms::refy, nsHtml5Atoms::refY),
    ALL_NO_PREFIX);
  ATTR_VERYTHINMATHSPACE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::verythinmathspace), ALL_NO_PREFIX);
  ATTR_VERYTHICKMATHSPACE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::verythickmathspace), ALL_NO_PREFIX);
  ATTR_VERYVERYTHINMATHSPACE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::veryverythinmathspace), ALL_NO_PREFIX);
  ATTR_VERYVERYTHICKMATHSPACE = new nsHtml5AttributeName(ALL_NO_NS, SAME_LOCAL(nsHtml5Atoms::veryverythickmathspace), ALL_NO_PREFIX);
  ATTRIBUTE_NAMES = new nsHtml5AttributeName*[583];
  ATTRIBUTE_NAMES[0] = ATTR_ALT;
  ATTRIBUTE_NAMES[1] = ATTR_DIR;
  ATTRIBUTE_NAMES[2] = ATTR_DUR;
  ATTRIBUTE_NAMES[3] = ATTR_END;
  ATTRIBUTE_NAMES[4] = ATTR_FOR;
  ATTRIBUTE_NAMES[5] = ATTR_IN2;
  ATTRIBUTE_NAMES[6] = ATTR_LOW;
  ATTRIBUTE_NAMES[7] = ATTR_MIN;
  ATTRIBUTE_NAMES[8] = ATTR_MAX;
  ATTRIBUTE_NAMES[9] = ATTR_REL;
  ATTRIBUTE_NAMES[10] = ATTR_REV;
  ATTRIBUTE_NAMES[11] = ATTR_SRC;
  ATTRIBUTE_NAMES[12] = ATTR_D;
  ATTRIBUTE_NAMES[13] = ATTR_K;
  ATTRIBUTE_NAMES[14] = ATTR_R;
  ATTRIBUTE_NAMES[15] = ATTR_X;
  ATTRIBUTE_NAMES[16] = ATTR_Y;
  ATTRIBUTE_NAMES[17] = ATTR_Z;
  ATTRIBUTE_NAMES[18] = ATTR_CAP_HEIGHT;
  ATTRIBUTE_NAMES[19] = ATTR_G1;
  ATTRIBUTE_NAMES[20] = ATTR_K1;
  ATTRIBUTE_NAMES[21] = ATTR_U1;
  ATTRIBUTE_NAMES[22] = ATTR_X1;
  ATTRIBUTE_NAMES[23] = ATTR_Y1;
  ATTRIBUTE_NAMES[24] = ATTR_G2;
  ATTRIBUTE_NAMES[25] = ATTR_K2;
  ATTRIBUTE_NAMES[26] = ATTR_U2;
  ATTRIBUTE_NAMES[27] = ATTR_X2;
  ATTRIBUTE_NAMES[28] = ATTR_Y2;
  ATTRIBUTE_NAMES[29] = ATTR_K3;
  ATTRIBUTE_NAMES[30] = ATTR_K4;
  ATTRIBUTE_NAMES[31] = ATTR_XML_SPACE;
  ATTRIBUTE_NAMES[32] = ATTR_XML_LANG;
  ATTRIBUTE_NAMES[33] = ATTR_XML_BASE;
  ATTRIBUTE_NAMES[34] = ATTR_ARIA_GRAB;
  ATTRIBUTE_NAMES[35] = ATTR_ARIA_VALUEMAX;
  ATTRIBUTE_NAMES[36] = ATTR_ARIA_LABELLEDBY;
  ATTRIBUTE_NAMES[37] = ATTR_ARIA_DESCRIBEDBY;
  ATTRIBUTE_NAMES[38] = ATTR_ARIA_DISABLED;
  ATTRIBUTE_NAMES[39] = ATTR_ARIA_CHECKED;
  ATTRIBUTE_NAMES[40] = ATTR_ARIA_SELECTED;
  ATTRIBUTE_NAMES[41] = ATTR_ARIA_DROPEFFECT;
  ATTRIBUTE_NAMES[42] = ATTR_ARIA_REQUIRED;
  ATTRIBUTE_NAMES[43] = ATTR_ARIA_EXPANDED;
  ATTRIBUTE_NAMES[44] = ATTR_ARIA_PRESSED;
  ATTRIBUTE_NAMES[45] = ATTR_ARIA_LEVEL;
  ATTRIBUTE_NAMES[46] = ATTR_ARIA_CHANNEL;
  ATTRIBUTE_NAMES[47] = ATTR_ARIA_HIDDEN;
  ATTRIBUTE_NAMES[48] = ATTR_ARIA_SECRET;
  ATTRIBUTE_NAMES[49] = ATTR_ARIA_POSINSET;
  ATTRIBUTE_NAMES[50] = ATTR_ARIA_ATOMIC;
  ATTRIBUTE_NAMES[51] = ATTR_ARIA_INVALID;
  ATTRIBUTE_NAMES[52] = ATTR_ARIA_TEMPLATEID;
  ATTRIBUTE_NAMES[53] = ATTR_ARIA_VALUEMIN;
  ATTRIBUTE_NAMES[54] = ATTR_ARIA_MULTISELECTABLE;
  ATTRIBUTE_NAMES[55] = ATTR_ARIA_CONTROLS;
  ATTRIBUTE_NAMES[56] = ATTR_ARIA_MULTILINE;
  ATTRIBUTE_NAMES[57] = ATTR_ARIA_READONLY;
  ATTRIBUTE_NAMES[58] = ATTR_ARIA_OWNS;
  ATTRIBUTE_NAMES[59] = ATTR_ARIA_ACTIVEDESCENDANT;
  ATTRIBUTE_NAMES[60] = ATTR_ARIA_RELEVANT;
  ATTRIBUTE_NAMES[61] = ATTR_ARIA_DATATYPE;
  ATTRIBUTE_NAMES[62] = ATTR_ARIA_VALUENOW;
  ATTRIBUTE_NAMES[63] = ATTR_ARIA_SORT;
  ATTRIBUTE_NAMES[64] = ATTR_ARIA_AUTOCOMPLETE;
  ATTRIBUTE_NAMES[65] = ATTR_ARIA_FLOWTO;
  ATTRIBUTE_NAMES[66] = ATTR_ARIA_BUSY;
  ATTRIBUTE_NAMES[67] = ATTR_ARIA_LIVE;
  ATTRIBUTE_NAMES[68] = ATTR_ARIA_HASPOPUP;
  ATTRIBUTE_NAMES[69] = ATTR_ARIA_SETSIZE;
  ATTRIBUTE_NAMES[70] = ATTR_CLEAR;
  ATTRIBUTE_NAMES[71] = ATTR_DATAFORMATAS;
  ATTRIBUTE_NAMES[72] = ATTR_DISABLED;
  ATTRIBUTE_NAMES[73] = ATTR_DATAFLD;
  ATTRIBUTE_NAMES[74] = ATTR_DEFAULT;
  ATTRIBUTE_NAMES[75] = ATTR_DATASRC;
  ATTRIBUTE_NAMES[76] = ATTR_DATA;
  ATTRIBUTE_NAMES[77] = ATTR_EQUALCOLUMNS;
  ATTRIBUTE_NAMES[78] = ATTR_EQUALROWS;
  ATTRIBUTE_NAMES[79] = ATTR_HSPACE;
  ATTRIBUTE_NAMES[80] = ATTR_ISMAP;
  ATTRIBUTE_NAMES[81] = ATTR_LOCAL;
  ATTRIBUTE_NAMES[82] = ATTR_LSPACE;
  ATTRIBUTE_NAMES[83] = ATTR_MOVABLELIMITS;
  ATTRIBUTE_NAMES[84] = ATTR_NOTATION;
  ATTRIBUTE_NAMES[85] = ATTR_ONDATASETCHANGED;
  ATTRIBUTE_NAMES[86] = ATTR_ONDATAAVAILABLE;
  ATTRIBUTE_NAMES[87] = ATTR_ONPASTE;
  ATTRIBUTE_NAMES[88] = ATTR_ONDATASETCOMPLETE;
  ATTRIBUTE_NAMES[89] = ATTR_RSPACE;
  ATTRIBUTE_NAMES[90] = ATTR_ROWALIGN;
  ATTRIBUTE_NAMES[91] = ATTR_ROTATE;
  ATTRIBUTE_NAMES[92] = ATTR_SEPARATOR;
  ATTRIBUTE_NAMES[93] = ATTR_SEPARATORS;
  ATTRIBUTE_NAMES[94] = ATTR_V_MATHEMATICAL;
  ATTRIBUTE_NAMES[95] = ATTR_VSPACE;
  ATTRIBUTE_NAMES[96] = ATTR_V_HANGING;
  ATTRIBUTE_NAMES[97] = ATTR_XCHANNELSELECTOR;
  ATTRIBUTE_NAMES[98] = ATTR_YCHANNELSELECTOR;
  ATTRIBUTE_NAMES[99] = ATTR_ARABIC_FORM;
  ATTRIBUTE_NAMES[100] = ATTR_ENABLE_BACKGROUND;
  ATTRIBUTE_NAMES[101] = ATTR_ONDBLCLICK;
  ATTRIBUTE_NAMES[102] = ATTR_ONABORT;
  ATTRIBUTE_NAMES[103] = ATTR_CALCMODE;
  ATTRIBUTE_NAMES[104] = ATTR_CHECKED;
  ATTRIBUTE_NAMES[105] = ATTR_DESCENT;
  ATTRIBUTE_NAMES[106] = ATTR_FENCE;
  ATTRIBUTE_NAMES[107] = ATTR_ONSCROLL;
  ATTRIBUTE_NAMES[108] = ATTR_ONACTIVATE;
  ATTRIBUTE_NAMES[109] = ATTR_OPACITY;
  ATTRIBUTE_NAMES[110] = ATTR_SPACING;
  ATTRIBUTE_NAMES[111] = ATTR_SPECULAREXPONENT;
  ATTRIBUTE_NAMES[112] = ATTR_SPECULARCONSTANT;
  ATTRIBUTE_NAMES[113] = ATTR_SPECIFICATION;
  ATTRIBUTE_NAMES[114] = ATTR_THICKMATHSPACE;
  ATTRIBUTE_NAMES[115] = ATTR_UNICODE;
  ATTRIBUTE_NAMES[116] = ATTR_UNICODE_BIDI;
  ATTRIBUTE_NAMES[117] = ATTR_UNICODE_RANGE;
  ATTRIBUTE_NAMES[118] = ATTR_BORDER;
  ATTRIBUTE_NAMES[119] = ATTR_ID;
  ATTRIBUTE_NAMES[120] = ATTR_GRADIENTTRANSFORM;
  ATTRIBUTE_NAMES[121] = ATTR_GRADIENTUNITS;
  ATTRIBUTE_NAMES[122] = ATTR_HIDDEN;
  ATTRIBUTE_NAMES[123] = ATTR_HEADERS;
  ATTRIBUTE_NAMES[124] = ATTR_READONLY;
  ATTRIBUTE_NAMES[125] = ATTR_RENDERING_INTENT;
  ATTRIBUTE_NAMES[126] = ATTR_SEED;
  ATTRIBUTE_NAMES[127] = ATTR_SRCDOC;
  ATTRIBUTE_NAMES[128] = ATTR_STDDEVIATION;
  ATTRIBUTE_NAMES[129] = ATTR_SANDBOX;
  ATTRIBUTE_NAMES[130] = ATTR_V_IDEOGRAPHIC;
  ATTRIBUTE_NAMES[131] = ATTR_WORD_SPACING;
  ATTRIBUTE_NAMES[132] = ATTR_ACCENTUNDER;
  ATTRIBUTE_NAMES[133] = ATTR_ACCEPT_CHARSET;
  ATTRIBUTE_NAMES[134] = ATTR_ACCESSKEY;
  ATTRIBUTE_NAMES[135] = ATTR_ACCENT_HEIGHT;
  ATTRIBUTE_NAMES[136] = ATTR_ACCENT;
  ATTRIBUTE_NAMES[137] = ATTR_ASCENT;
  ATTRIBUTE_NAMES[138] = ATTR_ACCEPT;
  ATTRIBUTE_NAMES[139] = ATTR_BEVELLED;
  ATTRIBUTE_NAMES[140] = ATTR_BASEFREQUENCY;
  ATTRIBUTE_NAMES[141] = ATTR_BASELINE_SHIFT;
  ATTRIBUTE_NAMES[142] = ATTR_BASEPROFILE;
  ATTRIBUTE_NAMES[143] = ATTR_BASELINE;
  ATTRIBUTE_NAMES[144] = ATTR_BASE;
  ATTRIBUTE_NAMES[145] = ATTR_CODE;
  ATTRIBUTE_NAMES[146] = ATTR_CODETYPE;
  ATTRIBUTE_NAMES[147] = ATTR_CODEBASE;
  ATTRIBUTE_NAMES[148] = ATTR_CITE;
  ATTRIBUTE_NAMES[149] = ATTR_DEFER;
  ATTRIBUTE_NAMES[150] = ATTR_DATETIME;
  ATTRIBUTE_NAMES[151] = ATTR_DIRECTION;
  ATTRIBUTE_NAMES[152] = ATTR_EDGEMODE;
  ATTRIBUTE_NAMES[153] = ATTR_EDGE;
  ATTRIBUTE_NAMES[154] = ATTR_FACE;
  ATTRIBUTE_NAMES[155] = ATTR_HIDEFOCUS;
  ATTRIBUTE_NAMES[156] = ATTR_INDEX;
  ATTRIBUTE_NAMES[157] = ATTR_IRRELEVANT;
  ATTRIBUTE_NAMES[158] = ATTR_INTERCEPT;
  ATTRIBUTE_NAMES[159] = ATTR_INTEGRITY;
  ATTRIBUTE_NAMES[160] = ATTR_LINEBREAK;
  ATTRIBUTE_NAMES[161] = ATTR_LABEL;
  ATTRIBUTE_NAMES[162] = ATTR_LINETHICKNESS;
  ATTRIBUTE_NAMES[163] = ATTR_MODE;
  ATTRIBUTE_NAMES[164] = ATTR_NAME;
  ATTRIBUTE_NAMES[165] = ATTR_NORESIZE;
  ATTRIBUTE_NAMES[166] = ATTR_ONBEFOREUNLOAD;
  ATTRIBUTE_NAMES[167] = ATTR_ONREPEAT;
  ATTRIBUTE_NAMES[168] = ATTR_OBJECT;
  ATTRIBUTE_NAMES[169] = ATTR_ONSELECT;
  ATTRIBUTE_NAMES[170] = ATTR_ORDER;
  ATTRIBUTE_NAMES[171] = ATTR_OTHER;
  ATTRIBUTE_NAMES[172] = ATTR_ONRESET;
  ATTRIBUTE_NAMES[173] = ATTR_ONCELLCHANGE;
  ATTRIBUTE_NAMES[174] = ATTR_ONREADYSTATECHANGE;
  ATTRIBUTE_NAMES[175] = ATTR_ONMESSAGE;
  ATTRIBUTE_NAMES[176] = ATTR_ONBEGIN;
  ATTRIBUTE_NAMES[177] = ATTR_ONHELP;
  ATTRIBUTE_NAMES[178] = ATTR_ONBEFOREPRINT;
  ATTRIBUTE_NAMES[179] = ATTR_ORIENT;
  ATTRIBUTE_NAMES[180] = ATTR_ORIENTATION;
  ATTRIBUTE_NAMES[181] = ATTR_ONBEFORECOPY;
  ATTRIBUTE_NAMES[182] = ATTR_ONSELECTSTART;
  ATTRIBUTE_NAMES[183] = ATTR_ONBEFOREPASTE;
  ATTRIBUTE_NAMES[184] = ATTR_ONBEFOREUPDATE;
  ATTRIBUTE_NAMES[185] = ATTR_ONDEACTIVATE;
  ATTRIBUTE_NAMES[186] = ATTR_ONBEFOREACTIVATE;
  ATTRIBUTE_NAMES[187] = ATTR_ONBEFORDEACTIVATE;
  ATTRIBUTE_NAMES[188] = ATTR_ONKEYPRESS;
  ATTRIBUTE_NAMES[189] = ATTR_ONKEYUP;
  ATTRIBUTE_NAMES[190] = ATTR_ONBEFOREEDITFOCUS;
  ATTRIBUTE_NAMES[191] = ATTR_ONBEFORECUT;
  ATTRIBUTE_NAMES[192] = ATTR_ONKEYDOWN;
  ATTRIBUTE_NAMES[193] = ATTR_ONRESIZE;
  ATTRIBUTE_NAMES[194] = ATTR_REPEAT;
  ATTRIBUTE_NAMES[195] = ATTR_REPEAT_MAX;
  ATTRIBUTE_NAMES[196] = ATTR_REFERRERPOLICY;
  ATTRIBUTE_NAMES[197] = ATTR_RULES;
  ATTRIBUTE_NAMES[198] = ATTR_REPEAT_MIN;
  ATTRIBUTE_NAMES[199] = ATTR_ROLE;
  ATTRIBUTE_NAMES[200] = ATTR_REPEATCOUNT;
  ATTRIBUTE_NAMES[201] = ATTR_REPEAT_START;
  ATTRIBUTE_NAMES[202] = ATTR_REPEAT_TEMPLATE;
  ATTRIBUTE_NAMES[203] = ATTR_REPEATDUR;
  ATTRIBUTE_NAMES[204] = ATTR_SELECTED;
  ATTRIBUTE_NAMES[205] = ATTR_SPEED;
  ATTRIBUTE_NAMES[206] = ATTR_SIZES;
  ATTRIBUTE_NAMES[207] = ATTR_SUPERSCRIPTSHIFT;
  ATTRIBUTE_NAMES[208] = ATTR_STRETCHY;
  ATTRIBUTE_NAMES[209] = ATTR_SCHEME;
  ATTRIBUTE_NAMES[210] = ATTR_SPREADMETHOD;
  ATTRIBUTE_NAMES[211] = ATTR_SELECTION;
  ATTRIBUTE_NAMES[212] = ATTR_SIZE;
  ATTRIBUTE_NAMES[213] = ATTR_TYPE;
  ATTRIBUTE_NAMES[214] = ATTR_UNSELECTABLE;
  ATTRIBUTE_NAMES[215] = ATTR_UNDERLINE_POSITION;
  ATTRIBUTE_NAMES[216] = ATTR_UNDERLINE_THICKNESS;
  ATTRIBUTE_NAMES[217] = ATTR_X_HEIGHT;
  ATTRIBUTE_NAMES[218] = ATTR_DIFFUSECONSTANT;
  ATTRIBUTE_NAMES[219] = ATTR_HREF;
  ATTRIBUTE_NAMES[220] = ATTR_HREFLANG;
  ATTRIBUTE_NAMES[221] = ATTR_ONAFTERPRINT;
  ATTRIBUTE_NAMES[222] = ATTR_ONAFTERUPDATE;
  ATTRIBUTE_NAMES[223] = ATTR_PROFILE;
  ATTRIBUTE_NAMES[224] = ATTR_SURFACESCALE;
  ATTRIBUTE_NAMES[225] = ATTR_XREF;
  ATTRIBUTE_NAMES[226] = ATTR_ALIGN;
  ATTRIBUTE_NAMES[227] = ATTR_ALIGNMENT_BASELINE;
  ATTRIBUTE_NAMES[228] = ATTR_ALIGNMENTSCOPE;
  ATTRIBUTE_NAMES[229] = ATTR_DRAGGABLE;
  ATTRIBUTE_NAMES[230] = ATTR_HEIGHT;
  ATTRIBUTE_NAMES[231] = ATTR_HANGING;
  ATTRIBUTE_NAMES[232] = ATTR_IMAGE_RENDERING;
  ATTRIBUTE_NAMES[233] = ATTR_LANGUAGE;
  ATTRIBUTE_NAMES[234] = ATTR_LANG;
  ATTRIBUTE_NAMES[235] = ATTR_LARGEOP;
  ATTRIBUTE_NAMES[236] = ATTR_LONGDESC;
  ATTRIBUTE_NAMES[237] = ATTR_LENGTHADJUST;
  ATTRIBUTE_NAMES[238] = ATTR_MARGINHEIGHT;
  ATTRIBUTE_NAMES[239] = ATTR_MARGINWIDTH;
  ATTRIBUTE_NAMES[240] = ATTR_NARGS;
  ATTRIBUTE_NAMES[241] = ATTR_ORIGIN;
  ATTRIBUTE_NAMES[242] = ATTR_PING;
  ATTRIBUTE_NAMES[243] = ATTR_TARGET;
  ATTRIBUTE_NAMES[244] = ATTR_TARGETX;
  ATTRIBUTE_NAMES[245] = ATTR_TARGETY;
  ATTRIBUTE_NAMES[246] = ATTR_ALPHABETIC;
  ATTRIBUTE_NAMES[247] = ATTR_ARCHIVE;
  ATTRIBUTE_NAMES[248] = ATTR_HIGH;
  ATTRIBUTE_NAMES[249] = ATTR_LIGHTING_COLOR;
  ATTRIBUTE_NAMES[250] = ATTR_MATHEMATICAL;
  ATTRIBUTE_NAMES[251] = ATTR_MATHBACKGROUND;
  ATTRIBUTE_NAMES[252] = ATTR_METHOD;
  ATTRIBUTE_NAMES[253] = ATTR_MATHVARIANT;
  ATTRIBUTE_NAMES[254] = ATTR_MATHCOLOR;
  ATTRIBUTE_NAMES[255] = ATTR_MATHSIZE;
  ATTRIBUTE_NAMES[256] = ATTR_NOSHADE;
  ATTRIBUTE_NAMES[257] = ATTR_ONCHANGE;
  ATTRIBUTE_NAMES[258] = ATTR_PATHLENGTH;
  ATTRIBUTE_NAMES[259] = ATTR_PATH;
  ATTRIBUTE_NAMES[260] = ATTR_ALTIMG;
  ATTRIBUTE_NAMES[261] = ATTR_ACTIONTYPE;
  ATTRIBUTE_NAMES[262] = ATTR_ACTION;
  ATTRIBUTE_NAMES[263] = ATTR_ACTIVE;
  ATTRIBUTE_NAMES[264] = ATTR_ADDITIVE;
  ATTRIBUTE_NAMES[265] = ATTR_BEGIN;
  ATTRIBUTE_NAMES[266] = ATTR_DOMINANT_BASELINE;
  ATTRIBUTE_NAMES[267] = ATTR_DIVISOR;
  ATTRIBUTE_NAMES[268] = ATTR_DEFINITIONURL;
  ATTRIBUTE_NAMES[269] = ATTR_HORIZ_ADV_X;
  ATTRIBUTE_NAMES[270] = ATTR_HORIZ_ORIGIN_X;
  ATTRIBUTE_NAMES[271] = ATTR_HORIZ_ORIGIN_Y;
  ATTRIBUTE_NAMES[272] = ATTR_LIMITINGCONEANGLE;
  ATTRIBUTE_NAMES[273] = ATTR_MEDIUMMATHSPACE;
  ATTRIBUTE_NAMES[274] = ATTR_MEDIA;
  ATTRIBUTE_NAMES[275] = ATTR_MANIFEST;
  ATTRIBUTE_NAMES[276] = ATTR_ONFILTERCHANGE;
  ATTRIBUTE_NAMES[277] = ATTR_ONFINISH;
  ATTRIBUTE_NAMES[278] = ATTR_OPTIMUM;
  ATTRIBUTE_NAMES[279] = ATTR_RADIOGROUP;
  ATTRIBUTE_NAMES[280] = ATTR_RADIUS;
  ATTRIBUTE_NAMES[281] = ATTR_SCRIPTLEVEL;
  ATTRIBUTE_NAMES[282] = ATTR_SCRIPTSIZEMULTIPLIER;
  ATTRIBUTE_NAMES[283] = ATTR_STRING;
  ATTRIBUTE_NAMES[284] = ATTR_STRIKETHROUGH_POSITION;
  ATTRIBUTE_NAMES[285] = ATTR_STRIKETHROUGH_THICKNESS;
  ATTRIBUTE_NAMES[286] = ATTR_SCRIPTMINSIZE;
  ATTRIBUTE_NAMES[287] = ATTR_TABINDEX;
  ATTRIBUTE_NAMES[288] = ATTR_VALIGN;
  ATTRIBUTE_NAMES[289] = ATTR_VISIBILITY;
  ATTRIBUTE_NAMES[290] = ATTR_BACKGROUND;
  ATTRIBUTE_NAMES[291] = ATTR_LINK;
  ATTRIBUTE_NAMES[292] = ATTR_MARKER_MID;
  ATTRIBUTE_NAMES[293] = ATTR_MARKERHEIGHT;
  ATTRIBUTE_NAMES[294] = ATTR_MARKER_END;
  ATTRIBUTE_NAMES[295] = ATTR_MASK;
  ATTRIBUTE_NAMES[296] = ATTR_MARKER_START;
  ATTRIBUTE_NAMES[297] = ATTR_MARKERWIDTH;
  ATTRIBUTE_NAMES[298] = ATTR_MASKUNITS;
  ATTRIBUTE_NAMES[299] = ATTR_MARKERUNITS;
  ATTRIBUTE_NAMES[300] = ATTR_MASKCONTENTUNITS;
  ATTRIBUTE_NAMES[301] = ATTR_AMPLITUDE;
  ATTRIBUTE_NAMES[302] = ATTR_CELLSPACING;
  ATTRIBUTE_NAMES[303] = ATTR_CELLPADDING;
  ATTRIBUTE_NAMES[304] = ATTR_DECLARE;
  ATTRIBUTE_NAMES[305] = ATTR_FILL_RULE;
  ATTRIBUTE_NAMES[306] = ATTR_FILL;
  ATTRIBUTE_NAMES[307] = ATTR_FILL_OPACITY;
  ATTRIBUTE_NAMES[308] = ATTR_MAXLENGTH;
  ATTRIBUTE_NAMES[309] = ATTR_ONCLICK;
  ATTRIBUTE_NAMES[310] = ATTR_ONBLUR;
  ATTRIBUTE_NAMES[311] = ATTR_REPLACE;
  ATTRIBUTE_NAMES[312] = ATTR_ROWLINES;
  ATTRIBUTE_NAMES[313] = ATTR_SCALE;
  ATTRIBUTE_NAMES[314] = ATTR_STYLE;
  ATTRIBUTE_NAMES[315] = ATTR_TABLEVALUES;
  ATTRIBUTE_NAMES[316] = ATTR_TITLE;
  ATTRIBUTE_NAMES[317] = ATTR_V_ALPHABETIC;
  ATTRIBUTE_NAMES[318] = ATTR_AZIMUTH;
  ATTRIBUTE_NAMES[319] = ATTR_FORMAT;
  ATTRIBUTE_NAMES[320] = ATTR_FRAMEBORDER;
  ATTRIBUTE_NAMES[321] = ATTR_FRAME;
  ATTRIBUTE_NAMES[322] = ATTR_FRAMESPACING;
  ATTRIBUTE_NAMES[323] = ATTR_FROM;
  ATTRIBUTE_NAMES[324] = ATTR_FORM;
  ATTRIBUTE_NAMES[325] = ATTR_PROMPT;
  ATTRIBUTE_NAMES[326] = ATTR_PRIMITIVEUNITS;
  ATTRIBUTE_NAMES[327] = ATTR_SYMMETRIC;
  ATTRIBUTE_NAMES[328] = ATTR_STEMH;
  ATTRIBUTE_NAMES[329] = ATTR_STEMV;
  ATTRIBUTE_NAMES[330] = ATTR_SEAMLESS;
  ATTRIBUTE_NAMES[331] = ATTR_SUMMARY;
  ATTRIBUTE_NAMES[332] = ATTR_USEMAP;
  ATTRIBUTE_NAMES[333] = ATTR_ZOOMANDPAN;
  ATTRIBUTE_NAMES[334] = ATTR_ASYNC;
  ATTRIBUTE_NAMES[335] = ATTR_ALINK;
  ATTRIBUTE_NAMES[336] = ATTR_IN;
  ATTRIBUTE_NAMES[337] = ATTR_ICON;
  ATTRIBUTE_NAMES[338] = ATTR_KERNELMATRIX;
  ATTRIBUTE_NAMES[339] = ATTR_KERNING;
  ATTRIBUTE_NAMES[340] = ATTR_KERNELUNITLENGTH;
  ATTRIBUTE_NAMES[341] = ATTR_ONUNLOAD;
  ATTRIBUTE_NAMES[342] = ATTR_OPEN;
  ATTRIBUTE_NAMES[343] = ATTR_ONINVALID;
  ATTRIBUTE_NAMES[344] = ATTR_ONEND;
  ATTRIBUTE_NAMES[345] = ATTR_ONINPUT;
  ATTRIBUTE_NAMES[346] = ATTR_POINTER_EVENTS;
  ATTRIBUTE_NAMES[347] = ATTR_POINTS;
  ATTRIBUTE_NAMES[348] = ATTR_POINTSATX;
  ATTRIBUTE_NAMES[349] = ATTR_POINTSATY;
  ATTRIBUTE_NAMES[350] = ATTR_POINTSATZ;
  ATTRIBUTE_NAMES[351] = ATTR_SPAN;
  ATTRIBUTE_NAMES[352] = ATTR_STANDBY;
  ATTRIBUTE_NAMES[353] = ATTR_THINMATHSPACE;
  ATTRIBUTE_NAMES[354] = ATTR_TRANSFORM;
  ATTRIBUTE_NAMES[355] = ATTR_VLINK;
  ATTRIBUTE_NAMES[356] = ATTR_WHEN;
  ATTRIBUTE_NAMES[357] = ATTR_XLINK_HREF;
  ATTRIBUTE_NAMES[358] = ATTR_XLINK_TITLE;
  ATTRIBUTE_NAMES[359] = ATTR_XLINK_ROLE;
  ATTRIBUTE_NAMES[360] = ATTR_XLINK_ARCROLE;
  ATTRIBUTE_NAMES[361] = ATTR_XMLNS_XLINK;
  ATTRIBUTE_NAMES[362] = ATTR_XMLNS;
  ATTRIBUTE_NAMES[363] = ATTR_XLINK_TYPE;
  ATTRIBUTE_NAMES[364] = ATTR_XLINK_SHOW;
  ATTRIBUTE_NAMES[365] = ATTR_XLINK_ACTUATE;
  ATTRIBUTE_NAMES[366] = ATTR_AUTOPLAY;
  ATTRIBUTE_NAMES[367] = ATTR_AUTOSUBMIT;
  ATTRIBUTE_NAMES[368] = ATTR_AUTOCOMPLETE;
  ATTRIBUTE_NAMES[369] = ATTR_AUTOFOCUS;
  ATTRIBUTE_NAMES[370] = ATTR_BGCOLOR;
  ATTRIBUTE_NAMES[371] = ATTR_COLOR_PROFILE;
  ATTRIBUTE_NAMES[372] = ATTR_COLOR_RENDERING;
  ATTRIBUTE_NAMES[373] = ATTR_COLOR_INTERPOLATION;
  ATTRIBUTE_NAMES[374] = ATTR_COLOR;
  ATTRIBUTE_NAMES[375] = ATTR_COLOR_INTERPOLATION_FILTERS;
  ATTRIBUTE_NAMES[376] = ATTR_ENCODING;
  ATTRIBUTE_NAMES[377] = ATTR_EXPONENT;
  ATTRIBUTE_NAMES[378] = ATTR_FLOOD_COLOR;
  ATTRIBUTE_NAMES[379] = ATTR_FLOOD_OPACITY;
  ATTRIBUTE_NAMES[380] = ATTR_IDEOGRAPHIC;
  ATTRIBUTE_NAMES[381] = ATTR_LQUOTE;
  ATTRIBUTE_NAMES[382] = ATTR_PANOSE_1;
  ATTRIBUTE_NAMES[383] = ATTR_NUMOCTAVES;
  ATTRIBUTE_NAMES[384] = ATTR_ONLOAD;
  ATTRIBUTE_NAMES[385] = ATTR_ONBOUNCE;
  ATTRIBUTE_NAMES[386] = ATTR_ONCONTROLSELECT;
  ATTRIBUTE_NAMES[387] = ATTR_ONROWSINSERTED;
  ATTRIBUTE_NAMES[388] = ATTR_ONMOUSEWHEEL;
  ATTRIBUTE_NAMES[389] = ATTR_ONROWENTER;
  ATTRIBUTE_NAMES[390] = ATTR_ONMOUSEENTER;
  ATTRIBUTE_NAMES[391] = ATTR_ONMOUSEOVER;
  ATTRIBUTE_NAMES[392] = ATTR_ONFORMCHANGE;
  ATTRIBUTE_NAMES[393] = ATTR_ONFOCUSIN;
  ATTRIBUTE_NAMES[394] = ATTR_ONROWEXIT;
  ATTRIBUTE_NAMES[395] = ATTR_ONMOVEEND;
  ATTRIBUTE_NAMES[396] = ATTR_ONCONTEXTMENU;
  ATTRIBUTE_NAMES[397] = ATTR_ONZOOM;
  ATTRIBUTE_NAMES[398] = ATTR_ONLOSECAPTURE;
  ATTRIBUTE_NAMES[399] = ATTR_ONCOPY;
  ATTRIBUTE_NAMES[400] = ATTR_ONMOVESTART;
  ATTRIBUTE_NAMES[401] = ATTR_ONROWSDELETE;
  ATTRIBUTE_NAMES[402] = ATTR_ONMOUSELEAVE;
  ATTRIBUTE_NAMES[403] = ATTR_ONMOVE;
  ATTRIBUTE_NAMES[404] = ATTR_ONMOUSEMOVE;
  ATTRIBUTE_NAMES[405] = ATTR_ONMOUSEUP;
  ATTRIBUTE_NAMES[406] = ATTR_ONFOCUS;
  ATTRIBUTE_NAMES[407] = ATTR_ONMOUSEOUT;
  ATTRIBUTE_NAMES[408] = ATTR_ONFORMINPUT;
  ATTRIBUTE_NAMES[409] = ATTR_ONFOCUSOUT;
  ATTRIBUTE_NAMES[410] = ATTR_ONMOUSEDOWN;
  ATTRIBUTE_NAMES[411] = ATTR_TO;
  ATTRIBUTE_NAMES[412] = ATTR_RQUOTE;
  ATTRIBUTE_NAMES[413] = ATTR_STROKE_LINECAP;
  ATTRIBUTE_NAMES[414] = ATTR_SCROLLDELAY;
  ATTRIBUTE_NAMES[415] = ATTR_STROKE_DASHARRAY;
  ATTRIBUTE_NAMES[416] = ATTR_STROKE_DASHOFFSET;
  ATTRIBUTE_NAMES[417] = ATTR_STROKE_LINEJOIN;
  ATTRIBUTE_NAMES[418] = ATTR_STROKE_MITERLIMIT;
  ATTRIBUTE_NAMES[419] = ATTR_STROKE;
  ATTRIBUTE_NAMES[420] = ATTR_SCROLLING;
  ATTRIBUTE_NAMES[421] = ATTR_STROKE_WIDTH;
  ATTRIBUTE_NAMES[422] = ATTR_STROKE_OPACITY;
  ATTRIBUTE_NAMES[423] = ATTR_COMPACT;
  ATTRIBUTE_NAMES[424] = ATTR_CLIP;
  ATTRIBUTE_NAMES[425] = ATTR_CLIP_RULE;
  ATTRIBUTE_NAMES[426] = ATTR_CLIP_PATH;
  ATTRIBUTE_NAMES[427] = ATTR_CLIPPATHUNITS;
  ATTRIBUTE_NAMES[428] = ATTR_DISPLAY;
  ATTRIBUTE_NAMES[429] = ATTR_DISPLAYSTYLE;
  ATTRIBUTE_NAMES[430] = ATTR_GLYPH_ORIENTATION_VERTICAL;
  ATTRIBUTE_NAMES[431] = ATTR_GLYPH_ORIENTATION_HORIZONTAL;
  ATTRIBUTE_NAMES[432] = ATTR_GLYPHREF;
  ATTRIBUTE_NAMES[433] = ATTR_GLYPH_NAME;
  ATTRIBUTE_NAMES[434] = ATTR_HTTP_EQUIV;
  ATTRIBUTE_NAMES[435] = ATTR_KEYPOINTS;
  ATTRIBUTE_NAMES[436] = ATTR_LOOP;
  ATTRIBUTE_NAMES[437] = ATTR_PROPERTY;
  ATTRIBUTE_NAMES[438] = ATTR_SCOPED;
  ATTRIBUTE_NAMES[439] = ATTR_STEP;
  ATTRIBUTE_NAMES[440] = ATTR_SHAPE_RENDERING;
  ATTRIBUTE_NAMES[441] = ATTR_SCOPE;
  ATTRIBUTE_NAMES[442] = ATTR_SHAPE;
  ATTRIBUTE_NAMES[443] = ATTR_SLOPE;
  ATTRIBUTE_NAMES[444] = ATTR_STOP_COLOR;
  ATTRIBUTE_NAMES[445] = ATTR_STOP_OPACITY;
  ATTRIBUTE_NAMES[446] = ATTR_TEMPLATE;
  ATTRIBUTE_NAMES[447] = ATTR_WRAP;
  ATTRIBUTE_NAMES[448] = ATTR_ABBR;
  ATTRIBUTE_NAMES[449] = ATTR_ATTRIBUTENAME;
  ATTRIBUTE_NAMES[450] = ATTR_ATTRIBUTETYPE;
  ATTRIBUTE_NAMES[451] = ATTR_CHAR;
  ATTRIBUTE_NAMES[452] = ATTR_COORDS;
  ATTRIBUTE_NAMES[453] = ATTR_CHAROFF;
  ATTRIBUTE_NAMES[454] = ATTR_CHARSET;
  ATTRIBUTE_NAMES[455] = ATTR_MACROS;
  ATTRIBUTE_NAMES[456] = ATTR_NOWRAP;
  ATTRIBUTE_NAMES[457] = ATTR_NOHREF;
  ATTRIBUTE_NAMES[458] = ATTR_ONDRAG;
  ATTRIBUTE_NAMES[459] = ATTR_ONDRAGENTER;
  ATTRIBUTE_NAMES[460] = ATTR_ONDRAGOVER;
  ATTRIBUTE_NAMES[461] = ATTR_ONPROPERTYCHANGE;
  ATTRIBUTE_NAMES[462] = ATTR_ONDRAGEND;
  ATTRIBUTE_NAMES[463] = ATTR_ONDROP;
  ATTRIBUTE_NAMES[464] = ATTR_ONDRAGDROP;
  ATTRIBUTE_NAMES[465] = ATTR_OVERLINE_POSITION;
  ATTRIBUTE_NAMES[466] = ATTR_ONERROR;
  ATTRIBUTE_NAMES[467] = ATTR_OPERATOR;
  ATTRIBUTE_NAMES[468] = ATTR_OVERFLOW;
  ATTRIBUTE_NAMES[469] = ATTR_ONDRAGSTART;
  ATTRIBUTE_NAMES[470] = ATTR_ONERRORUPDATE;
  ATTRIBUTE_NAMES[471] = ATTR_OVERLINE_THICKNESS;
  ATTRIBUTE_NAMES[472] = ATTR_ONDRAGLEAVE;
  ATTRIBUTE_NAMES[473] = ATTR_STARTOFFSET;
  ATTRIBUTE_NAMES[474] = ATTR_START;
  ATTRIBUTE_NAMES[475] = ATTR_AXIS;
  ATTRIBUTE_NAMES[476] = ATTR_BIAS;
  ATTRIBUTE_NAMES[477] = ATTR_COLSPAN;
  ATTRIBUTE_NAMES[478] = ATTR_CLASSID;
  ATTRIBUTE_NAMES[479] = ATTR_CROSSORIGIN;
  ATTRIBUTE_NAMES[480] = ATTR_COLS;
  ATTRIBUTE_NAMES[481] = ATTR_CURSOR;
  ATTRIBUTE_NAMES[482] = ATTR_CLOSURE;
  ATTRIBUTE_NAMES[483] = ATTR_CLOSE;
  ATTRIBUTE_NAMES[484] = ATTR_CLASS;
  ATTRIBUTE_NAMES[485] = ATTR_KEYSYSTEM;
  ATTRIBUTE_NAMES[486] = ATTR_KEYSPLINES;
  ATTRIBUTE_NAMES[487] = ATTR_LOWSRC;
  ATTRIBUTE_NAMES[488] = ATTR_MAXSIZE;
  ATTRIBUTE_NAMES[489] = ATTR_MINSIZE;
  ATTRIBUTE_NAMES[490] = ATTR_OFFSET;
  ATTRIBUTE_NAMES[491] = ATTR_PRESERVEALPHA;
  ATTRIBUTE_NAMES[492] = ATTR_PRESERVEASPECTRATIO;
  ATTRIBUTE_NAMES[493] = ATTR_ROWSPAN;
  ATTRIBUTE_NAMES[494] = ATTR_ROWSPACING;
  ATTRIBUTE_NAMES[495] = ATTR_ROWS;
  ATTRIBUTE_NAMES[496] = ATTR_SRCSET;
  ATTRIBUTE_NAMES[497] = ATTR_SUBSCRIPTSHIFT;
  ATTRIBUTE_NAMES[498] = ATTR_VERSION;
  ATTRIBUTE_NAMES[499] = ATTR_ALTTEXT;
  ATTRIBUTE_NAMES[500] = ATTR_CONTENTEDITABLE;
  ATTRIBUTE_NAMES[501] = ATTR_CONTROLS;
  ATTRIBUTE_NAMES[502] = ATTR_CONTENT;
  ATTRIBUTE_NAMES[503] = ATTR_CONTEXTMENU;
  ATTRIBUTE_NAMES[504] = ATTR_DEPTH;
  ATTRIBUTE_NAMES[505] = ATTR_ENCTYPE;
  ATTRIBUTE_NAMES[506] = ATTR_FONT_STRETCH;
  ATTRIBUTE_NAMES[507] = ATTR_FILTER;
  ATTRIBUTE_NAMES[508] = ATTR_FONTWEIGHT;
  ATTRIBUTE_NAMES[509] = ATTR_FONT_WEIGHT;
  ATTRIBUTE_NAMES[510] = ATTR_FONTSTYLE;
  ATTRIBUTE_NAMES[511] = ATTR_FONT_STYLE;
  ATTRIBUTE_NAMES[512] = ATTR_FONTFAMILY;
  ATTRIBUTE_NAMES[513] = ATTR_FONT_FAMILY;
  ATTRIBUTE_NAMES[514] = ATTR_FONT_VARIANT;
  ATTRIBUTE_NAMES[515] = ATTR_FONT_SIZE_ADJUST;
  ATTRIBUTE_NAMES[516] = ATTR_FILTERUNITS;
  ATTRIBUTE_NAMES[517] = ATTR_FONTSIZE;
  ATTRIBUTE_NAMES[518] = ATTR_FONT_SIZE;
  ATTRIBUTE_NAMES[519] = ATTR_KEYTIMES;
  ATTRIBUTE_NAMES[520] = ATTR_LETTER_SPACING;
  ATTRIBUTE_NAMES[521] = ATTR_LIST;
  ATTRIBUTE_NAMES[522] = ATTR_MULTIPLE;
  ATTRIBUTE_NAMES[523] = ATTR_RT;
  ATTRIBUTE_NAMES[524] = ATTR_ONSTOP;
  ATTRIBUTE_NAMES[525] = ATTR_ONSTART;
  ATTRIBUTE_NAMES[526] = ATTR_POSTER;
  ATTRIBUTE_NAMES[527] = ATTR_PATTERNTRANSFORM;
  ATTRIBUTE_NAMES[528] = ATTR_PATTERN;
  ATTRIBUTE_NAMES[529] = ATTR_PATTERNUNITS;
  ATTRIBUTE_NAMES[530] = ATTR_PATTERNCONTENTUNITS;
  ATTRIBUTE_NAMES[531] = ATTR_RESTART;
  ATTRIBUTE_NAMES[532] = ATTR_STITCHTILES;
  ATTRIBUTE_NAMES[533] = ATTR_SYSTEMLANGUAGE;
  ATTRIBUTE_NAMES[534] = ATTR_TEXT_RENDERING;
  ATTRIBUTE_NAMES[535] = ATTR_VERT_ORIGIN_X;
  ATTRIBUTE_NAMES[536] = ATTR_VERT_ADV_Y;
  ATTRIBUTE_NAMES[537] = ATTR_VERT_ORIGIN_Y;
  ATTRIBUTE_NAMES[538] = ATTR_TEXT_DECORATION;
  ATTRIBUTE_NAMES[539] = ATTR_TEXT_ANCHOR;
  ATTRIBUTE_NAMES[540] = ATTR_TEXTLENGTH;
  ATTRIBUTE_NAMES[541] = ATTR_TEXT;
  ATTRIBUTE_NAMES[542] = ATTR_UNITS_PER_EM;
  ATTRIBUTE_NAMES[543] = ATTR_WRITING_MODE;
  ATTRIBUTE_NAMES[544] = ATTR_WIDTHS;
  ATTRIBUTE_NAMES[545] = ATTR_WIDTH;
  ATTRIBUTE_NAMES[546] = ATTR_ACCUMULATE;
  ATTRIBUTE_NAMES[547] = ATTR_COLUMNSPAN;
  ATTRIBUTE_NAMES[548] = ATTR_COLUMNLINES;
  ATTRIBUTE_NAMES[549] = ATTR_COLUMNALIGN;
  ATTRIBUTE_NAMES[550] = ATTR_COLUMNSPACING;
  ATTRIBUTE_NAMES[551] = ATTR_COLUMNWIDTH;
  ATTRIBUTE_NAMES[552] = ATTR_GROUPALIGN;
  ATTRIBUTE_NAMES[553] = ATTR_INPUTMODE;
  ATTRIBUTE_NAMES[554] = ATTR_OCCURRENCE;
  ATTRIBUTE_NAMES[555] = ATTR_ONSUBMIT;
  ATTRIBUTE_NAMES[556] = ATTR_ONCUT;
  ATTRIBUTE_NAMES[557] = ATTR_REQUIRED;
  ATTRIBUTE_NAMES[558] = ATTR_REQUIREDFEATURES;
  ATTRIBUTE_NAMES[559] = ATTR_RESULT;
  ATTRIBUTE_NAMES[560] = ATTR_REQUIREDEXTENSIONS;
  ATTRIBUTE_NAMES[561] = ATTR_VALUES;
  ATTRIBUTE_NAMES[562] = ATTR_VALUETYPE;
  ATTRIBUTE_NAMES[563] = ATTR_VALUE;
  ATTRIBUTE_NAMES[564] = ATTR_ELEVATION;
  ATTRIBUTE_NAMES[565] = ATTR_VIEWTARGET;
  ATTRIBUTE_NAMES[566] = ATTR_VIEWBOX;
  ATTRIBUTE_NAMES[567] = ATTR_CX;
  ATTRIBUTE_NAMES[568] = ATTR_DX;
  ATTRIBUTE_NAMES[569] = ATTR_FX;
  ATTRIBUTE_NAMES[570] = ATTR_BBOX;
  ATTRIBUTE_NAMES[571] = ATTR_RX;
  ATTRIBUTE_NAMES[572] = ATTR_REFX;
  ATTRIBUTE_NAMES[573] = ATTR_BY;
  ATTRIBUTE_NAMES[574] = ATTR_CY;
  ATTRIBUTE_NAMES[575] = ATTR_DY;
  ATTRIBUTE_NAMES[576] = ATTR_FY;
  ATTRIBUTE_NAMES[577] = ATTR_RY;
  ATTRIBUTE_NAMES[578] = ATTR_REFY;
  ATTRIBUTE_NAMES[579] = ATTR_VERYTHINMATHSPACE;
  ATTRIBUTE_NAMES[580] = ATTR_VERYTHICKMATHSPACE;
  ATTRIBUTE_NAMES[581] = ATTR_VERYVERYTHINMATHSPACE;
  ATTRIBUTE_NAMES[582] = ATTR_VERYVERYTHICKMATHSPACE;
}

void
nsHtml5AttributeName::releaseStatics()
{
  delete[] ALL_NO_NS;
  delete[] XMLNS_NS;
  delete[] XML_NS;
  delete[] XLINK_NS;
  delete[] ALL_NO_PREFIX;
  delete[] XMLNS_PREFIX;
  delete[] XLINK_PREFIX;
  delete[] XML_PREFIX;
  delete ATTR_ALT;
  delete ATTR_DIR;
  delete ATTR_DUR;
  delete ATTR_END;
  delete ATTR_FOR;
  delete ATTR_IN2;
  delete ATTR_LOW;
  delete ATTR_MIN;
  delete ATTR_MAX;
  delete ATTR_REL;
  delete ATTR_REV;
  delete ATTR_SRC;
  delete ATTR_D;
  delete ATTR_K;
  delete ATTR_R;
  delete ATTR_X;
  delete ATTR_Y;
  delete ATTR_Z;
  delete ATTR_CAP_HEIGHT;
  delete ATTR_G1;
  delete ATTR_K1;
  delete ATTR_U1;
  delete ATTR_X1;
  delete ATTR_Y1;
  delete ATTR_G2;
  delete ATTR_K2;
  delete ATTR_U2;
  delete ATTR_X2;
  delete ATTR_Y2;
  delete ATTR_K3;
  delete ATTR_K4;
  delete ATTR_XML_SPACE;
  delete ATTR_XML_LANG;
  delete ATTR_XML_BASE;
  delete ATTR_ARIA_GRAB;
  delete ATTR_ARIA_VALUEMAX;
  delete ATTR_ARIA_LABELLEDBY;
  delete ATTR_ARIA_DESCRIBEDBY;
  delete ATTR_ARIA_DISABLED;
  delete ATTR_ARIA_CHECKED;
  delete ATTR_ARIA_SELECTED;
  delete ATTR_ARIA_DROPEFFECT;
  delete ATTR_ARIA_REQUIRED;
  delete ATTR_ARIA_EXPANDED;
  delete ATTR_ARIA_PRESSED;
  delete ATTR_ARIA_LEVEL;
  delete ATTR_ARIA_CHANNEL;
  delete ATTR_ARIA_HIDDEN;
  delete ATTR_ARIA_SECRET;
  delete ATTR_ARIA_POSINSET;
  delete ATTR_ARIA_ATOMIC;
  delete ATTR_ARIA_INVALID;
  delete ATTR_ARIA_TEMPLATEID;
  delete ATTR_ARIA_VALUEMIN;
  delete ATTR_ARIA_MULTISELECTABLE;
  delete ATTR_ARIA_CONTROLS;
  delete ATTR_ARIA_MULTILINE;
  delete ATTR_ARIA_READONLY;
  delete ATTR_ARIA_OWNS;
  delete ATTR_ARIA_ACTIVEDESCENDANT;
  delete ATTR_ARIA_RELEVANT;
  delete ATTR_ARIA_DATATYPE;
  delete ATTR_ARIA_VALUENOW;
  delete ATTR_ARIA_SORT;
  delete ATTR_ARIA_AUTOCOMPLETE;
  delete ATTR_ARIA_FLOWTO;
  delete ATTR_ARIA_BUSY;
  delete ATTR_ARIA_LIVE;
  delete ATTR_ARIA_HASPOPUP;
  delete ATTR_ARIA_SETSIZE;
  delete ATTR_CLEAR;
  delete ATTR_DATAFORMATAS;
  delete ATTR_DISABLED;
  delete ATTR_DATAFLD;
  delete ATTR_DEFAULT;
  delete ATTR_DATASRC;
  delete ATTR_DATA;
  delete ATTR_EQUALCOLUMNS;
  delete ATTR_EQUALROWS;
  delete ATTR_HSPACE;
  delete ATTR_ISMAP;
  delete ATTR_LOCAL;
  delete ATTR_LSPACE;
  delete ATTR_MOVABLELIMITS;
  delete ATTR_NOTATION;
  delete ATTR_ONDATASETCHANGED;
  delete ATTR_ONDATAAVAILABLE;
  delete ATTR_ONPASTE;
  delete ATTR_ONDATASETCOMPLETE;
  delete ATTR_RSPACE;
  delete ATTR_ROWALIGN;
  delete ATTR_ROTATE;
  delete ATTR_SEPARATOR;
  delete ATTR_SEPARATORS;
  delete ATTR_V_MATHEMATICAL;
  delete ATTR_VSPACE;
  delete ATTR_V_HANGING;
  delete ATTR_XCHANNELSELECTOR;
  delete ATTR_YCHANNELSELECTOR;
  delete ATTR_ARABIC_FORM;
  delete ATTR_ENABLE_BACKGROUND;
  delete ATTR_ONDBLCLICK;
  delete ATTR_ONABORT;
  delete ATTR_CALCMODE;
  delete ATTR_CHECKED;
  delete ATTR_DESCENT;
  delete ATTR_FENCE;
  delete ATTR_ONSCROLL;
  delete ATTR_ONACTIVATE;
  delete ATTR_OPACITY;
  delete ATTR_SPACING;
  delete ATTR_SPECULAREXPONENT;
  delete ATTR_SPECULARCONSTANT;
  delete ATTR_SPECIFICATION;
  delete ATTR_THICKMATHSPACE;
  delete ATTR_UNICODE;
  delete ATTR_UNICODE_BIDI;
  delete ATTR_UNICODE_RANGE;
  delete ATTR_BORDER;
  delete ATTR_ID;
  delete ATTR_GRADIENTTRANSFORM;
  delete ATTR_GRADIENTUNITS;
  delete ATTR_HIDDEN;
  delete ATTR_HEADERS;
  delete ATTR_READONLY;
  delete ATTR_RENDERING_INTENT;
  delete ATTR_SEED;
  delete ATTR_SRCDOC;
  delete ATTR_STDDEVIATION;
  delete ATTR_SANDBOX;
  delete ATTR_V_IDEOGRAPHIC;
  delete ATTR_WORD_SPACING;
  delete ATTR_ACCENTUNDER;
  delete ATTR_ACCEPT_CHARSET;
  delete ATTR_ACCESSKEY;
  delete ATTR_ACCENT_HEIGHT;
  delete ATTR_ACCENT;
  delete ATTR_ASCENT;
  delete ATTR_ACCEPT;
  delete ATTR_BEVELLED;
  delete ATTR_BASEFREQUENCY;
  delete ATTR_BASELINE_SHIFT;
  delete ATTR_BASEPROFILE;
  delete ATTR_BASELINE;
  delete ATTR_BASE;
  delete ATTR_CODE;
  delete ATTR_CODETYPE;
  delete ATTR_CODEBASE;
  delete ATTR_CITE;
  delete ATTR_DEFER;
  delete ATTR_DATETIME;
  delete ATTR_DIRECTION;
  delete ATTR_EDGEMODE;
  delete ATTR_EDGE;
  delete ATTR_FACE;
  delete ATTR_HIDEFOCUS;
  delete ATTR_INDEX;
  delete ATTR_IRRELEVANT;
  delete ATTR_INTERCEPT;
  delete ATTR_INTEGRITY;
  delete ATTR_LINEBREAK;
  delete ATTR_LABEL;
  delete ATTR_LINETHICKNESS;
  delete ATTR_MODE;
  delete ATTR_NAME;
  delete ATTR_NORESIZE;
  delete ATTR_ONBEFOREUNLOAD;
  delete ATTR_ONREPEAT;
  delete ATTR_OBJECT;
  delete ATTR_ONSELECT;
  delete ATTR_ORDER;
  delete ATTR_OTHER;
  delete ATTR_ONRESET;
  delete ATTR_ONCELLCHANGE;
  delete ATTR_ONREADYSTATECHANGE;
  delete ATTR_ONMESSAGE;
  delete ATTR_ONBEGIN;
  delete ATTR_ONHELP;
  delete ATTR_ONBEFOREPRINT;
  delete ATTR_ORIENT;
  delete ATTR_ORIENTATION;
  delete ATTR_ONBEFORECOPY;
  delete ATTR_ONSELECTSTART;
  delete ATTR_ONBEFOREPASTE;
  delete ATTR_ONBEFOREUPDATE;
  delete ATTR_ONDEACTIVATE;
  delete ATTR_ONBEFOREACTIVATE;
  delete ATTR_ONBEFORDEACTIVATE;
  delete ATTR_ONKEYPRESS;
  delete ATTR_ONKEYUP;
  delete ATTR_ONBEFOREEDITFOCUS;
  delete ATTR_ONBEFORECUT;
  delete ATTR_ONKEYDOWN;
  delete ATTR_ONRESIZE;
  delete ATTR_REPEAT;
  delete ATTR_REPEAT_MAX;
  delete ATTR_REFERRERPOLICY;
  delete ATTR_RULES;
  delete ATTR_REPEAT_MIN;
  delete ATTR_ROLE;
  delete ATTR_REPEATCOUNT;
  delete ATTR_REPEAT_START;
  delete ATTR_REPEAT_TEMPLATE;
  delete ATTR_REPEATDUR;
  delete ATTR_SELECTED;
  delete ATTR_SPEED;
  delete ATTR_SIZES;
  delete ATTR_SUPERSCRIPTSHIFT;
  delete ATTR_STRETCHY;
  delete ATTR_SCHEME;
  delete ATTR_SPREADMETHOD;
  delete ATTR_SELECTION;
  delete ATTR_SIZE;
  delete ATTR_TYPE;
  delete ATTR_UNSELECTABLE;
  delete ATTR_UNDERLINE_POSITION;
  delete ATTR_UNDERLINE_THICKNESS;
  delete ATTR_X_HEIGHT;
  delete ATTR_DIFFUSECONSTANT;
  delete ATTR_HREF;
  delete ATTR_HREFLANG;
  delete ATTR_ONAFTERPRINT;
  delete ATTR_ONAFTERUPDATE;
  delete ATTR_PROFILE;
  delete ATTR_SURFACESCALE;
  delete ATTR_XREF;
  delete ATTR_ALIGN;
  delete ATTR_ALIGNMENT_BASELINE;
  delete ATTR_ALIGNMENTSCOPE;
  delete ATTR_DRAGGABLE;
  delete ATTR_HEIGHT;
  delete ATTR_HANGING;
  delete ATTR_IMAGE_RENDERING;
  delete ATTR_LANGUAGE;
  delete ATTR_LANG;
  delete ATTR_LARGEOP;
  delete ATTR_LONGDESC;
  delete ATTR_LENGTHADJUST;
  delete ATTR_MARGINHEIGHT;
  delete ATTR_MARGINWIDTH;
  delete ATTR_NARGS;
  delete ATTR_ORIGIN;
  delete ATTR_PING;
  delete ATTR_TARGET;
  delete ATTR_TARGETX;
  delete ATTR_TARGETY;
  delete ATTR_ALPHABETIC;
  delete ATTR_ARCHIVE;
  delete ATTR_HIGH;
  delete ATTR_LIGHTING_COLOR;
  delete ATTR_MATHEMATICAL;
  delete ATTR_MATHBACKGROUND;
  delete ATTR_METHOD;
  delete ATTR_MATHVARIANT;
  delete ATTR_MATHCOLOR;
  delete ATTR_MATHSIZE;
  delete ATTR_NOSHADE;
  delete ATTR_ONCHANGE;
  delete ATTR_PATHLENGTH;
  delete ATTR_PATH;
  delete ATTR_ALTIMG;
  delete ATTR_ACTIONTYPE;
  delete ATTR_ACTION;
  delete ATTR_ACTIVE;
  delete ATTR_ADDITIVE;
  delete ATTR_BEGIN;
  delete ATTR_DOMINANT_BASELINE;
  delete ATTR_DIVISOR;
  delete ATTR_DEFINITIONURL;
  delete ATTR_HORIZ_ADV_X;
  delete ATTR_HORIZ_ORIGIN_X;
  delete ATTR_HORIZ_ORIGIN_Y;
  delete ATTR_LIMITINGCONEANGLE;
  delete ATTR_MEDIUMMATHSPACE;
  delete ATTR_MEDIA;
  delete ATTR_MANIFEST;
  delete ATTR_ONFILTERCHANGE;
  delete ATTR_ONFINISH;
  delete ATTR_OPTIMUM;
  delete ATTR_RADIOGROUP;
  delete ATTR_RADIUS;
  delete ATTR_SCRIPTLEVEL;
  delete ATTR_SCRIPTSIZEMULTIPLIER;
  delete ATTR_STRING;
  delete ATTR_STRIKETHROUGH_POSITION;
  delete ATTR_STRIKETHROUGH_THICKNESS;
  delete ATTR_SCRIPTMINSIZE;
  delete ATTR_TABINDEX;
  delete ATTR_VALIGN;
  delete ATTR_VISIBILITY;
  delete ATTR_BACKGROUND;
  delete ATTR_LINK;
  delete ATTR_MARKER_MID;
  delete ATTR_MARKERHEIGHT;
  delete ATTR_MARKER_END;
  delete ATTR_MASK;
  delete ATTR_MARKER_START;
  delete ATTR_MARKERWIDTH;
  delete ATTR_MASKUNITS;
  delete ATTR_MARKERUNITS;
  delete ATTR_MASKCONTENTUNITS;
  delete ATTR_AMPLITUDE;
  delete ATTR_CELLSPACING;
  delete ATTR_CELLPADDING;
  delete ATTR_DECLARE;
  delete ATTR_FILL_RULE;
  delete ATTR_FILL;
  delete ATTR_FILL_OPACITY;
  delete ATTR_MAXLENGTH;
  delete ATTR_ONCLICK;
  delete ATTR_ONBLUR;
  delete ATTR_REPLACE;
  delete ATTR_ROWLINES;
  delete ATTR_SCALE;
  delete ATTR_STYLE;
  delete ATTR_TABLEVALUES;
  delete ATTR_TITLE;
  delete ATTR_V_ALPHABETIC;
  delete ATTR_AZIMUTH;
  delete ATTR_FORMAT;
  delete ATTR_FRAMEBORDER;
  delete ATTR_FRAME;
  delete ATTR_FRAMESPACING;
  delete ATTR_FROM;
  delete ATTR_FORM;
  delete ATTR_PROMPT;
  delete ATTR_PRIMITIVEUNITS;
  delete ATTR_SYMMETRIC;
  delete ATTR_STEMH;
  delete ATTR_STEMV;
  delete ATTR_SEAMLESS;
  delete ATTR_SUMMARY;
  delete ATTR_USEMAP;
  delete ATTR_ZOOMANDPAN;
  delete ATTR_ASYNC;
  delete ATTR_ALINK;
  delete ATTR_IN;
  delete ATTR_ICON;
  delete ATTR_KERNELMATRIX;
  delete ATTR_KERNING;
  delete ATTR_KERNELUNITLENGTH;
  delete ATTR_ONUNLOAD;
  delete ATTR_OPEN;
  delete ATTR_ONINVALID;
  delete ATTR_ONEND;
  delete ATTR_ONINPUT;
  delete ATTR_POINTER_EVENTS;
  delete ATTR_POINTS;
  delete ATTR_POINTSATX;
  delete ATTR_POINTSATY;
  delete ATTR_POINTSATZ;
  delete ATTR_SPAN;
  delete ATTR_STANDBY;
  delete ATTR_THINMATHSPACE;
  delete ATTR_TRANSFORM;
  delete ATTR_VLINK;
  delete ATTR_WHEN;
  delete ATTR_XLINK_HREF;
  delete ATTR_XLINK_TITLE;
  delete ATTR_XLINK_ROLE;
  delete ATTR_XLINK_ARCROLE;
  delete ATTR_XMLNS_XLINK;
  delete ATTR_XMLNS;
  delete ATTR_XLINK_TYPE;
  delete ATTR_XLINK_SHOW;
  delete ATTR_XLINK_ACTUATE;
  delete ATTR_AUTOPLAY;
  delete ATTR_AUTOSUBMIT;
  delete ATTR_AUTOCOMPLETE;
  delete ATTR_AUTOFOCUS;
  delete ATTR_BGCOLOR;
  delete ATTR_COLOR_PROFILE;
  delete ATTR_COLOR_RENDERING;
  delete ATTR_COLOR_INTERPOLATION;
  delete ATTR_COLOR;
  delete ATTR_COLOR_INTERPOLATION_FILTERS;
  delete ATTR_ENCODING;
  delete ATTR_EXPONENT;
  delete ATTR_FLOOD_COLOR;
  delete ATTR_FLOOD_OPACITY;
  delete ATTR_IDEOGRAPHIC;
  delete ATTR_LQUOTE;
  delete ATTR_PANOSE_1;
  delete ATTR_NUMOCTAVES;
  delete ATTR_ONLOAD;
  delete ATTR_ONBOUNCE;
  delete ATTR_ONCONTROLSELECT;
  delete ATTR_ONROWSINSERTED;
  delete ATTR_ONMOUSEWHEEL;
  delete ATTR_ONROWENTER;
  delete ATTR_ONMOUSEENTER;
  delete ATTR_ONMOUSEOVER;
  delete ATTR_ONFORMCHANGE;
  delete ATTR_ONFOCUSIN;
  delete ATTR_ONROWEXIT;
  delete ATTR_ONMOVEEND;
  delete ATTR_ONCONTEXTMENU;
  delete ATTR_ONZOOM;
  delete ATTR_ONLOSECAPTURE;
  delete ATTR_ONCOPY;
  delete ATTR_ONMOVESTART;
  delete ATTR_ONROWSDELETE;
  delete ATTR_ONMOUSELEAVE;
  delete ATTR_ONMOVE;
  delete ATTR_ONMOUSEMOVE;
  delete ATTR_ONMOUSEUP;
  delete ATTR_ONFOCUS;
  delete ATTR_ONMOUSEOUT;
  delete ATTR_ONFORMINPUT;
  delete ATTR_ONFOCUSOUT;
  delete ATTR_ONMOUSEDOWN;
  delete ATTR_TO;
  delete ATTR_RQUOTE;
  delete ATTR_STROKE_LINECAP;
  delete ATTR_SCROLLDELAY;
  delete ATTR_STROKE_DASHARRAY;
  delete ATTR_STROKE_DASHOFFSET;
  delete ATTR_STROKE_LINEJOIN;
  delete ATTR_STROKE_MITERLIMIT;
  delete ATTR_STROKE;
  delete ATTR_SCROLLING;
  delete ATTR_STROKE_WIDTH;
  delete ATTR_STROKE_OPACITY;
  delete ATTR_COMPACT;
  delete ATTR_CLIP;
  delete ATTR_CLIP_RULE;
  delete ATTR_CLIP_PATH;
  delete ATTR_CLIPPATHUNITS;
  delete ATTR_DISPLAY;
  delete ATTR_DISPLAYSTYLE;
  delete ATTR_GLYPH_ORIENTATION_VERTICAL;
  delete ATTR_GLYPH_ORIENTATION_HORIZONTAL;
  delete ATTR_GLYPHREF;
  delete ATTR_GLYPH_NAME;
  delete ATTR_HTTP_EQUIV;
  delete ATTR_KEYPOINTS;
  delete ATTR_LOOP;
  delete ATTR_PROPERTY;
  delete ATTR_SCOPED;
  delete ATTR_STEP;
  delete ATTR_SHAPE_RENDERING;
  delete ATTR_SCOPE;
  delete ATTR_SHAPE;
  delete ATTR_SLOPE;
  delete ATTR_STOP_COLOR;
  delete ATTR_STOP_OPACITY;
  delete ATTR_TEMPLATE;
  delete ATTR_WRAP;
  delete ATTR_ABBR;
  delete ATTR_ATTRIBUTENAME;
  delete ATTR_ATTRIBUTETYPE;
  delete ATTR_CHAR;
  delete ATTR_COORDS;
  delete ATTR_CHAROFF;
  delete ATTR_CHARSET;
  delete ATTR_MACROS;
  delete ATTR_NOWRAP;
  delete ATTR_NOHREF;
  delete ATTR_ONDRAG;
  delete ATTR_ONDRAGENTER;
  delete ATTR_ONDRAGOVER;
  delete ATTR_ONPROPERTYCHANGE;
  delete ATTR_ONDRAGEND;
  delete ATTR_ONDROP;
  delete ATTR_ONDRAGDROP;
  delete ATTR_OVERLINE_POSITION;
  delete ATTR_ONERROR;
  delete ATTR_OPERATOR;
  delete ATTR_OVERFLOW;
  delete ATTR_ONDRAGSTART;
  delete ATTR_ONERRORUPDATE;
  delete ATTR_OVERLINE_THICKNESS;
  delete ATTR_ONDRAGLEAVE;
  delete ATTR_STARTOFFSET;
  delete ATTR_START;
  delete ATTR_AXIS;
  delete ATTR_BIAS;
  delete ATTR_COLSPAN;
  delete ATTR_CLASSID;
  delete ATTR_CROSSORIGIN;
  delete ATTR_COLS;
  delete ATTR_CURSOR;
  delete ATTR_CLOSURE;
  delete ATTR_CLOSE;
  delete ATTR_CLASS;
  delete ATTR_KEYSYSTEM;
  delete ATTR_KEYSPLINES;
  delete ATTR_LOWSRC;
  delete ATTR_MAXSIZE;
  delete ATTR_MINSIZE;
  delete ATTR_OFFSET;
  delete ATTR_PRESERVEALPHA;
  delete ATTR_PRESERVEASPECTRATIO;
  delete ATTR_ROWSPAN;
  delete ATTR_ROWSPACING;
  delete ATTR_ROWS;
  delete ATTR_SRCSET;
  delete ATTR_SUBSCRIPTSHIFT;
  delete ATTR_VERSION;
  delete ATTR_ALTTEXT;
  delete ATTR_CONTENTEDITABLE;
  delete ATTR_CONTROLS;
  delete ATTR_CONTENT;
  delete ATTR_CONTEXTMENU;
  delete ATTR_DEPTH;
  delete ATTR_ENCTYPE;
  delete ATTR_FONT_STRETCH;
  delete ATTR_FILTER;
  delete ATTR_FONTWEIGHT;
  delete ATTR_FONT_WEIGHT;
  delete ATTR_FONTSTYLE;
  delete ATTR_FONT_STYLE;
  delete ATTR_FONTFAMILY;
  delete ATTR_FONT_FAMILY;
  delete ATTR_FONT_VARIANT;
  delete ATTR_FONT_SIZE_ADJUST;
  delete ATTR_FILTERUNITS;
  delete ATTR_FONTSIZE;
  delete ATTR_FONT_SIZE;
  delete ATTR_KEYTIMES;
  delete ATTR_LETTER_SPACING;
  delete ATTR_LIST;
  delete ATTR_MULTIPLE;
  delete ATTR_RT;
  delete ATTR_ONSTOP;
  delete ATTR_ONSTART;
  delete ATTR_POSTER;
  delete ATTR_PATTERNTRANSFORM;
  delete ATTR_PATTERN;
  delete ATTR_PATTERNUNITS;
  delete ATTR_PATTERNCONTENTUNITS;
  delete ATTR_RESTART;
  delete ATTR_STITCHTILES;
  delete ATTR_SYSTEMLANGUAGE;
  delete ATTR_TEXT_RENDERING;
  delete ATTR_VERT_ORIGIN_X;
  delete ATTR_VERT_ADV_Y;
  delete ATTR_VERT_ORIGIN_Y;
  delete ATTR_TEXT_DECORATION;
  delete ATTR_TEXT_ANCHOR;
  delete ATTR_TEXTLENGTH;
  delete ATTR_TEXT;
  delete ATTR_UNITS_PER_EM;
  delete ATTR_WRITING_MODE;
  delete ATTR_WIDTHS;
  delete ATTR_WIDTH;
  delete ATTR_ACCUMULATE;
  delete ATTR_COLUMNSPAN;
  delete ATTR_COLUMNLINES;
  delete ATTR_COLUMNALIGN;
  delete ATTR_COLUMNSPACING;
  delete ATTR_COLUMNWIDTH;
  delete ATTR_GROUPALIGN;
  delete ATTR_INPUTMODE;
  delete ATTR_OCCURRENCE;
  delete ATTR_ONSUBMIT;
  delete ATTR_ONCUT;
  delete ATTR_REQUIRED;
  delete ATTR_REQUIREDFEATURES;
  delete ATTR_RESULT;
  delete ATTR_REQUIREDEXTENSIONS;
  delete ATTR_VALUES;
  delete ATTR_VALUETYPE;
  delete ATTR_VALUE;
  delete ATTR_ELEVATION;
  delete ATTR_VIEWTARGET;
  delete ATTR_VIEWBOX;
  delete ATTR_CX;
  delete ATTR_DX;
  delete ATTR_FX;
  delete ATTR_BBOX;
  delete ATTR_RX;
  delete ATTR_REFX;
  delete ATTR_BY;
  delete ATTR_CY;
  delete ATTR_DY;
  delete ATTR_FY;
  delete ATTR_RY;
  delete ATTR_REFY;
  delete ATTR_VERYTHINMATHSPACE;
  delete ATTR_VERYTHICKMATHSPACE;
  delete ATTR_VERYVERYTHINMATHSPACE;
  delete ATTR_VERYVERYTHICKMATHSPACE;
  delete[] ATTRIBUTE_NAMES;
}


