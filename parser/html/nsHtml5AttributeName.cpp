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

#include "nsHtml5ElementName.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5AttributeName.h"

int32_t* nsHtml5AttributeName::ALL_NO_NS = 0;
int32_t* nsHtml5AttributeName::XMLNS_NS = 0;
int32_t* nsHtml5AttributeName::XML_NS = 0;
int32_t* nsHtml5AttributeName::XLINK_NS = 0;
nsStaticAtom** nsHtml5AttributeName::ALL_NO_PREFIX = 0;
nsStaticAtom** nsHtml5AttributeName::XMLNS_PREFIX = 0;
nsStaticAtom** nsHtml5AttributeName::XLINK_PREFIX = 0;
nsStaticAtom** nsHtml5AttributeName::XML_PREFIX = 0;

nsHtml5AttributeName::nsHtml5AttributeName(int32_t* uri, nsStaticAtom* html,
                                           nsStaticAtom* mathml,
                                           nsStaticAtom* svg,
                                           nsStaticAtom** prefix)
    : uri(uri), prefix(prefix), custom(false) {
  MOZ_COUNT_CTOR(nsHtml5AttributeName);
  this->local[HTML] = html;
  this->local[MATHML] = mathml;
  this->local[SVG] = svg;
}

nsHtml5AttributeName::nsHtml5AttributeName()
    : uri(ALL_NO_NS), prefix(ALL_NO_PREFIX), custom(true) {
  MOZ_COUNT_CTOR(nsHtml5AttributeName);
  this->local[0] = nullptr;
  this->local[1] = nullptr;
  this->local[2] = nullptr;
}

nsHtml5AttributeName::~nsHtml5AttributeName() {
  MOZ_COUNT_DTOR(nsHtml5AttributeName);
  if (custom) {
    NS_IF_RELEASE(local[0]);
  }
}

int32_t nsHtml5AttributeName::getUri(int32_t mode) { return uri[mode]; }

nsAtom* nsHtml5AttributeName::getLocal(int32_t mode) { return local[mode]; }

nsStaticAtom* nsHtml5AttributeName::getPrefix(int32_t mode) {
  return prefix[mode];
}

