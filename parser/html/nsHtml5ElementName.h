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

#ifndef nsHtml5ElementName_h
#define nsHtml5ElementName_h

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

class nsHtml5StreamParser;

class nsHtml5Tokenizer;
class nsHtml5TreeBuilder;
class nsHtml5MetaScanner;
class nsHtml5AttributeName;
class nsHtml5HtmlAttributes;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;


class nsHtml5ElementName
{
  public:
    static nsHtml5ElementName* ELT_NULL_ELEMENT_NAME;
    nsIAtom* name;
    nsIAtom* camelCaseName;
    int32_t flags;
    inline int32_t getFlags()
    {
      return flags;
    }

    int32_t getGroup();
    bool isCustom();
    static nsHtml5ElementName* elementNameByBuffer(char16_t* buf, int32_t offset, int32_t length, nsHtml5AtomTable* interner);
  private:
    inline static uint32_t bufToHash(char16_t* buf, int32_t length)
    {
      uint32_t len = length;
      uint32_t first = buf[0];
      first <<= 19;
      uint32_t second = 1 << 23;
      uint32_t third = 0;
      uint32_t fourth = 0;
      uint32_t fifth = 0;
      if (length >= 4) {
        second = buf[length - 4];
        second <<= 4;
        third = buf[length - 3];
        third <<= 9;
        fourth = buf[length - 2];
        fourth <<= 14;
        fifth = buf[length - 1];
        fifth <<= 24;
      } else if (length == 3) {
        second = buf[1];
        second <<= 4;
        third = buf[2];
        third <<= 9;
      } else if (length == 2) {
        second = buf[1];
        second <<= 24;
      }
      return len + first + second + third + fourth + fifth;
    }

