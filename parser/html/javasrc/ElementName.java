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

package nu.validator.htmlparser.impl;

import nu.validator.htmlparser.annotation.Inline;
import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.NoLength;
import nu.validator.htmlparser.annotation.Unsigned;
import nu.validator.htmlparser.common.Interner;

public final class ElementName
// uncomment when regenerating self
//        implements Comparable<ElementName>
{

    /**
     * The mask for extracting the dispatch group.
     */
    public static final int GROUP_MASK = 127;

    /**
     * Indicates that the element is not a pre-interned element. Forbidden
     * on preinterned elements.
     */
    public static final int NOT_INTERNED = (1 << 30);

    /**
     * Indicates that the element is in the "special" category. This bit
     * should not be pre-set on MathML or SVG specials--only on HTML specials.
     */
    public static final int SPECIAL = (1 << 29);

    /**
     * The element is foster-parenting. This bit should be pre-set on elements
     * that are foster-parenting as HTML.
     */
    public static final int FOSTER_PARENTING = (1 << 28);

    /**
     * The element is scoping. This bit should be pre-set on elements
     * that are scoping as HTML.
     */
    public static final int SCOPING = (1 << 27);

    /**
     * The element is scoping as SVG.
     */
    public static final int SCOPING_AS_SVG = (1 << 26);

    /**
     * The element is scoping as MathML.
     */
    public static final int SCOPING_AS_MATHML = (1 << 25);

    /**
     * The element is an HTML integration point.
     */
    public static final int HTML_INTEGRATION_POINT = (1 << 24);

    /**
     * The element has an optional end tag.
     */
    public static final int OPTIONAL_END_TAG = (1 << 23);

    private @Local String name;

    private @Local String camelCaseName;

    /**
     * The lowest 7 bits are the dispatch group. The high bits are flags.
     */
    public final int flags;

    @Inline public @Local String getName() {
        return name;
    }

    @Inline public @Local String getCamelCaseName() {
        return camelCaseName;
    }

    @Inline public int getFlags() {
        return flags;
    }

    @Inline public int getGroup() {
        return flags & ElementName.GROUP_MASK;
    }

    @Inline public boolean isInterned() {
        return (flags & ElementName.NOT_INTERNED) == 0;
    }

    @Inline static int levelOrderBinarySearch(int[] data, int key) {
        int n = data.length;
        int i = 0;

        while (i < n) {
            int val = data[i];
            if (val < key) {
                i = 2 * i + 2;
            } else if (val > key) {
                i = 2 * i + 1;
            } else {
                return i;
            }
        }

        return -1;
    }

    @Inline static ElementName elementNameByBuffer(@NoLength char[] buf, int offset, int length, Interner interner) {
        @Unsigned int hash = ElementName.bufToHash(buf, length);
        int[] hashes;
        hashes = ElementName.ELEMENT_HASHES;
        int index = levelOrderBinarySearch(hashes, hash);
        if (index < 0) {
            return null;
        } else {
            ElementName elementName = ElementName.ELEMENT_NAMES[index];
            @Local String name = elementName.name;
            if (!Portability.localEqualsBuffer(name, buf, offset, length)) {
                return null;
            }
            return elementName;
        }
    }

    /**
     * This method has to return a unique positive integer for each well-known
     * lower-cased element name.
     *
     * @param buf
     * @param len
     * @return
     */
    @Inline private static @Unsigned int bufToHash(@NoLength char[] buf, int length) {
        @Unsigned int len = length;
        @Unsigned int first = buf[0];
        first <<= 19;
        @Unsigned int second = 1 << 23;
        @Unsigned int third = 0;
        @Unsigned int fourth = 0;
        @Unsigned int fifth = 0;
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

    private ElementName(@Local String name, @Local String camelCaseName,
            int flags) {
        this.name = name;
        this.camelCaseName = camelCaseName;
        this.flags = flags;
    }

    public ElementName() {
        this.name = null;
        this.camelCaseName = null;
        this.flags = TreeBuilder.OTHER | NOT_INTERNED;
    }

    public void destructor() {
        // The translator adds refcount debug code here.
    }

    @Inline public void setNameForNonInterned(@Local String name) {
        // No need to worry about refcounting the local name, because in the
        // C++ case the scoped atom table remembers its own atoms.
        this.name = name;
        this.camelCaseName = name;
        // The assertion below relies on TreeBuilder.OTHER being zero!
        // TreeBuilder.OTHER isn't referenced here, because it would create
        // a circular C++ header dependency given that this method is inlined.
        assert this.flags == ElementName.NOT_INTERNED;
    }

    public static final ElementName ANNOTATION_XML = new ElementName("annotation-xml", "annotation-xml", TreeBuilder.ANNOTATION_XML | SCOPING_AS_MATHML);

    // START CODE ONLY USED FOR GENERATING CODE uncomment and run to regenerate

//    /**
//     * @see java.lang.Object#toString()
//     */
//    @Override public String toString() {
//        return "(\"" + name + "\", \"" + camelCaseName + "\", " + decomposedFlags() + ")";
//    }
//
//    private String decomposedFlags() {
//        StringBuilder buf = new StringBuilder("TreeBuilder.");
//        buf.append(treeBuilderGroupToName());
//        if ((flags & SPECIAL) != 0) {
//            buf.append(" | SPECIAL");
//        }
//        if ((flags & FOSTER_PARENTING) != 0) {
//            buf.append(" | FOSTER_PARENTING");
//        }
//        if ((flags & SCOPING) != 0) {
//            buf.append(" | SCOPING");
//        }
//        if ((flags & SCOPING_AS_MATHML) != 0) {
//            buf.append(" | SCOPING_AS_MATHML");
//        }
//        if ((flags & SCOPING_AS_SVG) != 0) {
//            buf.append(" | SCOPING_AS_SVG");
//        }
//        if ((flags & OPTIONAL_END_TAG) != 0) {
//            buf.append(" | OPTIONAL_END_TAG");
//        }
//        return buf.toString();
//    }
//
//    private String constName() {
//        char[] buf = new char[name.length()];
//        for (int i = 0; i < name.length(); i++) {
//            char c = name.charAt(i);
//            if (c == '-') {
//                if (!"annotation-xml".equals(name)) {
//                    throw new RuntimeException("Non-annotation-xml element name with hyphen: " + name);
//                }
//                buf[i] = '_';
//            } else if (c >= '0' && c <= '9') {
//                buf[i] = c;
//            } else {
//                buf[i] = (char) (c - 0x20);
//            }
//        }
//        return new String(buf);
//    }
//
//    private int hash() {
//        return bufToHash(name.toCharArray(), name.length());
//    }
//
//    public int compareTo(ElementName other) {
//        int thisHash = this.hash();
//        int otherHash = other.hash();
//        if (thisHash < otherHash) {
//            return -1;
//        } else if (thisHash == otherHash) {
//            return 0;
//        } else {
//            return 1;
//        }
//    }
//
//    private String treeBuilderGroupToName() {
//        switch (getGroup()) {
//            case TreeBuilder.OTHER:
//                return "OTHER";
//            case TreeBuilder.A:
//                return "A";
//            case TreeBuilder.BASE:
//                return "BASE";
//            case TreeBuilder.BODY:
//                return "BODY";
//            case TreeBuilder.BR:
//                return "BR";
//            case TreeBuilder.BUTTON:
//                return "BUTTON";
//            case TreeBuilder.CAPTION:
//                return "CAPTION";
//            case TreeBuilder.COL:
//                return "COL";
//            case TreeBuilder.COLGROUP:
//                return "COLGROUP";
//            case TreeBuilder.FONT:
//                return "FONT";
//            case TreeBuilder.FORM:
//                return "FORM";
//            case TreeBuilder.FRAME:
//                return "FRAME";
//            case TreeBuilder.FRAMESET:
//                return "FRAMESET";
//            case TreeBuilder.IMAGE:
//                return "IMAGE";
//            case TreeBuilder.INPUT:
//                return "INPUT";
//            case TreeBuilder.ISINDEX:
//                return "ISINDEX";
//            case TreeBuilder.LI:
//                return "LI";
//            case TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND:
//                return "LINK_OR_BASEFONT_OR_BGSOUND";
//            case TreeBuilder.MATH:
//                return "MATH";
//            case TreeBuilder.META:
//                return "META";
//            case TreeBuilder.SVG:
//                return "SVG";
//            case TreeBuilder.HEAD:
//                return "HEAD";
//            case TreeBuilder.HR:
//                return "HR";
//            case TreeBuilder.HTML:
//                return "HTML";
//            case TreeBuilder.KEYGEN:
//                return "KEYGEN";
//            case TreeBuilder.NOBR:
//                return "NOBR";
//            case TreeBuilder.NOFRAMES:
//                return "NOFRAMES";
//            case TreeBuilder.NOSCRIPT:
//                return "NOSCRIPT";
//            case TreeBuilder.OPTGROUP:
//                return "OPTGROUP";
//            case TreeBuilder.OPTION:
//                return "OPTION";
//            case TreeBuilder.P:
//                return "P";
//            case TreeBuilder.PLAINTEXT:
//                return "PLAINTEXT";
//            case TreeBuilder.SCRIPT:
//                return "SCRIPT";
//            case TreeBuilder.SELECT:
//                return "SELECT";
//            case TreeBuilder.STYLE:
//                return "STYLE";
//            case TreeBuilder.TABLE:
//                return "TABLE";
//            case TreeBuilder.TEXTAREA:
//                return "TEXTAREA";
//            case TreeBuilder.TITLE:
//                return "TITLE";
//            case TreeBuilder.TEMPLATE:
//                return "TEMPLATE";
//            case TreeBuilder.TR:
//                return "TR";
//            case TreeBuilder.XMP:
//                return "XMP";
//            case TreeBuilder.TBODY_OR_THEAD_OR_TFOOT:
//                return "TBODY_OR_THEAD_OR_TFOOT";
//            case TreeBuilder.TD_OR_TH:
//                return "TD_OR_TH";
//            case TreeBuilder.DD_OR_DT:
//                return "DD_OR_DT";
//            case TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
//                return "H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6";
//            case TreeBuilder.OBJECT:
//                return "OBJECT";
//            case TreeBuilder.OUTPUT:
//                return "OUTPUT";
//            case TreeBuilder.MARQUEE_OR_APPLET:
//                return "MARQUEE_OR_APPLET";
//            case TreeBuilder.PRE_OR_LISTING:
//                return "PRE_OR_LISTING";
//            case TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
//                return "B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U";
//            case TreeBuilder.UL_OR_OL_OR_DL:
//                return "UL_OR_OL_OR_DL";
//            case TreeBuilder.IFRAME:
//                return "IFRAME";
//            case TreeBuilder.NOEMBED:
//                return "NOEMBED";
//            case TreeBuilder.EMBED:
//                return "EMBED";
//            case TreeBuilder.IMG:
//                return "IMG";
//            case TreeBuilder.AREA_OR_WBR:
//                return "AREA_OR_WBR";
//            case TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
//                return "DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU";
//            case TreeBuilder.FIELDSET:
//                return "FIELDSET";
//            case TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY:
//                return "ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY";
//            case TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR:
//                return "RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR";
//            case TreeBuilder.RB_OR_RTC:
//                return "RB_OR_RTC";
//            case TreeBuilder.RT_OR_RP:
//                return "RT_OR_RP";
//            case TreeBuilder.PARAM_OR_SOURCE_OR_TRACK:
//                return "PARAM_OR_SOURCE_OR_TRACK";
//            case TreeBuilder.MGLYPH_OR_MALIGNMARK:
//                return "MGLYPH_OR_MALIGNMARK";
//            case TreeBuilder.MI_MO_MN_MS_MTEXT:
//                return "MI_MO_MN_MS_MTEXT";
//            case TreeBuilder.ANNOTATION_XML:
//                return "ANNOTATION_XML";
//            case TreeBuilder.FOREIGNOBJECT_OR_DESC:
//                return "FOREIGNOBJECT_OR_DESC";
//            case TreeBuilder.MENUITEM:
//                return "MENUITEM";
//        }
//        return null;
//    }
//
//    private static void fillLevelOrderArray(List<ElementName> sorted, int depth,
//            int rootIdx, ElementName[] levelOrder) {
//        if (rootIdx >= levelOrder.length) {
//            return;
//        }
//
//        if (depth > 0) {
//            fillLevelOrderArray(sorted, depth - 1, rootIdx * 2 + 1, levelOrder);
//        }
//
//        if (!sorted.isEmpty()) {
//            levelOrder[rootIdx] = sorted.remove(0);
//        }
//
//        if (depth > 0) {
//            fillLevelOrderArray(sorted, depth - 1, rootIdx * 2 + 2, levelOrder);
//        }
//    }
//
//    /**
//     * Regenerate self
//     *
//     * @param args
//     */
//    public static void main(String[] args) {
//        Arrays.sort(ELEMENT_NAMES);
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            int hash = ELEMENT_NAMES[i].hash();
//            if (hash < 0) {
//                System.err.println("Negative hash: " + ELEMENT_NAMES[i].name);
//                return;
//            }
//            for (int j = i + 1; j < ELEMENT_NAMES.length; j++) {
//                if (hash == ELEMENT_NAMES[j].hash()) {
//                    System.err.println("Hash collision: " + ELEMENT_NAMES[i].name
//                            + ", " + ELEMENT_NAMES[j].name);
//                    return;
//                }
//            }
//        }
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            ElementName el = ELEMENT_NAMES[i];
//            System.out.println("public static final ElementName "
//                    + el.constName() + " = new ElementName" + el.toString()
//                    + ";");
//        }
//
//        LinkedList<ElementName> sortedNames = new LinkedList<ElementName>();
//        Collections.addAll(sortedNames, ELEMENT_NAMES);
//        ElementName[] levelOrder = new ElementName[ELEMENT_NAMES.length];
//        int bstDepth = (int) Math.ceil(Math.log(ELEMENT_NAMES.length) / Math.log(2));
//        fillLevelOrderArray(sortedNames, bstDepth, 0, levelOrder);
//
//        System.out.println("private final static @NoLength ElementName[] ELEMENT_NAMES = {");
//        for (int i = 0; i < levelOrder.length; i++) {
//            ElementName el = levelOrder[i];
//            System.out.println(el.constName() + ",");
//        }
//        System.out.println("};");
//        System.out.println("private final static int[] ELEMENT_HASHES = {");
//        for (int i = 0; i < levelOrder.length; i++) {
//            ElementName el = levelOrder[i];
//            System.out.println(Integer.toString(el.hash()) + ",");
//        }
//        System.out.println("};");
//    }

    // START GENERATED CODE
    public static final ElementName BIG = new ElementName("big", "big", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName BDO = new ElementName("bdo", "bdo", TreeBuilder.OTHER);
    public static final ElementName COL = new ElementName("col", "col", TreeBuilder.COL | SPECIAL);
    public static final ElementName DEL = new ElementName("del", "del", TreeBuilder.OTHER);
    public static final ElementName DFN = new ElementName("dfn", "dfn", TreeBuilder.OTHER);
    public static final ElementName DIR = new ElementName("dir", "dir", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName DIV = new ElementName("div", "div", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName IMG = new ElementName("img", "img", TreeBuilder.IMG | SPECIAL);
    public static final ElementName INS = new ElementName("ins", "ins", TreeBuilder.OTHER);
    public static final ElementName KBD = new ElementName("kbd", "kbd", TreeBuilder.OTHER);
    public static final ElementName MAP = new ElementName("map", "map", TreeBuilder.OTHER);
    public static final ElementName NAV = new ElementName("nav", "nav", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName PRE = new ElementName("pre", "pre", TreeBuilder.PRE_OR_LISTING | SPECIAL);
    public static final ElementName A = new ElementName("a", "a", TreeBuilder.A);
    public static final ElementName B = new ElementName("b", "b", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName RTC = new ElementName("rtc", "rtc", TreeBuilder.RB_OR_RTC | OPTIONAL_END_TAG);
    public static final ElementName SUB = new ElementName("sub", "sub", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SVG = new ElementName("svg", "svg", TreeBuilder.SVG);
    public static final ElementName SUP = new ElementName("sup", "sup", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SET = new ElementName("set", "set", TreeBuilder.OTHER);
    public static final ElementName USE = new ElementName("use", "use", TreeBuilder.OTHER);
    public static final ElementName VAR = new ElementName("var", "var", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName G = new ElementName("g", "g", TreeBuilder.OTHER);
    public static final ElementName WBR = new ElementName("wbr", "wbr", TreeBuilder.AREA_OR_WBR | SPECIAL);
    public static final ElementName XMP = new ElementName("xmp", "xmp", TreeBuilder.XMP | SPECIAL);
    public static final ElementName I = new ElementName("i", "i", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName P = new ElementName("p", "p", TreeBuilder.P | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName Q = new ElementName("q", "q", TreeBuilder.OTHER);
    public static final ElementName S = new ElementName("s", "s", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName U = new ElementName("u", "u", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName H1 = new ElementName("h1", "h1", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H2 = new ElementName("h2", "h2", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H3 = new ElementName("h3", "h3", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H4 = new ElementName("h4", "h4", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H5 = new ElementName("h5", "h5", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H6 = new ElementName("h6", "h6", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName AREA = new ElementName("area", "area", TreeBuilder.AREA_OR_WBR | SPECIAL);
    public static final ElementName FEFUNCA = new ElementName("fefunca", "feFuncA", TreeBuilder.OTHER);
    public static final ElementName METADATA = new ElementName("metadata", "metadata", TreeBuilder.OTHER);
    public static final ElementName META = new ElementName("meta", "meta", TreeBuilder.META | SPECIAL);
    public static final ElementName TEXTAREA = new ElementName("textarea", "textarea", TreeBuilder.TEXTAREA | SPECIAL);
    public static final ElementName FEFUNCB = new ElementName("fefuncb", "feFuncB", TreeBuilder.OTHER);
    public static final ElementName RB = new ElementName("rb", "rb", TreeBuilder.RB_OR_RTC | OPTIONAL_END_TAG);
    public static final ElementName DESC = new ElementName("desc", "desc", TreeBuilder.FOREIGNOBJECT_OR_DESC | SCOPING_AS_SVG);
    public static final ElementName DD = new ElementName("dd", "dd", TreeBuilder.DD_OR_DT | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName BGSOUND = new ElementName("bgsound", "bgsound", TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName EMBED = new ElementName("embed", "embed", TreeBuilder.EMBED | SPECIAL);
    public static final ElementName FEBLEND = new ElementName("feblend", "feBlend", TreeBuilder.OTHER);
    public static final ElementName FEFLOOD = new ElementName("feflood", "feFlood", TreeBuilder.OTHER);
    public static final ElementName HEAD = new ElementName("head", "head", TreeBuilder.HEAD | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName LEGEND = new ElementName("legend", "legend", TreeBuilder.OTHER);
    public static final ElementName NOEMBED = new ElementName("noembed", "noembed", TreeBuilder.NOEMBED | SPECIAL);
    public static final ElementName TD = new ElementName("td", "td", TreeBuilder.TD_OR_TH | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName THEAD = new ElementName("thead", "thead", TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName ASIDE = new ElementName("aside", "aside", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName ARTICLE = new ElementName("article", "article", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName ANIMATE = new ElementName("animate", "animate", TreeBuilder.OTHER);
    public static final ElementName BASE = new ElementName("base", "base", TreeBuilder.BASE | SPECIAL);
    public static final ElementName BLOCKQUOTE = new ElementName("blockquote", "blockquote", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName CODE = new ElementName("code", "code", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName CIRCLE = new ElementName("circle", "circle", TreeBuilder.OTHER);
    public static final ElementName CITE = new ElementName("cite", "cite", TreeBuilder.OTHER);
    public static final ElementName ELLIPSE = new ElementName("ellipse", "ellipse", TreeBuilder.OTHER);
    public static final ElementName FETURBULENCE = new ElementName("feturbulence", "feTurbulence", TreeBuilder.OTHER);
    public static final ElementName FEMERGENODE = new ElementName("femergenode", "feMergeNode", TreeBuilder.OTHER);
    public static final ElementName FEIMAGE = new ElementName("feimage", "feImage", TreeBuilder.OTHER);
    public static final ElementName FEMERGE = new ElementName("femerge", "feMerge", TreeBuilder.OTHER);
    public static final ElementName FETILE = new ElementName("fetile", "feTile", TreeBuilder.OTHER);
    public static final ElementName FRAME = new ElementName("frame", "frame", TreeBuilder.FRAME | SPECIAL);
    public static final ElementName FIGURE = new ElementName("figure", "figure", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FECOMPOSITE = new ElementName("fecomposite", "feComposite", TreeBuilder.OTHER);
    public static final ElementName IMAGE = new ElementName("image", "image", TreeBuilder.IMAGE);
    public static final ElementName IFRAME = new ElementName("iframe", "iframe", TreeBuilder.IFRAME | SPECIAL);
    public static final ElementName LINE = new ElementName("line", "line", TreeBuilder.OTHER);
    public static final ElementName MARQUEE = new ElementName("marquee", "marquee", TreeBuilder.MARQUEE_OR_APPLET | SPECIAL | SCOPING);
    public static final ElementName POLYLINE = new ElementName("polyline", "polyline", TreeBuilder.OTHER);
    public static final ElementName PICTURE = new ElementName("picture", "picture", TreeBuilder.OTHER);
    public static final ElementName SOURCE = new ElementName("source", "source", TreeBuilder.PARAM_OR_SOURCE_OR_TRACK);
    public static final ElementName STRIKE = new ElementName("strike", "strike", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName STYLE = new ElementName("style", "style", TreeBuilder.STYLE | SPECIAL);
    public static final ElementName TABLE = new ElementName("table", "table", TreeBuilder.TABLE | SPECIAL | FOSTER_PARENTING | SCOPING);
    public static final ElementName TITLE = new ElementName("title", "title", TreeBuilder.TITLE | SPECIAL | SCOPING_AS_SVG);
    public static final ElementName TIME = new ElementName("time", "time", TreeBuilder.OTHER);
    public static final ElementName TEMPLATE = new ElementName("template", "template", TreeBuilder.TEMPLATE | SPECIAL | SCOPING);
    public static final ElementName ALTGLYPHDEF = new ElementName("altglyphdef", "altGlyphDef", TreeBuilder.OTHER);
    public static final ElementName GLYPHREF = new ElementName("glyphref", "glyphRef", TreeBuilder.OTHER);
    public static final ElementName DIALOG = new ElementName("dialog", "dialog", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FEFUNCG = new ElementName("fefuncg", "feFuncG", TreeBuilder.OTHER);
    public static final ElementName FEDIFFUSELIGHTING = new ElementName("fediffuselighting", "feDiffuseLighting", TreeBuilder.OTHER);
    public static final ElementName FESPECULARLIGHTING = new ElementName("fespecularlighting", "feSpecularLighting", TreeBuilder.OTHER);
    public static final ElementName LISTING = new ElementName("listing", "listing", TreeBuilder.PRE_OR_LISTING | SPECIAL);
    public static final ElementName STRONG = new ElementName("strong", "strong", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName ALTGLYPH = new ElementName("altglyph", "altGlyph", TreeBuilder.OTHER);
    public static final ElementName CLIPPATH = new ElementName("clippath", "clipPath", TreeBuilder.OTHER);
    public static final ElementName MGLYPH = new ElementName("mglyph", "mglyph", TreeBuilder.MGLYPH_OR_MALIGNMARK);
    public static final ElementName MATH = new ElementName("math", "math", TreeBuilder.MATH);
    public static final ElementName MPATH = new ElementName("mpath", "mpath", TreeBuilder.OTHER);
    public static final ElementName PATH = new ElementName("path", "path", TreeBuilder.OTHER);
    public static final ElementName TH = new ElementName("th", "th", TreeBuilder.TD_OR_TH | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName SWITCH = new ElementName("switch", "switch", TreeBuilder.OTHER);
    public static final ElementName TEXTPATH = new ElementName("textpath", "textPath", TreeBuilder.OTHER);
    public static final ElementName LI = new ElementName("li", "li", TreeBuilder.LI | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName MI = new ElementName("mi", "mi", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName LINK = new ElementName("link", "link", TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName MARK = new ElementName("mark", "mark", TreeBuilder.OTHER);
    public static final ElementName MALIGNMARK = new ElementName("malignmark", "malignmark", TreeBuilder.MGLYPH_OR_MALIGNMARK);
    public static final ElementName MASK = new ElementName("mask", "mask", TreeBuilder.OTHER);
    public static final ElementName TRACK = new ElementName("track", "track", TreeBuilder.PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
    public static final ElementName DL = new ElementName("dl", "dl", TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName HTML = new ElementName("html", "html", TreeBuilder.HTML | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName OL = new ElementName("ol", "ol", TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName LABEL = new ElementName("label", "label", TreeBuilder.OTHER);
    public static final ElementName UL = new ElementName("ul", "ul", TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName SMALL = new ElementName("small", "small", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName SYMBOL = new ElementName("symbol", "symbol", TreeBuilder.OTHER);
    public static final ElementName ALTGLYPHITEM = new ElementName("altglyphitem", "altGlyphItem", TreeBuilder.OTHER);
    public static final ElementName ANIMATETRANSFORM = new ElementName("animatetransform", "animateTransform", TreeBuilder.OTHER);
    public static final ElementName ACRONYM = new ElementName("acronym", "acronym", TreeBuilder.OTHER);
    public static final ElementName EM = new ElementName("em", "em", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName FORM = new ElementName("form", "form", TreeBuilder.FORM | SPECIAL);
    public static final ElementName MENUITEM = new ElementName("menuitem", "menuitem", TreeBuilder.MENUITEM);
    public static final ElementName PARAM = new ElementName("param", "param", TreeBuilder.PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
    public static final ElementName ANIMATEMOTION = new ElementName("animatemotion", "animateMotion", TreeBuilder.OTHER);
    public static final ElementName BUTTON = new ElementName("button", "button", TreeBuilder.BUTTON | SPECIAL);
    public static final ElementName CAPTION = new ElementName("caption", "caption", TreeBuilder.CAPTION | SPECIAL | SCOPING);
    public static final ElementName FIGCAPTION = new ElementName("figcaption", "figcaption", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName MN = new ElementName("mn", "mn", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName KEYGEN = new ElementName("keygen", "keygen", TreeBuilder.KEYGEN);
    public static final ElementName MAIN = new ElementName("main", "main", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName OPTION = new ElementName("option", "option", TreeBuilder.OPTION | OPTIONAL_END_TAG);
    public static final ElementName POLYGON = new ElementName("polygon", "polygon", TreeBuilder.OTHER);
    public static final ElementName PATTERN = new ElementName("pattern", "pattern", TreeBuilder.OTHER);
    public static final ElementName SPAN = new ElementName("span", "span", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SECTION = new ElementName("section", "section", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName TSPAN = new ElementName("tspan", "tspan", TreeBuilder.OTHER);
    public static final ElementName AUDIO = new ElementName("audio", "audio", TreeBuilder.OTHER);
    public static final ElementName MO = new ElementName("mo", "mo", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName VIDEO = new ElementName("video", "video", TreeBuilder.OTHER);
    public static final ElementName COLGROUP = new ElementName("colgroup", "colgroup", TreeBuilder.COLGROUP | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName FEDISPLACEMENTMAP = new ElementName("fedisplacementmap", "feDisplacementMap", TreeBuilder.OTHER);
    public static final ElementName HGROUP = new ElementName("hgroup", "hgroup", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName RP = new ElementName("rp", "rp", TreeBuilder.RT_OR_RP | OPTIONAL_END_TAG);
    public static final ElementName OPTGROUP = new ElementName("optgroup", "optgroup", TreeBuilder.OPTGROUP | OPTIONAL_END_TAG);
    public static final ElementName SAMP = new ElementName("samp", "samp", TreeBuilder.OTHER);
    public static final ElementName STOP = new ElementName("stop", "stop", TreeBuilder.OTHER);
    public static final ElementName BR = new ElementName("br", "br", TreeBuilder.BR | SPECIAL);
    public static final ElementName ABBR = new ElementName("abbr", "abbr", TreeBuilder.OTHER);
    public static final ElementName ANIMATECOLOR = new ElementName("animatecolor", "animateColor", TreeBuilder.OTHER);
    public static final ElementName CENTER = new ElementName("center", "center", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName HR = new ElementName("hr", "hr", TreeBuilder.HR | SPECIAL);
    public static final ElementName FEFUNCR = new ElementName("fefuncr", "feFuncR", TreeBuilder.OTHER);
    public static final ElementName FECOMPONENTTRANSFER = new ElementName("fecomponenttransfer", "feComponentTransfer", TreeBuilder.OTHER);
    public static final ElementName FILTER = new ElementName("filter", "filter", TreeBuilder.OTHER);
    public static final ElementName FOOTER = new ElementName("footer", "footer", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FEGAUSSIANBLUR = new ElementName("fegaussianblur", "feGaussianBlur", TreeBuilder.OTHER);
    public static final ElementName HEADER = new ElementName("header", "header", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName MARKER = new ElementName("marker", "marker", TreeBuilder.OTHER);
    public static final ElementName METER = new ElementName("meter", "meter", TreeBuilder.OTHER);
    public static final ElementName NOBR = new ElementName("nobr", "nobr", TreeBuilder.NOBR);
    public static final ElementName TR = new ElementName("tr", "tr", TreeBuilder.TR | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName ADDRESS = new ElementName("address", "address", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName CANVAS = new ElementName("canvas", "canvas", TreeBuilder.OTHER);
    public static final ElementName DEFS = new ElementName("defs", "defs", TreeBuilder.OTHER);
    public static final ElementName DETAILS = new ElementName("details", "details", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName MS = new ElementName("ms", "ms", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName NOFRAMES = new ElementName("noframes", "noframes", TreeBuilder.NOFRAMES | SPECIAL);
    public static final ElementName PROGRESS = new ElementName("progress", "progress", TreeBuilder.OTHER);
    public static final ElementName DT = new ElementName("dt", "dt", TreeBuilder.DD_OR_DT | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName APPLET = new ElementName("applet", "applet", TreeBuilder.MARQUEE_OR_APPLET | SPECIAL | SCOPING);
    public static final ElementName BASEFONT = new ElementName("basefont", "basefont", TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName FOREIGNOBJECT = new ElementName("foreignobject", "foreignObject", TreeBuilder.FOREIGNOBJECT_OR_DESC | SCOPING_AS_SVG);
    public static final ElementName FIELDSET = new ElementName("fieldset", "fieldset", TreeBuilder.FIELDSET | SPECIAL);
    public static final ElementName FRAMESET = new ElementName("frameset", "frameset", TreeBuilder.FRAMESET | SPECIAL);
    public static final ElementName FEOFFSET = new ElementName("feoffset", "feOffset", TreeBuilder.OTHER);
    public static final ElementName FESPOTLIGHT = new ElementName("fespotlight", "feSpotLight", TreeBuilder.OTHER);
    public static final ElementName FEPOINTLIGHT = new ElementName("fepointlight", "fePointLight", TreeBuilder.OTHER);
    public static final ElementName FEDISTANTLIGHT = new ElementName("fedistantlight", "feDistantLight", TreeBuilder.OTHER);
    public static final ElementName FONT = new ElementName("font", "font", TreeBuilder.FONT);
    public static final ElementName INPUT = new ElementName("input", "input", TreeBuilder.INPUT | SPECIAL);
    public static final ElementName LINEARGRADIENT = new ElementName("lineargradient", "linearGradient", TreeBuilder.OTHER);
    public static final ElementName MTEXT = new ElementName("mtext", "mtext", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName NOSCRIPT = new ElementName("noscript", "noscript", TreeBuilder.NOSCRIPT | SPECIAL);
    public static final ElementName RT = new ElementName("rt", "rt", TreeBuilder.RT_OR_RP | OPTIONAL_END_TAG);
    public static final ElementName OBJECT = new ElementName("object", "object", TreeBuilder.OBJECT | SPECIAL | SCOPING);
    public static final ElementName OUTPUT = new ElementName("output", "output", TreeBuilder.OUTPUT);
    public static final ElementName PLAINTEXT = new ElementName("plaintext", "plaintext", TreeBuilder.PLAINTEXT | SPECIAL);
    public static final ElementName TT = new ElementName("tt", "tt", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName RECT = new ElementName("rect", "rect", TreeBuilder.OTHER);
    public static final ElementName RADIALGRADIENT = new ElementName("radialgradient", "radialGradient", TreeBuilder.OTHER);
    public static final ElementName SELECT = new ElementName("select", "select", TreeBuilder.SELECT | SPECIAL);
    public static final ElementName SCRIPT = new ElementName("script", "script", TreeBuilder.SCRIPT | SPECIAL);
    public static final ElementName TFOOT = new ElementName("tfoot", "tfoot", TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName TEXT = new ElementName("text", "text", TreeBuilder.OTHER);
    public static final ElementName MENU = new ElementName("menu", "menu", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName FEDROPSHADOW = new ElementName("fedropshadow", "feDropShadow", TreeBuilder.OTHER);
    public static final ElementName VIEW = new ElementName("view", "view", TreeBuilder.OTHER);
    public static final ElementName FECOLORMATRIX = new ElementName("fecolormatrix", "feColorMatrix", TreeBuilder.OTHER);
    public static final ElementName FECONVOLVEMATRIX = new ElementName("feconvolvematrix", "feConvolveMatrix", TreeBuilder.OTHER);
    public static final ElementName BODY = new ElementName("body", "body", TreeBuilder.BODY | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName FEMORPHOLOGY = new ElementName("femorphology", "feMorphology", TreeBuilder.OTHER);
    public static final ElementName RUBY = new ElementName("ruby", "ruby", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SUMMARY = new ElementName("summary", "summary", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName TBODY = new ElementName("tbody", "tbody", TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    private final static @NoLength ElementName[] ELEMENT_NAMES = {
    KEYGEN,
    FETURBULENCE,
    FIELDSET,
    H2,
    MATH,
    HEADER,
    RECT,
    RTC,
    FEBLEND,
    STYLE,
    LABEL,
    SAMP,
    DETAILS,
    LINEARGRADIENT,
    VIEW,
    IMG,
    WBR,
    META,
    ARTICLE,
    IMAGE,
    FEFUNCG,
    LINK,
    FORM,
    AUDIO,
    FECOMPONENTTRANSFER,
    TR,
    DT,
    FEPOINTLIGHT,
    OBJECT,
    TFOOT,
    FEMORPHOLOGY,
    DEL,
    NAV,
    SET,
    Q,
    H6,
    DESC,
    NOEMBED,
    CODE,
    FETILE,
    POLYLINE,
    TEMPLATE,
    STRONG,
    SWITCH,
    TRACK,
    ALTGLYPHITEM,
    BUTTON,
    PATTERN,
    FEDISPLACEMENTMAP,
    ANIMATECOLOR,
    FOOTER,
    METER,
    CANVAS,
    NOFRAMES,
    BASEFONT,
    FEOFFSET,
    FONT,
    NOSCRIPT,
    PLAINTEXT,
    SELECT,
    MENU,
    FECONVOLVEMATRIX,
    SUMMARY,
    BDO,
    DIR,
    KBD,
    A,
    SVG,
    VAR,
    I,
    U,
    H4,
    FEFUNCA,
    FEFUNCB,
    BGSOUND,
    HEAD,
    THEAD,
    BASE,
    CITE,
    FEIMAGE,
    FIGURE,
    LINE,
    SOURCE,
    TITLE,
    GLYPHREF,
    FESPECULARLIGHTING,
    CLIPPATH,
    PATH,
    LI,
    MALIGNMARK,
    HTML,
    SMALL,
    ACRONYM,
    PARAM,
    FIGCAPTION,
    OPTION,
    SECTION,
    VIDEO,
    RP,
    BR,
    HR,
    FILTER,
    FEGAUSSIANBLUR,
    MARKER,
    NOBR,
    ADDRESS,
    DEFS,
    MS,
    PROGRESS,
    APPLET,
    FOREIGNOBJECT,
    FRAMESET,
    FESPOTLIGHT,
    FEDISTANTLIGHT,
    INPUT,
    MTEXT,
    RT,
    OUTPUT,
    TT,
    RADIALGRADIENT,
    SCRIPT,
    TEXT,
    FEDROPSHADOW,
    FECOLORMATRIX,
    BODY,
    RUBY,
    TBODY,
    BIG,
    COL,
    DFN,
    DIV,
    INS,
    MAP,
    PRE,
    B,
    SUB,
    SUP,
    USE,
    G,
    XMP,
    P,
    S,
    H1,
    H3,
    H5,
    AREA,
    METADATA,
    TEXTAREA,
    RB,
    DD,
    EMBED,
    FEFLOOD,
    LEGEND,
    TD,
    ASIDE,
    ANIMATE,
    BLOCKQUOTE,
    CIRCLE,
    ELLIPSE,
    FEMERGENODE,
    FEMERGE,
    FRAME,
    FECOMPOSITE,
    IFRAME,
    MARQUEE,
    PICTURE,
    STRIKE,
    TABLE,
    TIME,
    ALTGLYPHDEF,
    DIALOG,
    FEDIFFUSELIGHTING,
    LISTING,
    ALTGLYPH,
    MGLYPH,
    MPATH,
    TH,
    TEXTPATH,
    MI,
    MARK,
    MASK,
    DL,
    OL,
    UL,
    SYMBOL,
    ANIMATETRANSFORM,
    EM,
    MENUITEM,
    ANIMATEMOTION,
    CAPTION,
    MN,
    MAIN,
    POLYGON,
    SPAN,
    TSPAN,
    MO,
    COLGROUP,
    HGROUP,
    OPTGROUP,
    STOP,
    ABBR,
    CENTER,
    FEFUNCR,
    };
    private final static int[] ELEMENT_HASHES = {
    1903302038,
    1749656156,
    2001349704,
    893386754,
    1803929812,
    1968836118,
    2007601444,
    59821379,
    1733054663,
    1756625221,
    1870268949,
    1941178676,
    1983633431,
    2004635806,
    2060065124,
    55104723,
    62450211,
    1686491348,
    1747176599,
    1751288021,
    1783210839,
    1853642948,
    1884120164,
    1914900309,
    1967788867,
    1973420034,
    1998585858,
    2001392796,
    2006028454,
    2008851557,
    2085266636,
    52485715,
    57733651,
    60354131,
    67633153,
    960495618,
    1715310660,
    1737099991,
    1748100148,
    1749801286,
    1755076808,
    1757268168,
    1790207270,
    1806806678,
    1857653029,
    1881288348,
    1898753862,
    1906135367,
    1934172497,
    1965334268,
    1967795958,
    1971465813,
    1982935782,
    1988763672,
    1999397992,
    2001349736,
    2001495140,
    2005719336,
    2006896969,
    2008125638,
    2021937364,
    2068523856,
    2092255447,
    51438659,
    52488851,
    56151587,
    59244545,
    60347747,
    61925907,
    63438849,
    69730305,
    926941186,
    1682547543,
    1699324759,
    1730965751,
    1733890180,
    1740181637,
    1747814436,
    1748359220,
    1749715159,
    1749905526,
    1752979652,
    1756474198,
    1757146773,
    1766992520,
    1783388498,
    1798686984,
    1805502724,
    1818230786,
    1854228698,
    1868312196,
    1874053333,
    1881613047,
    1889085973,
    1900845386,
    1905563974,
    1907661127,
    1925844629,
    1938817026,
    1963982850,
    1967128578,
    1967795910,
    1968053806,
    1971461414,
    1971938532,
    1982173479,
    1983533124,
    1986527234,
    1990037800,
    1998724870,
    2001309869,
    2001349720,
    2001392795,
    2001392798,
    2003183333,
    2005324101,
    2005925890,
    2006329158,
    2006974466,
    2007781534,
    2008340774,
    2008994116,
    2051837468,
    2068523853,
    2083120164,
    2091479332,
    2092557349,
    51434643,
    51961587,
    52486755,
    52490899,
    55110883,
    57206291,
    58773795,
    59768833,
    60345171,
    60352339,
    61395251,
    62390273,
    62973651,
    67108865,
    68681729,
    876609538,
    910163970,
    943718402,
    1679960596,
    1686489160,
    1689922072,
    1703936002,
    1730150402,
    1732381397,
    1733076167,
    1736200310,
    1738539010,
    1747048757,
    1747306711,
    1747838298,
    1748225318,
    1749395095,
    1749673195,
    1749723735,
    1749813541,
    1749932347,
    1751386406,
    1753362711,
    1755148615,
    1756600614,
    1757137429,
    1757157700,
    1763839627,
    1782357526,
    1783388497,
    1786534215,
    1797585096,
    1803876550,
    1803929861,
    1805647874,
    1807599880,
    1818755074,
    1854228692,
    1854245076,
    1864368130,
    1870135298,
    1873281026,
    1874102998,
    1881498736,
    1881669634,
    1887579800,
    1898223949,
    1899272519,
    1902641154,
    1904412884,
    1906087319,
    1907435316,
    1907959605,
    1919418370,
    1932928296,
    1935549734,
    1939219752,
    1941221172,
    1965115924,
    1966223078,
    1967760215,
    };
}