bool nsHtml5AttributeName::equalsAnother(nsHtml5AttributeName* another) {
  return this->getLocal(nsHtml5AttributeName::HTML) ==
         another->getLocal(nsHtml5AttributeName::HTML);
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_R = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_X = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Y = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Z = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_X1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Y1 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_X2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_Y2 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K3 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_K4 = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XML_SPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XML_LANG = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DISABLED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DEFAULT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DATA = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_EQUALCOLUMNS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_EQUALROWS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ISMAP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LOCAL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MOVABLELIMITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOTATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDATAAVAILABLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONPASTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROWALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROTATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SEPARATOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SEPARATORS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_VSPACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XCHANNELSELECTOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_YCHANNELSELECTOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ENABLE_BACKGROUND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDBLCLICK = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONABORT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CALCMODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CHECKED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FENCE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FETCHPRIORITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NONCE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSCROLL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONACTIVATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OPACITY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPECULAREXPONENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPECULARCONSTANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BORDER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ID = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GRADIENTTRANSFORM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GRADIENTUNITS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HIDDEN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HEADERS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LOADING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_READONLY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RENDERING_INTENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SHADOWROOTMODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SEED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SRCDOC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STDDEVIATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SANDBOX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SHADOWROOTDELEGATESFOCUS =
    nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WORD_SPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCENTUNDER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCEPT_CHARSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCESSKEY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCENT = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ENTERKEYHINT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FACE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_INDEX = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONREADYSTATECHANGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMESSAGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEGIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREPRINT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ORIENT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ORIENTATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFORECOPY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONSELECTSTART = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFOREPASTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONKEYPRESS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONKEYUP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONBEFORECUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONKEYDOWN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONRESIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEAT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REFERRERPOLICY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RULES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ROLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEATCOUNT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REPEATDUR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SELECTED = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SIZES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SUPERSCRIPTSHIFT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STRETCHY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCHEME = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SPREADMETHOD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SELECTION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SIZE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TYPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DIFFUSECONSTANT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HREFLANG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONAFTERPRINT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PROFILE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SURFACESCALE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_XREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALIGNMENT_BASELINE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ALIGNMENTSCOPE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DRAGGABLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IMAGESIZES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IMAGESRCSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IMAGE_RENDERING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LANGUAGE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LANG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LARGEOP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LONGDESC = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LENGTHADJUST = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARGINHEIGHT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MARGINWIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ORIGIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_PING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TARGET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TARGETX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TARGETY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ARCHIVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_HIGH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LIGHTING_COLOR = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LIMITINGCONEANGLE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MEDIA = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_MANIFEST = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFINISH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OPTIMUM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RADIOGROUP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RADIUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCRIPTLEVEL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_SCRIPTSIZEMULTIPLIER = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TRANSFORM_ORIGIN = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AUTOCOMPLETE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AUTOFOCUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AUTOCAPITALIZE = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_LQUOTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NUMOCTAVES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOMODULE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONLOAD = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEWHEEL = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEENTER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEOVER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFOCUSIN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCONTEXTMENU = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONZOOM = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONCOPY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSELEAVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEMOVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEUP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFOCUS = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEOUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONFOCUSOUT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONMOUSEDOWN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TO = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RQUOTE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STROKE_LINECAP = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOWRAP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_NOHREF = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAG = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGENTER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGOVER = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGEND = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDROP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGDROP = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONERROR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OPERATOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_OVERFLOW = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGSTART = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ONDRAGLEAVE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_STARTOFFSET = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_START = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_AS = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_IS = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXT_DECORATION = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXT_ANCHOR = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXTLENGTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_TEXT = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WRITING_MODE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_WIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_ACCUMULATE = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNSPAN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNLINES = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNSPACING = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_COLUMNWIDTH = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_GROUPALIGN = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_INPUTMODE = nullptr;
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
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REFX = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_BY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_CY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_DY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_FY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_RY = nullptr;
nsHtml5AttributeName* nsHtml5AttributeName::ATTR_REFY = nullptr;
nsHtml5AttributeName** nsHtml5AttributeName::ATTRIBUTE_NAMES = 0;
static int32_t const ATTRIBUTE_HASHES_DATA[] = {
    1865910331, 1748503880, 1965512429, 1681174213, 1781007934, 1915757815,
    2001826027, 1680165421, 1721347639, 1754835516, 1814560070, 1903612236,
    1924517489, 1984430082, 2019887833, 71827457,   1680282148, 1689324870,
    1740119884, 1753550036, 1756762256, 1791068279, 1824159037, 1884079398,
    1908462185, 1922413307, 1934970504, 1972922984, 2000096287, 2008401563,
    2073034754, 57205395,   911736834,  1680181996, 1680368221, 1685882101,
    1704526375, 1734182982, 1747479606, 1749549708, 1754644293, 1756147974,
    1767725700, 1786775671, 1804081401, 1820727381, 1854366938, 1872343590,
    1890996553, 1906408542, 1910503637, 1917857531, 1922677495, 1932959284,
    1941435445, 1972656710, 1983157559, 1990107683, 2001634458, 2006459190,
    2010716309, 2026893641, 2082471938, 53006051,   60345635,   885522434,
    1680095865, 1680165533, 1680229115, 1680343801, 1680437801, 1682440540,
    1687620127, 1692408896, 1716623661, 1731048742, 1739583824, 1747295467,
    1747906667, 1748971848, 1751755561, 1754579720, 1754698327, 1754899031,
    1756360955, 1756889417, 1773606972, 1785053243, 1787365531, 1803561214,
    1805715690, 1816104145, 1823574314, 1848600826, 1854497001, 1867462756,
    1874270021, 1884295780, 1898415413, 1905628916, 1906423097, 1910441627,
    1915025672, 1916286197, 1921977416, 1922607670, 1923088386, 1924629705,
    1933369607, 1939976792, 1941550652, 1966442279, 1972904518, 1975062341,
    1983398182, 1988784439, 1991625270, 2000752725, 2001710298, 2004846654,
    2007021895, 2009079867, 2016810187, 2024647008, 2060474743, 2081423362,
    2089811970, 52488851,   55077603,   59825747,   68157441,   878182402,
    901775362,  1037879561, 1680159327, 1680165437, 1680165692, 1680198203,
    1680231247, 1680315086, 1680345965, 1680413393, 1680452349, 1681879063,
    1683805446, 1686731997, 1689048326, 1689839946, 1699185409, 1714763319,
    1721189160, 1723336432, 1733874289, 1736416327, 1739927860, 1740222216,
    1747309881, 1747800157, 1748021284, 1748566068, 1749350104, 1751507685,
    1753049109, 1754434872, 1754612424, 1754647074, 1754794646, 1754860061,
    1754927689, 1756219733, 1756704824, 1756836998, 1757421892, 1771569964,
    1780879045, 1784574102, 1786622296, 1786851500, 1788842244, 1797886599,
    1804054854, 1804405895, 1814517574, 1814656840, 1816178925, 1821958888,
    1823829083, 1825437894, 1854285018, 1854466380, 1854497008, 1866496199,
    1871251689, 1873656984, 1874788501, 1884246821, 1889569526, 1891937366,
    1900544002, 1903759600, 1905754853, 1906419001, 1907701479, 1909819252,
    1910441773, 1910527802, 1915341049, 1916247343, 1917295176, 1921061206,
    1922400908, 1922566877, 1922665179, 1922679610, 1924443742, 1924583073,
    1924773438, 1933123337, 1934917290, 1937336473, 1941286708, 1941440197,
    1943317364, 1966384692, 1972151670, 1972744954, 1972908839, 1972996699,
    1982254612, 1983290011, 1983432389, 1987422362, 1989522022, 1991220282,
    1993343287, 2000160071, 2001527900, 2001669449, 2001732764, 2001898809,
    2005342360, 2006824246, 2007064819, 2009041198, 2009231684, 2016711994,
    2017010843, 2023342821, 2024794274, 2034765641, 2065694722, 2075005220,
    2081947650, 2083520514, 2091784484, 50917059,   52489043,   53537523,
    56685811,   57210387,   59830867,   60817409,   71303169,   72351745,
    884998146,  894959618,  902299650,  928514050,  1038063816, 1680140893,
    1680159328, 1680165436, 1680165487, 1680165613, 1680181850, 1680185931,
    1680198381, 1680230940, 1680251485, 1680311085, 1680323325, 1680345685,
    1680347981, 1680411449, 1680433915, 1680446153, 1680511804, 1681733672,
    1681969220, 1682587945, 1684319541, 1685902598, 1687164232, 1687751191,
    1689130184, 1689788441, 1691145478, 1692933184, 1704262346, 1714745560,
    1716303957, 1720503541, 1721305962, 1723309623, 1723336528, 1732771842,
    1733919469, 1734404167, 1739561208, 1739914974, 1740096054, 1740130375,
    1742183484, 1747299630, 1747446838, 1747792072, 1747839118, 1747939528,
    1748306996, 1748552744, 1748869205, 1749027145, 1749399124, 1749856356,
    1751679545, 1752985897, 1753297133, 1754214628, 1754546894, 1754606246,
    1754643237, 1754645079, 1754647353, 1754792749, 1754798923, 1754858317,
    1754872618, 1754907227, 1754958648, 1756190926, 1756302628, 1756471625,
    1756737685, 1756804936, 1756874572, 1757053236, 1765800271, 1767875272,
    1772032615, 1776114564, 1780975314, 1782518297, 1785051290, 1785174319,
    1786740932, 1786821704, 1787193500, 1788254870, 1790814502, 1791070327,
    1801312388, 1804036350, 1804069019, 1804235064, 1804978712, 1805715716,
    1814558026, 1814656326, 1814986837, 1816144023, 1820262641, 1820928104,
    1822002839, 1823580230, 1823841492, 1824377064, 1825677514, 1853862084,
    1854302364, 1854464212, 1854474395, 1854497003, 1864698185, 1865910347,
    1867448617, 1867620412, 1872034503, 1873590471, 1874261045, 1874698443,
    1881750231, 1884142379, 1884267068, 1884343396, 1889633006, 1891186903,
    1894552650, 1898428101, 1902640276, 1903659239, 1905541832, 1905672729,
    1905902311, 1906408598, 1906421049, 1907660596, 1908316832, 1909438149,
    1910328970, 1910441770, 1910487243, 1910507338, 1910572893, 1915295948,
    1915394254, 1916210285, 1916278099, 1916337499, 1917327080, 1917953597,
    1921894426, 1922319046, 1922413292, 1922470745, 1922567078, 1922665052,
    1922671417, 1922679386, 1922699851, 1924206934, 1924462384, 1924570799,
    1924585254, 1924738716, 1932870919, 1932986153, 1933145837, 1933508940,
    1934917372, 1935597338, 1937777860, 1941253366, 1941409583, 1941438085,
    1941454586, 1942026440, 1965349396, 1965561677, 1966439670, 1966454567,
    1972196486, 1972744939, 1972863609, 1972904522, 1972909592, 1972962123,
    1974849131, 1980235778, 1982640164, 1983266615, 1983347764, 1983416119,
    1983461061, 1987410233, 1988132214, 1988788535, 1990062797, 1991021879,
    1991392548, 1991643278, 1999273799, 2000125224, 2000162011, 2001210183,
    2001578182, 2001634459, 2001669450, 2001710299, 2001814704, 2001898808,
    2004199576, 2004957380, 2005925890, 2006516551, 2007019632, 2007064812,
    2008084807, 2008408414, 2009071951, 2009141482, 2010452700, 2015950026,
    2016787611, 2016910397, 2018908874, 2023146024, 2024616088, 2024763702,
    2026741958, 2026975253, 2060302634, 2065170434, 2066743298};
staticJArray<int32_t, int32_t> nsHtml5AttributeName::ATTRIBUTE_HASHES = {
    ATTRIBUTE_HASHES_DATA, MOZ_ARRAY_LENGTH(ATTRIBUTE_HASHES_DATA)};
void nsHtml5AttributeName::initializeStatics() {
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
  ALL_NO_PREFIX = new nsStaticAtom*[3];
  ALL_NO_PREFIX[0] = nullptr;
  ALL_NO_PREFIX[1] = nullptr;
  ALL_NO_PREFIX[2] = nullptr;
  XMLNS_PREFIX = new nsStaticAtom*[3];
  XMLNS_PREFIX[0] = nullptr;
  XMLNS_PREFIX[1] = nsGkAtoms::xmlns;
  XMLNS_PREFIX[2] = nsGkAtoms::xmlns;
  XLINK_PREFIX = new nsStaticAtom*[3];
  XLINK_PREFIX[0] = nullptr;
  XLINK_PREFIX[1] = nsGkAtoms::xlink;
  XLINK_PREFIX[2] = nsGkAtoms::xlink;
  XML_PREFIX = new nsStaticAtom*[3];
  XML_PREFIX[0] = nullptr;
  XML_PREFIX[1] = nsGkAtoms::xml;
  XML_PREFIX[2] = nsGkAtoms::xml;
  ATTR_ALT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::alt, nsGkAtoms::alt,
                                      nsGkAtoms::alt, ALL_NO_PREFIX);
  ATTR_DIR = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::dir, nsGkAtoms::dir,
                                      nsGkAtoms::dir, ALL_NO_PREFIX);
  ATTR_DUR = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::dur, nsGkAtoms::dur,
                                      nsGkAtoms::dur, ALL_NO_PREFIX);
  ATTR_END = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::end, nsGkAtoms::end,
                                      nsGkAtoms::end, ALL_NO_PREFIX);
  ATTR_FOR =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::_for, nsGkAtoms::_for,
                               nsGkAtoms::_for, ALL_NO_PREFIX);
  ATTR_IN2 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::in2, nsGkAtoms::in2,
                                      nsGkAtoms::in2, ALL_NO_PREFIX);
  ATTR_LOW = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::low, nsGkAtoms::low,
                                      nsGkAtoms::low, ALL_NO_PREFIX);
  ATTR_MIN = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::min, nsGkAtoms::min,
                                      nsGkAtoms::min, ALL_NO_PREFIX);
  ATTR_MAX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::max, nsGkAtoms::max,
                                      nsGkAtoms::max, ALL_NO_PREFIX);
  ATTR_REL = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rel, nsGkAtoms::rel,
                                      nsGkAtoms::rel, ALL_NO_PREFIX);
  ATTR_REV = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rev, nsGkAtoms::rev,
                                      nsGkAtoms::rev, ALL_NO_PREFIX);
  ATTR_SRC = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::src, nsGkAtoms::src,
                                      nsGkAtoms::src, ALL_NO_PREFIX);
  ATTR_D = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::d, nsGkAtoms::d,
                                    nsGkAtoms::d, ALL_NO_PREFIX);
  ATTR_R = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::r, nsGkAtoms::r,
                                    nsGkAtoms::r, ALL_NO_PREFIX);
  ATTR_X = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::x, nsGkAtoms::x,
                                    nsGkAtoms::x, ALL_NO_PREFIX);
  ATTR_Y = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::y, nsGkAtoms::y,
                                    nsGkAtoms::y, ALL_NO_PREFIX);
  ATTR_Z = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::z, nsGkAtoms::z,
                                    nsGkAtoms::z, ALL_NO_PREFIX);
  ATTR_K1 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::k1, nsGkAtoms::k1,
                                     nsGkAtoms::k1, ALL_NO_PREFIX);
  ATTR_X1 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::x1, nsGkAtoms::x1,
                                     nsGkAtoms::x1, ALL_NO_PREFIX);
  ATTR_Y1 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::y1, nsGkAtoms::y1,
                                     nsGkAtoms::y1, ALL_NO_PREFIX);
  ATTR_K2 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::k2, nsGkAtoms::k2,
                                     nsGkAtoms::k2, ALL_NO_PREFIX);
  ATTR_X2 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::x2, nsGkAtoms::x2,
                                     nsGkAtoms::x2, ALL_NO_PREFIX);
  ATTR_Y2 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::y2, nsGkAtoms::y2,
                                     nsGkAtoms::y2, ALL_NO_PREFIX);
  ATTR_K3 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::k3, nsGkAtoms::k3,
                                     nsGkAtoms::k3, ALL_NO_PREFIX);
  ATTR_K4 = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::k4, nsGkAtoms::k4,
                                     nsGkAtoms::k4, ALL_NO_PREFIX);
  ATTR_XML_SPACE =
      new nsHtml5AttributeName(XML_NS, nsGkAtoms::xml_space, nsGkAtoms::space,
                               nsGkAtoms::space, XML_PREFIX);
  ATTR_XML_LANG =
      new nsHtml5AttributeName(XML_NS, nsGkAtoms::xml_lang, nsGkAtoms::lang,
                               nsGkAtoms::lang, XML_PREFIX);
  ATTR_ARIA_GRAB = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_grab, nsGkAtoms::aria_grab,
      nsGkAtoms::aria_grab, ALL_NO_PREFIX);
  ATTR_ARIA_VALUEMAX = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_valuemax, nsGkAtoms::aria_valuemax,
      nsGkAtoms::aria_valuemax, ALL_NO_PREFIX);
  ATTR_ARIA_LABELLEDBY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_labelledby, nsGkAtoms::aria_labelledby,
      nsGkAtoms::aria_labelledby, ALL_NO_PREFIX);
  ATTR_ARIA_DESCRIBEDBY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_describedby, nsGkAtoms::aria_describedby,
      nsGkAtoms::aria_describedby, ALL_NO_PREFIX);
  ATTR_ARIA_DISABLED = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_disabled, nsGkAtoms::aria_disabled,
      nsGkAtoms::aria_disabled, ALL_NO_PREFIX);
  ATTR_ARIA_CHECKED = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_checked, nsGkAtoms::aria_checked,
      nsGkAtoms::aria_checked, ALL_NO_PREFIX);
  ATTR_ARIA_SELECTED = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_selected, nsGkAtoms::aria_selected,
      nsGkAtoms::aria_selected, ALL_NO_PREFIX);
  ATTR_ARIA_DROPEFFECT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_dropeffect, nsGkAtoms::aria_dropeffect,
      nsGkAtoms::aria_dropeffect, ALL_NO_PREFIX);
  ATTR_ARIA_REQUIRED = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_required, nsGkAtoms::aria_required,
      nsGkAtoms::aria_required, ALL_NO_PREFIX);
  ATTR_ARIA_EXPANDED = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_expanded, nsGkAtoms::aria_expanded,
      nsGkAtoms::aria_expanded, ALL_NO_PREFIX);
  ATTR_ARIA_PRESSED = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_pressed, nsGkAtoms::aria_pressed,
      nsGkAtoms::aria_pressed, ALL_NO_PREFIX);
  ATTR_ARIA_LEVEL = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_level, nsGkAtoms::aria_level,
      nsGkAtoms::aria_level, ALL_NO_PREFIX);
  ATTR_ARIA_CHANNEL = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_channel, nsGkAtoms::aria_channel,
      nsGkAtoms::aria_channel, ALL_NO_PREFIX);
  ATTR_ARIA_HIDDEN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_hidden, nsGkAtoms::aria_hidden,
      nsGkAtoms::aria_hidden, ALL_NO_PREFIX);
  ATTR_ARIA_SECRET = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_secret, nsGkAtoms::aria_secret,
      nsGkAtoms::aria_secret, ALL_NO_PREFIX);
  ATTR_ARIA_POSINSET = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_posinset, nsGkAtoms::aria_posinset,
      nsGkAtoms::aria_posinset, ALL_NO_PREFIX);
  ATTR_ARIA_ATOMIC = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_atomic, nsGkAtoms::aria_atomic,
      nsGkAtoms::aria_atomic, ALL_NO_PREFIX);
  ATTR_ARIA_INVALID = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_invalid, nsGkAtoms::aria_invalid,
      nsGkAtoms::aria_invalid, ALL_NO_PREFIX);
  ATTR_ARIA_TEMPLATEID = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_templateid, nsGkAtoms::aria_templateid,
      nsGkAtoms::aria_templateid, ALL_NO_PREFIX);
  ATTR_ARIA_VALUEMIN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_valuemin, nsGkAtoms::aria_valuemin,
      nsGkAtoms::aria_valuemin, ALL_NO_PREFIX);
  ATTR_ARIA_MULTISELECTABLE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::aria_multiselectable,
                               nsGkAtoms::aria_multiselectable,
                               nsGkAtoms::aria_multiselectable, ALL_NO_PREFIX);
  ATTR_ARIA_CONTROLS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_controls, nsGkAtoms::aria_controls,
      nsGkAtoms::aria_controls, ALL_NO_PREFIX);
  ATTR_ARIA_MULTILINE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_multiline, nsGkAtoms::aria_multiline,
      nsGkAtoms::aria_multiline, ALL_NO_PREFIX);
  ATTR_ARIA_READONLY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_readonly, nsGkAtoms::aria_readonly,
      nsGkAtoms::aria_readonly, ALL_NO_PREFIX);
  ATTR_ARIA_OWNS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_owns, nsGkAtoms::aria_owns,
      nsGkAtoms::aria_owns, ALL_NO_PREFIX);
  ATTR_ARIA_ACTIVEDESCENDANT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::aria_activedescendant,
                               nsGkAtoms::aria_activedescendant,
                               nsGkAtoms::aria_activedescendant, ALL_NO_PREFIX);
  ATTR_ARIA_RELEVANT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_relevant, nsGkAtoms::aria_relevant,
      nsGkAtoms::aria_relevant, ALL_NO_PREFIX);
  ATTR_ARIA_DATATYPE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_datatype, nsGkAtoms::aria_datatype,
      nsGkAtoms::aria_datatype, ALL_NO_PREFIX);
  ATTR_ARIA_VALUENOW = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_valuenow, nsGkAtoms::aria_valuenow,
      nsGkAtoms::aria_valuenow, ALL_NO_PREFIX);
  ATTR_ARIA_SORT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_sort, nsGkAtoms::aria_sort,
      nsGkAtoms::aria_sort, ALL_NO_PREFIX);
  ATTR_ARIA_AUTOCOMPLETE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_autocomplete, nsGkAtoms::aria_autocomplete,
      nsGkAtoms::aria_autocomplete, ALL_NO_PREFIX);
  ATTR_ARIA_FLOWTO = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_flowto, nsGkAtoms::aria_flowto,
      nsGkAtoms::aria_flowto, ALL_NO_PREFIX);
  ATTR_ARIA_BUSY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_busy, nsGkAtoms::aria_busy,
      nsGkAtoms::aria_busy, ALL_NO_PREFIX);
  ATTR_ARIA_LIVE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_live, nsGkAtoms::aria_live,
      nsGkAtoms::aria_live, ALL_NO_PREFIX);
  ATTR_ARIA_HASPOPUP = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_haspopup, nsGkAtoms::aria_haspopup,
      nsGkAtoms::aria_haspopup, ALL_NO_PREFIX);
  ATTR_ARIA_SETSIZE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::aria_setsize, nsGkAtoms::aria_setsize,
      nsGkAtoms::aria_setsize, ALL_NO_PREFIX);
  ATTR_CLEAR =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::clear, nsGkAtoms::clear,
                               nsGkAtoms::clear, ALL_NO_PREFIX);
  ATTR_DISABLED = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::disabled,
                                           nsGkAtoms::disabled,
                                           nsGkAtoms::disabled, ALL_NO_PREFIX);
  ATTR_DEFAULT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::_default,
                                          nsGkAtoms::_default,
                                          nsGkAtoms::_default, ALL_NO_PREFIX);
  ATTR_DATA =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::data, nsGkAtoms::data,
                               nsGkAtoms::data, ALL_NO_PREFIX);
  ATTR_EQUALCOLUMNS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::equalcolumns_, nsGkAtoms::equalcolumns_,
      nsGkAtoms::equalcolumns_, ALL_NO_PREFIX);
  ATTR_EQUALROWS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::equalrows_, nsGkAtoms::equalrows_,
      nsGkAtoms::equalrows_, ALL_NO_PREFIX);
  ATTR_HSPACE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::hspace, nsGkAtoms::hspace,
                               nsGkAtoms::hspace, ALL_NO_PREFIX);
  ATTR_ISMAP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::ismap, nsGkAtoms::ismap,
                               nsGkAtoms::ismap, ALL_NO_PREFIX);
  ATTR_LOCAL =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::local, nsGkAtoms::local,
                               nsGkAtoms::local, ALL_NO_PREFIX);
  ATTR_LSPACE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::lspace_,
                                         nsGkAtoms::lspace_, nsGkAtoms::lspace_,
                                         ALL_NO_PREFIX);
  ATTR_MOVABLELIMITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::movablelimits_, nsGkAtoms::movablelimits_,
      nsGkAtoms::movablelimits_, ALL_NO_PREFIX);
  ATTR_NOTATION = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::notation_,
                                           nsGkAtoms::notation_,
                                           nsGkAtoms::notation_, ALL_NO_PREFIX);
  ATTR_ONDATAAVAILABLE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondataavailable, nsGkAtoms::ondataavailable,
      nsGkAtoms::ondataavailable, ALL_NO_PREFIX);
  ATTR_ONPASTE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onpaste,
                                          nsGkAtoms::onpaste,
                                          nsGkAtoms::onpaste, ALL_NO_PREFIX);
  ATTR_RSPACE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rspace_,
                                         nsGkAtoms::rspace_, nsGkAtoms::rspace_,
                                         ALL_NO_PREFIX);
  ATTR_ROWALIGN = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rowalign_,
                                           nsGkAtoms::rowalign_,
                                           nsGkAtoms::rowalign_, ALL_NO_PREFIX);
  ATTR_ROTATE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rotate, nsGkAtoms::rotate,
                               nsGkAtoms::rotate, ALL_NO_PREFIX);
  ATTR_SEPARATOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::separator_, nsGkAtoms::separator_,
      nsGkAtoms::separator_, ALL_NO_PREFIX);
  ATTR_SEPARATORS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::separators_, nsGkAtoms::separators_,
      nsGkAtoms::separators_, ALL_NO_PREFIX);
  ATTR_VSPACE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::vspace, nsGkAtoms::vspace,
                               nsGkAtoms::vspace, ALL_NO_PREFIX);
  ATTR_XCHANNELSELECTOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::xchannelselector, nsGkAtoms::xchannelselector,
      nsGkAtoms::xChannelSelector, ALL_NO_PREFIX);
  ATTR_YCHANNELSELECTOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ychannelselector, nsGkAtoms::ychannelselector,
      nsGkAtoms::yChannelSelector, ALL_NO_PREFIX);
  ATTR_ENABLE_BACKGROUND = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::enable_background, nsGkAtoms::enable_background,
      nsGkAtoms::enable_background, ALL_NO_PREFIX);
  ATTR_ONDBLCLICK = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondblclick, nsGkAtoms::ondblclick,
      nsGkAtoms::ondblclick, ALL_NO_PREFIX);
  ATTR_ONABORT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onabort,
                                          nsGkAtoms::onabort,
                                          nsGkAtoms::onabort, ALL_NO_PREFIX);
  ATTR_CALCMODE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::calcmode,
                                           nsGkAtoms::calcmode,
                                           nsGkAtoms::calcMode, ALL_NO_PREFIX);
  ATTR_CHECKED = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::checked,
                                          nsGkAtoms::checked,
                                          nsGkAtoms::checked, ALL_NO_PREFIX);
  ATTR_FENCE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::fence_, nsGkAtoms::fence_,
                               nsGkAtoms::fence_, ALL_NO_PREFIX);
  ATTR_FETCHPRIORITY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::fetchpriority, nsGkAtoms::fetchpriority,
      nsGkAtoms::fetchpriority, ALL_NO_PREFIX);
  ATTR_NONCE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::nonce, nsGkAtoms::nonce,
                               nsGkAtoms::nonce, ALL_NO_PREFIX);
  ATTR_ONSCROLL = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onscroll,
                                           nsGkAtoms::onscroll,
                                           nsGkAtoms::onscroll, ALL_NO_PREFIX);
  ATTR_ONACTIVATE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onactivate, nsGkAtoms::onactivate,
      nsGkAtoms::onactivate, ALL_NO_PREFIX);
  ATTR_OPACITY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::opacity,
                                          nsGkAtoms::opacity,
                                          nsGkAtoms::opacity, ALL_NO_PREFIX);
  ATTR_SPACING = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::spacing,
                                          nsGkAtoms::spacing,
                                          nsGkAtoms::spacing, ALL_NO_PREFIX);
  ATTR_SPECULAREXPONENT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::specularexponent, nsGkAtoms::specularexponent,
      nsGkAtoms::specularExponent, ALL_NO_PREFIX);
  ATTR_SPECULARCONSTANT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::specularconstant, nsGkAtoms::specularconstant,
      nsGkAtoms::specularConstant, ALL_NO_PREFIX);
  ATTR_BORDER =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::border, nsGkAtoms::border,
                               nsGkAtoms::border, ALL_NO_PREFIX);
  ATTR_ID = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::id, nsGkAtoms::id,
                                     nsGkAtoms::id, ALL_NO_PREFIX);
  ATTR_GRADIENTTRANSFORM = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::gradienttransform, nsGkAtoms::gradienttransform,
      nsGkAtoms::gradientTransform, ALL_NO_PREFIX);
  ATTR_GRADIENTUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::gradientunits, nsGkAtoms::gradientunits,
      nsGkAtoms::gradientUnits, ALL_NO_PREFIX);
  ATTR_HIDDEN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::hidden, nsGkAtoms::hidden,
                               nsGkAtoms::hidden, ALL_NO_PREFIX);
  ATTR_HEADERS = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::headers,
                                          nsGkAtoms::headers,
                                          nsGkAtoms::headers, ALL_NO_PREFIX);
  ATTR_LOADING = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::loading,
                                          nsGkAtoms::loading,
                                          nsGkAtoms::loading, ALL_NO_PREFIX);
  ATTR_READONLY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::readonly,
                                           nsGkAtoms::readonly,
                                           nsGkAtoms::readonly, ALL_NO_PREFIX);
  ATTR_RENDERING_INTENT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::rendering_intent, nsGkAtoms::rendering_intent,
      nsGkAtoms::rendering_intent, ALL_NO_PREFIX);
  ATTR_SHADOWROOTMODE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::shadowrootmode, nsGkAtoms::shadowrootmode,
      nsGkAtoms::shadowrootmode, ALL_NO_PREFIX);
  ATTR_SEED =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::seed, nsGkAtoms::seed,
                               nsGkAtoms::seed, ALL_NO_PREFIX);
  ATTR_SRCDOC =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::srcdoc, nsGkAtoms::srcdoc,
                               nsGkAtoms::srcdoc, ALL_NO_PREFIX);
  ATTR_STDDEVIATION = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stddeviation, nsGkAtoms::stddeviation,
      nsGkAtoms::stdDeviation, ALL_NO_PREFIX);
  ATTR_SANDBOX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::sandbox,
                                          nsGkAtoms::sandbox,
                                          nsGkAtoms::sandbox, ALL_NO_PREFIX);
  ATTR_SHADOWROOTDELEGATESFOCUS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::shadowrootdelegatesfocus,
      nsGkAtoms::shadowrootdelegatesfocus, nsGkAtoms::shadowrootdelegatesfocus,
      ALL_NO_PREFIX);
  ATTR_WORD_SPACING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::word_spacing, nsGkAtoms::word_spacing,
      nsGkAtoms::word_spacing, ALL_NO_PREFIX);
  ATTR_ACCENTUNDER = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::accentunder_, nsGkAtoms::accentunder_,
      nsGkAtoms::accentunder_, ALL_NO_PREFIX);
  ATTR_ACCEPT_CHARSET = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::acceptcharset, nsGkAtoms::acceptcharset,
      nsGkAtoms::acceptcharset, ALL_NO_PREFIX);
  ATTR_ACCESSKEY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::accesskey, nsGkAtoms::accesskey,
      nsGkAtoms::accesskey, ALL_NO_PREFIX);
  ATTR_ACCENT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::accent_,
                                         nsGkAtoms::accent_, nsGkAtoms::accent_,
                                         ALL_NO_PREFIX);
  ATTR_ACCEPT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::accept, nsGkAtoms::accept,
                               nsGkAtoms::accept, ALL_NO_PREFIX);
  ATTR_BEVELLED = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::bevelled_,
                                           nsGkAtoms::bevelled_,
                                           nsGkAtoms::bevelled_, ALL_NO_PREFIX);
  ATTR_BASEFREQUENCY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::basefrequency, nsGkAtoms::basefrequency,
      nsGkAtoms::baseFrequency, ALL_NO_PREFIX);
  ATTR_BASELINE_SHIFT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::baseline_shift, nsGkAtoms::baseline_shift,
      nsGkAtoms::baseline_shift, ALL_NO_PREFIX);
  ATTR_BASEPROFILE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::baseprofile, nsGkAtoms::baseprofile,
      nsGkAtoms::baseProfile, ALL_NO_PREFIX);
  ATTR_BASELINE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::baseline,
                                           nsGkAtoms::baseline,
                                           nsGkAtoms::baseline, ALL_NO_PREFIX);
  ATTR_BASE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::base, nsGkAtoms::base,
                               nsGkAtoms::base, ALL_NO_PREFIX);
  ATTR_CODE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::code, nsGkAtoms::code,
                               nsGkAtoms::code, ALL_NO_PREFIX);
  ATTR_CODETYPE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::codetype,
                                           nsGkAtoms::codetype,
                                           nsGkAtoms::codetype, ALL_NO_PREFIX);
  ATTR_CODEBASE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::codebase,
                                           nsGkAtoms::codebase,
                                           nsGkAtoms::codebase, ALL_NO_PREFIX);
  ATTR_CITE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::cite, nsGkAtoms::cite,
                               nsGkAtoms::cite, ALL_NO_PREFIX);
  ATTR_DEFER =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::defer, nsGkAtoms::defer,
                               nsGkAtoms::defer, ALL_NO_PREFIX);
  ATTR_DATETIME = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::datetime,
                                           nsGkAtoms::datetime,
                                           nsGkAtoms::datetime, ALL_NO_PREFIX);
  ATTR_DIRECTION = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::direction, nsGkAtoms::direction,
      nsGkAtoms::direction, ALL_NO_PREFIX);
  ATTR_EDGEMODE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::edgemode,
                                           nsGkAtoms::edgemode,
                                           nsGkAtoms::edgeMode, ALL_NO_PREFIX);
  ATTR_EDGE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::edge_, nsGkAtoms::edge_,
                               nsGkAtoms::edge_, ALL_NO_PREFIX);
  ATTR_ENTERKEYHINT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::enterkeyhint, nsGkAtoms::enterkeyhint,
      nsGkAtoms::enterkeyhint, ALL_NO_PREFIX);
  ATTR_FACE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::face, nsGkAtoms::face,
                               nsGkAtoms::face, ALL_NO_PREFIX);
  ATTR_INDEX =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::index, nsGkAtoms::index,
                               nsGkAtoms::index, ALL_NO_PREFIX);
  ATTR_INTERCEPT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::intercept, nsGkAtoms::intercept,
      nsGkAtoms::intercept, ALL_NO_PREFIX);
  ATTR_INTEGRITY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::integrity, nsGkAtoms::integrity,
      nsGkAtoms::integrity, ALL_NO_PREFIX);
  ATTR_LINEBREAK = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::linebreak_, nsGkAtoms::linebreak_,
      nsGkAtoms::linebreak_, ALL_NO_PREFIX);
  ATTR_LABEL =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::label, nsGkAtoms::label,
                               nsGkAtoms::label, ALL_NO_PREFIX);
  ATTR_LINETHICKNESS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::linethickness_, nsGkAtoms::linethickness_,
      nsGkAtoms::linethickness_, ALL_NO_PREFIX);
  ATTR_MODE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::mode, nsGkAtoms::mode,
                               nsGkAtoms::mode, ALL_NO_PREFIX);
  ATTR_NAME =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::name, nsGkAtoms::name,
                               nsGkAtoms::name, ALL_NO_PREFIX);
  ATTR_NORESIZE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::noresize,
                                           nsGkAtoms::noresize,
                                           nsGkAtoms::noresize, ALL_NO_PREFIX);
  ATTR_ONBEFOREUNLOAD = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onbeforeunload, nsGkAtoms::onbeforeunload,
      nsGkAtoms::onbeforeunload, ALL_NO_PREFIX);
  ATTR_ONREPEAT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onrepeat,
                                           nsGkAtoms::onrepeat,
                                           nsGkAtoms::onrepeat, ALL_NO_PREFIX);
  ATTR_OBJECT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::object, nsGkAtoms::object,
                               nsGkAtoms::object, ALL_NO_PREFIX);
  ATTR_ONSELECT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onselect,
                                           nsGkAtoms::onselect,
                                           nsGkAtoms::onselect, ALL_NO_PREFIX);
  ATTR_ORDER =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::order, nsGkAtoms::order,
                               nsGkAtoms::order, ALL_NO_PREFIX);
  ATTR_OTHER =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::other, nsGkAtoms::other,
                               nsGkAtoms::other, ALL_NO_PREFIX);
  ATTR_ONRESET = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onreset,
                                          nsGkAtoms::onreset,
                                          nsGkAtoms::onreset, ALL_NO_PREFIX);
  ATTR_ONREADYSTATECHANGE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onreadystatechange, nsGkAtoms::onreadystatechange,
      nsGkAtoms::onreadystatechange, ALL_NO_PREFIX);
  ATTR_ONMESSAGE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmessage, nsGkAtoms::onmessage,
      nsGkAtoms::onmessage, ALL_NO_PREFIX);
  ATTR_ONBEGIN = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onbegin,
                                          nsGkAtoms::onbegin,
                                          nsGkAtoms::onbegin, ALL_NO_PREFIX);
  ATTR_ONBEFOREPRINT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onbeforeprint, nsGkAtoms::onbeforeprint,
      nsGkAtoms::onbeforeprint, ALL_NO_PREFIX);
  ATTR_ORIENT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::orient, nsGkAtoms::orient,
                               nsGkAtoms::orient, ALL_NO_PREFIX);
  ATTR_ORIENTATION = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::orientation, nsGkAtoms::orientation,
      nsGkAtoms::orientation, ALL_NO_PREFIX);
  ATTR_ONBEFORECOPY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onbeforecopy, nsGkAtoms::onbeforecopy,
      nsGkAtoms::onbeforecopy, ALL_NO_PREFIX);
  ATTR_ONSELECTSTART = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onselectstart, nsGkAtoms::onselectstart,
      nsGkAtoms::onselectstart, ALL_NO_PREFIX);
  ATTR_ONBEFOREPASTE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onbeforepaste, nsGkAtoms::onbeforepaste,
      nsGkAtoms::onbeforepaste, ALL_NO_PREFIX);
  ATTR_ONKEYPRESS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onkeypress, nsGkAtoms::onkeypress,
      nsGkAtoms::onkeypress, ALL_NO_PREFIX);
  ATTR_ONKEYUP = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onkeyup,
                                          nsGkAtoms::onkeyup,
                                          nsGkAtoms::onkeyup, ALL_NO_PREFIX);
  ATTR_ONBEFORECUT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onbeforecut, nsGkAtoms::onbeforecut,
      nsGkAtoms::onbeforecut, ALL_NO_PREFIX);
  ATTR_ONKEYDOWN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onkeydown, nsGkAtoms::onkeydown,
      nsGkAtoms::onkeydown, ALL_NO_PREFIX);
  ATTR_ONRESIZE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onresize,
                                           nsGkAtoms::onresize,
                                           nsGkAtoms::onresize, ALL_NO_PREFIX);
  ATTR_REPEAT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::repeat, nsGkAtoms::repeat,
                               nsGkAtoms::repeat, ALL_NO_PREFIX);
  ATTR_REFERRERPOLICY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::referrerpolicy, nsGkAtoms::referrerpolicy,
      nsGkAtoms::referrerpolicy, ALL_NO_PREFIX);
  ATTR_RULES =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rules, nsGkAtoms::rules,
                               nsGkAtoms::rules, ALL_NO_PREFIX);
  ATTR_ROLE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::role, nsGkAtoms::role,
                               nsGkAtoms::role, ALL_NO_PREFIX);
  ATTR_REPEATCOUNT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::repeatcount, nsGkAtoms::repeatcount,
      nsGkAtoms::repeatCount, ALL_NO_PREFIX);
  ATTR_REPEATDUR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::repeatdur, nsGkAtoms::repeatdur,
      nsGkAtoms::repeatDur, ALL_NO_PREFIX);
  ATTR_SELECTED = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::selected,
                                           nsGkAtoms::selected,
                                           nsGkAtoms::selected, ALL_NO_PREFIX);
  ATTR_SIZES =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::sizes, nsGkAtoms::sizes,
                               nsGkAtoms::sizes, ALL_NO_PREFIX);
  ATTR_SUPERSCRIPTSHIFT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::superscriptshift_, nsGkAtoms::superscriptshift_,
      nsGkAtoms::superscriptshift_, ALL_NO_PREFIX);
  ATTR_STRETCHY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::stretchy_,
                                           nsGkAtoms::stretchy_,
                                           nsGkAtoms::stretchy_, ALL_NO_PREFIX);
  ATTR_SCHEME =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::scheme, nsGkAtoms::scheme,
                               nsGkAtoms::scheme, ALL_NO_PREFIX);
  ATTR_SPREADMETHOD = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::spreadmethod, nsGkAtoms::spreadmethod,
      nsGkAtoms::spreadMethod, ALL_NO_PREFIX);
  ATTR_SELECTION = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::selection_, nsGkAtoms::selection_,
      nsGkAtoms::selection_, ALL_NO_PREFIX);
  ATTR_SIZE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::size, nsGkAtoms::size,
                               nsGkAtoms::size, ALL_NO_PREFIX);
  ATTR_TYPE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::type, nsGkAtoms::type,
                               nsGkAtoms::type, ALL_NO_PREFIX);
  ATTR_DIFFUSECONSTANT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::diffuseconstant, nsGkAtoms::diffuseconstant,
      nsGkAtoms::diffuseConstant, ALL_NO_PREFIX);
  ATTR_HREF =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::href, nsGkAtoms::href,
                               nsGkAtoms::href, ALL_NO_PREFIX);
  ATTR_HREFLANG = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::hreflang,
                                           nsGkAtoms::hreflang,
                                           nsGkAtoms::hreflang, ALL_NO_PREFIX);
  ATTR_ONAFTERPRINT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onafterprint, nsGkAtoms::onafterprint,
      nsGkAtoms::onafterprint, ALL_NO_PREFIX);
  ATTR_PROFILE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::profile,
                                          nsGkAtoms::profile,
                                          nsGkAtoms::profile, ALL_NO_PREFIX);
  ATTR_SURFACESCALE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::surfacescale, nsGkAtoms::surfacescale,
      nsGkAtoms::surfaceScale, ALL_NO_PREFIX);
  ATTR_XREF =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::xref_, nsGkAtoms::xref_,
                               nsGkAtoms::xref_, ALL_NO_PREFIX);
  ATTR_ALIGN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::align, nsGkAtoms::align,
                               nsGkAtoms::align, ALL_NO_PREFIX);
  ATTR_ALIGNMENT_BASELINE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::alignment_baseline, nsGkAtoms::alignment_baseline,
      nsGkAtoms::alignment_baseline, ALL_NO_PREFIX);
  ATTR_ALIGNMENTSCOPE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::alignmentscope_, nsGkAtoms::alignmentscope_,
      nsGkAtoms::alignmentscope_, ALL_NO_PREFIX);
  ATTR_DRAGGABLE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::draggable, nsGkAtoms::draggable,
      nsGkAtoms::draggable, ALL_NO_PREFIX);
  ATTR_HEIGHT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::height, nsGkAtoms::height,
                               nsGkAtoms::height, ALL_NO_PREFIX);
  ATTR_IMAGESIZES = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::imagesizes, nsGkAtoms::imagesizes,
      nsGkAtoms::imagesizes, ALL_NO_PREFIX);
  ATTR_IMAGESRCSET = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::imagesrcset, nsGkAtoms::imagesrcset,
      nsGkAtoms::imagesrcset, ALL_NO_PREFIX);
  ATTR_IMAGE_RENDERING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::image_rendering, nsGkAtoms::image_rendering,
      nsGkAtoms::image_rendering, ALL_NO_PREFIX);
  ATTR_LANGUAGE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::language,
                                           nsGkAtoms::language,
                                           nsGkAtoms::language, ALL_NO_PREFIX);
  ATTR_LANG =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::lang, nsGkAtoms::lang,
                               nsGkAtoms::lang, ALL_NO_PREFIX);
  ATTR_LARGEOP = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::largeop_,
                                          nsGkAtoms::largeop_,
                                          nsGkAtoms::largeop_, ALL_NO_PREFIX);
  ATTR_LONGDESC = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::longdesc,
                                           nsGkAtoms::longdesc,
                                           nsGkAtoms::longdesc, ALL_NO_PREFIX);
  ATTR_LENGTHADJUST = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::lengthadjust, nsGkAtoms::lengthadjust,
      nsGkAtoms::lengthAdjust, ALL_NO_PREFIX);
  ATTR_MARGINHEIGHT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::marginheight, nsGkAtoms::marginheight,
      nsGkAtoms::marginheight, ALL_NO_PREFIX);
  ATTR_MARGINWIDTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::marginwidth, nsGkAtoms::marginwidth,
      nsGkAtoms::marginwidth, ALL_NO_PREFIX);
  ATTR_ORIGIN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::origin, nsGkAtoms::origin,
                               nsGkAtoms::origin, ALL_NO_PREFIX);
  ATTR_PING =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::ping, nsGkAtoms::ping,
                               nsGkAtoms::ping, ALL_NO_PREFIX);
  ATTR_TARGET =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::target, nsGkAtoms::target,
                               nsGkAtoms::target, ALL_NO_PREFIX);
  ATTR_TARGETX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::targetx,
                                          nsGkAtoms::targetx,
                                          nsGkAtoms::targetX, ALL_NO_PREFIX);
  ATTR_TARGETY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::targety,
                                          nsGkAtoms::targety,
                                          nsGkAtoms::targetY, ALL_NO_PREFIX);
  ATTR_ARCHIVE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::archive,
                                          nsGkAtoms::archive,
                                          nsGkAtoms::archive, ALL_NO_PREFIX);
  ATTR_HIGH =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::high, nsGkAtoms::high,
                               nsGkAtoms::high, ALL_NO_PREFIX);
  ATTR_LIGHTING_COLOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::lighting_color, nsGkAtoms::lighting_color,
      nsGkAtoms::lighting_color, ALL_NO_PREFIX);
  ATTR_MATHBACKGROUND = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::mathbackground_, nsGkAtoms::mathbackground_,
      nsGkAtoms::mathbackground_, ALL_NO_PREFIX);
  ATTR_METHOD =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::method, nsGkAtoms::method,
                               nsGkAtoms::method, ALL_NO_PREFIX);
  ATTR_MATHVARIANT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::mathvariant_, nsGkAtoms::mathvariant_,
      nsGkAtoms::mathvariant_, ALL_NO_PREFIX);
  ATTR_MATHCOLOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::mathcolor_, nsGkAtoms::mathcolor_,
      nsGkAtoms::mathcolor_, ALL_NO_PREFIX);
  ATTR_MATHSIZE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::mathsize_,
                                           nsGkAtoms::mathsize_,
                                           nsGkAtoms::mathsize_, ALL_NO_PREFIX);
  ATTR_NOSHADE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::noshade,
                                          nsGkAtoms::noshade,
                                          nsGkAtoms::noshade, ALL_NO_PREFIX);
  ATTR_ONCHANGE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onchange,
                                           nsGkAtoms::onchange,
                                           nsGkAtoms::onchange, ALL_NO_PREFIX);
  ATTR_PATHLENGTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::pathlength, nsGkAtoms::pathlength,
      nsGkAtoms::pathLength, ALL_NO_PREFIX);
  ATTR_PATH =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::path, nsGkAtoms::path,
                               nsGkAtoms::path, ALL_NO_PREFIX);
  ATTR_ALTIMG = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::altimg_,
                                         nsGkAtoms::altimg_, nsGkAtoms::altimg_,
                                         ALL_NO_PREFIX);
  ATTR_ACTIONTYPE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::actiontype_, nsGkAtoms::actiontype_,
      nsGkAtoms::actiontype_, ALL_NO_PREFIX);
  ATTR_ACTION =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::action, nsGkAtoms::action,
                               nsGkAtoms::action, ALL_NO_PREFIX);
  ATTR_ACTIVE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::active, nsGkAtoms::active,
                               nsGkAtoms::active, ALL_NO_PREFIX);
  ATTR_ADDITIVE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::additive,
                                           nsGkAtoms::additive,
                                           nsGkAtoms::additive, ALL_NO_PREFIX);
  ATTR_BEGIN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::begin, nsGkAtoms::begin,
                               nsGkAtoms::begin, ALL_NO_PREFIX);
  ATTR_DOMINANT_BASELINE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::dominant_baseline, nsGkAtoms::dominant_baseline,
      nsGkAtoms::dominant_baseline, ALL_NO_PREFIX);
  ATTR_DIVISOR = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::divisor,
                                          nsGkAtoms::divisor,
                                          nsGkAtoms::divisor, ALL_NO_PREFIX);
  ATTR_DEFINITIONURL = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::definitionurl, nsGkAtoms::definitionURL_,
      nsGkAtoms::definitionurl, ALL_NO_PREFIX);
  ATTR_LIMITINGCONEANGLE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::limitingconeangle, nsGkAtoms::limitingconeangle,
      nsGkAtoms::limitingConeAngle, ALL_NO_PREFIX);
  ATTR_MEDIA =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::media, nsGkAtoms::media,
                               nsGkAtoms::media, ALL_NO_PREFIX);
  ATTR_MANIFEST = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::manifest,
                                           nsGkAtoms::manifest,
                                           nsGkAtoms::manifest, ALL_NO_PREFIX);
  ATTR_ONFINISH = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onfinish,
                                           nsGkAtoms::onfinish,
                                           nsGkAtoms::onfinish, ALL_NO_PREFIX);
  ATTR_OPTIMUM = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::optimum,
                                          nsGkAtoms::optimum,
                                          nsGkAtoms::optimum, ALL_NO_PREFIX);
  ATTR_RADIOGROUP = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::radiogroup, nsGkAtoms::radiogroup,
      nsGkAtoms::radiogroup, ALL_NO_PREFIX);
  ATTR_RADIUS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::radius, nsGkAtoms::radius,
                               nsGkAtoms::radius, ALL_NO_PREFIX);
  ATTR_SCRIPTLEVEL = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::scriptlevel_, nsGkAtoms::scriptlevel_,
      nsGkAtoms::scriptlevel_, ALL_NO_PREFIX);
  ATTR_SCRIPTSIZEMULTIPLIER =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::scriptsizemultiplier_,
                               nsGkAtoms::scriptsizemultiplier_,
                               nsGkAtoms::scriptsizemultiplier_, ALL_NO_PREFIX);
  ATTR_SCRIPTMINSIZE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::scriptminsize_, nsGkAtoms::scriptminsize_,
      nsGkAtoms::scriptminsize_, ALL_NO_PREFIX);
  ATTR_TABINDEX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::tabindex,
                                           nsGkAtoms::tabindex,
                                           nsGkAtoms::tabindex, ALL_NO_PREFIX);
  ATTR_VALIGN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::valign, nsGkAtoms::valign,
                               nsGkAtoms::valign, ALL_NO_PREFIX);
  ATTR_VISIBILITY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::visibility, nsGkAtoms::visibility,
      nsGkAtoms::visibility, ALL_NO_PREFIX);
  ATTR_BACKGROUND = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::background, nsGkAtoms::background,
      nsGkAtoms::background, ALL_NO_PREFIX);
  ATTR_LINK =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::link, nsGkAtoms::link,
                               nsGkAtoms::link, ALL_NO_PREFIX);
  ATTR_MARKER_MID = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::marker_mid, nsGkAtoms::marker_mid,
      nsGkAtoms::marker_mid, ALL_NO_PREFIX);
  ATTR_MARKERHEIGHT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::markerheight, nsGkAtoms::markerheight,
      nsGkAtoms::markerHeight, ALL_NO_PREFIX);
  ATTR_MARKER_END = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::marker_end, nsGkAtoms::marker_end,
      nsGkAtoms::marker_end, ALL_NO_PREFIX);
  ATTR_MASK =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::mask, nsGkAtoms::mask,
                               nsGkAtoms::mask, ALL_NO_PREFIX);
  ATTR_MARKER_START = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::marker_start, nsGkAtoms::marker_start,
      nsGkAtoms::marker_start, ALL_NO_PREFIX);
  ATTR_MARKERWIDTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::markerwidth, nsGkAtoms::markerwidth,
      nsGkAtoms::markerWidth, ALL_NO_PREFIX);
  ATTR_MASKUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::maskunits, nsGkAtoms::maskunits,
      nsGkAtoms::maskUnits, ALL_NO_PREFIX);
  ATTR_MARKERUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::markerunits, nsGkAtoms::markerunits,
      nsGkAtoms::markerUnits, ALL_NO_PREFIX);
  ATTR_MASKCONTENTUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::maskcontentunits, nsGkAtoms::maskcontentunits,
      nsGkAtoms::maskContentUnits, ALL_NO_PREFIX);
  ATTR_AMPLITUDE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::amplitude, nsGkAtoms::amplitude,
      nsGkAtoms::amplitude, ALL_NO_PREFIX);
  ATTR_CELLSPACING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::cellspacing, nsGkAtoms::cellspacing,
      nsGkAtoms::cellspacing, ALL_NO_PREFIX);
  ATTR_CELLPADDING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::cellpadding, nsGkAtoms::cellpadding,
      nsGkAtoms::cellpadding, ALL_NO_PREFIX);
  ATTR_DECLARE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::declare,
                                          nsGkAtoms::declare,
                                          nsGkAtoms::declare, ALL_NO_PREFIX);
  ATTR_FILL_RULE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::fill_rule, nsGkAtoms::fill_rule,
      nsGkAtoms::fill_rule, ALL_NO_PREFIX);
  ATTR_FILL =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::fill, nsGkAtoms::fill,
                               nsGkAtoms::fill, ALL_NO_PREFIX);
  ATTR_FILL_OPACITY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::fill_opacity, nsGkAtoms::fill_opacity,
      nsGkAtoms::fill_opacity, ALL_NO_PREFIX);
  ATTR_MAXLENGTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::maxlength, nsGkAtoms::maxlength,
      nsGkAtoms::maxlength, ALL_NO_PREFIX);
  ATTR_ONCLICK = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onclick,
                                          nsGkAtoms::onclick,
                                          nsGkAtoms::onclick, ALL_NO_PREFIX);
  ATTR_ONBLUR =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onblur, nsGkAtoms::onblur,
                               nsGkAtoms::onblur, ALL_NO_PREFIX);
  ATTR_REPLACE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::replace,
                                          nsGkAtoms::replace,
                                          nsGkAtoms::replace, ALL_NO_PREFIX);
  ATTR_ROWLINES = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rowlines_,
                                           nsGkAtoms::rowlines_,
                                           nsGkAtoms::rowlines_, ALL_NO_PREFIX);
  ATTR_SCALE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::scale, nsGkAtoms::scale,
                               nsGkAtoms::scale, ALL_NO_PREFIX);
  ATTR_STYLE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::style, nsGkAtoms::style,
                               nsGkAtoms::style, ALL_NO_PREFIX);
  ATTR_TABLEVALUES = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::tablevalues, nsGkAtoms::tablevalues,
      nsGkAtoms::tableValues, ALL_NO_PREFIX);
  ATTR_TITLE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::title, nsGkAtoms::title,
                               nsGkAtoms::title, ALL_NO_PREFIX);
  ATTR_AZIMUTH = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::azimuth,
                                          nsGkAtoms::azimuth,
                                          nsGkAtoms::azimuth, ALL_NO_PREFIX);
  ATTR_FORMAT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::format, nsGkAtoms::format,
                               nsGkAtoms::format, ALL_NO_PREFIX);
  ATTR_FRAMEBORDER = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::frameborder, nsGkAtoms::frameborder,
      nsGkAtoms::frameborder, ALL_NO_PREFIX);
  ATTR_FRAME =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::frame, nsGkAtoms::frame,
                               nsGkAtoms::frame, ALL_NO_PREFIX);
  ATTR_FRAMESPACING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::framespacing_, nsGkAtoms::framespacing_,
      nsGkAtoms::framespacing_, ALL_NO_PREFIX);
  ATTR_FROM =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::from, nsGkAtoms::from,
                               nsGkAtoms::from, ALL_NO_PREFIX);
  ATTR_FORM =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::form, nsGkAtoms::form,
                               nsGkAtoms::form, ALL_NO_PREFIX);
  ATTR_PROMPT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::prompt, nsGkAtoms::prompt,
                               nsGkAtoms::prompt, ALL_NO_PREFIX);
  ATTR_PRIMITIVEUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::primitiveunits, nsGkAtoms::primitiveunits,
      nsGkAtoms::primitiveUnits, ALL_NO_PREFIX);
  ATTR_SYMMETRIC = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::symmetric_, nsGkAtoms::symmetric_,
      nsGkAtoms::symmetric_, ALL_NO_PREFIX);
  ATTR_SUMMARY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::summary,
                                          nsGkAtoms::summary,
                                          nsGkAtoms::summary, ALL_NO_PREFIX);
  ATTR_USEMAP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::usemap, nsGkAtoms::usemap,
                               nsGkAtoms::usemap, ALL_NO_PREFIX);
  ATTR_ZOOMANDPAN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::zoomandpan, nsGkAtoms::zoomandpan,
      nsGkAtoms::zoomAndPan, ALL_NO_PREFIX);
  ATTR_ASYNC =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::async, nsGkAtoms::async,
                               nsGkAtoms::async, ALL_NO_PREFIX);
  ATTR_ALINK =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::alink, nsGkAtoms::alink,
                               nsGkAtoms::alink, ALL_NO_PREFIX);
  ATTR_IN = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::in, nsGkAtoms::in,
                                     nsGkAtoms::in, ALL_NO_PREFIX);
  ATTR_ICON =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::icon, nsGkAtoms::icon,
                               nsGkAtoms::icon, ALL_NO_PREFIX);
  ATTR_KERNELMATRIX = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::kernelmatrix, nsGkAtoms::kernelmatrix,
      nsGkAtoms::kernelMatrix, ALL_NO_PREFIX);
  ATTR_KERNING = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::kerning,
                                          nsGkAtoms::kerning,
                                          nsGkAtoms::kerning, ALL_NO_PREFIX);
  ATTR_KERNELUNITLENGTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::kernelunitlength, nsGkAtoms::kernelunitlength,
      nsGkAtoms::kernelUnitLength, ALL_NO_PREFIX);
  ATTR_ONUNLOAD = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onunload,
                                           nsGkAtoms::onunload,
                                           nsGkAtoms::onunload, ALL_NO_PREFIX);
  ATTR_OPEN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::open, nsGkAtoms::open,
                               nsGkAtoms::open, ALL_NO_PREFIX);
  ATTR_ONINVALID = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::oninvalid, nsGkAtoms::oninvalid,
      nsGkAtoms::oninvalid, ALL_NO_PREFIX);
  ATTR_ONEND =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onend, nsGkAtoms::onend,
                               nsGkAtoms::onend, ALL_NO_PREFIX);
  ATTR_ONINPUT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::oninput,
                                          nsGkAtoms::oninput,
                                          nsGkAtoms::oninput, ALL_NO_PREFIX);
  ATTR_POINTER_EVENTS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::pointer_events, nsGkAtoms::pointer_events,
      nsGkAtoms::pointer_events, ALL_NO_PREFIX);
  ATTR_POINTS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::points, nsGkAtoms::points,
                               nsGkAtoms::points, ALL_NO_PREFIX);
  ATTR_POINTSATX = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::pointsatx, nsGkAtoms::pointsatx,
      nsGkAtoms::pointsAtX, ALL_NO_PREFIX);
  ATTR_POINTSATY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::pointsaty, nsGkAtoms::pointsaty,
      nsGkAtoms::pointsAtY, ALL_NO_PREFIX);
  ATTR_POINTSATZ = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::pointsatz, nsGkAtoms::pointsatz,
      nsGkAtoms::pointsAtZ, ALL_NO_PREFIX);
  ATTR_SPAN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::span, nsGkAtoms::span,
                               nsGkAtoms::span, ALL_NO_PREFIX);
  ATTR_STANDBY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::standby,
                                          nsGkAtoms::standby,
                                          nsGkAtoms::standby, ALL_NO_PREFIX);
  ATTR_TRANSFORM_ORIGIN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::transform_origin, nsGkAtoms::transform_origin,
      nsGkAtoms::transform_origin, ALL_NO_PREFIX);
  ATTR_TRANSFORM = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::transform, nsGkAtoms::transform,
      nsGkAtoms::transform, ALL_NO_PREFIX);
  ATTR_VLINK =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::vlink, nsGkAtoms::vlink,
                               nsGkAtoms::vlink, ALL_NO_PREFIX);
  ATTR_WHEN =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::when, nsGkAtoms::when,
                               nsGkAtoms::when, ALL_NO_PREFIX);
  ATTR_XLINK_HREF =
      new nsHtml5AttributeName(XLINK_NS, nsGkAtoms::xlink_href, nsGkAtoms::href,
                               nsGkAtoms::href, XLINK_PREFIX);
  ATTR_XLINK_TITLE = new nsHtml5AttributeName(XLINK_NS, nsGkAtoms::xlink_title,
                                              nsGkAtoms::title,
                                              nsGkAtoms::title, XLINK_PREFIX);
  ATTR_XLINK_ROLE =
      new nsHtml5AttributeName(XLINK_NS, nsGkAtoms::xlink_role, nsGkAtoms::role,
                               nsGkAtoms::role, XLINK_PREFIX);
  ATTR_XLINK_ARCROLE = new nsHtml5AttributeName(
      XLINK_NS, nsGkAtoms::xlink_arcrole, nsGkAtoms::arcrole,
      nsGkAtoms::arcrole, XLINK_PREFIX);
  ATTR_XMLNS_XLINK = new nsHtml5AttributeName(XMLNS_NS, nsGkAtoms::xmlns_xlink,
                                              nsGkAtoms::xlink,
                                              nsGkAtoms::xlink, XMLNS_PREFIX);
  ATTR_XMLNS =
      new nsHtml5AttributeName(XMLNS_NS, nsGkAtoms::xmlns, nsGkAtoms::xmlns,
                               nsGkAtoms::xmlns, ALL_NO_PREFIX);
  ATTR_XLINK_TYPE =
      new nsHtml5AttributeName(XLINK_NS, nsGkAtoms::xlink_type, nsGkAtoms::type,
                               nsGkAtoms::type, XLINK_PREFIX);
  ATTR_XLINK_SHOW =
      new nsHtml5AttributeName(XLINK_NS, nsGkAtoms::xlink_show, nsGkAtoms::show,
                               nsGkAtoms::show, XLINK_PREFIX);
  ATTR_XLINK_ACTUATE = new nsHtml5AttributeName(
      XLINK_NS, nsGkAtoms::xlink_actuate, nsGkAtoms::actuate,
      nsGkAtoms::actuate, XLINK_PREFIX);
  ATTR_AUTOPLAY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::autoplay,
                                           nsGkAtoms::autoplay,
                                           nsGkAtoms::autoplay, ALL_NO_PREFIX);
  ATTR_AUTOCOMPLETE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::autocomplete, nsGkAtoms::autocomplete,
      nsGkAtoms::autocomplete, ALL_NO_PREFIX);
  ATTR_AUTOFOCUS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::autofocus, nsGkAtoms::autofocus,
      nsGkAtoms::autofocus, ALL_NO_PREFIX);
  ATTR_AUTOCAPITALIZE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::autocapitalize, nsGkAtoms::autocapitalize,
      nsGkAtoms::autocapitalize, ALL_NO_PREFIX);
  ATTR_BGCOLOR = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::bgcolor,
                                          nsGkAtoms::bgcolor,
                                          nsGkAtoms::bgcolor, ALL_NO_PREFIX);
  ATTR_COLOR_PROFILE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::colorProfile, nsGkAtoms::colorProfile,
      nsGkAtoms::colorProfile, ALL_NO_PREFIX);
  ATTR_COLOR_RENDERING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::color_rendering, nsGkAtoms::color_rendering,
      nsGkAtoms::color_rendering, ALL_NO_PREFIX);
  ATTR_COLOR_INTERPOLATION = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::colorInterpolation, nsGkAtoms::colorInterpolation,
      nsGkAtoms::colorInterpolation, ALL_NO_PREFIX);
  ATTR_COLOR =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::color, nsGkAtoms::color,
                               nsGkAtoms::color, ALL_NO_PREFIX);
  ATTR_COLOR_INTERPOLATION_FILTERS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::colorInterpolationFilters,
      nsGkAtoms::colorInterpolationFilters,
      nsGkAtoms::colorInterpolationFilters, ALL_NO_PREFIX);
  ATTR_ENCODING = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::encoding,
                                           nsGkAtoms::encoding,
                                           nsGkAtoms::encoding, ALL_NO_PREFIX);
  ATTR_EXPONENT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::exponent,
                                           nsGkAtoms::exponent,
                                           nsGkAtoms::exponent, ALL_NO_PREFIX);
  ATTR_FLOOD_COLOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::flood_color, nsGkAtoms::flood_color,
      nsGkAtoms::flood_color, ALL_NO_PREFIX);
  ATTR_FLOOD_OPACITY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::flood_opacity, nsGkAtoms::flood_opacity,
      nsGkAtoms::flood_opacity, ALL_NO_PREFIX);
  ATTR_LQUOTE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::lquote_,
                                         nsGkAtoms::lquote_, nsGkAtoms::lquote_,
                                         ALL_NO_PREFIX);
  ATTR_NUMOCTAVES = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::numoctaves, nsGkAtoms::numoctaves,
      nsGkAtoms::numOctaves, ALL_NO_PREFIX);
  ATTR_NOMODULE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::nomodule,
                                           nsGkAtoms::nomodule,
                                           nsGkAtoms::nomodule, ALL_NO_PREFIX);
  ATTR_ONLOAD =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onload, nsGkAtoms::onload,
                               nsGkAtoms::onload, ALL_NO_PREFIX);
  ATTR_ONMOUSEWHEEL = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmousewheel, nsGkAtoms::onmousewheel,
      nsGkAtoms::onmousewheel, ALL_NO_PREFIX);
  ATTR_ONMOUSEENTER = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmouseenter, nsGkAtoms::onmouseenter,
      nsGkAtoms::onmouseenter, ALL_NO_PREFIX);
  ATTR_ONMOUSEOVER = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmouseover, nsGkAtoms::onmouseover,
      nsGkAtoms::onmouseover, ALL_NO_PREFIX);
  ATTR_ONFOCUSIN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onfocusin, nsGkAtoms::onfocusin,
      nsGkAtoms::onfocusin, ALL_NO_PREFIX);
  ATTR_ONCONTEXTMENU = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::oncontextmenu, nsGkAtoms::oncontextmenu,
      nsGkAtoms::oncontextmenu, ALL_NO_PREFIX);
  ATTR_ONZOOM =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onzoom, nsGkAtoms::onzoom,
                               nsGkAtoms::onzoom, ALL_NO_PREFIX);
  ATTR_ONCOPY =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::oncopy, nsGkAtoms::oncopy,
                               nsGkAtoms::oncopy, ALL_NO_PREFIX);
  ATTR_ONMOUSELEAVE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmouseleave, nsGkAtoms::onmouseleave,
      nsGkAtoms::onmouseleave, ALL_NO_PREFIX);
  ATTR_ONMOUSEMOVE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmousemove, nsGkAtoms::onmousemove,
      nsGkAtoms::onmousemove, ALL_NO_PREFIX);
  ATTR_ONMOUSEUP = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmouseup, nsGkAtoms::onmouseup,
      nsGkAtoms::onmouseup, ALL_NO_PREFIX);
  ATTR_ONFOCUS = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onfocus,
                                          nsGkAtoms::onfocus,
                                          nsGkAtoms::onfocus, ALL_NO_PREFIX);
  ATTR_ONMOUSEOUT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmouseout, nsGkAtoms::onmouseout,
      nsGkAtoms::onmouseout, ALL_NO_PREFIX);
  ATTR_ONFOCUSOUT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onfocusout, nsGkAtoms::onfocusout,
      nsGkAtoms::onfocusout, ALL_NO_PREFIX);
  ATTR_ONMOUSEDOWN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::onmousedown, nsGkAtoms::onmousedown,
      nsGkAtoms::onmousedown, ALL_NO_PREFIX);
  ATTR_TO = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::to, nsGkAtoms::to,
                                     nsGkAtoms::to, ALL_NO_PREFIX);
  ATTR_RQUOTE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rquote_,
                                         nsGkAtoms::rquote_, nsGkAtoms::rquote_,
                                         ALL_NO_PREFIX);
  ATTR_STROKE_LINECAP = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stroke_linecap, nsGkAtoms::stroke_linecap,
      nsGkAtoms::stroke_linecap, ALL_NO_PREFIX);
  ATTR_STROKE_DASHARRAY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stroke_dasharray, nsGkAtoms::stroke_dasharray,
      nsGkAtoms::stroke_dasharray, ALL_NO_PREFIX);
  ATTR_STROKE_DASHOFFSET = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stroke_dashoffset, nsGkAtoms::stroke_dashoffset,
      nsGkAtoms::stroke_dashoffset, ALL_NO_PREFIX);
  ATTR_STROKE_LINEJOIN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stroke_linejoin, nsGkAtoms::stroke_linejoin,
      nsGkAtoms::stroke_linejoin, ALL_NO_PREFIX);
  ATTR_STROKE_MITERLIMIT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stroke_miterlimit, nsGkAtoms::stroke_miterlimit,
      nsGkAtoms::stroke_miterlimit, ALL_NO_PREFIX);
  ATTR_STROKE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::stroke, nsGkAtoms::stroke,
                               nsGkAtoms::stroke, ALL_NO_PREFIX);
  ATTR_SCROLLING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::scrolling, nsGkAtoms::scrolling,
      nsGkAtoms::scrolling, ALL_NO_PREFIX);
  ATTR_STROKE_WIDTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stroke_width, nsGkAtoms::stroke_width,
      nsGkAtoms::stroke_width, ALL_NO_PREFIX);
  ATTR_STROKE_OPACITY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stroke_opacity, nsGkAtoms::stroke_opacity,
      nsGkAtoms::stroke_opacity, ALL_NO_PREFIX);
  ATTR_COMPACT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::compact,
                                          nsGkAtoms::compact,
                                          nsGkAtoms::compact, ALL_NO_PREFIX);
  ATTR_CLIP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::clip, nsGkAtoms::clip,
                               nsGkAtoms::clip, ALL_NO_PREFIX);
  ATTR_CLIP_RULE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::clip_rule, nsGkAtoms::clip_rule,
      nsGkAtoms::clip_rule, ALL_NO_PREFIX);
  ATTR_CLIP_PATH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::clip_path, nsGkAtoms::clip_path,
      nsGkAtoms::clip_path, ALL_NO_PREFIX);
  ATTR_CLIPPATHUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::clippathunits, nsGkAtoms::clippathunits,
      nsGkAtoms::clipPathUnits, ALL_NO_PREFIX);
  ATTR_DISPLAY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::display,
                                          nsGkAtoms::display,
                                          nsGkAtoms::display, ALL_NO_PREFIX);
  ATTR_DISPLAYSTYLE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::displaystyle_, nsGkAtoms::displaystyle_,
      nsGkAtoms::displaystyle_, ALL_NO_PREFIX);
  ATTR_GLYPH_ORIENTATION_VERTICAL = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::glyph_orientation_vertical,
      nsGkAtoms::glyph_orientation_vertical,
      nsGkAtoms::glyph_orientation_vertical, ALL_NO_PREFIX);
  ATTR_GLYPH_ORIENTATION_HORIZONTAL = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::glyph_orientation_horizontal,
      nsGkAtoms::glyph_orientation_horizontal,
      nsGkAtoms::glyph_orientation_horizontal, ALL_NO_PREFIX);
  ATTR_GLYPHREF = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::glyphref,
                                           nsGkAtoms::glyphref,
                                           nsGkAtoms::glyphRef, ALL_NO_PREFIX);
  ATTR_HTTP_EQUIV = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::httpEquiv, nsGkAtoms::httpEquiv,
      nsGkAtoms::httpEquiv, ALL_NO_PREFIX);
  ATTR_KEYPOINTS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::keypoints, nsGkAtoms::keypoints,
      nsGkAtoms::keyPoints, ALL_NO_PREFIX);
  ATTR_LOOP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::loop, nsGkAtoms::loop,
                               nsGkAtoms::loop, ALL_NO_PREFIX);
  ATTR_PROPERTY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::property,
                                           nsGkAtoms::property,
                                           nsGkAtoms::property, ALL_NO_PREFIX);
  ATTR_SCOPED =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::scoped, nsGkAtoms::scoped,
                               nsGkAtoms::scoped, ALL_NO_PREFIX);
  ATTR_STEP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::step, nsGkAtoms::step,
                               nsGkAtoms::step, ALL_NO_PREFIX);
  ATTR_SHAPE_RENDERING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::shape_rendering, nsGkAtoms::shape_rendering,
      nsGkAtoms::shape_rendering, ALL_NO_PREFIX);
  ATTR_SCOPE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::scope, nsGkAtoms::scope,
                               nsGkAtoms::scope, ALL_NO_PREFIX);
  ATTR_SHAPE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::shape, nsGkAtoms::shape,
                               nsGkAtoms::shape, ALL_NO_PREFIX);
  ATTR_SLOPE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::slope, nsGkAtoms::slope,
                               nsGkAtoms::slope, ALL_NO_PREFIX);
  ATTR_STOP_COLOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stop_color, nsGkAtoms::stop_color,
      nsGkAtoms::stop_color, ALL_NO_PREFIX);
  ATTR_STOP_OPACITY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stop_opacity, nsGkAtoms::stop_opacity,
      nsGkAtoms::stop_opacity, ALL_NO_PREFIX);
  ATTR_TEMPLATE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::_template,
                                           nsGkAtoms::_template,
                                           nsGkAtoms::_template, ALL_NO_PREFIX);
  ATTR_WRAP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::wrap, nsGkAtoms::wrap,
                               nsGkAtoms::wrap, ALL_NO_PREFIX);
  ATTR_ABBR =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::abbr, nsGkAtoms::abbr,
                               nsGkAtoms::abbr, ALL_NO_PREFIX);
  ATTR_ATTRIBUTENAME = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::attributename, nsGkAtoms::attributename,
      nsGkAtoms::attributeName, ALL_NO_PREFIX);
  ATTR_ATTRIBUTETYPE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::attributetype, nsGkAtoms::attributetype,
      nsGkAtoms::attributeType, ALL_NO_PREFIX);
  ATTR_CHAR =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::_char, nsGkAtoms::_char,
                               nsGkAtoms::_char, ALL_NO_PREFIX);
  ATTR_COORDS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::coords, nsGkAtoms::coords,
                               nsGkAtoms::coords, ALL_NO_PREFIX);
  ATTR_CHAROFF = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::charoff,
                                          nsGkAtoms::charoff,
                                          nsGkAtoms::charoff, ALL_NO_PREFIX);
  ATTR_CHARSET = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::charset,
                                          nsGkAtoms::charset,
                                          nsGkAtoms::charset, ALL_NO_PREFIX);
  ATTR_NOWRAP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::nowrap, nsGkAtoms::nowrap,
                               nsGkAtoms::nowrap, ALL_NO_PREFIX);
  ATTR_NOHREF =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::nohref, nsGkAtoms::nohref,
                               nsGkAtoms::nohref, ALL_NO_PREFIX);
  ATTR_ONDRAG =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::ondrag, nsGkAtoms::ondrag,
                               nsGkAtoms::ondrag, ALL_NO_PREFIX);
  ATTR_ONDRAGENTER = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondragenter, nsGkAtoms::ondragenter,
      nsGkAtoms::ondragenter, ALL_NO_PREFIX);
  ATTR_ONDRAGOVER = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondragover, nsGkAtoms::ondragover,
      nsGkAtoms::ondragover, ALL_NO_PREFIX);
  ATTR_ONDRAGEND = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondragend, nsGkAtoms::ondragend,
      nsGkAtoms::ondragend, ALL_NO_PREFIX);
  ATTR_ONDROP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::ondrop, nsGkAtoms::ondrop,
                               nsGkAtoms::ondrop, ALL_NO_PREFIX);
  ATTR_ONDRAGDROP = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondragdrop, nsGkAtoms::ondragdrop,
      nsGkAtoms::ondragdrop, ALL_NO_PREFIX);
  ATTR_ONERROR = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onerror,
                                          nsGkAtoms::onerror,
                                          nsGkAtoms::onerror, ALL_NO_PREFIX);
  ATTR_OPERATOR = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::_operator,
                                           nsGkAtoms::_operator,
                                           nsGkAtoms::_operator, ALL_NO_PREFIX);
  ATTR_OVERFLOW = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::overflow,
                                           nsGkAtoms::overflow,
                                           nsGkAtoms::overflow, ALL_NO_PREFIX);
  ATTR_ONDRAGSTART = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondragstart, nsGkAtoms::ondragstart,
      nsGkAtoms::ondragstart, ALL_NO_PREFIX);
  ATTR_ONDRAGLEAVE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::ondragleave, nsGkAtoms::ondragleave,
      nsGkAtoms::ondragleave, ALL_NO_PREFIX);
  ATTR_STARTOFFSET = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::startoffset, nsGkAtoms::startoffset,
      nsGkAtoms::startOffset, ALL_NO_PREFIX);
  ATTR_START =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::start, nsGkAtoms::start,
                               nsGkAtoms::start, ALL_NO_PREFIX);
  ATTR_AS = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::as, nsGkAtoms::as,
                                     nsGkAtoms::as, ALL_NO_PREFIX);
  ATTR_AXIS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::axis, nsGkAtoms::axis,
                               nsGkAtoms::axis, ALL_NO_PREFIX);
  ATTR_BIAS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::bias, nsGkAtoms::bias,
                               nsGkAtoms::bias, ALL_NO_PREFIX);
  ATTR_COLSPAN = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::colspan,
                                          nsGkAtoms::colspan,
                                          nsGkAtoms::colspan, ALL_NO_PREFIX);
  ATTR_CLASSID = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::classid,
                                          nsGkAtoms::classid,
                                          nsGkAtoms::classid, ALL_NO_PREFIX);
  ATTR_CROSSORIGIN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::crossorigin, nsGkAtoms::crossorigin,
      nsGkAtoms::crossorigin, ALL_NO_PREFIX);
  ATTR_COLS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::cols, nsGkAtoms::cols,
                               nsGkAtoms::cols, ALL_NO_PREFIX);
  ATTR_CURSOR =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::cursor, nsGkAtoms::cursor,
                               nsGkAtoms::cursor, ALL_NO_PREFIX);
  ATTR_CLOSURE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::closure_,
                                          nsGkAtoms::closure_,
                                          nsGkAtoms::closure_, ALL_NO_PREFIX);
  ATTR_CLOSE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::close, nsGkAtoms::close,
                               nsGkAtoms::close, ALL_NO_PREFIX);
  ATTR_CLASS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::_class, nsGkAtoms::_class,
                               nsGkAtoms::_class, ALL_NO_PREFIX);
  ATTR_IS = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::is, nsGkAtoms::is,
                                     nsGkAtoms::is, ALL_NO_PREFIX);
  ATTR_KEYSYSTEM = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::keysystem, nsGkAtoms::keysystem,
      nsGkAtoms::keysystem, ALL_NO_PREFIX);
  ATTR_KEYSPLINES = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::keysplines, nsGkAtoms::keysplines,
      nsGkAtoms::keySplines, ALL_NO_PREFIX);
  ATTR_LOWSRC =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::lowsrc, nsGkAtoms::lowsrc,
                               nsGkAtoms::lowsrc, ALL_NO_PREFIX);
  ATTR_MAXSIZE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::maxsize_,
                                          nsGkAtoms::maxsize_,
                                          nsGkAtoms::maxsize_, ALL_NO_PREFIX);
  ATTR_MINSIZE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::minsize_,
                                          nsGkAtoms::minsize_,
                                          nsGkAtoms::minsize_, ALL_NO_PREFIX);
  ATTR_OFFSET =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::offset, nsGkAtoms::offset,
                               nsGkAtoms::offset, ALL_NO_PREFIX);
  ATTR_PRESERVEALPHA = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::preservealpha, nsGkAtoms::preservealpha,
      nsGkAtoms::preserveAlpha, ALL_NO_PREFIX);
  ATTR_PRESERVEASPECTRATIO = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::preserveaspectratio, nsGkAtoms::preserveaspectratio,
      nsGkAtoms::preserveAspectRatio, ALL_NO_PREFIX);
  ATTR_ROWSPAN = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rowspan,
                                          nsGkAtoms::rowspan,
                                          nsGkAtoms::rowspan, ALL_NO_PREFIX);
  ATTR_ROWSPACING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::rowspacing_, nsGkAtoms::rowspacing_,
      nsGkAtoms::rowspacing_, ALL_NO_PREFIX);
  ATTR_ROWS =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rows, nsGkAtoms::rows,
                               nsGkAtoms::rows, ALL_NO_PREFIX);
  ATTR_SRCSET =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::srcset, nsGkAtoms::srcset,
                               nsGkAtoms::srcset, ALL_NO_PREFIX);
  ATTR_SUBSCRIPTSHIFT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::subscriptshift_, nsGkAtoms::subscriptshift_,
      nsGkAtoms::subscriptshift_, ALL_NO_PREFIX);
  ATTR_VERSION = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::version,
                                          nsGkAtoms::version,
                                          nsGkAtoms::version, ALL_NO_PREFIX);
  ATTR_ALTTEXT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::alttext,
                                          nsGkAtoms::alttext,
                                          nsGkAtoms::alttext, ALL_NO_PREFIX);
  ATTR_CONTENTEDITABLE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::contenteditable, nsGkAtoms::contenteditable,
      nsGkAtoms::contenteditable, ALL_NO_PREFIX);
  ATTR_CONTROLS = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::controls,
                                           nsGkAtoms::controls,
                                           nsGkAtoms::controls, ALL_NO_PREFIX);
  ATTR_CONTENT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::content,
                                          nsGkAtoms::content,
                                          nsGkAtoms::content, ALL_NO_PREFIX);
  ATTR_CONTEXTMENU = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::contextmenu, nsGkAtoms::contextmenu,
      nsGkAtoms::contextmenu, ALL_NO_PREFIX);
  ATTR_DEPTH =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::depth_, nsGkAtoms::depth_,
                               nsGkAtoms::depth_, ALL_NO_PREFIX);
  ATTR_ENCTYPE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::enctype,
                                          nsGkAtoms::enctype,
                                          nsGkAtoms::enctype, ALL_NO_PREFIX);
  ATTR_FONT_STRETCH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::font_stretch, nsGkAtoms::font_stretch,
      nsGkAtoms::font_stretch, ALL_NO_PREFIX);
  ATTR_FILTER =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::filter, nsGkAtoms::filter,
                               nsGkAtoms::filter, ALL_NO_PREFIX);
  ATTR_FONTWEIGHT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::fontweight_, nsGkAtoms::fontweight_,
      nsGkAtoms::fontweight_, ALL_NO_PREFIX);
  ATTR_FONT_WEIGHT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::fontWeight, nsGkAtoms::fontWeight,
      nsGkAtoms::fontWeight, ALL_NO_PREFIX);
  ATTR_FONTSTYLE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::fontstyle_, nsGkAtoms::fontstyle_,
      nsGkAtoms::fontstyle_, ALL_NO_PREFIX);
  ATTR_FONT_STYLE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::font_style, nsGkAtoms::font_style,
      nsGkAtoms::font_style, ALL_NO_PREFIX);
  ATTR_FONTFAMILY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::fontfamily_, nsGkAtoms::fontfamily_,
      nsGkAtoms::fontfamily_, ALL_NO_PREFIX);
  ATTR_FONT_FAMILY = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::font_family, nsGkAtoms::font_family,
      nsGkAtoms::font_family, ALL_NO_PREFIX);
  ATTR_FONT_VARIANT = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::font_variant, nsGkAtoms::font_variant,
      nsGkAtoms::font_variant, ALL_NO_PREFIX);
  ATTR_FONT_SIZE_ADJUST = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::font_size_adjust, nsGkAtoms::font_size_adjust,
      nsGkAtoms::font_size_adjust, ALL_NO_PREFIX);
  ATTR_FILTERUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::filterunits, nsGkAtoms::filterunits,
      nsGkAtoms::filterUnits, ALL_NO_PREFIX);
  ATTR_FONTSIZE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::fontsize_,
                                           nsGkAtoms::fontsize_,
                                           nsGkAtoms::fontsize_, ALL_NO_PREFIX);
  ATTR_FONT_SIZE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::font_size, nsGkAtoms::font_size,
      nsGkAtoms::font_size, ALL_NO_PREFIX);
  ATTR_KEYTIMES = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::keytimes,
                                           nsGkAtoms::keytimes,
                                           nsGkAtoms::keyTimes, ALL_NO_PREFIX);
  ATTR_LETTER_SPACING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::letter_spacing, nsGkAtoms::letter_spacing,
      nsGkAtoms::letter_spacing, ALL_NO_PREFIX);
  ATTR_LIST =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::list_, nsGkAtoms::list_,
                               nsGkAtoms::list_, ALL_NO_PREFIX);
  ATTR_MULTIPLE = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::multiple,
                                           nsGkAtoms::multiple,
                                           nsGkAtoms::multiple, ALL_NO_PREFIX);
  ATTR_RT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rt, nsGkAtoms::rt,
                                     nsGkAtoms::rt, ALL_NO_PREFIX);
  ATTR_ONSTOP =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onstop, nsGkAtoms::onstop,
                               nsGkAtoms::onstop, ALL_NO_PREFIX);
  ATTR_ONSTART = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onstart,
                                          nsGkAtoms::onstart,
                                          nsGkAtoms::onstart, ALL_NO_PREFIX);
  ATTR_POSTER =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::poster, nsGkAtoms::poster,
                               nsGkAtoms::poster, ALL_NO_PREFIX);
  ATTR_PATTERNTRANSFORM = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::patterntransform, nsGkAtoms::patterntransform,
      nsGkAtoms::patternTransform, ALL_NO_PREFIX);
  ATTR_PATTERN = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::pattern,
                                          nsGkAtoms::pattern,
                                          nsGkAtoms::pattern, ALL_NO_PREFIX);
  ATTR_PATTERNUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::patternunits, nsGkAtoms::patternunits,
      nsGkAtoms::patternUnits, ALL_NO_PREFIX);
  ATTR_PATTERNCONTENTUNITS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::patterncontentunits, nsGkAtoms::patterncontentunits,
      nsGkAtoms::patternContentUnits, ALL_NO_PREFIX);
  ATTR_RESTART = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::restart,
                                          nsGkAtoms::restart,
                                          nsGkAtoms::restart, ALL_NO_PREFIX);
  ATTR_STITCHTILES = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::stitchtiles, nsGkAtoms::stitchtiles,
      nsGkAtoms::stitchTiles, ALL_NO_PREFIX);
  ATTR_SYSTEMLANGUAGE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::systemlanguage, nsGkAtoms::systemlanguage,
      nsGkAtoms::systemLanguage, ALL_NO_PREFIX);
  ATTR_TEXT_RENDERING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::text_rendering, nsGkAtoms::text_rendering,
      nsGkAtoms::text_rendering, ALL_NO_PREFIX);
  ATTR_TEXT_DECORATION = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::text_decoration, nsGkAtoms::text_decoration,
      nsGkAtoms::text_decoration, ALL_NO_PREFIX);
  ATTR_TEXT_ANCHOR = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::text_anchor, nsGkAtoms::text_anchor,
      nsGkAtoms::text_anchor, ALL_NO_PREFIX);
  ATTR_TEXTLENGTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::textlength, nsGkAtoms::textlength,
      nsGkAtoms::textLength, ALL_NO_PREFIX);
  ATTR_TEXT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::text, nsGkAtoms::text,
                               nsGkAtoms::text, ALL_NO_PREFIX);
  ATTR_WRITING_MODE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::writing_mode, nsGkAtoms::writing_mode,
      nsGkAtoms::writing_mode, ALL_NO_PREFIX);
  ATTR_WIDTH =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::width, nsGkAtoms::width,
                               nsGkAtoms::width, ALL_NO_PREFIX);
  ATTR_ACCUMULATE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::accumulate, nsGkAtoms::accumulate,
      nsGkAtoms::accumulate, ALL_NO_PREFIX);
  ATTR_COLUMNSPAN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::columnspan_, nsGkAtoms::columnspan_,
      nsGkAtoms::columnspan_, ALL_NO_PREFIX);
  ATTR_COLUMNLINES = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::columnlines_, nsGkAtoms::columnlines_,
      nsGkAtoms::columnlines_, ALL_NO_PREFIX);
  ATTR_COLUMNALIGN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::columnalign_, nsGkAtoms::columnalign_,
      nsGkAtoms::columnalign_, ALL_NO_PREFIX);
  ATTR_COLUMNSPACING = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::columnspacing_, nsGkAtoms::columnspacing_,
      nsGkAtoms::columnspacing_, ALL_NO_PREFIX);
  ATTR_COLUMNWIDTH = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::columnwidth_, nsGkAtoms::columnwidth_,
      nsGkAtoms::columnwidth_, ALL_NO_PREFIX);
  ATTR_GROUPALIGN = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::groupalign_, nsGkAtoms::groupalign_,
      nsGkAtoms::groupalign_, ALL_NO_PREFIX);
  ATTR_INPUTMODE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::inputmode, nsGkAtoms::inputmode,
      nsGkAtoms::inputmode, ALL_NO_PREFIX);
  ATTR_ONSUBMIT = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::onsubmit,
                                           nsGkAtoms::onsubmit,
                                           nsGkAtoms::onsubmit, ALL_NO_PREFIX);
  ATTR_ONCUT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::oncut, nsGkAtoms::oncut,
                               nsGkAtoms::oncut, ALL_NO_PREFIX);
  ATTR_REQUIRED = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::required,
                                           nsGkAtoms::required,
                                           nsGkAtoms::required, ALL_NO_PREFIX);
  ATTR_REQUIREDFEATURES = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::requiredfeatures, nsGkAtoms::requiredfeatures,
      nsGkAtoms::requiredFeatures, ALL_NO_PREFIX);
  ATTR_RESULT =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::result, nsGkAtoms::result,
                               nsGkAtoms::result, ALL_NO_PREFIX);
  ATTR_REQUIREDEXTENSIONS = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::requiredextensions, nsGkAtoms::requiredextensions,
      nsGkAtoms::requiredExtensions, ALL_NO_PREFIX);
  ATTR_VALUES =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::values, nsGkAtoms::values,
                               nsGkAtoms::values, ALL_NO_PREFIX);
  ATTR_VALUETYPE = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::valuetype, nsGkAtoms::valuetype,
      nsGkAtoms::valuetype, ALL_NO_PREFIX);
  ATTR_VALUE =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::value, nsGkAtoms::value,
                               nsGkAtoms::value, ALL_NO_PREFIX);
  ATTR_ELEVATION = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::elevation, nsGkAtoms::elevation,
      nsGkAtoms::elevation, ALL_NO_PREFIX);
  ATTR_VIEWTARGET = new nsHtml5AttributeName(
      ALL_NO_NS, nsGkAtoms::viewtarget, nsGkAtoms::viewtarget,
      nsGkAtoms::viewTarget, ALL_NO_PREFIX);
  ATTR_VIEWBOX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::viewbox,
                                          nsGkAtoms::viewbox,
                                          nsGkAtoms::viewBox, ALL_NO_PREFIX);
  ATTR_CX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::cx, nsGkAtoms::cx,
                                     nsGkAtoms::cx, ALL_NO_PREFIX);
  ATTR_DX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::dx, nsGkAtoms::dx,
                                     nsGkAtoms::dx, ALL_NO_PREFIX);
  ATTR_FX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::fx, nsGkAtoms::fx,
                                     nsGkAtoms::fx, ALL_NO_PREFIX);
  ATTR_RX = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::rx, nsGkAtoms::rx,
                                     nsGkAtoms::rx, ALL_NO_PREFIX);
  ATTR_REFX =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::refx, nsGkAtoms::refx,
                               nsGkAtoms::refX, ALL_NO_PREFIX);
  ATTR_BY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::by, nsGkAtoms::by,
                                     nsGkAtoms::by, ALL_NO_PREFIX);
  ATTR_CY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::cy, nsGkAtoms::cy,
                                     nsGkAtoms::cy, ALL_NO_PREFIX);
  ATTR_DY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::dy, nsGkAtoms::dy,
                                     nsGkAtoms::dy, ALL_NO_PREFIX);
  ATTR_FY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::fy, nsGkAtoms::fy,
                                     nsGkAtoms::fy, ALL_NO_PREFIX);
  ATTR_RY = new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::ry, nsGkAtoms::ry,
                                     nsGkAtoms::ry, ALL_NO_PREFIX);
  ATTR_REFY =
      new nsHtml5AttributeName(ALL_NO_NS, nsGkAtoms::refy, nsGkAtoms::refy,
                               nsGkAtoms::refY, ALL_NO_PREFIX);
  ATTRIBUTE_NAMES = new nsHtml5AttributeName*[503];
  ATTRIBUTE_NAMES[0] = ATTR_CELLSPACING;
  ATTRIBUTE_NAMES[1] = ATTR_CODETYPE;
  ATTRIBUTE_NAMES[2] = ATTR_ATTRIBUTENAME;
  ATTRIBUTE_NAMES[3] = ATTR_CLEAR;
  ATTRIBUTE_NAMES[4] = ATTR_ALIGNMENTSCOPE;
  ATTRIBUTE_NAMES[5] = ATTR_BGCOLOR;
  ATTRIBUTE_NAMES[6] = ATTR_FILTERUNITS;
  ATTRIBUTE_NAMES[7] = ATTR_ARIA_DISABLED;
  ATTRIBUTE_NAMES[8] = ATTR_OPACITY;
  ATTRIBUTE_NAMES[9] = ATTR_ONBEFORECOPY;
  ATTRIBUTE_NAMES[10] = ATTR_ACTION;
  ATTRIBUTE_NAMES[11] = ATTR_KERNELMATRIX;
  ATTRIBUTE_NAMES[12] = ATTR_STROKE_DASHOFFSET;
  ATTRIBUTE_NAMES[13] = ATTR_IS;
  ATTRIBUTE_NAMES[14] = ATTR_INPUTMODE;
  ATTRIBUTE_NAMES[15] = ATTR_Y;
  ATTRIBUTE_NAMES[16] = ATTR_ARIA_MULTISELECTABLE;
  ATTRIBUTE_NAMES[17] = ATTR_ROTATE;
  ATTRIBUTE_NAMES[18] = ATTR_STDDEVIATION;
  ATTRIBUTE_NAMES[19] = ATTR_MODE;
  ATTRIBUTE_NAMES[20] = ATTR_SUPERSCRIPTSHIFT;
  ATTRIBUTE_NAMES[21] = ATTR_TARGETX;
  ATTRIBUTE_NAMES[22] = ATTR_SCRIPTMINSIZE;
  ATTRIBUTE_NAMES[23] = ATTR_FORMAT;
  ATTRIBUTE_NAMES[24] = ATTR_TRANSFORM;
  ATTRIBUTE_NAMES[25] = ATTR_ONMOUSEOVER;
  ATTRIBUTE_NAMES[26] = ATTR_GLYPHREF;
  ATTRIBUTE_NAMES[27] = ATTR_OVERFLOW;
  ATTRIBUTE_NAMES[28] = ATTR_CONTENTEDITABLE;
  ATTRIBUTE_NAMES[29] = ATTR_STITCHTILES;
  ATTRIBUTE_NAMES[30] = ATTR_RX;
  ATTRIBUTE_NAMES[31] = ATTR_MIN;
  ATTRIBUTE_NAMES[32] = ATTR_K3;
  ATTRIBUTE_NAMES[33] = ATTR_ARIA_CHANNEL;
  ATTRIBUTE_NAMES[34] = ATTR_ARIA_VALUENOW;
  ATTRIBUTE_NAMES[35] = ATTR_LOCAL;
  ATTRIBUTE_NAMES[36] = ATTR_ONABORT;
  ATTRIBUTE_NAMES[37] = ATTR_HIDDEN;
  ATTRIBUTE_NAMES[38] = ATTR_ACCEPT;
  ATTRIBUTE_NAMES[39] = ATTR_ENTERKEYHINT;
  ATTRIBUTE_NAMES[40] = ATTR_OTHER;
  ATTRIBUTE_NAMES[41] = ATTR_REPEAT;
  ATTRIBUTE_NAMES[42] = ATTR_HREF;
  ATTRIBUTE_NAMES[43] = ATTR_LARGEOP;
  ATTRIBUTE_NAMES[44] = ATTR_MATHCOLOR;
  ATTRIBUTE_NAMES[45] = ATTR_MEDIA;
  ATTRIBUTE_NAMES[46] = ATTR_MARKER_END;
  ATTRIBUTE_NAMES[47] = ATTR_ONBLUR;
  ATTRIBUTE_NAMES[48] = ATTR_SYMMETRIC;
  ATTRIBUTE_NAMES[49] = ATTR_POINTER_EVENTS;
  ATTRIBUTE_NAMES[50] = ATTR_XMLNS;
  ATTRIBUTE_NAMES[51] = ATTR_FLOOD_COLOR;
  ATTRIBUTE_NAMES[52] = ATTR_ONFOCUS;
  ATTRIBUTE_NAMES[53] = ATTR_CLIP;
  ATTRIBUTE_NAMES[54] = ATTR_SCOPE;
  ATTRIBUTE_NAMES[55] = ATTR_ONDRAG;
  ATTRIBUTE_NAMES[56] = ATTR_COLSPAN;
  ATTRIBUTE_NAMES[57] = ATTR_PRESERVEASPECTRATIO;
  ATTRIBUTE_NAMES[58] = ATTR_FONTWEIGHT;
  ATTRIBUTE_NAMES[59] = ATTR_ONSTOP;
  ATTRIBUTE_NAMES[60] = ATTR_WIDTH;
  ATTRIBUTE_NAMES[61] = ATTR_VALUETYPE;
  ATTRIBUTE_NAMES[62] = ATTR_DY;
  ATTRIBUTE_NAMES[63] = ATTR_END;
  ATTRIBUTE_NAMES[64] = ATTR_SRC;
  ATTRIBUTE_NAMES[65] = ATTR_Y1;
  ATTRIBUTE_NAMES[66] = ATTR_ARIA_GRAB;
  ATTRIBUTE_NAMES[67] = ATTR_ARIA_REQUIRED;
  ATTRIBUTE_NAMES[68] = ATTR_ARIA_ATOMIC;
  ATTRIBUTE_NAMES[69] = ATTR_ARIA_OWNS;
  ATTRIBUTE_NAMES[70] = ATTR_ARIA_BUSY;
  ATTRIBUTE_NAMES[71] = ATTR_EQUALCOLUMNS;
  ATTRIBUTE_NAMES[72] = ATTR_ONDATAAVAILABLE;
  ATTRIBUTE_NAMES[73] = ATTR_XCHANNELSELECTOR;
  ATTRIBUTE_NAMES[74] = ATTR_FETCHPRIORITY;
  ATTRIBUTE_NAMES[75] = ATTR_BORDER;
  ATTRIBUTE_NAMES[76] = ATTR_RENDERING_INTENT;
  ATTRIBUTE_NAMES[77] = ATTR_ACCENTUNDER;
  ATTRIBUTE_NAMES[78] = ATTR_BASEPROFILE;
  ATTRIBUTE_NAMES[79] = ATTR_DATETIME;
  ATTRIBUTE_NAMES[80] = ATTR_INTEGRITY;
  ATTRIBUTE_NAMES[81] = ATTR_ONREPEAT;
  ATTRIBUTE_NAMES[82] = ATTR_ONBEGIN;
  ATTRIBUTE_NAMES[83] = ATTR_ONKEYUP;
  ATTRIBUTE_NAMES[84] = ATTR_REPEATCOUNT;
  ATTRIBUTE_NAMES[85] = ATTR_SELECTION;
  ATTRIBUTE_NAMES[86] = ATTR_SURFACESCALE;
  ATTRIBUTE_NAMES[87] = ATTR_IMAGESRCSET;
  ATTRIBUTE_NAMES[88] = ATTR_MARGINWIDTH;
  ATTRIBUTE_NAMES[89] = ATTR_LIGHTING_COLOR;
  ATTRIBUTE_NAMES[90] = ATTR_PATHLENGTH;
  ATTRIBUTE_NAMES[91] = ATTR_DOMINANT_BASELINE;
  ATTRIBUTE_NAMES[92] = ATTR_RADIOGROUP;
  ATTRIBUTE_NAMES[93] = ATTR_BACKGROUND;
  ATTRIBUTE_NAMES[94] = ATTR_MASKUNITS;
  ATTRIBUTE_NAMES[95] = ATTR_FILL;
  ATTRIBUTE_NAMES[96] = ATTR_STYLE;
  ATTRIBUTE_NAMES[97] = ATTR_FROM;
  ATTRIBUTE_NAMES[98] = ATTR_ASYNC;
  ATTRIBUTE_NAMES[99] = ATTR_OPEN;
  ATTRIBUTE_NAMES[100] = ATTR_POINTSATZ;
  ATTRIBUTE_NAMES[101] = ATTR_XLINK_TITLE;
  ATTRIBUTE_NAMES[102] = ATTR_AUTOPLAY;
  ATTRIBUTE_NAMES[103] = ATTR_COLOR;
  ATTRIBUTE_NAMES[104] = ATTR_NOMODULE;
  ATTRIBUTE_NAMES[105] = ATTR_ONCOPY;
  ATTRIBUTE_NAMES[106] = ATTR_TO;
  ATTRIBUTE_NAMES[107] = ATTR_SCROLLING;
  ATTRIBUTE_NAMES[108] = ATTR_DISPLAY;
  ATTRIBUTE_NAMES[109] = ATTR_PROPERTY;
  ATTRIBUTE_NAMES[110] = ATTR_STOP_OPACITY;
  ATTRIBUTE_NAMES[111] = ATTR_CHAROFF;
  ATTRIBUTE_NAMES[112] = ATTR_ONDROP;
  ATTRIBUTE_NAMES[113] = ATTR_START;
  ATTRIBUTE_NAMES[114] = ATTR_CURSOR;
  ATTRIBUTE_NAMES[115] = ATTR_MAXSIZE;
  ATTRIBUTE_NAMES[116] = ATTR_SRCSET;
  ATTRIBUTE_NAMES[117] = ATTR_DEPTH;
  ATTRIBUTE_NAMES[118] = ATTR_FONTFAMILY;
  ATTRIBUTE_NAMES[119] = ATTR_LETTER_SPACING;
  ATTRIBUTE_NAMES[120] = ATTR_PATTERN;
  ATTRIBUTE_NAMES[121] = ATTR_TEXT_ANCHOR;
  ATTRIBUTE_NAMES[122] = ATTR_COLUMNALIGN;
  ATTRIBUTE_NAMES[123] = ATTR_REQUIREDFEATURES;
  ATTRIBUTE_NAMES[124] = ATTR_VIEWBOX;
  ATTRIBUTE_NAMES[125] = ATTR_BY;
  ATTRIBUTE_NAMES[126] = ATTR_RY;
  ATTRIBUTE_NAMES[127] = ATTR_DIR;
  ATTRIBUTE_NAMES[128] = ATTR_IN2;
  ATTRIBUTE_NAMES[129] = ATTR_REL;
  ATTRIBUTE_NAMES[130] = ATTR_R;
  ATTRIBUTE_NAMES[131] = ATTR_K1;
  ATTRIBUTE_NAMES[132] = ATTR_X2;
  ATTRIBUTE_NAMES[133] = ATTR_XML_SPACE;
  ATTRIBUTE_NAMES[134] = ATTR_ARIA_LABELLEDBY;
  ATTRIBUTE_NAMES[135] = ATTR_ARIA_SELECTED;
  ATTRIBUTE_NAMES[136] = ATTR_ARIA_PRESSED;
  ATTRIBUTE_NAMES[137] = ATTR_ARIA_SECRET;
  ATTRIBUTE_NAMES[138] = ATTR_ARIA_TEMPLATEID;
  ATTRIBUTE_NAMES[139] = ATTR_ARIA_MULTILINE;
  ATTRIBUTE_NAMES[140] = ATTR_ARIA_RELEVANT;
  ATTRIBUTE_NAMES[141] = ATTR_ARIA_AUTOCOMPLETE;
  ATTRIBUTE_NAMES[142] = ATTR_ARIA_HASPOPUP;
  ATTRIBUTE_NAMES[143] = ATTR_DEFAULT;
  ATTRIBUTE_NAMES[144] = ATTR_HSPACE;
  ATTRIBUTE_NAMES[145] = ATTR_MOVABLELIMITS;
  ATTRIBUTE_NAMES[146] = ATTR_RSPACE;
  ATTRIBUTE_NAMES[147] = ATTR_SEPARATORS;
  ATTRIBUTE_NAMES[148] = ATTR_ENABLE_BACKGROUND;
  ATTRIBUTE_NAMES[149] = ATTR_CHECKED;
  ATTRIBUTE_NAMES[150] = ATTR_ONSCROLL;
  ATTRIBUTE_NAMES[151] = ATTR_SPECULAREXPONENT;
  ATTRIBUTE_NAMES[152] = ATTR_GRADIENTTRANSFORM;
  ATTRIBUTE_NAMES[153] = ATTR_LOADING;
  ATTRIBUTE_NAMES[154] = ATTR_SEED;
  ATTRIBUTE_NAMES[155] = ATTR_SHADOWROOTDELEGATESFOCUS;
  ATTRIBUTE_NAMES[156] = ATTR_ACCESSKEY;
  ATTRIBUTE_NAMES[157] = ATTR_BASEFREQUENCY;
  ATTRIBUTE_NAMES[158] = ATTR_BASE;
  ATTRIBUTE_NAMES[159] = ATTR_CITE;
  ATTRIBUTE_NAMES[160] = ATTR_EDGEMODE;
  ATTRIBUTE_NAMES[161] = ATTR_INDEX;
  ATTRIBUTE_NAMES[162] = ATTR_LABEL;
  ATTRIBUTE_NAMES[163] = ATTR_NORESIZE;
  ATTRIBUTE_NAMES[164] = ATTR_ONSELECT;
  ATTRIBUTE_NAMES[165] = ATTR_ONREADYSTATECHANGE;
  ATTRIBUTE_NAMES[166] = ATTR_ORIENT;
  ATTRIBUTE_NAMES[167] = ATTR_ONBEFOREPASTE;
  ATTRIBUTE_NAMES[168] = ATTR_ONKEYDOWN;
  ATTRIBUTE_NAMES[169] = ATTR_RULES;
  ATTRIBUTE_NAMES[170] = ATTR_SELECTED;
  ATTRIBUTE_NAMES[171] = ATTR_SCHEME;
  ATTRIBUTE_NAMES[172] = ATTR_TYPE;
  ATTRIBUTE_NAMES[173] = ATTR_ONAFTERPRINT;
  ATTRIBUTE_NAMES[174] = ATTR_ALIGN;
  ATTRIBUTE_NAMES[175] = ATTR_HEIGHT;
  ATTRIBUTE_NAMES[176] = ATTR_LANGUAGE;
  ATTRIBUTE_NAMES[177] = ATTR_LENGTHADJUST;
  ATTRIBUTE_NAMES[178] = ATTR_PING;
  ATTRIBUTE_NAMES[179] = ATTR_ARCHIVE;
  ATTRIBUTE_NAMES[180] = ATTR_METHOD;
  ATTRIBUTE_NAMES[181] = ATTR_NOSHADE;
  ATTRIBUTE_NAMES[182] = ATTR_ALTIMG;
  ATTRIBUTE_NAMES[183] = ATTR_ADDITIVE;
  ATTRIBUTE_NAMES[184] = ATTR_DEFINITIONURL;
  ATTRIBUTE_NAMES[185] = ATTR_ONFINISH;
  ATTRIBUTE_NAMES[186] = ATTR_SCRIPTLEVEL;
  ATTRIBUTE_NAMES[187] = ATTR_VALIGN;
  ATTRIBUTE_NAMES[188] = ATTR_MARKER_MID;
  ATTRIBUTE_NAMES[189] = ATTR_MARKER_START;
  ATTRIBUTE_NAMES[190] = ATTR_MASKCONTENTUNITS;
  ATTRIBUTE_NAMES[191] = ATTR_DECLARE;
  ATTRIBUTE_NAMES[192] = ATTR_MAXLENGTH;
  ATTRIBUTE_NAMES[193] = ATTR_ROWLINES;
  ATTRIBUTE_NAMES[194] = ATTR_TITLE;
  ATTRIBUTE_NAMES[195] = ATTR_FRAME;
  ATTRIBUTE_NAMES[196] = ATTR_PROMPT;
  ATTRIBUTE_NAMES[197] = ATTR_USEMAP;
  ATTRIBUTE_NAMES[198] = ATTR_IN;
  ATTRIBUTE_NAMES[199] = ATTR_KERNELUNITLENGTH;
  ATTRIBUTE_NAMES[200] = ATTR_ONEND;
  ATTRIBUTE_NAMES[201] = ATTR_POINTSATX;
  ATTRIBUTE_NAMES[202] = ATTR_STANDBY;
  ATTRIBUTE_NAMES[203] = ATTR_WHEN;
  ATTRIBUTE_NAMES[204] = ATTR_XLINK_ARCROLE;
  ATTRIBUTE_NAMES[205] = ATTR_XLINK_SHOW;
  ATTRIBUTE_NAMES[206] = ATTR_AUTOFOCUS;
  ATTRIBUTE_NAMES[207] = ATTR_COLOR_RENDERING;
  ATTRIBUTE_NAMES[208] = ATTR_ENCODING;
  ATTRIBUTE_NAMES[209] = ATTR_LQUOTE;
  ATTRIBUTE_NAMES[210] = ATTR_ONMOUSEWHEEL;
  ATTRIBUTE_NAMES[211] = ATTR_ONCONTEXTMENU;
  ATTRIBUTE_NAMES[212] = ATTR_ONMOUSEMOVE;
  ATTRIBUTE_NAMES[213] = ATTR_ONFOCUSOUT;
  ATTRIBUTE_NAMES[214] = ATTR_STROKE_LINECAP;
  ATTRIBUTE_NAMES[215] = ATTR_STROKE_MITERLIMIT;
  ATTRIBUTE_NAMES[216] = ATTR_STROKE_OPACITY;
  ATTRIBUTE_NAMES[217] = ATTR_CLIP_PATH;
  ATTRIBUTE_NAMES[218] = ATTR_GLYPH_ORIENTATION_VERTICAL;
  ATTRIBUTE_NAMES[219] = ATTR_KEYPOINTS;
  ATTRIBUTE_NAMES[220] = ATTR_STEP;
  ATTRIBUTE_NAMES[221] = ATTR_SLOPE;
  ATTRIBUTE_NAMES[222] = ATTR_WRAP;
  ATTRIBUTE_NAMES[223] = ATTR_CHAR;
  ATTRIBUTE_NAMES[224] = ATTR_NOWRAP;
  ATTRIBUTE_NAMES[225] = ATTR_ONDRAGOVER;
  ATTRIBUTE_NAMES[226] = ATTR_ONERROR;
  ATTRIBUTE_NAMES[227] = ATTR_ONDRAGLEAVE;
  ATTRIBUTE_NAMES[228] = ATTR_AXIS;
  ATTRIBUTE_NAMES[229] = ATTR_CROSSORIGIN;
  ATTRIBUTE_NAMES[230] = ATTR_CLOSE;
  ATTRIBUTE_NAMES[231] = ATTR_KEYSPLINES;
  ATTRIBUTE_NAMES[232] = ATTR_OFFSET;
  ATTRIBUTE_NAMES[233] = ATTR_ROWSPACING;
  ATTRIBUTE_NAMES[234] = ATTR_VERSION;
  ATTRIBUTE_NAMES[235] = ATTR_CONTENT;
  ATTRIBUTE_NAMES[236] = ATTR_FONT_STRETCH;
  ATTRIBUTE_NAMES[237] = ATTR_FONTSTYLE;
  ATTRIBUTE_NAMES[238] = ATTR_FONT_VARIANT;
  ATTRIBUTE_NAMES[239] = ATTR_FONT_SIZE;
  ATTRIBUTE_NAMES[240] = ATTR_MULTIPLE;
  ATTRIBUTE_NAMES[241] = ATTR_POSTER;
  ATTRIBUTE_NAMES[242] = ATTR_PATTERNCONTENTUNITS;
  ATTRIBUTE_NAMES[243] = ATTR_TEXT_RENDERING;
  ATTRIBUTE_NAMES[244] = ATTR_TEXT;
  ATTRIBUTE_NAMES[245] = ATTR_COLUMNSPAN;
  ATTRIBUTE_NAMES[246] = ATTR_COLUMNWIDTH;
  ATTRIBUTE_NAMES[247] = ATTR_ONCUT;
  ATTRIBUTE_NAMES[248] = ATTR_REQUIREDEXTENSIONS;
  ATTRIBUTE_NAMES[249] = ATTR_ELEVATION;
  ATTRIBUTE_NAMES[250] = ATTR_DX;
  ATTRIBUTE_NAMES[251] = ATTR_REFX;
  ATTRIBUTE_NAMES[252] = ATTR_CY;
  ATTRIBUTE_NAMES[253] = ATTR_FY;
  ATTRIBUTE_NAMES[254] = ATTR_REFY;
  ATTRIBUTE_NAMES[255] = ATTR_ALT;
  ATTRIBUTE_NAMES[256] = ATTR_DUR;
  ATTRIBUTE_NAMES[257] = ATTR_FOR;
  ATTRIBUTE_NAMES[258] = ATTR_LOW;
  ATTRIBUTE_NAMES[259] = ATTR_MAX;
  ATTRIBUTE_NAMES[260] = ATTR_REV;
  ATTRIBUTE_NAMES[261] = ATTR_D;
  ATTRIBUTE_NAMES[262] = ATTR_X;
  ATTRIBUTE_NAMES[263] = ATTR_Z;
  ATTRIBUTE_NAMES[264] = ATTR_X1;
  ATTRIBUTE_NAMES[265] = ATTR_K2;
  ATTRIBUTE_NAMES[266] = ATTR_Y2;
  ATTRIBUTE_NAMES[267] = ATTR_K4;
  ATTRIBUTE_NAMES[268] = ATTR_XML_LANG;
  ATTRIBUTE_NAMES[269] = ATTR_ARIA_VALUEMAX;
  ATTRIBUTE_NAMES[270] = ATTR_ARIA_DESCRIBEDBY;
  ATTRIBUTE_NAMES[271] = ATTR_ARIA_CHECKED;
  ATTRIBUTE_NAMES[272] = ATTR_ARIA_DROPEFFECT;
  ATTRIBUTE_NAMES[273] = ATTR_ARIA_EXPANDED;
  ATTRIBUTE_NAMES[274] = ATTR_ARIA_LEVEL;
  ATTRIBUTE_NAMES[275] = ATTR_ARIA_HIDDEN;
  ATTRIBUTE_NAMES[276] = ATTR_ARIA_POSINSET;
  ATTRIBUTE_NAMES[277] = ATTR_ARIA_INVALID;
  ATTRIBUTE_NAMES[278] = ATTR_ARIA_VALUEMIN;
  ATTRIBUTE_NAMES[279] = ATTR_ARIA_CONTROLS;
  ATTRIBUTE_NAMES[280] = ATTR_ARIA_READONLY;
  ATTRIBUTE_NAMES[281] = ATTR_ARIA_ACTIVEDESCENDANT;
  ATTRIBUTE_NAMES[282] = ATTR_ARIA_DATATYPE;
  ATTRIBUTE_NAMES[283] = ATTR_ARIA_SORT;
  ATTRIBUTE_NAMES[284] = ATTR_ARIA_FLOWTO;
  ATTRIBUTE_NAMES[285] = ATTR_ARIA_LIVE;
  ATTRIBUTE_NAMES[286] = ATTR_ARIA_SETSIZE;
  ATTRIBUTE_NAMES[287] = ATTR_DISABLED;
  ATTRIBUTE_NAMES[288] = ATTR_DATA;
  ATTRIBUTE_NAMES[289] = ATTR_EQUALROWS;
  ATTRIBUTE_NAMES[290] = ATTR_ISMAP;
  ATTRIBUTE_NAMES[291] = ATTR_LSPACE;
  ATTRIBUTE_NAMES[292] = ATTR_NOTATION;
  ATTRIBUTE_NAMES[293] = ATTR_ONPASTE;
  ATTRIBUTE_NAMES[294] = ATTR_ROWALIGN;
  ATTRIBUTE_NAMES[295] = ATTR_SEPARATOR;
  ATTRIBUTE_NAMES[296] = ATTR_VSPACE;
  ATTRIBUTE_NAMES[297] = ATTR_YCHANNELSELECTOR;
  ATTRIBUTE_NAMES[298] = ATTR_ONDBLCLICK;
  ATTRIBUTE_NAMES[299] = ATTR_CALCMODE;
  ATTRIBUTE_NAMES[300] = ATTR_FENCE;
  ATTRIBUTE_NAMES[301] = ATTR_NONCE;
  ATTRIBUTE_NAMES[302] = ATTR_ONACTIVATE;
  ATTRIBUTE_NAMES[303] = ATTR_SPACING;
  ATTRIBUTE_NAMES[304] = ATTR_SPECULARCONSTANT;
  ATTRIBUTE_NAMES[305] = ATTR_ID;
  ATTRIBUTE_NAMES[306] = ATTR_GRADIENTUNITS;
  ATTRIBUTE_NAMES[307] = ATTR_HEADERS;
  ATTRIBUTE_NAMES[308] = ATTR_READONLY;
  ATTRIBUTE_NAMES[309] = ATTR_SHADOWROOTMODE;
  ATTRIBUTE_NAMES[310] = ATTR_SRCDOC;
  ATTRIBUTE_NAMES[311] = ATTR_SANDBOX;
  ATTRIBUTE_NAMES[312] = ATTR_WORD_SPACING;
  ATTRIBUTE_NAMES[313] = ATTR_ACCEPT_CHARSET;
  ATTRIBUTE_NAMES[314] = ATTR_ACCENT;
  ATTRIBUTE_NAMES[315] = ATTR_BEVELLED;
  ATTRIBUTE_NAMES[316] = ATTR_BASELINE_SHIFT;
  ATTRIBUTE_NAMES[317] = ATTR_BASELINE;
  ATTRIBUTE_NAMES[318] = ATTR_CODE;
  ATTRIBUTE_NAMES[319] = ATTR_CODEBASE;
  ATTRIBUTE_NAMES[320] = ATTR_DEFER;
  ATTRIBUTE_NAMES[321] = ATTR_DIRECTION;
  ATTRIBUTE_NAMES[322] = ATTR_EDGE;
  ATTRIBUTE_NAMES[323] = ATTR_FACE;
  ATTRIBUTE_NAMES[324] = ATTR_INTERCEPT;
  ATTRIBUTE_NAMES[325] = ATTR_LINEBREAK;
  ATTRIBUTE_NAMES[326] = ATTR_LINETHICKNESS;
  ATTRIBUTE_NAMES[327] = ATTR_NAME;
  ATTRIBUTE_NAMES[328] = ATTR_ONBEFOREUNLOAD;
  ATTRIBUTE_NAMES[329] = ATTR_OBJECT;
  ATTRIBUTE_NAMES[330] = ATTR_ORDER;
  ATTRIBUTE_NAMES[331] = ATTR_ONRESET;
  ATTRIBUTE_NAMES[332] = ATTR_ONMESSAGE;
  ATTRIBUTE_NAMES[333] = ATTR_ONBEFOREPRINT;
  ATTRIBUTE_NAMES[334] = ATTR_ORIENTATION;
  ATTRIBUTE_NAMES[335] = ATTR_ONSELECTSTART;
  ATTRIBUTE_NAMES[336] = ATTR_ONKEYPRESS;
  ATTRIBUTE_NAMES[337] = ATTR_ONBEFORECUT;
  ATTRIBUTE_NAMES[338] = ATTR_ONRESIZE;
  ATTRIBUTE_NAMES[339] = ATTR_REFERRERPOLICY;
  ATTRIBUTE_NAMES[340] = ATTR_ROLE;
  ATTRIBUTE_NAMES[341] = ATTR_REPEATDUR;
  ATTRIBUTE_NAMES[342] = ATTR_SIZES;
  ATTRIBUTE_NAMES[343] = ATTR_STRETCHY;
  ATTRIBUTE_NAMES[344] = ATTR_SPREADMETHOD;
  ATTRIBUTE_NAMES[345] = ATTR_SIZE;
  ATTRIBUTE_NAMES[346] = ATTR_DIFFUSECONSTANT;
  ATTRIBUTE_NAMES[347] = ATTR_HREFLANG;
  ATTRIBUTE_NAMES[348] = ATTR_PROFILE;
  ATTRIBUTE_NAMES[349] = ATTR_XREF;
  ATTRIBUTE_NAMES[350] = ATTR_ALIGNMENT_BASELINE;
  ATTRIBUTE_NAMES[351] = ATTR_DRAGGABLE;
  ATTRIBUTE_NAMES[352] = ATTR_IMAGESIZES;
  ATTRIBUTE_NAMES[353] = ATTR_IMAGE_RENDERING;
  ATTRIBUTE_NAMES[354] = ATTR_LANG;
  ATTRIBUTE_NAMES[355] = ATTR_LONGDESC;
  ATTRIBUTE_NAMES[356] = ATTR_MARGINHEIGHT;
  ATTRIBUTE_NAMES[357] = ATTR_ORIGIN;
  ATTRIBUTE_NAMES[358] = ATTR_TARGET;
  ATTRIBUTE_NAMES[359] = ATTR_TARGETY;
  ATTRIBUTE_NAMES[360] = ATTR_HIGH;
  ATTRIBUTE_NAMES[361] = ATTR_MATHBACKGROUND;
  ATTRIBUTE_NAMES[362] = ATTR_MATHVARIANT;
  ATTRIBUTE_NAMES[363] = ATTR_MATHSIZE;
  ATTRIBUTE_NAMES[364] = ATTR_ONCHANGE;
  ATTRIBUTE_NAMES[365] = ATTR_PATH;
  ATTRIBUTE_NAMES[366] = ATTR_ACTIONTYPE;
  ATTRIBUTE_NAMES[367] = ATTR_ACTIVE;
  ATTRIBUTE_NAMES[368] = ATTR_BEGIN;
  ATTRIBUTE_NAMES[369] = ATTR_DIVISOR;
  ATTRIBUTE_NAMES[370] = ATTR_LIMITINGCONEANGLE;
  ATTRIBUTE_NAMES[371] = ATTR_MANIFEST;
  ATTRIBUTE_NAMES[372] = ATTR_OPTIMUM;
  ATTRIBUTE_NAMES[373] = ATTR_RADIUS;
  ATTRIBUTE_NAMES[374] = ATTR_SCRIPTSIZEMULTIPLIER;
  ATTRIBUTE_NAMES[375] = ATTR_TABINDEX;
  ATTRIBUTE_NAMES[376] = ATTR_VISIBILITY;
  ATTRIBUTE_NAMES[377] = ATTR_LINK;
  ATTRIBUTE_NAMES[378] = ATTR_MARKERHEIGHT;
  ATTRIBUTE_NAMES[379] = ATTR_MASK;
  ATTRIBUTE_NAMES[380] = ATTR_MARKERWIDTH;
  ATTRIBUTE_NAMES[381] = ATTR_MARKERUNITS;
  ATTRIBUTE_NAMES[382] = ATTR_AMPLITUDE;
  ATTRIBUTE_NAMES[383] = ATTR_CELLPADDING;
  ATTRIBUTE_NAMES[384] = ATTR_FILL_RULE;
  ATTRIBUTE_NAMES[385] = ATTR_FILL_OPACITY;
  ATTRIBUTE_NAMES[386] = ATTR_ONCLICK;
  ATTRIBUTE_NAMES[387] = ATTR_REPLACE;
  ATTRIBUTE_NAMES[388] = ATTR_SCALE;
  ATTRIBUTE_NAMES[389] = ATTR_TABLEVALUES;
  ATTRIBUTE_NAMES[390] = ATTR_AZIMUTH;
  ATTRIBUTE_NAMES[391] = ATTR_FRAMEBORDER;
  ATTRIBUTE_NAMES[392] = ATTR_FRAMESPACING;
  ATTRIBUTE_NAMES[393] = ATTR_FORM;
  ATTRIBUTE_NAMES[394] = ATTR_PRIMITIVEUNITS;
  ATTRIBUTE_NAMES[395] = ATTR_SUMMARY;
  ATTRIBUTE_NAMES[396] = ATTR_ZOOMANDPAN;
  ATTRIBUTE_NAMES[397] = ATTR_ALINK;
  ATTRIBUTE_NAMES[398] = ATTR_ICON;
  ATTRIBUTE_NAMES[399] = ATTR_KERNING;
  ATTRIBUTE_NAMES[400] = ATTR_ONUNLOAD;
  ATTRIBUTE_NAMES[401] = ATTR_ONINVALID;
  ATTRIBUTE_NAMES[402] = ATTR_ONINPUT;
  ATTRIBUTE_NAMES[403] = ATTR_POINTS;
  ATTRIBUTE_NAMES[404] = ATTR_POINTSATY;
  ATTRIBUTE_NAMES[405] = ATTR_SPAN;
  ATTRIBUTE_NAMES[406] = ATTR_TRANSFORM_ORIGIN;
  ATTRIBUTE_NAMES[407] = ATTR_VLINK;
  ATTRIBUTE_NAMES[408] = ATTR_XLINK_HREF;
  ATTRIBUTE_NAMES[409] = ATTR_XLINK_ROLE;
  ATTRIBUTE_NAMES[410] = ATTR_XMLNS_XLINK;
  ATTRIBUTE_NAMES[411] = ATTR_XLINK_TYPE;
  ATTRIBUTE_NAMES[412] = ATTR_XLINK_ACTUATE;
  ATTRIBUTE_NAMES[413] = ATTR_AUTOCOMPLETE;
  ATTRIBUTE_NAMES[414] = ATTR_AUTOCAPITALIZE;
  ATTRIBUTE_NAMES[415] = ATTR_COLOR_PROFILE;
  ATTRIBUTE_NAMES[416] = ATTR_COLOR_INTERPOLATION;
  ATTRIBUTE_NAMES[417] = ATTR_COLOR_INTERPOLATION_FILTERS;
  ATTRIBUTE_NAMES[418] = ATTR_EXPONENT;
  ATTRIBUTE_NAMES[419] = ATTR_FLOOD_OPACITY;
  ATTRIBUTE_NAMES[420] = ATTR_NUMOCTAVES;
  ATTRIBUTE_NAMES[421] = ATTR_ONLOAD;
  ATTRIBUTE_NAMES[422] = ATTR_ONMOUSEENTER;
  ATTRIBUTE_NAMES[423] = ATTR_ONFOCUSIN;
  ATTRIBUTE_NAMES[424] = ATTR_ONZOOM;
  ATTRIBUTE_NAMES[425] = ATTR_ONMOUSELEAVE;
  ATTRIBUTE_NAMES[426] = ATTR_ONMOUSEUP;
  ATTRIBUTE_NAMES[427] = ATTR_ONMOUSEOUT;
  ATTRIBUTE_NAMES[428] = ATTR_ONMOUSEDOWN;
  ATTRIBUTE_NAMES[429] = ATTR_RQUOTE;
  ATTRIBUTE_NAMES[430] = ATTR_STROKE_DASHARRAY;
  ATTRIBUTE_NAMES[431] = ATTR_STROKE_LINEJOIN;
  ATTRIBUTE_NAMES[432] = ATTR_STROKE;
  ATTRIBUTE_NAMES[433] = ATTR_STROKE_WIDTH;
  ATTRIBUTE_NAMES[434] = ATTR_COMPACT;
  ATTRIBUTE_NAMES[435] = ATTR_CLIP_RULE;
  ATTRIBUTE_NAMES[436] = ATTR_CLIPPATHUNITS;
  ATTRIBUTE_NAMES[437] = ATTR_DISPLAYSTYLE;
  ATTRIBUTE_NAMES[438] = ATTR_GLYPH_ORIENTATION_HORIZONTAL;
  ATTRIBUTE_NAMES[439] = ATTR_HTTP_EQUIV;
  ATTRIBUTE_NAMES[440] = ATTR_LOOP;
  ATTRIBUTE_NAMES[441] = ATTR_SCOPED;
  ATTRIBUTE_NAMES[442] = ATTR_SHAPE_RENDERING;
  ATTRIBUTE_NAMES[443] = ATTR_SHAPE;
  ATTRIBUTE_NAMES[444] = ATTR_STOP_COLOR;
  ATTRIBUTE_NAMES[445] = ATTR_TEMPLATE;
  ATTRIBUTE_NAMES[446] = ATTR_ABBR;
  ATTRIBUTE_NAMES[447] = ATTR_ATTRIBUTETYPE;
  ATTRIBUTE_NAMES[448] = ATTR_COORDS;
  ATTRIBUTE_NAMES[449] = ATTR_CHARSET;
  ATTRIBUTE_NAMES[450] = ATTR_NOHREF;
  ATTRIBUTE_NAMES[451] = ATTR_ONDRAGENTER;
  ATTRIBUTE_NAMES[452] = ATTR_ONDRAGEND;
  ATTRIBUTE_NAMES[453] = ATTR_ONDRAGDROP;
  ATTRIBUTE_NAMES[454] = ATTR_OPERATOR;
  ATTRIBUTE_NAMES[455] = ATTR_ONDRAGSTART;
  ATTRIBUTE_NAMES[456] = ATTR_STARTOFFSET;
  ATTRIBUTE_NAMES[457] = ATTR_AS;
  ATTRIBUTE_NAMES[458] = ATTR_BIAS;
  ATTRIBUTE_NAMES[459] = ATTR_CLASSID;
  ATTRIBUTE_NAMES[460] = ATTR_COLS;
  ATTRIBUTE_NAMES[461] = ATTR_CLOSURE;
  ATTRIBUTE_NAMES[462] = ATTR_CLASS;
  ATTRIBUTE_NAMES[463] = ATTR_KEYSYSTEM;
  ATTRIBUTE_NAMES[464] = ATTR_LOWSRC;
  ATTRIBUTE_NAMES[465] = ATTR_MINSIZE;
  ATTRIBUTE_NAMES[466] = ATTR_PRESERVEALPHA;
  ATTRIBUTE_NAMES[467] = ATTR_ROWSPAN;
  ATTRIBUTE_NAMES[468] = ATTR_ROWS;
  ATTRIBUTE_NAMES[469] = ATTR_SUBSCRIPTSHIFT;
  ATTRIBUTE_NAMES[470] = ATTR_ALTTEXT;
  ATTRIBUTE_NAMES[471] = ATTR_CONTROLS;
  ATTRIBUTE_NAMES[472] = ATTR_CONTEXTMENU;
  ATTRIBUTE_NAMES[473] = ATTR_ENCTYPE;
  ATTRIBUTE_NAMES[474] = ATTR_FILTER;
  ATTRIBUTE_NAMES[475] = ATTR_FONT_WEIGHT;
  ATTRIBUTE_NAMES[476] = ATTR_FONT_STYLE;
  ATTRIBUTE_NAMES[477] = ATTR_FONT_FAMILY;
  ATTRIBUTE_NAMES[478] = ATTR_FONT_SIZE_ADJUST;
  ATTRIBUTE_NAMES[479] = ATTR_FONTSIZE;
  ATTRIBUTE_NAMES[480] = ATTR_KEYTIMES;
  ATTRIBUTE_NAMES[481] = ATTR_LIST;
  ATTRIBUTE_NAMES[482] = ATTR_RT;
  ATTRIBUTE_NAMES[483] = ATTR_ONSTART;
  ATTRIBUTE_NAMES[484] = ATTR_PATTERNTRANSFORM;
  ATTRIBUTE_NAMES[485] = ATTR_PATTERNUNITS;
  ATTRIBUTE_NAMES[486] = ATTR_RESTART;
  ATTRIBUTE_NAMES[487] = ATTR_SYSTEMLANGUAGE;
  ATTRIBUTE_NAMES[488] = ATTR_TEXT_DECORATION;
  ATTRIBUTE_NAMES[489] = ATTR_TEXTLENGTH;
  ATTRIBUTE_NAMES[490] = ATTR_WRITING_MODE;
  ATTRIBUTE_NAMES[491] = ATTR_ACCUMULATE;
  ATTRIBUTE_NAMES[492] = ATTR_COLUMNLINES;
  ATTRIBUTE_NAMES[493] = ATTR_COLUMNSPACING;
  ATTRIBUTE_NAMES[494] = ATTR_GROUPALIGN;
  ATTRIBUTE_NAMES[495] = ATTR_ONSUBMIT;
  ATTRIBUTE_NAMES[496] = ATTR_REQUIRED;
  ATTRIBUTE_NAMES[497] = ATTR_RESULT;
  ATTRIBUTE_NAMES[498] = ATTR_VALUES;
  ATTRIBUTE_NAMES[499] = ATTR_VALUE;
  ATTRIBUTE_NAMES[500] = ATTR_VIEWTARGET;
  ATTRIBUTE_NAMES[501] = ATTR_CX;
  ATTRIBUTE_NAMES[502] = ATTR_FX;
}