    nsHtml5ElementName(nsIAtom* name, nsIAtom* camelCaseName, int32_t flags);
  protected:
    explicit nsHtml5ElementName(nsIAtom* name);
  public:
    virtual void release();
    virtual ~nsHtml5ElementName();
    virtual nsHtml5ElementName* cloneElementName(nsHtml5AtomTable* interner);
    static nsHtml5ElementName* ELT_BIG;
    static nsHtml5ElementName* ELT_BDO;
    static nsHtml5ElementName* ELT_COL;
    static nsHtml5ElementName* ELT_DEL;
    static nsHtml5ElementName* ELT_DFN;
    static nsHtml5ElementName* ELT_DIR;
    static nsHtml5ElementName* ELT_DIV;
    static nsHtml5ElementName* ELT_IMG;
    static nsHtml5ElementName* ELT_INS;
    static nsHtml5ElementName* ELT_KBD;
    static nsHtml5ElementName* ELT_MAP;
    static nsHtml5ElementName* ELT_NAV;
    static nsHtml5ElementName* ELT_PRE;
    static nsHtml5ElementName* ELT_A;
    static nsHtml5ElementName* ELT_B;
    static nsHtml5ElementName* ELT_RTC;
    static nsHtml5ElementName* ELT_SUB;
    static nsHtml5ElementName* ELT_SVG;
    static nsHtml5ElementName* ELT_SUP;
    static nsHtml5ElementName* ELT_SET;
    static nsHtml5ElementName* ELT_USE;
    static nsHtml5ElementName* ELT_VAR;
    static nsHtml5ElementName* ELT_G;
    static nsHtml5ElementName* ELT_WBR;
    static nsHtml5ElementName* ELT_XMP;
    static nsHtml5ElementName* ELT_I;
    static nsHtml5ElementName* ELT_P;
    static nsHtml5ElementName* ELT_Q;
    static nsHtml5ElementName* ELT_S;
    static nsHtml5ElementName* ELT_U;
    static nsHtml5ElementName* ELT_H1;
    static nsHtml5ElementName* ELT_H2;
    static nsHtml5ElementName* ELT_H3;
    static nsHtml5ElementName* ELT_H4;
    static nsHtml5ElementName* ELT_H5;
    static nsHtml5ElementName* ELT_H6;
    static nsHtml5ElementName* ELT_AREA;
    static nsHtml5ElementName* ELT_FEFUNCA;
    static nsHtml5ElementName* ELT_METADATA;
    static nsHtml5ElementName* ELT_META;
    static nsHtml5ElementName* ELT_TEXTAREA;
    static nsHtml5ElementName* ELT_FEFUNCB;
    static nsHtml5ElementName* ELT_RB;
    static nsHtml5ElementName* ELT_DESC;
    static nsHtml5ElementName* ELT_DD;
    static nsHtml5ElementName* ELT_BGSOUND;
    static nsHtml5ElementName* ELT_EMBED;
    static nsHtml5ElementName* ELT_FEBLEND;
    static nsHtml5ElementName* ELT_FEFLOOD;
    static nsHtml5ElementName* ELT_HEAD;
    static nsHtml5ElementName* ELT_LEGEND;
    static nsHtml5ElementName* ELT_NOEMBED;
    static nsHtml5ElementName* ELT_TD;
    static nsHtml5ElementName* ELT_THEAD;
    static nsHtml5ElementName* ELT_ASIDE;
    static nsHtml5ElementName* ELT_ARTICLE;
    static nsHtml5ElementName* ELT_ANIMATE;
    static nsHtml5ElementName* ELT_BASE;
    static nsHtml5ElementName* ELT_BLOCKQUOTE;
    static nsHtml5ElementName* ELT_CODE;
    static nsHtml5ElementName* ELT_CIRCLE;
    static nsHtml5ElementName* ELT_CITE;
    static nsHtml5ElementName* ELT_ELLIPSE;
    static nsHtml5ElementName* ELT_FETURBULENCE;
    static nsHtml5ElementName* ELT_FEMERGENODE;
    static nsHtml5ElementName* ELT_FEIMAGE;
    static nsHtml5ElementName* ELT_FEMERGE;
    static nsHtml5ElementName* ELT_FETILE;
    static nsHtml5ElementName* ELT_FRAME;
    static nsHtml5ElementName* ELT_FIGURE;
    static nsHtml5ElementName* ELT_FECOMPOSITE;
    static nsHtml5ElementName* ELT_IMAGE;
    static nsHtml5ElementName* ELT_IFRAME;
    static nsHtml5ElementName* ELT_LINE;
    static nsHtml5ElementName* ELT_MARQUEE;
    static nsHtml5ElementName* ELT_POLYLINE;
    static nsHtml5ElementName* ELT_PICTURE;
    static nsHtml5ElementName* ELT_SOURCE;
    static nsHtml5ElementName* ELT_STRIKE;
    static nsHtml5ElementName* ELT_STYLE;
    static nsHtml5ElementName* ELT_TABLE;
    static nsHtml5ElementName* ELT_TITLE;
    static nsHtml5ElementName* ELT_TIME;
    static nsHtml5ElementName* ELT_TEMPLATE;
    static nsHtml5ElementName* ELT_ALTGLYPHDEF;
    static nsHtml5ElementName* ELT_GLYPHREF;
    static nsHtml5ElementName* ELT_DIALOG;
    static nsHtml5ElementName* ELT_FEFUNCG;
    static nsHtml5ElementName* ELT_FEDIFFUSELIGHTING;
    static nsHtml5ElementName* ELT_FESPECULARLIGHTING;
    static nsHtml5ElementName* ELT_LISTING;
    static nsHtml5ElementName* ELT_STRONG;
    static nsHtml5ElementName* ELT_ALTGLYPH;
    static nsHtml5ElementName* ELT_CLIPPATH;
    static nsHtml5ElementName* ELT_MGLYPH;
    static nsHtml5ElementName* ELT_MATH;
    static nsHtml5ElementName* ELT_MPATH;
    static nsHtml5ElementName* ELT_PATH;
    static nsHtml5ElementName* ELT_TH;
    static nsHtml5ElementName* ELT_SWITCH;
    static nsHtml5ElementName* ELT_TEXTPATH;
    static nsHtml5ElementName* ELT_LI;
    static nsHtml5ElementName* ELT_MI;
    static nsHtml5ElementName* ELT_LINK;
    static nsHtml5ElementName* ELT_MARK;
    static nsHtml5ElementName* ELT_MALIGNMARK;
    static nsHtml5ElementName* ELT_MASK;
    static nsHtml5ElementName* ELT_TRACK;
    static nsHtml5ElementName* ELT_DL;
    static nsHtml5ElementName* ELT_ANNOTATION_XML;
    static nsHtml5ElementName* ELT_HTML;
    static nsHtml5ElementName* ELT_OL;
    static nsHtml5ElementName* ELT_LABEL;
    static nsHtml5ElementName* ELT_UL;
    static nsHtml5ElementName* ELT_SMALL;
    static nsHtml5ElementName* ELT_SYMBOL;
    static nsHtml5ElementName* ELT_ALTGLYPHITEM;
    static nsHtml5ElementName* ELT_ANIMATETRANSFORM;
    static nsHtml5ElementName* ELT_ACRONYM;
    static nsHtml5ElementName* ELT_EM;
    static nsHtml5ElementName* ELT_FORM;
    static nsHtml5ElementName* ELT_MENUITEM;
    static nsHtml5ElementName* ELT_PARAM;
    static nsHtml5ElementName* ELT_ANIMATEMOTION;
    static nsHtml5ElementName* ELT_BUTTON;
    static nsHtml5ElementName* ELT_CAPTION;
    static nsHtml5ElementName* ELT_FIGCAPTION;
    static nsHtml5ElementName* ELT_MN;
    static nsHtml5ElementName* ELT_KEYGEN;
    static nsHtml5ElementName* ELT_MAIN;
    static nsHtml5ElementName* ELT_OPTION;
    static nsHtml5ElementName* ELT_POLYGON;
    static nsHtml5ElementName* ELT_PATTERN;
    static nsHtml5ElementName* ELT_SPAN;
    static nsHtml5ElementName* ELT_SECTION;
    static nsHtml5ElementName* ELT_TSPAN;
    static nsHtml5ElementName* ELT_AUDIO;
    static nsHtml5ElementName* ELT_MO;
    static nsHtml5ElementName* ELT_VIDEO;
    static nsHtml5ElementName* ELT_COLGROUP;
    static nsHtml5ElementName* ELT_FEDISPLACEMENTMAP;
    static nsHtml5ElementName* ELT_HGROUP;
    static nsHtml5ElementName* ELT_RP;
    static nsHtml5ElementName* ELT_OPTGROUP;
    static nsHtml5ElementName* ELT_SAMP;
    static nsHtml5ElementName* ELT_STOP;
    static nsHtml5ElementName* ELT_BR;
    static nsHtml5ElementName* ELT_ABBR;
    static nsHtml5ElementName* ELT_ANIMATECOLOR;
    static nsHtml5ElementName* ELT_CENTER;
    static nsHtml5ElementName* ELT_HR;
    static nsHtml5ElementName* ELT_FEFUNCR;
    static nsHtml5ElementName* ELT_FECOMPONENTTRANSFER;
    static nsHtml5ElementName* ELT_FILTER;
    static nsHtml5ElementName* ELT_FOOTER;
    static nsHtml5ElementName* ELT_FEGAUSSIANBLUR;
    static nsHtml5ElementName* ELT_HEADER;
    static nsHtml5ElementName* ELT_MARKER;
    static nsHtml5ElementName* ELT_METER;
    static nsHtml5ElementName* ELT_NOBR;
    static nsHtml5ElementName* ELT_TR;
    static nsHtml5ElementName* ELT_ADDRESS;
    static nsHtml5ElementName* ELT_CANVAS;
    static nsHtml5ElementName* ELT_DEFS;
    static nsHtml5ElementName* ELT_DETAILS;
    static nsHtml5ElementName* ELT_MS;
    static nsHtml5ElementName* ELT_NOFRAMES;
    static nsHtml5ElementName* ELT_PROGRESS;
    static nsHtml5ElementName* ELT_DT;
    static nsHtml5ElementName* ELT_APPLET;
    static nsHtml5ElementName* ELT_BASEFONT;
    static nsHtml5ElementName* ELT_FOREIGNOBJECT;
    static nsHtml5ElementName* ELT_FIELDSET;
    static nsHtml5ElementName* ELT_FRAMESET;
    static nsHtml5ElementName* ELT_FEOFFSET;
    static nsHtml5ElementName* ELT_FESPOTLIGHT;
    static nsHtml5ElementName* ELT_FEPOINTLIGHT;
    static nsHtml5ElementName* ELT_FEDISTANTLIGHT;
    static nsHtml5ElementName* ELT_FONT;
    static nsHtml5ElementName* ELT_INPUT;
    static nsHtml5ElementName* ELT_LINEARGRADIENT;
    static nsHtml5ElementName* ELT_MTEXT;
    static nsHtml5ElementName* ELT_NOSCRIPT;
    static nsHtml5ElementName* ELT_RT;
    static nsHtml5ElementName* ELT_OBJECT;
    static nsHtml5ElementName* ELT_OUTPUT;
    static nsHtml5ElementName* ELT_PLAINTEXT;
    static nsHtml5ElementName* ELT_TT;
    static nsHtml5ElementName* ELT_RECT;
    static nsHtml5ElementName* ELT_RADIALGRADIENT;
    static nsHtml5ElementName* ELT_SELECT;
    static nsHtml5ElementName* ELT_SCRIPT;
    static nsHtml5ElementName* ELT_TFOOT;
    static nsHtml5ElementName* ELT_TEXT;
    static nsHtml5ElementName* ELT_MENU;
    static nsHtml5ElementName* ELT_FEDROPSHADOW;
    static nsHtml5ElementName* ELT_VIEW;
    static nsHtml5ElementName* ELT_FECOLORMATRIX;
    static nsHtml5ElementName* ELT_FECONVOLVEMATRIX;
    static nsHtml5ElementName* ELT_ISINDEX;
    static nsHtml5ElementName* ELT_BODY;
    static nsHtml5ElementName* ELT_FEMORPHOLOGY;
    static nsHtml5ElementName* ELT_RUBY;
    static nsHtml5ElementName* ELT_SUMMARY;
    static nsHtml5ElementName* ELT_TBODY;

  private:
    static nsHtml5ElementName** ELEMENT_NAMES;
    static staticJArray<int32_t,int32_t> ELEMENT_HASHES;
  public:
    static void initializeStatics();
    static void releaseStatics();
};

#define NS_HTML5ELEMENT_NAME_GROUP_MASK 127
#define NS_HTML5ELEMENT_NAME_CUSTOM (1 << 30)
#define NS_HTML5ELEMENT_NAME_SPECIAL (1 << 29)
#define NS_HTML5ELEMENT_NAME_FOSTER_PARENTING (1 << 28)
#define NS_HTML5ELEMENT_NAME_SCOPING (1 << 27)
#define NS_HTML5ELEMENT_NAME_SCOPING_AS_SVG (1 << 26)
#define NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML (1 << 25)
#define NS_HTML5ELEMENT_NAME_HTML_INTEGRATION_POINT (1 << 24)
#define NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG (1 << 23)


#endif