void nsHtml5AttributeName::releaseStatics() {
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
  delete ATTR_R;
  delete ATTR_X;
  delete ATTR_Y;
  delete ATTR_Z;
  delete ATTR_K1;
  delete ATTR_X1;
  delete ATTR_Y1;
  delete ATTR_K2;
  delete ATTR_X2;
  delete ATTR_Y2;
  delete ATTR_K3;
  delete ATTR_K4;
  delete ATTR_XML_SPACE;
  delete ATTR_XML_LANG;
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
  delete ATTR_DISABLED;
  delete ATTR_DEFAULT;
  delete ATTR_DATA;
  delete ATTR_EQUALCOLUMNS;
  delete ATTR_EQUALROWS;
  delete ATTR_HSPACE;
  delete ATTR_ISMAP;
  delete ATTR_LOCAL;
  delete ATTR_LSPACE;
  delete ATTR_MOVABLELIMITS;
  delete ATTR_NOTATION;
  delete ATTR_ONDATAAVAILABLE;
  delete ATTR_ONPASTE;
  delete ATTR_RSPACE;
  delete ATTR_ROWALIGN;
  delete ATTR_ROTATE;
  delete ATTR_SEPARATOR;
  delete ATTR_SEPARATORS;
  delete ATTR_VSPACE;
  delete ATTR_XCHANNELSELECTOR;
  delete ATTR_YCHANNELSELECTOR;
  delete ATTR_ENABLE_BACKGROUND;
  delete ATTR_ONDBLCLICK;
  delete ATTR_ONABORT;
  delete ATTR_CALCMODE;
  delete ATTR_CHECKED;
  delete ATTR_FENCE;
  delete ATTR_FETCHPRIORITY;
  delete ATTR_NONCE;
  delete ATTR_ONSCROLL;
  delete ATTR_ONACTIVATE;
  delete ATTR_OPACITY;
  delete ATTR_SPACING;
  delete ATTR_SPECULAREXPONENT;
  delete ATTR_SPECULARCONSTANT;
  delete ATTR_BORDER;
  delete ATTR_ID;
  delete ATTR_GRADIENTTRANSFORM;
  delete ATTR_GRADIENTUNITS;
  delete ATTR_HIDDEN;
  delete ATTR_HEADERS;
  delete ATTR_LOADING;
  delete ATTR_READONLY;
  delete ATTR_RENDERING_INTENT;
  delete ATTR_SHADOWROOTMODE;
  delete ATTR_SEED;
  delete ATTR_SRCDOC;
  delete ATTR_STDDEVIATION;
  delete ATTR_SANDBOX;
  delete ATTR_SHADOWROOTDELEGATESFOCUS;
  delete ATTR_WORD_SPACING;
  delete ATTR_ACCENTUNDER;
  delete ATTR_ACCEPT_CHARSET;
  delete ATTR_ACCESSKEY;
  delete ATTR_ACCENT;
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
  delete ATTR_ENTERKEYHINT;
  delete ATTR_FACE;
  delete ATTR_INDEX;
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
  delete ATTR_ONREADYSTATECHANGE;
  delete ATTR_ONMESSAGE;
  delete ATTR_ONBEGIN;
  delete ATTR_ONBEFOREPRINT;
  delete ATTR_ORIENT;
  delete ATTR_ORIENTATION;
  delete ATTR_ONBEFORECOPY;
  delete ATTR_ONSELECTSTART;
  delete ATTR_ONBEFOREPASTE;
  delete ATTR_ONKEYPRESS;
  delete ATTR_ONKEYUP;
  delete ATTR_ONBEFORECUT;
  delete ATTR_ONKEYDOWN;
  delete ATTR_ONRESIZE;
  delete ATTR_REPEAT;
  delete ATTR_REFERRERPOLICY;
  delete ATTR_RULES;
  delete ATTR_ROLE;
  delete ATTR_REPEATCOUNT;
  delete ATTR_REPEATDUR;
  delete ATTR_SELECTED;
  delete ATTR_SIZES;
  delete ATTR_SUPERSCRIPTSHIFT;
  delete ATTR_STRETCHY;
  delete ATTR_SCHEME;
  delete ATTR_SPREADMETHOD;
  delete ATTR_SELECTION;
  delete ATTR_SIZE;
  delete ATTR_TYPE;
  delete ATTR_DIFFUSECONSTANT;
  delete ATTR_HREF;
  delete ATTR_HREFLANG;
  delete ATTR_ONAFTERPRINT;
  delete ATTR_PROFILE;
  delete ATTR_SURFACESCALE;
  delete ATTR_XREF;
  delete ATTR_ALIGN;
  delete ATTR_ALIGNMENT_BASELINE;
  delete ATTR_ALIGNMENTSCOPE;
  delete ATTR_DRAGGABLE;
  delete ATTR_HEIGHT;
  delete ATTR_IMAGESIZES;
  delete ATTR_IMAGESRCSET;
  delete ATTR_IMAGE_RENDERING;
  delete ATTR_LANGUAGE;
  delete ATTR_LANG;
  delete ATTR_LARGEOP;
  delete ATTR_LONGDESC;
  delete ATTR_LENGTHADJUST;
  delete ATTR_MARGINHEIGHT;
  delete ATTR_MARGINWIDTH;
  delete ATTR_ORIGIN;
  delete ATTR_PING;
  delete ATTR_TARGET;
  delete ATTR_TARGETX;
  delete ATTR_TARGETY;
  delete ATTR_ARCHIVE;
  delete ATTR_HIGH;
  delete ATTR_LIGHTING_COLOR;
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
  delete ATTR_LIMITINGCONEANGLE;
  delete ATTR_MEDIA;
  delete ATTR_MANIFEST;
  delete ATTR_ONFINISH;
  delete ATTR_OPTIMUM;
  delete ATTR_RADIOGROUP;
  delete ATTR_RADIUS;
  delete ATTR_SCRIPTLEVEL;
  delete ATTR_SCRIPTSIZEMULTIPLIER;
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
  delete ATTR_TRANSFORM_ORIGIN;
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
  delete ATTR_AUTOCOMPLETE;
  delete ATTR_AUTOFOCUS;
  delete ATTR_AUTOCAPITALIZE;
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
  delete ATTR_LQUOTE;
  delete ATTR_NUMOCTAVES;
  delete ATTR_NOMODULE;
  delete ATTR_ONLOAD;
  delete ATTR_ONMOUSEWHEEL;
  delete ATTR_ONMOUSEENTER;
  delete ATTR_ONMOUSEOVER;
  delete ATTR_ONFOCUSIN;
  delete ATTR_ONCONTEXTMENU;
  delete ATTR_ONZOOM;
  delete ATTR_ONCOPY;
  delete ATTR_ONMOUSELEAVE;
  delete ATTR_ONMOUSEMOVE;
  delete ATTR_ONMOUSEUP;
  delete ATTR_ONFOCUS;
  delete ATTR_ONMOUSEOUT;
  delete ATTR_ONFOCUSOUT;
  delete ATTR_ONMOUSEDOWN;
  delete ATTR_TO;
  delete ATTR_RQUOTE;
  delete ATTR_STROKE_LINECAP;
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
  delete ATTR_NOWRAP;
  delete ATTR_NOHREF;
  delete ATTR_ONDRAG;
  delete ATTR_ONDRAGENTER;
  delete ATTR_ONDRAGOVER;
  delete ATTR_ONDRAGEND;
  delete ATTR_ONDROP;
  delete ATTR_ONDRAGDROP;
  delete ATTR_ONERROR;
  delete ATTR_OPERATOR;
  delete ATTR_OVERFLOW;
  delete ATTR_ONDRAGSTART;
  delete ATTR_ONDRAGLEAVE;
  delete ATTR_STARTOFFSET;
  delete ATTR_START;
  delete ATTR_AS;
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
  delete ATTR_IS;
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
  delete ATTR_TEXT_DECORATION;
  delete ATTR_TEXT_ANCHOR;
  delete ATTR_TEXTLENGTH;
  delete ATTR_TEXT;
  delete ATTR_WRITING_MODE;
  delete ATTR_WIDTH;
  delete ATTR_ACCUMULATE;
  delete ATTR_COLUMNSPAN;
  delete ATTR_COLUMNLINES;
  delete ATTR_COLUMNALIGN;
  delete ATTR_COLUMNSPACING;
  delete ATTR_COLUMNWIDTH;
  delete ATTR_GROUPALIGN;
  delete ATTR_INPUTMODE;
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
  delete ATTR_RX;
  delete ATTR_REFX;
  delete ATTR_BY;
  delete ATTR_CY;
  delete ATTR_DY;
  delete ATTR_FY;
  delete ATTR_RY;
  delete ATTR_REFY;
  delete[] ATTRIBUTE_NAMES;
}
