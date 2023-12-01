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

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

import nu.validator.htmlparser.annotation.CppInlineLength;
import nu.validator.htmlparser.annotation.Inline;
import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.StaticLocal;
import nu.validator.htmlparser.annotation.WeakLocal;
import nu.validator.htmlparser.annotation.NoLength;
import nu.validator.htmlparser.annotation.NsUri;
import nu.validator.htmlparser.annotation.Prefix;
import nu.validator.htmlparser.annotation.QName;
import nu.validator.htmlparser.annotation.Unsigned;
import nu.validator.htmlparser.common.Interner;

public final class AttributeName
// Uncomment to regenerate
// implements Comparable<AttributeName>
{
    // [NOCPP[

    public static final int NCNAME_HTML = 1;

    public static final int NCNAME_FOREIGN = (1 << 1) | (1 << 2);

    public static final int NCNAME_LANG = (1 << 3);

    public static final int IS_XMLNS = (1 << 4);

    public static final int CASE_FOLDED = (1 << 5);

    public static final int BOOLEAN = (1 << 6);

    // ]NOCPP]

    /**
     * An array representing no namespace regardless of namespace mode (HTML,
     * SVG, MathML, lang-mapping HTML) used.
     */
    static final @NoLength @NsUri String[] ALL_NO_NS = { "", "", "",
    // [NOCPP[
            ""
    // ]NOCPP]
    };

    /**
     * An array that has no namespace for the HTML mode but the XMLNS namespace
     * for the SVG and MathML modes.
     */
    private static final @NoLength @NsUri String[] XMLNS_NS = { "",
            "http://www.w3.org/2000/xmlns/", "http://www.w3.org/2000/xmlns/",
            // [NOCPP[
            ""
    // ]NOCPP]
    };

    /**
     * An array that has no namespace for the HTML mode but the XML namespace
     * for the SVG and MathML modes.
     */
    private static final @NoLength @NsUri String[] XML_NS = { "",
            "http://www.w3.org/XML/1998/namespace",
            "http://www.w3.org/XML/1998/namespace",
            // [NOCPP[
            ""
    // ]NOCPP]
    };

    /**
     * An array that has no namespace for the HTML mode but the XLink namespace
     * for the SVG and MathML modes.
     */
    private static final @NoLength @NsUri String[] XLINK_NS = { "",
            "http://www.w3.org/1999/xlink", "http://www.w3.org/1999/xlink",
            // [NOCPP[
            ""
    // ]NOCPP]
    };

    // [NOCPP[
    /**
     * An array that has no namespace for the HTML, SVG and MathML modes but has
     * the XML namespace for the lang-mapping HTML mode.
     */
    private static final @NoLength @NsUri String[] LANG_NS = { "", "", "",
            "http://www.w3.org/XML/1998/namespace" };

    // ]NOCPP]

    /**
     * An array for no prefixes in any mode.
     */
    static final @NoLength @Prefix String[] ALL_NO_PREFIX = { null, null, null,
    // [NOCPP[
            null
    // ]NOCPP]
    };

    /**
     * An array for no prefixe in the HTML mode and the <code>xmlns</code>
     * prefix in the SVG and MathML modes.
     */
    private static final @NoLength @Prefix String[] XMLNS_PREFIX = { null,
            "xmlns", "xmlns",
            // [NOCPP[
            null
    // ]NOCPP]
    };

    /**
     * An array for no prefixe in the HTML mode and the <code>xlink</code>
     * prefix in the SVG and MathML modes.
     */
    private static final @NoLength @Prefix String[] XLINK_PREFIX = { null,
            "xlink", "xlink",
            // [NOCPP[
            null
    // ]NOCPP]
    };

    /**
     * An array for no prefixe in the HTML mode and the <code>xml</code> prefix
     * in the SVG and MathML modes.
     */
    private static final @NoLength @Prefix String[] XML_PREFIX = { null, "xml",
            "xml",
            // [NOCPP[
            null
    // ]NOCPP]
    };

    // [NOCPP[

    private static final @NoLength @Prefix String[] LANG_PREFIX = { null, null,
            null, "xml" };

    private static @QName String[] COMPUTE_QNAME(String[] local, String[] prefix) {
        @QName String[] arr = new String[4];
        for (int i = 0; i < arr.length; i++) {
            if (prefix[i] == null) {
                arr[i] = local[i];
            } else {
                arr[i] = (prefix[i] + ':' + local[i]).intern();
            }
        }
        return arr;
    }

    // ]NOCPP]

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

    /**
     * Returns an attribute name by buffer.
     *
     * <p>
     * C++ ownership: The return value is either released by the caller if the
     * attribute is a duplicate or the ownership is transferred to
     * HtmlAttributes and released upon clearing or destroying that object.
     *
     * @param buf
     *            the buffer
     * @param offset
     *            ignored
     * @param length
     *            length of data
     * @param checkNcName
     *            whether to check ncnameness
     * @return an <code>AttributeName</code> corresponding to the argument data
     */
    @Inline static AttributeName nameByBuffer(@NoLength char[] buf,
            int length, Interner interner) {
        // XXX deal with offset
        @Unsigned int hash = AttributeName.bufToHash(buf, length);
        int[] hashes;
        hashes = AttributeName.ATTRIBUTE_HASHES;
        int index = levelOrderBinarySearch(hashes, hash);
        if (index < 0) {
            return null;
        }
        AttributeName attributeName = AttributeName.ATTRIBUTE_NAMES[index];
        @Local String name = attributeName.getLocal(0);
        if (!Portability.localEqualsBuffer(name, buf, length)) {
            return null;
        }
        return attributeName;
    }

    /**
     * This method has to return a unique positive integer for each well-known
     * lower-cased attribute name.
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
        @Unsigned int sixth = 0;
        if (length >= 4) {
            second = buf[length - 4];
            second <<= 4;
            third = buf[1];
            third <<= 9;
            fourth = buf[length - 2];
            fourth <<= 14;
            fifth = buf[3];
            fifth <<= 24;
            sixth = buf[length - 1];
            sixth <<= 11;
        } else if (length == 3) {
            second = buf[1];
            second <<= 4;
            third = buf[2];
            third <<= 9;
        } else if (length == 2) {
            second = buf[1];
            second <<= 24;
        }
        return len + first + second + third + fourth + fifth + sixth;
    }

    /**
     * The mode value for HTML.
     */
    public static final int HTML = 0;

    /**
     * The mode value for MathML.
     */
    public static final int MATHML = 1;

    /**
     * The mode value for SVG.
     */
    public static final int SVG = 2;

    // [NOCPP[

    /**
     * The mode value for lang-mapping HTML.
     */
    public static final int HTML_LANG = 3;

    // ]NOCPP]

    /**
     * The namespaces indexable by mode.
     */
    private final @CppInlineLength(3) @NsUri @NoLength String[] uri;

    /**
     * The local names indexable by mode.
     *
     * These are weak because they're either all static, or
     * all the same, in wich case we just need to take one reference.
     */
    private final @CppInlineLength(3) @WeakLocal @NoLength String[] local;

    /**
     * The prefixes indexably by mode.
     */
    private final @CppInlineLength(3) @Prefix @NoLength String[] prefix;

    // CPPONLY: private final boolean custom;

    // [NOCPP[

    private final int flags;

    /**
     * The qnames indexable by mode.
     */
    private final @QName @NoLength String[] qName;

    // ]NOCPP]

    /**
     * The startup-time constructor.
     *
     * @param uri
     *            the namespace
     * @param local
     *            the local name
     * @param prefix
     *            the prefix
     * @param ncname
     *            the ncnameness
     * @param xmlns
     *            whether this is an xmlns attribute
     */
    private AttributeName(@NsUri @NoLength String[] uri,
            @StaticLocal String html, @StaticLocal String mathml, @StaticLocal String svg,
            // [NOCPP[
            @StaticLocal String htmlLang,
            // ]NOCPP]
            @Prefix @NoLength String[] prefix
            // [NOCPP[
            , int flags
            // ]NOCPP]
    ) {
        this.uri = uri;
        this.prefix = prefix;
        // [NOCPP[
        this.local = new String[4];
        this.qName = COMPUTE_QNAME(local, prefix);
        this.flags = flags;
        // ]NOCPP]
        this.local[HTML] = html;
        this.local[MATHML] = mathml;
        this.local[SVG] = svg;
        // [NOCPP[
        this.local[HTML_LANG] = htmlLang;
        // ]NOCPP]
        // CPPONLY: this.custom = false;
    }

    // CPPONLY: public AttributeName() {
    // CPPONLY:     this.uri = ALL_NO_NS;
    // CPPONLY:     this.local[0] = null;
    // CPPONLY:     this.local[1] = null;
    // CPPONLY:     this.local[2] = null;
    // CPPONLY:     this.prefix = ALL_NO_PREFIX;
    // CPPONLY:     this.custom = true;
    // CPPONLY: }
    // CPPONLY:
    // CPPONLY: @Inline public boolean isInterned() {
    // CPPONLY:     return !custom;
    // CPPONLY: }
    // CPPONLY:
    // CPPONLY: @Inline public void setNameForNonInterned(@Local String name) {
    // CPPONLY:     assert custom;
    // CPPONLY:     Portability.addrefIfNonNull(name);
    // CPPONLY:     Portability.releaseIfNonNull(local[0]);
    // CPPONLY:     local[0] = name;
    // CPPONLY:     local[1] = name;
    // CPPONLY:     local[2] = name;
    // CPPONLY: }

    /**
     * Creates an <code>AttributeName</code> for a local name.
     *
     * @param name
     *            the name
     * @param checkNcName
     *            whether to check ncnameness
     * @return an <code>AttributeName</code>
     */
    // [NOCPP[
    static AttributeName createAttributeName(@Local String name, boolean checkNcName) {
        int flags = NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG;
        if (name.startsWith("xmlns:")) {
            flags = IS_XMLNS;
        } else if (checkNcName && !NCName.isNCName(name)) {
            flags = 0;
        }
        return new AttributeName(ALL_NO_NS,
                name, name, name, name, ALL_NO_PREFIX, flags);
    }
    // ]NOCPP]

    /**
     * The C++ destructor.
     */
    @SuppressWarnings("unused") private void destructor() {
        // CPPONLY: if (custom) {
        // CPPONLY:     Portability.releaseIfNonNull(local[0]);
        // CPPONLY: }
    }

    // [NOCPP[
    /**
     * Creator for use when the XML violation policy requires an attribute name
     * to be changed.
     *
     * @param name
     *            the name of the attribute to create
     */
    static AttributeName create(@Local String name) {
        return new AttributeName(AttributeName.ALL_NO_NS,
                name, name, name, name, ALL_NO_PREFIX,
                NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    }

    /**
     * Queries whether this name is an XML 1.0 4th ed. NCName.
     *
     * @param mode
     *            the SVG/MathML/HTML mode
     * @return <code>true</code> if this is an NCName in the given mode
     */
    public boolean isNcName(int mode) {
        return (flags & (1 << mode)) != 0;
    }

    /**
     * Queries whether this is an <code>xmlns</code> attribute.
     *
     * @return <code>true</code> if this is an <code>xmlns</code> attribute
     */
    public boolean isXmlns() {
        return (flags & IS_XMLNS) != 0;
    }

    /**
     * Queries whether this attribute has a case-folded value in the HTML4 mode
     * of the parser.
     *
     * @return <code>true</code> if the value is case-folded
     */
    boolean isCaseFolded() {
        return (flags & CASE_FOLDED) != 0;
    }

    boolean isBoolean() {
        return (flags & BOOLEAN) != 0;
    }

    public @QName String getQName(int mode) {
        return qName[mode];
    }

    // ]NOCPP]

    public @NsUri String getUri(int mode) {
        return uri[mode];
    }

    public @Local String getLocal(int mode) {
        return local[mode];
    }

    public @Prefix String getPrefix(int mode) {
        return prefix[mode];
    }

    boolean equalsAnother(AttributeName another) {
        return this.getLocal(AttributeName.HTML) == another.getLocal(AttributeName.HTML);
    }

    // START CODE ONLY USED FOR GENERATING CODE uncomment to regenerate

//    /**
//     * @see java.lang.Object#toString()
//     */
//    @Override public String toString() {
//        return "(" + formatNs() + ", " + formatLocal() + ", " + formatPrefix()
//                + ", " + formatFlags() + ")";
//    }
//
//    private String formatFlags() {
//        StringBuilder builder = new StringBuilder();
//        if ((flags & NCNAME_HTML) != 0) {
//            if (builder.length() != 0) {
//                builder.append(" | ");
//            }
//            builder.append("NCNAME_HTML");
//        }
//        if ((flags & NCNAME_FOREIGN) != 0) {
//            if (builder.length() != 0) {
//                builder.append(" | ");
//            }
//            builder.append("NCNAME_FOREIGN");
//        }
//        if ((flags & NCNAME_LANG) != 0) {
//            if (builder.length() != 0) {
//                builder.append(" | ");
//            }
//            builder.append("NCNAME_LANG");
//        }
//        if (isXmlns()) {
//            if (builder.length() != 0) {
//                builder.append(" | ");
//            }
//            builder.append("IS_XMLNS");
//        }
//        if (isCaseFolded()) {
//            if (builder.length() != 0) {
//                builder.append(" | ");
//            }
//            builder.append("CASE_FOLDED");
//        }
//        if (isBoolean()) {
//            if (builder.length() != 0) {
//                builder.append(" | ");
//            }
//            builder.append("BOOLEAN");
//        }
//        if (builder.length() == 0) {
//            return "0";
//        }
//        return builder.toString();
//    }
//
//    public int compareTo(AttributeName other) {
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
//    private String formatPrefix() {
//        if (prefix[0] == null && prefix[1] == null && prefix[2] == null
//                && prefix[3] == null) {
//            return "ALL_NO_PREFIX";
//        } else if (prefix[0] == null && prefix[1] == prefix[2]
//                && prefix[3] == null) {
//            if ("xmlns".equals(prefix[1])) {
//                return "XMLNS_PREFIX";
//            } else if ("xml".equals(prefix[1])) {
//                return "XML_PREFIX";
//            } else if ("xlink".equals(prefix[1])) {
//                return "XLINK_PREFIX";
//            } else {
//                throw new IllegalStateException();
//            }
//        } else if (prefix[0] == null && prefix[1] == null && prefix[2] == null
//                && prefix[3] == "xml") {
//            return "LANG_PREFIX";
//        } else {
//            throw new IllegalStateException();
//        }
//    }
//
//    private String formatLocal() {
//        return "\"" + local[0] + "\", \"" + local[1] + "\", \"" + local[2] + "\", \"" + local[3] + "\"";
//    }
//
//    private String formatNs() {
//        if (uri[0] == "" && uri[1] == "" && uri[2] == "" && uri[3] == "") {
//            return "ALL_NO_NS";
//        } else if (uri[0] == "" && uri[1] == uri[2] && uri[3] == "") {
//            if ("http://www.w3.org/2000/xmlns/".equals(uri[1])) {
//                return "XMLNS_NS";
//            } else if ("http://www.w3.org/XML/1998/namespace".equals(uri[1])) {
//                return "XML_NS";
//            } else if ("http://www.w3.org/1999/xlink".equals(uri[1])) {
//                return "XLINK_NS";
//            } else {
//                throw new IllegalStateException();
//            }
//        } else if (uri[0] == "" && uri[1] == "" && uri[2] == ""
//                && uri[3] == "http://www.w3.org/XML/1998/namespace") {
//            return "LANG_NS";
//        } else {
//            throw new IllegalStateException();
//        }
//    }
//
//    private String constName() {
//        String name = getLocal(HTML);
//        char[] buf = new char[name.length()];
//        for (int i = 0; i < name.length(); i++) {
//            char c = name.charAt(i);
//            if (c == '-' || c == ':') {
//                buf[i] = '_';
//            } else if (c >= 'a' && c <= 'z') {
//                buf[i] = (char) (c - 0x20);
//            } else {
//                buf[i] = c;
//            }
//        }
//        return new String(buf);
//    }
//
//    private int hash() {
//        String name = getLocal(HTML);
//        return bufToHash(name.toCharArray(), name.length());
//    }
//
//    private static void fillLevelOrderArray(List<AttributeName> sorted, int depth,
//                                            int rootIdx, AttributeName[] levelOrder) {
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
//     * Regenerate self with: mvn compile exec:java -Dexec.mainClass="nu.validator.htmlparser.impl.AttributeName"
//     *
//     * @param args
//     */
//    public static void main(String[] args) {
//        Arrays.sort(ATTRIBUTE_NAMES);
//        for (int i = 0; i < ATTRIBUTE_NAMES.length; i++) {
//            int hash = ATTRIBUTE_NAMES[i].hash();
//            if (hash < 0) {
//                System.err.println("Negative hash: " + ATTRIBUTE_NAMES[i].local[0]);
//                return;
//            }
//            for (int j = i + 1; j < ATTRIBUTE_NAMES.length; j++) {
//                if (hash == ATTRIBUTE_NAMES[j].hash()) {
//                    System.err.println(
//                            "Hash collision: " + ATTRIBUTE_NAMES[i].local[0] + ", "
//                                    + ATTRIBUTE_NAMES[j].local[0]);
//                    return;
//                }
//            }
//        }
//        for (int i = 0; i < ATTRIBUTE_NAMES.length; i++) {
//            AttributeName att = ATTRIBUTE_NAMES[i];
//            System.out.println("public static final AttributeName "
//                    + att.constName() + " = new AttributeName" + att.toString()
//                    + ";");
//        }
//
//        LinkedList<AttributeName> sortedNames = new LinkedList<AttributeName>();
//        Collections.addAll(sortedNames, ATTRIBUTE_NAMES);
//        AttributeName[] levelOrder = new AttributeName[ATTRIBUTE_NAMES.length];
//        int bstDepth = (int) Math.ceil(Math.log(ATTRIBUTE_NAMES.length) / Math.log(2));
//        fillLevelOrderArray(sortedNames, bstDepth, 0, levelOrder);
//
//        System.out.println("private final static @NoLength AttributeName[] ATTRIBUTE_NAMES = {");
//        for (int i = 0; i < levelOrder.length; i++) {
//            AttributeName att = levelOrder[i];
//            System.out.println(att.constName() + ",");
//        }
//        System.out.println("};");
//        System.out.println("private final static int[] ATTRIBUTE_HASHES = {");
//        for (int i = 0; i < levelOrder.length; i++) {
//            AttributeName att = levelOrder[i];
//            System.out.println(Integer.toString(att.hash()) + ",");
//        }
//        System.out.println("};");
//    }

    // START GENERATED CODE
    public static final AttributeName ALT = new AttributeName(ALL_NO_NS, "alt", "alt", "alt", "alt", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DIR = new AttributeName(ALL_NO_NS, "dir", "dir", "dir", "dir", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName DUR = new AttributeName(ALL_NO_NS, "dur", "dur", "dur", "dur", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName END = new AttributeName(ALL_NO_NS, "end", "end", "end", "end", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FOR = new AttributeName(ALL_NO_NS, "for", "for", "for", "for", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IN2 = new AttributeName(ALL_NO_NS, "in2", "in2", "in2", "in2", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOW = new AttributeName(ALL_NO_NS, "low", "low", "low", "low", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MIN = new AttributeName(ALL_NO_NS, "min", "min", "min", "min", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MAX = new AttributeName(ALL_NO_NS, "max", "max", "max", "max", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REL = new AttributeName(ALL_NO_NS, "rel", "rel", "rel", "rel", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REV = new AttributeName(ALL_NO_NS, "rev", "rev", "rev", "rev", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SRC = new AttributeName(ALL_NO_NS, "src", "src", "src", "src", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName D = new AttributeName(ALL_NO_NS, "d", "d", "d", "d", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName R = new AttributeName(ALL_NO_NS, "r", "r", "r", "r", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X = new AttributeName(ALL_NO_NS, "x", "x", "x", "x", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y = new AttributeName(ALL_NO_NS, "y", "y", "y", "y", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Z = new AttributeName(ALL_NO_NS, "z", "z", "z", "z", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K1 = new AttributeName(ALL_NO_NS, "k1", "k1", "k1", "k1", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X1 = new AttributeName(ALL_NO_NS, "x1", "x1", "x1", "x1", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y1 = new AttributeName(ALL_NO_NS, "y1", "y1", "y1", "y1", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K2 = new AttributeName(ALL_NO_NS, "k2", "k2", "k2", "k2", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X2 = new AttributeName(ALL_NO_NS, "x2", "x2", "x2", "x2", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y2 = new AttributeName(ALL_NO_NS, "y2", "y2", "y2", "y2", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K3 = new AttributeName(ALL_NO_NS, "k3", "k3", "k3", "k3", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K4 = new AttributeName(ALL_NO_NS, "k4", "k4", "k4", "k4", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XML_SPACE = new AttributeName(XML_NS, "xml:space", "space", "space", "xml:space", XML_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XML_LANG = new AttributeName(XML_NS, "xml:lang", "lang", "lang", "xml:lang", XML_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName ARIA_GRAB = new AttributeName(ALL_NO_NS, "aria-grab", "aria-grab", "aria-grab", "aria-grab", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_VALUEMAX = new AttributeName(ALL_NO_NS, "aria-valuemax", "aria-valuemax", "aria-valuemax", "aria-valuemax", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_LABELLEDBY = new AttributeName(ALL_NO_NS, "aria-labelledby", "aria-labelledby", "aria-labelledby", "aria-labelledby", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DESCRIBEDBY = new AttributeName(ALL_NO_NS, "aria-describedby", "aria-describedby", "aria-describedby", "aria-describedby", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DISABLED = new AttributeName(ALL_NO_NS, "aria-disabled", "aria-disabled", "aria-disabled", "aria-disabled", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_CHECKED = new AttributeName(ALL_NO_NS, "aria-checked", "aria-checked", "aria-checked", "aria-checked", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SELECTED = new AttributeName(ALL_NO_NS, "aria-selected", "aria-selected", "aria-selected", "aria-selected", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DROPEFFECT = new AttributeName(ALL_NO_NS, "aria-dropeffect", "aria-dropeffect", "aria-dropeffect", "aria-dropeffect", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_REQUIRED = new AttributeName(ALL_NO_NS, "aria-required", "aria-required", "aria-required", "aria-required", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_EXPANDED = new AttributeName(ALL_NO_NS, "aria-expanded", "aria-expanded", "aria-expanded", "aria-expanded", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_PRESSED = new AttributeName(ALL_NO_NS, "aria-pressed", "aria-pressed", "aria-pressed", "aria-pressed", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_LEVEL = new AttributeName(ALL_NO_NS, "aria-level", "aria-level", "aria-level", "aria-level", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_CHANNEL = new AttributeName(ALL_NO_NS, "aria-channel", "aria-channel", "aria-channel", "aria-channel", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_HIDDEN = new AttributeName(ALL_NO_NS, "aria-hidden", "aria-hidden", "aria-hidden", "aria-hidden", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SECRET = new AttributeName(ALL_NO_NS, "aria-secret", "aria-secret", "aria-secret", "aria-secret", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_POSINSET = new AttributeName(ALL_NO_NS, "aria-posinset", "aria-posinset", "aria-posinset", "aria-posinset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_ATOMIC = new AttributeName(ALL_NO_NS, "aria-atomic", "aria-atomic", "aria-atomic", "aria-atomic", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_INVALID = new AttributeName(ALL_NO_NS, "aria-invalid", "aria-invalid", "aria-invalid", "aria-invalid", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_TEMPLATEID = new AttributeName(ALL_NO_NS, "aria-templateid", "aria-templateid", "aria-templateid", "aria-templateid", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_VALUEMIN = new AttributeName(ALL_NO_NS, "aria-valuemin", "aria-valuemin", "aria-valuemin", "aria-valuemin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_MULTISELECTABLE = new AttributeName(ALL_NO_NS, "aria-multiselectable", "aria-multiselectable", "aria-multiselectable", "aria-multiselectable", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_CONTROLS = new AttributeName(ALL_NO_NS, "aria-controls", "aria-controls", "aria-controls", "aria-controls", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_MULTILINE = new AttributeName(ALL_NO_NS, "aria-multiline", "aria-multiline", "aria-multiline", "aria-multiline", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_READONLY = new AttributeName(ALL_NO_NS, "aria-readonly", "aria-readonly", "aria-readonly", "aria-readonly", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_OWNS = new AttributeName(ALL_NO_NS, "aria-owns", "aria-owns", "aria-owns", "aria-owns", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_ACTIVEDESCENDANT = new AttributeName(ALL_NO_NS, "aria-activedescendant", "aria-activedescendant", "aria-activedescendant", "aria-activedescendant", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_RELEVANT = new AttributeName(ALL_NO_NS, "aria-relevant", "aria-relevant", "aria-relevant", "aria-relevant", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DATATYPE = new AttributeName(ALL_NO_NS, "aria-datatype", "aria-datatype", "aria-datatype", "aria-datatype", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_VALUENOW = new AttributeName(ALL_NO_NS, "aria-valuenow", "aria-valuenow", "aria-valuenow", "aria-valuenow", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SORT = new AttributeName(ALL_NO_NS, "aria-sort", "aria-sort", "aria-sort", "aria-sort", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_AUTOCOMPLETE = new AttributeName(ALL_NO_NS, "aria-autocomplete", "aria-autocomplete", "aria-autocomplete", "aria-autocomplete", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_FLOWTO = new AttributeName(ALL_NO_NS, "aria-flowto", "aria-flowto", "aria-flowto", "aria-flowto", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_BUSY = new AttributeName(ALL_NO_NS, "aria-busy", "aria-busy", "aria-busy", "aria-busy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_LIVE = new AttributeName(ALL_NO_NS, "aria-live", "aria-live", "aria-live", "aria-live", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_HASPOPUP = new AttributeName(ALL_NO_NS, "aria-haspopup", "aria-haspopup", "aria-haspopup", "aria-haspopup", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SETSIZE = new AttributeName(ALL_NO_NS, "aria-setsize", "aria-setsize", "aria-setsize", "aria-setsize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLEAR = new AttributeName(ALL_NO_NS, "clear", "clear", "clear", "clear", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName DISABLED = new AttributeName(ALL_NO_NS, "disabled", "disabled", "disabled", "disabled", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DEFAULT = new AttributeName(ALL_NO_NS, "default", "default", "default", "default", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DATA = new AttributeName(ALL_NO_NS, "data", "data", "data", "data", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EQUALCOLUMNS = new AttributeName(ALL_NO_NS, "equalcolumns", "equalcolumns", "equalcolumns", "equalcolumns", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EQUALROWS = new AttributeName(ALL_NO_NS, "equalrows", "equalrows", "equalrows", "equalrows", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HSPACE = new AttributeName(ALL_NO_NS, "hspace", "hspace", "hspace", "hspace", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ISMAP = new AttributeName(ALL_NO_NS, "ismap", "ismap", "ismap", "ismap", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName LOCAL = new AttributeName(ALL_NO_NS, "local", "local", "local", "local", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LSPACE = new AttributeName(ALL_NO_NS, "lspace", "lspace", "lspace", "lspace", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MOVABLELIMITS = new AttributeName(ALL_NO_NS, "movablelimits", "movablelimits", "movablelimits", "movablelimits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOTATION = new AttributeName(ALL_NO_NS, "notation", "notation", "notation", "notation", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDATAAVAILABLE = new AttributeName(ALL_NO_NS, "ondataavailable", "ondataavailable", "ondataavailable", "ondataavailable", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONPASTE = new AttributeName(ALL_NO_NS, "onpaste", "onpaste", "onpaste", "onpaste", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RSPACE = new AttributeName(ALL_NO_NS, "rspace", "rspace", "rspace", "rspace", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWALIGN = new AttributeName(ALL_NO_NS, "rowalign", "rowalign", "rowalign", "rowalign", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROTATE = new AttributeName(ALL_NO_NS, "rotate", "rotate", "rotate", "rotate", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEPARATOR = new AttributeName(ALL_NO_NS, "separator", "separator", "separator", "separator", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEPARATORS = new AttributeName(ALL_NO_NS, "separators", "separators", "separators", "separators", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VSPACE = new AttributeName(ALL_NO_NS, "vspace", "vspace", "vspace", "vspace", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XCHANNELSELECTOR = new AttributeName(ALL_NO_NS, "xchannelselector", "xchannelselector", "xChannelSelector", "xchannelselector", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName YCHANNELSELECTOR = new AttributeName(ALL_NO_NS, "ychannelselector", "ychannelselector", "yChannelSelector", "ychannelselector", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENABLE_BACKGROUND = new AttributeName(ALL_NO_NS, "enable-background", "enable-background", "enable-background", "enable-background", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDBLCLICK = new AttributeName(ALL_NO_NS, "ondblclick", "ondblclick", "ondblclick", "ondblclick", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONABORT = new AttributeName(ALL_NO_NS, "onabort", "onabort", "onabort", "onabort", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CALCMODE = new AttributeName(ALL_NO_NS, "calcmode", "calcmode", "calcMode", "calcmode", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHECKED = new AttributeName(ALL_NO_NS, "checked", "checked", "checked", "checked", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName FENCE = new AttributeName(ALL_NO_NS, "fence", "fence", "fence", "fence", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FETCHPRIORITY = new AttributeName(ALL_NO_NS, "fetchpriority", "fetchpriority", "fetchpriority", "fetchpriority", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NONCE = new AttributeName(ALL_NO_NS, "nonce", "nonce", "nonce", "nonce", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSCROLL = new AttributeName(ALL_NO_NS, "onscroll", "onscroll", "onscroll", "onscroll", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONACTIVATE = new AttributeName(ALL_NO_NS, "onactivate", "onactivate", "onactivate", "onactivate", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPACITY = new AttributeName(ALL_NO_NS, "opacity", "opacity", "opacity", "opacity", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPACING = new AttributeName(ALL_NO_NS, "spacing", "spacing", "spacing", "spacing", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPECULAREXPONENT = new AttributeName(ALL_NO_NS, "specularexponent", "specularexponent", "specularExponent", "specularexponent", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPECULARCONSTANT = new AttributeName(ALL_NO_NS, "specularconstant", "specularconstant", "specularConstant", "specularconstant", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BORDER = new AttributeName(ALL_NO_NS, "border", "border", "border", "border", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ID = new AttributeName(ALL_NO_NS, "id", "id", "id", "id", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GRADIENTTRANSFORM = new AttributeName(ALL_NO_NS, "gradienttransform", "gradienttransform", "gradientTransform", "gradienttransform", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GRADIENTUNITS = new AttributeName(ALL_NO_NS, "gradientunits", "gradientunits", "gradientUnits", "gradientunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HIDDEN = new AttributeName(ALL_NO_NS, "hidden", "hidden", "hidden", "hidden", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HEADERS = new AttributeName(ALL_NO_NS, "headers", "headers", "headers", "headers", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOADING = new AttributeName(ALL_NO_NS, "loading", "loading", "loading", "loading", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName READONLY = new AttributeName(ALL_NO_NS, "readonly", "readonly", "readonly", "readonly", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName RENDERING_INTENT = new AttributeName(ALL_NO_NS, "rendering-intent", "rendering-intent", "rendering-intent", "rendering-intent", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEED = new AttributeName(ALL_NO_NS, "seed", "seed", "seed", "seed", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SRCDOC = new AttributeName(ALL_NO_NS, "srcdoc", "srcdoc", "srcdoc", "srcdoc", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STDDEVIATION = new AttributeName(ALL_NO_NS, "stddeviation", "stddeviation", "stdDeviation", "stddeviation", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SANDBOX = new AttributeName(ALL_NO_NS, "sandbox", "sandbox", "sandbox", "sandbox", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WORD_SPACING = new AttributeName(ALL_NO_NS, "word-spacing", "word-spacing", "word-spacing", "word-spacing", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCENTUNDER = new AttributeName(ALL_NO_NS, "accentunder", "accentunder", "accentunder", "accentunder", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCEPT_CHARSET = new AttributeName(ALL_NO_NS, "accept-charset", "accept-charset", "accept-charset", "accept-charset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCESSKEY = new AttributeName(ALL_NO_NS, "accesskey", "accesskey", "accesskey", "accesskey", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCENT = new AttributeName(ALL_NO_NS, "accent", "accent", "accent", "accent", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCEPT = new AttributeName(ALL_NO_NS, "accept", "accept", "accept", "accept", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BEVELLED = new AttributeName(ALL_NO_NS, "bevelled", "bevelled", "bevelled", "bevelled", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASEFREQUENCY = new AttributeName(ALL_NO_NS, "basefrequency", "basefrequency", "baseFrequency", "basefrequency", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASELINE_SHIFT = new AttributeName(ALL_NO_NS, "baseline-shift", "baseline-shift", "baseline-shift", "baseline-shift", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASEPROFILE = new AttributeName(ALL_NO_NS, "baseprofile", "baseprofile", "baseProfile", "baseprofile", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASELINE = new AttributeName(ALL_NO_NS, "baseline", "baseline", "baseline", "baseline", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASE = new AttributeName(ALL_NO_NS, "base", "base", "base", "base", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CODE = new AttributeName(ALL_NO_NS, "code", "code", "code", "code", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CODETYPE = new AttributeName(ALL_NO_NS, "codetype", "codetype", "codetype", "codetype", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CODEBASE = new AttributeName(ALL_NO_NS, "codebase", "codebase", "codebase", "codebase", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CITE = new AttributeName(ALL_NO_NS, "cite", "cite", "cite", "cite", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DEFER = new AttributeName(ALL_NO_NS, "defer", "defer", "defer", "defer", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DATETIME = new AttributeName(ALL_NO_NS, "datetime", "datetime", "datetime", "datetime", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DIRECTION = new AttributeName(ALL_NO_NS, "direction", "direction", "direction", "direction", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EDGEMODE = new AttributeName(ALL_NO_NS, "edgemode", "edgemode", "edgeMode", "edgemode", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EDGE = new AttributeName(ALL_NO_NS, "edge", "edge", "edge", "edge", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENTERKEYHINT = new AttributeName(ALL_NO_NS, "enterkeyhint", "enterkeyhint", "enterkeyhint", "enterkeyhint", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FACE = new AttributeName(ALL_NO_NS, "face", "face", "face", "face", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INDEX = new AttributeName(ALL_NO_NS, "index", "index", "index", "index", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INTERCEPT = new AttributeName(ALL_NO_NS, "intercept", "intercept", "intercept", "intercept", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INTEGRITY = new AttributeName(ALL_NO_NS, "integrity", "integrity", "integrity", "integrity", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LINEBREAK = new AttributeName(ALL_NO_NS, "linebreak", "linebreak", "linebreak", "linebreak", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LABEL = new AttributeName(ALL_NO_NS, "label", "label", "label", "label", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LINETHICKNESS = new AttributeName(ALL_NO_NS, "linethickness", "linethickness", "linethickness", "linethickness", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MODE = new AttributeName(ALL_NO_NS, "mode", "mode", "mode", "mode", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NAME = new AttributeName(ALL_NO_NS, "name", "name", "name", "name", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NORESIZE = new AttributeName(ALL_NO_NS, "noresize", "noresize", "noresize", "noresize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONBEFOREUNLOAD = new AttributeName(ALL_NO_NS, "onbeforeunload", "onbeforeunload", "onbeforeunload", "onbeforeunload", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONREPEAT = new AttributeName(ALL_NO_NS, "onrepeat", "onrepeat", "onrepeat", "onrepeat", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OBJECT = new AttributeName(ALL_NO_NS, "object", "object", "object", "object", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSELECT = new AttributeName(ALL_NO_NS, "onselect", "onselect", "onselect", "onselect", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORDER = new AttributeName(ALL_NO_NS, "order", "order", "order", "order", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OTHER = new AttributeName(ALL_NO_NS, "other", "other", "other", "other", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONRESET = new AttributeName(ALL_NO_NS, "onreset", "onreset", "onreset", "onreset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONREADYSTATECHANGE = new AttributeName(ALL_NO_NS, "onreadystatechange", "onreadystatechange", "onreadystatechange", "onreadystatechange", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMESSAGE = new AttributeName(ALL_NO_NS, "onmessage", "onmessage", "onmessage", "onmessage", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEGIN = new AttributeName(ALL_NO_NS, "onbegin", "onbegin", "onbegin", "onbegin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREPRINT = new AttributeName(ALL_NO_NS, "onbeforeprint", "onbeforeprint", "onbeforeprint", "onbeforeprint", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIENT = new AttributeName(ALL_NO_NS, "orient", "orient", "orient", "orient", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIENTATION = new AttributeName(ALL_NO_NS, "orientation", "orientation", "orientation", "orientation", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFORECOPY = new AttributeName(ALL_NO_NS, "onbeforecopy", "onbeforecopy", "onbeforecopy", "onbeforecopy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSELECTSTART = new AttributeName(ALL_NO_NS, "onselectstart", "onselectstart", "onselectstart", "onselectstart", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREPASTE = new AttributeName(ALL_NO_NS, "onbeforepaste", "onbeforepaste", "onbeforepaste", "onbeforepaste", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYPRESS = new AttributeName(ALL_NO_NS, "onkeypress", "onkeypress", "onkeypress", "onkeypress", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYUP = new AttributeName(ALL_NO_NS, "onkeyup", "onkeyup", "onkeyup", "onkeyup", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFORECUT = new AttributeName(ALL_NO_NS, "onbeforecut", "onbeforecut", "onbeforecut", "onbeforecut", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYDOWN = new AttributeName(ALL_NO_NS, "onkeydown", "onkeydown", "onkeydown", "onkeydown", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONRESIZE = new AttributeName(ALL_NO_NS, "onresize", "onresize", "onresize", "onresize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEAT = new AttributeName(ALL_NO_NS, "repeat", "repeat", "repeat", "repeat", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFERRERPOLICY = new AttributeName(ALL_NO_NS, "referrerpolicy", "referrerpolicy", "referrerpolicy", "referrerpolicy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RULES = new AttributeName(ALL_NO_NS, "rules", "rules", "rules", "rules", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName ROLE = new AttributeName(ALL_NO_NS, "role", "role", "role", "role", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEATCOUNT = new AttributeName(ALL_NO_NS, "repeatcount", "repeatcount", "repeatCount", "repeatcount", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEATDUR = new AttributeName(ALL_NO_NS, "repeatdur", "repeatdur", "repeatDur", "repeatdur", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SELECTED = new AttributeName(ALL_NO_NS, "selected", "selected", "selected", "selected", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName SIZES = new AttributeName(ALL_NO_NS, "sizes", "sizes", "sizes", "sizes", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SUPERSCRIPTSHIFT = new AttributeName(ALL_NO_NS, "superscriptshift", "superscriptshift", "superscriptshift", "superscriptshift", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STRETCHY = new AttributeName(ALL_NO_NS, "stretchy", "stretchy", "stretchy", "stretchy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCHEME = new AttributeName(ALL_NO_NS, "scheme", "scheme", "scheme", "scheme", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPREADMETHOD = new AttributeName(ALL_NO_NS, "spreadmethod", "spreadmethod", "spreadMethod", "spreadmethod", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SELECTION = new AttributeName(ALL_NO_NS, "selection", "selection", "selection", "selection", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SIZE = new AttributeName(ALL_NO_NS, "size", "size", "size", "size", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TYPE = new AttributeName(ALL_NO_NS, "type", "type", "type", "type", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName DIFFUSECONSTANT = new AttributeName(ALL_NO_NS, "diffuseconstant", "diffuseconstant", "diffuseConstant", "diffuseconstant", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HREF = new AttributeName(ALL_NO_NS, "href", "href", "href", "href", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HREFLANG = new AttributeName(ALL_NO_NS, "hreflang", "hreflang", "hreflang", "hreflang", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONAFTERPRINT = new AttributeName(ALL_NO_NS, "onafterprint", "onafterprint", "onafterprint", "onafterprint", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PROFILE = new AttributeName(ALL_NO_NS, "profile", "profile", "profile", "profile", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SURFACESCALE = new AttributeName(ALL_NO_NS, "surfacescale", "surfacescale", "surfaceScale", "surfacescale", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XREF = new AttributeName(ALL_NO_NS, "xref", "xref", "xref", "xref", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALIGN = new AttributeName(ALL_NO_NS, "align", "align", "align", "align", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName ALIGNMENT_BASELINE = new AttributeName(ALL_NO_NS, "alignment-baseline", "alignment-baseline", "alignment-baseline", "alignment-baseline", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALIGNMENTSCOPE = new AttributeName(ALL_NO_NS, "alignmentscope", "alignmentscope", "alignmentscope", "alignmentscope", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DRAGGABLE = new AttributeName(ALL_NO_NS, "draggable", "draggable", "draggable", "draggable", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HEIGHT = new AttributeName(ALL_NO_NS, "height", "height", "height", "height", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IMAGESIZES = new AttributeName(ALL_NO_NS, "imagesizes", "imagesizes", "imagesizes", "imagesizes", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IMAGESRCSET = new AttributeName(ALL_NO_NS, "imagesrcset", "imagesrcset", "imagesrcset", "imagesrcset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IMAGE_RENDERING = new AttributeName(ALL_NO_NS, "image-rendering", "image-rendering", "image-rendering", "image-rendering", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LANGUAGE = new AttributeName(ALL_NO_NS, "language", "language", "language", "language", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LANG = new AttributeName(LANG_NS, "lang", "lang", "lang", "lang", LANG_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LARGEOP = new AttributeName(ALL_NO_NS, "largeop", "largeop", "largeop", "largeop", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LONGDESC = new AttributeName(ALL_NO_NS, "longdesc", "longdesc", "longdesc", "longdesc", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LENGTHADJUST = new AttributeName(ALL_NO_NS, "lengthadjust", "lengthadjust", "lengthAdjust", "lengthadjust", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARGINHEIGHT = new AttributeName(ALL_NO_NS, "marginheight", "marginheight", "marginheight", "marginheight", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARGINWIDTH = new AttributeName(ALL_NO_NS, "marginwidth", "marginwidth", "marginwidth", "marginwidth", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIGIN = new AttributeName(ALL_NO_NS, "origin", "origin", "origin", "origin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PING = new AttributeName(ALL_NO_NS, "ping", "ping", "ping", "ping", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGET = new AttributeName(ALL_NO_NS, "target", "target", "target", "target", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGETX = new AttributeName(ALL_NO_NS, "targetx", "targetx", "targetX", "targetx", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGETY = new AttributeName(ALL_NO_NS, "targety", "targety", "targetY", "targety", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARCHIVE = new AttributeName(ALL_NO_NS, "archive", "archive", "archive", "archive", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HIGH = new AttributeName(ALL_NO_NS, "high", "high", "high", "high", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIGHTING_COLOR = new AttributeName(ALL_NO_NS, "lighting-color", "lighting-color", "lighting-color", "lighting-color", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MATHBACKGROUND = new AttributeName(ALL_NO_NS, "mathbackground", "mathbackground", "mathbackground", "mathbackground", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName METHOD = new AttributeName(ALL_NO_NS, "method", "method", "method", "method", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName MATHVARIANT = new AttributeName(ALL_NO_NS, "mathvariant", "mathvariant", "mathvariant", "mathvariant", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MATHCOLOR = new AttributeName(ALL_NO_NS, "mathcolor", "mathcolor", "mathcolor", "mathcolor", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MATHSIZE = new AttributeName(ALL_NO_NS, "mathsize", "mathsize", "mathsize", "mathsize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOSHADE = new AttributeName(ALL_NO_NS, "noshade", "noshade", "noshade", "noshade", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONCHANGE = new AttributeName(ALL_NO_NS, "onchange", "onchange", "onchange", "onchange", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATHLENGTH = new AttributeName(ALL_NO_NS, "pathlength", "pathlength", "pathLength", "pathlength", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATH = new AttributeName(ALL_NO_NS, "path", "path", "path", "path", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALTIMG = new AttributeName(ALL_NO_NS, "altimg", "altimg", "altimg", "altimg", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACTIONTYPE = new AttributeName(ALL_NO_NS, "actiontype", "actiontype", "actiontype", "actiontype", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACTION = new AttributeName(ALL_NO_NS, "action", "action", "action", "action", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACTIVE = new AttributeName(ALL_NO_NS, "active", "active", "active", "active", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ADDITIVE = new AttributeName(ALL_NO_NS, "additive", "additive", "additive", "additive", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BEGIN = new AttributeName(ALL_NO_NS, "begin", "begin", "begin", "begin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DOMINANT_BASELINE = new AttributeName(ALL_NO_NS, "dominant-baseline", "dominant-baseline", "dominant-baseline", "dominant-baseline", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DIVISOR = new AttributeName(ALL_NO_NS, "divisor", "divisor", "divisor", "divisor", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DEFINITIONURL = new AttributeName(ALL_NO_NS, "definitionurl", "definitionURL", "definitionurl", "definitionurl", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIMITINGCONEANGLE = new AttributeName(ALL_NO_NS, "limitingconeangle", "limitingconeangle", "limitingConeAngle", "limitingconeangle", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MEDIA = new AttributeName(ALL_NO_NS, "media", "media", "media", "media", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MANIFEST = new AttributeName(ALL_NO_NS, "manifest", "manifest", "manifest", "manifest", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFINISH = new AttributeName(ALL_NO_NS, "onfinish", "onfinish", "onfinish", "onfinish", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPTIMUM = new AttributeName(ALL_NO_NS, "optimum", "optimum", "optimum", "optimum", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RADIOGROUP = new AttributeName(ALL_NO_NS, "radiogroup", "radiogroup", "radiogroup", "radiogroup", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RADIUS = new AttributeName(ALL_NO_NS, "radius", "radius", "radius", "radius", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTLEVEL = new AttributeName(ALL_NO_NS, "scriptlevel", "scriptlevel", "scriptlevel", "scriptlevel", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTSIZEMULTIPLIER = new AttributeName(ALL_NO_NS, "scriptsizemultiplier", "scriptsizemultiplier", "scriptsizemultiplier", "scriptsizemultiplier", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTMINSIZE = new AttributeName(ALL_NO_NS, "scriptminsize", "scriptminsize", "scriptminsize", "scriptminsize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TABINDEX = new AttributeName(ALL_NO_NS, "tabindex", "tabindex", "tabindex", "tabindex", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VALIGN = new AttributeName(ALL_NO_NS, "valign", "valign", "valign", "valign", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName VISIBILITY = new AttributeName(ALL_NO_NS, "visibility", "visibility", "visibility", "visibility", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BACKGROUND = new AttributeName(ALL_NO_NS, "background", "background", "background", "background", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LINK = new AttributeName(ALL_NO_NS, "link", "link", "link", "link", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKER_MID = new AttributeName(ALL_NO_NS, "marker-mid", "marker-mid", "marker-mid", "marker-mid", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKERHEIGHT = new AttributeName(ALL_NO_NS, "markerheight", "markerheight", "markerHeight", "markerheight", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKER_END = new AttributeName(ALL_NO_NS, "marker-end", "marker-end", "marker-end", "marker-end", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MASK = new AttributeName(ALL_NO_NS, "mask", "mask", "mask", "mask", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKER_START = new AttributeName(ALL_NO_NS, "marker-start", "marker-start", "marker-start", "marker-start", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKERWIDTH = new AttributeName(ALL_NO_NS, "markerwidth", "markerwidth", "markerWidth", "markerwidth", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MASKUNITS = new AttributeName(ALL_NO_NS, "maskunits", "maskunits", "maskUnits", "maskunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKERUNITS = new AttributeName(ALL_NO_NS, "markerunits", "markerunits", "markerUnits", "markerunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MASKCONTENTUNITS = new AttributeName(ALL_NO_NS, "maskcontentunits", "maskcontentunits", "maskContentUnits", "maskcontentunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AMPLITUDE = new AttributeName(ALL_NO_NS, "amplitude", "amplitude", "amplitude", "amplitude", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CELLSPACING = new AttributeName(ALL_NO_NS, "cellspacing", "cellspacing", "cellspacing", "cellspacing", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CELLPADDING = new AttributeName(ALL_NO_NS, "cellpadding", "cellpadding", "cellpadding", "cellpadding", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DECLARE = new AttributeName(ALL_NO_NS, "declare", "declare", "declare", "declare", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName FILL_RULE = new AttributeName(ALL_NO_NS, "fill-rule", "fill-rule", "fill-rule", "fill-rule", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILL = new AttributeName(ALL_NO_NS, "fill", "fill", "fill", "fill", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILL_OPACITY = new AttributeName(ALL_NO_NS, "fill-opacity", "fill-opacity", "fill-opacity", "fill-opacity", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MAXLENGTH = new AttributeName(ALL_NO_NS, "maxlength", "maxlength", "maxlength", "maxlength", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCLICK = new AttributeName(ALL_NO_NS, "onclick", "onclick", "onclick", "onclick", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBLUR = new AttributeName(ALL_NO_NS, "onblur", "onblur", "onblur", "onblur", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPLACE = new AttributeName(ALL_NO_NS, "replace", "replace", "replace", "replace", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName ROWLINES = new AttributeName(ALL_NO_NS, "rowlines", "rowlines", "rowlines", "rowlines", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCALE = new AttributeName(ALL_NO_NS, "scale", "scale", "scale", "scale", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STYLE = new AttributeName(ALL_NO_NS, "style", "style", "style", "style", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TABLEVALUES = new AttributeName(ALL_NO_NS, "tablevalues", "tablevalues", "tableValues", "tablevalues", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TITLE = new AttributeName(ALL_NO_NS, "title", "title", "title", "title", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AZIMUTH = new AttributeName(ALL_NO_NS, "azimuth", "azimuth", "azimuth", "azimuth", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FORMAT = new AttributeName(ALL_NO_NS, "format", "format", "format", "format", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FRAMEBORDER = new AttributeName(ALL_NO_NS, "frameborder", "frameborder", "frameborder", "frameborder", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FRAME = new AttributeName(ALL_NO_NS, "frame", "frame", "frame", "frame", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName FRAMESPACING = new AttributeName(ALL_NO_NS, "framespacing", "framespacing", "framespacing", "framespacing", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FROM = new AttributeName(ALL_NO_NS, "from", "from", "from", "from", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FORM = new AttributeName(ALL_NO_NS, "form", "form", "form", "form", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PROMPT = new AttributeName(ALL_NO_NS, "prompt", "prompt", "prompt", "prompt", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PRIMITIVEUNITS = new AttributeName(ALL_NO_NS, "primitiveunits", "primitiveunits", "primitiveUnits", "primitiveunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SYMMETRIC = new AttributeName(ALL_NO_NS, "symmetric", "symmetric", "symmetric", "symmetric", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SUMMARY = new AttributeName(ALL_NO_NS, "summary", "summary", "summary", "summary", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName USEMAP = new AttributeName(ALL_NO_NS, "usemap", "usemap", "usemap", "usemap", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ZOOMANDPAN = new AttributeName(ALL_NO_NS, "zoomandpan", "zoomandpan", "zoomAndPan", "zoomandpan", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ASYNC = new AttributeName(ALL_NO_NS, "async", "async", "async", "async", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ALINK = new AttributeName(ALL_NO_NS, "alink", "alink", "alink", "alink", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IN = new AttributeName(ALL_NO_NS, "in", "in", "in", "in", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ICON = new AttributeName(ALL_NO_NS, "icon", "icon", "icon", "icon", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KERNELMATRIX = new AttributeName(ALL_NO_NS, "kernelmatrix", "kernelmatrix", "kernelMatrix", "kernelmatrix", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KERNING = new AttributeName(ALL_NO_NS, "kerning", "kerning", "kerning", "kerning", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KERNELUNITLENGTH = new AttributeName(ALL_NO_NS, "kernelunitlength", "kernelunitlength", "kernelUnitLength", "kernelunitlength", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONUNLOAD = new AttributeName(ALL_NO_NS, "onunload", "onunload", "onunload", "onunload", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPEN = new AttributeName(ALL_NO_NS, "open", "open", "open", "open", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONINVALID = new AttributeName(ALL_NO_NS, "oninvalid", "oninvalid", "oninvalid", "oninvalid", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONEND = new AttributeName(ALL_NO_NS, "onend", "onend", "onend", "onend", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONINPUT = new AttributeName(ALL_NO_NS, "oninput", "oninput", "oninput", "oninput", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTER_EVENTS = new AttributeName(ALL_NO_NS, "pointer-events", "pointer-events", "pointer-events", "pointer-events", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTS = new AttributeName(ALL_NO_NS, "points", "points", "points", "points", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTSATX = new AttributeName(ALL_NO_NS, "pointsatx", "pointsatx", "pointsAtX", "pointsatx", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTSATY = new AttributeName(ALL_NO_NS, "pointsaty", "pointsaty", "pointsAtY", "pointsaty", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTSATZ = new AttributeName(ALL_NO_NS, "pointsatz", "pointsatz", "pointsAtZ", "pointsatz", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPAN = new AttributeName(ALL_NO_NS, "span", "span", "span", "span", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STANDBY = new AttributeName(ALL_NO_NS, "standby", "standby", "standby", "standby", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TRANSFORM_ORIGIN = new AttributeName(ALL_NO_NS, "transform-origin", "transform-origin", "transform-origin", "transform-origin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TRANSFORM = new AttributeName(ALL_NO_NS, "transform", "transform", "transform", "transform", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VLINK = new AttributeName(ALL_NO_NS, "vlink", "vlink", "vlink", "vlink", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WHEN = new AttributeName(ALL_NO_NS, "when", "when", "when", "when", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XLINK_HREF = new AttributeName(XLINK_NS, "xlink:href", "href", "href", "xlink:href", XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_TITLE = new AttributeName(XLINK_NS, "xlink:title", "title", "title", "xlink:title", XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_ROLE = new AttributeName(XLINK_NS, "xlink:role", "role", "role", "xlink:role", XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_ARCROLE = new AttributeName(XLINK_NS, "xlink:arcrole", "arcrole", "arcrole", "xlink:arcrole", XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XMLNS_XLINK = new AttributeName(XMLNS_NS, "xmlns:xlink", "xlink", "xlink", "xmlns:xlink", XMLNS_PREFIX, IS_XMLNS);
    public static final AttributeName XMLNS = new AttributeName(XMLNS_NS, "xmlns", "xmlns", "xmlns", "xmlns", ALL_NO_PREFIX, IS_XMLNS);
    public static final AttributeName XLINK_TYPE = new AttributeName(XLINK_NS, "xlink:type", "type", "type", "xlink:type", XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_SHOW = new AttributeName(XLINK_NS, "xlink:show", "show", "show", "xlink:show", XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_ACTUATE = new AttributeName(XLINK_NS, "xlink:actuate", "actuate", "actuate", "xlink:actuate", XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName AUTOPLAY = new AttributeName(ALL_NO_NS, "autoplay", "autoplay", "autoplay", "autoplay", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AUTOCOMPLETE = new AttributeName(ALL_NO_NS, "autocomplete", "autocomplete", "autocomplete", "autocomplete", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName AUTOFOCUS = new AttributeName(ALL_NO_NS, "autofocus", "autofocus", "autofocus", "autofocus", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName AUTOCAPITALIZE = new AttributeName(ALL_NO_NS, "autocapitalize", "autocapitalize", "autocapitalize", "autocapitalize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BGCOLOR = new AttributeName(ALL_NO_NS, "bgcolor", "bgcolor", "bgcolor", "bgcolor", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_PROFILE = new AttributeName(ALL_NO_NS, "color-profile", "color-profile", "color-profile", "color-profile", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_RENDERING = new AttributeName(ALL_NO_NS, "color-rendering", "color-rendering", "color-rendering", "color-rendering", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_INTERPOLATION = new AttributeName(ALL_NO_NS, "color-interpolation", "color-interpolation", "color-interpolation", "color-interpolation", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR = new AttributeName(ALL_NO_NS, "color", "color", "color", "color", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_INTERPOLATION_FILTERS = new AttributeName(ALL_NO_NS, "color-interpolation-filters", "color-interpolation-filters", "color-interpolation-filters", "color-interpolation-filters", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENCODING = new AttributeName(ALL_NO_NS, "encoding", "encoding", "encoding", "encoding", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EXPONENT = new AttributeName(ALL_NO_NS, "exponent", "exponent", "exponent", "exponent", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FLOOD_COLOR = new AttributeName(ALL_NO_NS, "flood-color", "flood-color", "flood-color", "flood-color", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FLOOD_OPACITY = new AttributeName(ALL_NO_NS, "flood-opacity", "flood-opacity", "flood-opacity", "flood-opacity", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LQUOTE = new AttributeName(ALL_NO_NS, "lquote", "lquote", "lquote", "lquote", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NUMOCTAVES = new AttributeName(ALL_NO_NS, "numoctaves", "numoctaves", "numOctaves", "numoctaves", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOMODULE = new AttributeName(ALL_NO_NS, "nomodule", "nomodule", "nomodule", "nomodule", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONLOAD = new AttributeName(ALL_NO_NS, "onload", "onload", "onload", "onload", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEWHEEL = new AttributeName(ALL_NO_NS, "onmousewheel", "onmousewheel", "onmousewheel", "onmousewheel", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEENTER = new AttributeName(ALL_NO_NS, "onmouseenter", "onmouseenter", "onmouseenter", "onmouseenter", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEOVER = new AttributeName(ALL_NO_NS, "onmouseover", "onmouseover", "onmouseover", "onmouseover", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUSIN = new AttributeName(ALL_NO_NS, "onfocusin", "onfocusin", "onfocusin", "onfocusin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCONTEXTMENU = new AttributeName(ALL_NO_NS, "oncontextmenu", "oncontextmenu", "oncontextmenu", "oncontextmenu", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONZOOM = new AttributeName(ALL_NO_NS, "onzoom", "onzoom", "onzoom", "onzoom", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCOPY = new AttributeName(ALL_NO_NS, "oncopy", "oncopy", "oncopy", "oncopy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSELEAVE = new AttributeName(ALL_NO_NS, "onmouseleave", "onmouseleave", "onmouseleave", "onmouseleave", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEMOVE = new AttributeName(ALL_NO_NS, "onmousemove", "onmousemove", "onmousemove", "onmousemove", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEUP = new AttributeName(ALL_NO_NS, "onmouseup", "onmouseup", "onmouseup", "onmouseup", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUS = new AttributeName(ALL_NO_NS, "onfocus", "onfocus", "onfocus", "onfocus", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEOUT = new AttributeName(ALL_NO_NS, "onmouseout", "onmouseout", "onmouseout", "onmouseout", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUSOUT = new AttributeName(ALL_NO_NS, "onfocusout", "onfocusout", "onfocusout", "onfocusout", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEDOWN = new AttributeName(ALL_NO_NS, "onmousedown", "onmousedown", "onmousedown", "onmousedown", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TO = new AttributeName(ALL_NO_NS, "to", "to", "to", "to", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RQUOTE = new AttributeName(ALL_NO_NS, "rquote", "rquote", "rquote", "rquote", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_LINECAP = new AttributeName(ALL_NO_NS, "stroke-linecap", "stroke-linecap", "stroke-linecap", "stroke-linecap", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_DASHARRAY = new AttributeName(ALL_NO_NS, "stroke-dasharray", "stroke-dasharray", "stroke-dasharray", "stroke-dasharray", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_DASHOFFSET = new AttributeName(ALL_NO_NS, "stroke-dashoffset", "stroke-dashoffset", "stroke-dashoffset", "stroke-dashoffset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_LINEJOIN = new AttributeName(ALL_NO_NS, "stroke-linejoin", "stroke-linejoin", "stroke-linejoin", "stroke-linejoin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_MITERLIMIT = new AttributeName(ALL_NO_NS, "stroke-miterlimit", "stroke-miterlimit", "stroke-miterlimit", "stroke-miterlimit", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE = new AttributeName(ALL_NO_NS, "stroke", "stroke", "stroke", "stroke", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCROLLING = new AttributeName(ALL_NO_NS, "scrolling", "scrolling", "scrolling", "scrolling", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName STROKE_WIDTH = new AttributeName(ALL_NO_NS, "stroke-width", "stroke-width", "stroke-width", "stroke-width", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_OPACITY = new AttributeName(ALL_NO_NS, "stroke-opacity", "stroke-opacity", "stroke-opacity", "stroke-opacity", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COMPACT = new AttributeName(ALL_NO_NS, "compact", "compact", "compact", "compact", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName CLIP = new AttributeName(ALL_NO_NS, "clip", "clip", "clip", "clip", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLIP_RULE = new AttributeName(ALL_NO_NS, "clip-rule", "clip-rule", "clip-rule", "clip-rule", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLIP_PATH = new AttributeName(ALL_NO_NS, "clip-path", "clip-path", "clip-path", "clip-path", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLIPPATHUNITS = new AttributeName(ALL_NO_NS, "clippathunits", "clippathunits", "clipPathUnits", "clippathunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DISPLAY = new AttributeName(ALL_NO_NS, "display", "display", "display", "display", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DISPLAYSTYLE = new AttributeName(ALL_NO_NS, "displaystyle", "displaystyle", "displaystyle", "displaystyle", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GLYPH_ORIENTATION_VERTICAL = new AttributeName(ALL_NO_NS, "glyph-orientation-vertical", "glyph-orientation-vertical", "glyph-orientation-vertical", "glyph-orientation-vertical", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GLYPH_ORIENTATION_HORIZONTAL = new AttributeName(ALL_NO_NS, "glyph-orientation-horizontal", "glyph-orientation-horizontal", "glyph-orientation-horizontal", "glyph-orientation-horizontal", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GLYPHREF = new AttributeName(ALL_NO_NS, "glyphref", "glyphref", "glyphRef", "glyphref", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HTTP_EQUIV = new AttributeName(ALL_NO_NS, "http-equiv", "http-equiv", "http-equiv", "http-equiv", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYPOINTS = new AttributeName(ALL_NO_NS, "keypoints", "keypoints", "keyPoints", "keypoints", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOOP = new AttributeName(ALL_NO_NS, "loop", "loop", "loop", "loop", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PROPERTY = new AttributeName(ALL_NO_NS, "property", "property", "property", "property", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCOPED = new AttributeName(ALL_NO_NS, "scoped", "scoped", "scoped", "scoped", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STEP = new AttributeName(ALL_NO_NS, "step", "step", "step", "step", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName SHAPE_RENDERING = new AttributeName(ALL_NO_NS, "shape-rendering", "shape-rendering", "shape-rendering", "shape-rendering", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCOPE = new AttributeName(ALL_NO_NS, "scope", "scope", "scope", "scope", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName SHAPE = new AttributeName(ALL_NO_NS, "shape", "shape", "shape", "shape", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName SLOPE = new AttributeName(ALL_NO_NS, "slope", "slope", "slope", "slope", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STOP_COLOR = new AttributeName(ALL_NO_NS, "stop-color", "stop-color", "stop-color", "stop-color", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STOP_OPACITY = new AttributeName(ALL_NO_NS, "stop-opacity", "stop-opacity", "stop-opacity", "stop-opacity", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEMPLATE = new AttributeName(ALL_NO_NS, "template", "template", "template", "template", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WRAP = new AttributeName(ALL_NO_NS, "wrap", "wrap", "wrap", "wrap", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ABBR = new AttributeName(ALL_NO_NS, "abbr", "abbr", "abbr", "abbr", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ATTRIBUTENAME = new AttributeName(ALL_NO_NS, "attributename", "attributename", "attributeName", "attributename", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ATTRIBUTETYPE = new AttributeName(ALL_NO_NS, "attributetype", "attributetype", "attributeType", "attributetype", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHAR = new AttributeName(ALL_NO_NS, "char", "char", "char", "char", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COORDS = new AttributeName(ALL_NO_NS, "coords", "coords", "coords", "coords", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHAROFF = new AttributeName(ALL_NO_NS, "charoff", "charoff", "charoff", "charoff", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHARSET = new AttributeName(ALL_NO_NS, "charset", "charset", "charset", "charset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOWRAP = new AttributeName(ALL_NO_NS, "nowrap", "nowrap", "nowrap", "nowrap", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName NOHREF = new AttributeName(ALL_NO_NS, "nohref", "nohref", "nohref", "nohref", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONDRAG = new AttributeName(ALL_NO_NS, "ondrag", "ondrag", "ondrag", "ondrag", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGENTER = new AttributeName(ALL_NO_NS, "ondragenter", "ondragenter", "ondragenter", "ondragenter", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGOVER = new AttributeName(ALL_NO_NS, "ondragover", "ondragover", "ondragover", "ondragover", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGEND = new AttributeName(ALL_NO_NS, "ondragend", "ondragend", "ondragend", "ondragend", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDROP = new AttributeName(ALL_NO_NS, "ondrop", "ondrop", "ondrop", "ondrop", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGDROP = new AttributeName(ALL_NO_NS, "ondragdrop", "ondragdrop", "ondragdrop", "ondragdrop", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONERROR = new AttributeName(ALL_NO_NS, "onerror", "onerror", "onerror", "onerror", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPERATOR = new AttributeName(ALL_NO_NS, "operator", "operator", "operator", "operator", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OVERFLOW = new AttributeName(ALL_NO_NS, "overflow", "overflow", "overflow", "overflow", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGSTART = new AttributeName(ALL_NO_NS, "ondragstart", "ondragstart", "ondragstart", "ondragstart", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGLEAVE = new AttributeName(ALL_NO_NS, "ondragleave", "ondragleave", "ondragleave", "ondragleave", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STARTOFFSET = new AttributeName(ALL_NO_NS, "startoffset", "startoffset", "startOffset", "startoffset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName START = new AttributeName(ALL_NO_NS, "start", "start", "start", "start", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AS = new AttributeName(ALL_NO_NS, "as", "as", "as", "as", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AXIS = new AttributeName(ALL_NO_NS, "axis", "axis", "axis", "axis", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BIAS = new AttributeName(ALL_NO_NS, "bias", "bias", "bias", "bias", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLSPAN = new AttributeName(ALL_NO_NS, "colspan", "colspan", "colspan", "colspan", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLASSID = new AttributeName(ALL_NO_NS, "classid", "classid", "classid", "classid", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CROSSORIGIN = new AttributeName(ALL_NO_NS, "crossorigin", "crossorigin", "crossorigin", "crossorigin", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLS = new AttributeName(ALL_NO_NS, "cols", "cols", "cols", "cols", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CURSOR = new AttributeName(ALL_NO_NS, "cursor", "cursor", "cursor", "cursor", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLOSURE = new AttributeName(ALL_NO_NS, "closure", "closure", "closure", "closure", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLOSE = new AttributeName(ALL_NO_NS, "close", "close", "close", "close", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLASS = new AttributeName(ALL_NO_NS, "class", "class", "class", "class", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IS = new AttributeName(ALL_NO_NS, "is", "is", "is", "is", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYSYSTEM = new AttributeName(ALL_NO_NS, "keysystem", "keysystem", "keysystem", "keysystem", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYSPLINES = new AttributeName(ALL_NO_NS, "keysplines", "keysplines", "keySplines", "keysplines", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOWSRC = new AttributeName(ALL_NO_NS, "lowsrc", "lowsrc", "lowsrc", "lowsrc", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MAXSIZE = new AttributeName(ALL_NO_NS, "maxsize", "maxsize", "maxsize", "maxsize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MINSIZE = new AttributeName(ALL_NO_NS, "minsize", "minsize", "minsize", "minsize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OFFSET = new AttributeName(ALL_NO_NS, "offset", "offset", "offset", "offset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PRESERVEALPHA = new AttributeName(ALL_NO_NS, "preservealpha", "preservealpha", "preserveAlpha", "preservealpha", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PRESERVEASPECTRATIO = new AttributeName(ALL_NO_NS, "preserveaspectratio", "preserveaspectratio", "preserveAspectRatio", "preserveaspectratio", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWSPAN = new AttributeName(ALL_NO_NS, "rowspan", "rowspan", "rowspan", "rowspan", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWSPACING = new AttributeName(ALL_NO_NS, "rowspacing", "rowspacing", "rowspacing", "rowspacing", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWS = new AttributeName(ALL_NO_NS, "rows", "rows", "rows", "rows", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SRCSET = new AttributeName(ALL_NO_NS, "srcset", "srcset", "srcset", "srcset", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SUBSCRIPTSHIFT = new AttributeName(ALL_NO_NS, "subscriptshift", "subscriptshift", "subscriptshift", "subscriptshift", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERSION = new AttributeName(ALL_NO_NS, "version", "version", "version", "version", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALTTEXT = new AttributeName(ALL_NO_NS, "alttext", "alttext", "alttext", "alttext", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTENTEDITABLE = new AttributeName(ALL_NO_NS, "contenteditable", "contenteditable", "contenteditable", "contenteditable", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTROLS = new AttributeName(ALL_NO_NS, "controls", "controls", "controls", "controls", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTENT = new AttributeName(ALL_NO_NS, "content", "content", "content", "content", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTEXTMENU = new AttributeName(ALL_NO_NS, "contextmenu", "contextmenu", "contextmenu", "contextmenu", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DEPTH = new AttributeName(ALL_NO_NS, "depth", "depth", "depth", "depth", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENCTYPE = new AttributeName(ALL_NO_NS, "enctype", "enctype", "enctype", "enctype", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName FONT_STRETCH = new AttributeName(ALL_NO_NS, "font-stretch", "font-stretch", "font-stretch", "font-stretch", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILTER = new AttributeName(ALL_NO_NS, "filter", "filter", "filter", "filter", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTWEIGHT = new AttributeName(ALL_NO_NS, "fontweight", "fontweight", "fontweight", "fontweight", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_WEIGHT = new AttributeName(ALL_NO_NS, "font-weight", "font-weight", "font-weight", "font-weight", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTSTYLE = new AttributeName(ALL_NO_NS, "fontstyle", "fontstyle", "fontstyle", "fontstyle", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_STYLE = new AttributeName(ALL_NO_NS, "font-style", "font-style", "font-style", "font-style", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTFAMILY = new AttributeName(ALL_NO_NS, "fontfamily", "fontfamily", "fontfamily", "fontfamily", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_FAMILY = new AttributeName(ALL_NO_NS, "font-family", "font-family", "font-family", "font-family", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_VARIANT = new AttributeName(ALL_NO_NS, "font-variant", "font-variant", "font-variant", "font-variant", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_SIZE_ADJUST = new AttributeName(ALL_NO_NS, "font-size-adjust", "font-size-adjust", "font-size-adjust", "font-size-adjust", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILTERUNITS = new AttributeName(ALL_NO_NS, "filterunits", "filterunits", "filterUnits", "filterunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTSIZE = new AttributeName(ALL_NO_NS, "fontsize", "fontsize", "fontsize", "fontsize", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_SIZE = new AttributeName(ALL_NO_NS, "font-size", "font-size", "font-size", "font-size", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYTIMES = new AttributeName(ALL_NO_NS, "keytimes", "keytimes", "keyTimes", "keytimes", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LETTER_SPACING = new AttributeName(ALL_NO_NS, "letter-spacing", "letter-spacing", "letter-spacing", "letter-spacing", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIST = new AttributeName(ALL_NO_NS, "list", "list", "list", "list", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MULTIPLE = new AttributeName(ALL_NO_NS, "multiple", "multiple", "multiple", "multiple", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName RT = new AttributeName(ALL_NO_NS, "rt", "rt", "rt", "rt", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSTOP = new AttributeName(ALL_NO_NS, "onstop", "onstop", "onstop", "onstop", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSTART = new AttributeName(ALL_NO_NS, "onstart", "onstart", "onstart", "onstart", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POSTER = new AttributeName(ALL_NO_NS, "poster", "poster", "poster", "poster", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERNTRANSFORM = new AttributeName(ALL_NO_NS, "patterntransform", "patterntransform", "patternTransform", "patterntransform", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERN = new AttributeName(ALL_NO_NS, "pattern", "pattern", "pattern", "pattern", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERNUNITS = new AttributeName(ALL_NO_NS, "patternunits", "patternunits", "patternUnits", "patternunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERNCONTENTUNITS = new AttributeName(ALL_NO_NS, "patterncontentunits", "patterncontentunits", "patternContentUnits", "patterncontentunits", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RESTART = new AttributeName(ALL_NO_NS, "restart", "restart", "restart", "restart", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STITCHTILES = new AttributeName(ALL_NO_NS, "stitchtiles", "stitchtiles", "stitchTiles", "stitchtiles", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SYSTEMLANGUAGE = new AttributeName(ALL_NO_NS, "systemlanguage", "systemlanguage", "systemLanguage", "systemlanguage", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_RENDERING = new AttributeName(ALL_NO_NS, "text-rendering", "text-rendering", "text-rendering", "text-rendering", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_DECORATION = new AttributeName(ALL_NO_NS, "text-decoration", "text-decoration", "text-decoration", "text-decoration", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_ANCHOR = new AttributeName(ALL_NO_NS, "text-anchor", "text-anchor", "text-anchor", "text-anchor", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXTLENGTH = new AttributeName(ALL_NO_NS, "textlength", "textlength", "textLength", "textlength", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT = new AttributeName(ALL_NO_NS, "text", "text", "text", "text", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WRITING_MODE = new AttributeName(ALL_NO_NS, "writing-mode", "writing-mode", "writing-mode", "writing-mode", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WIDTH = new AttributeName(ALL_NO_NS, "width", "width", "width", "width", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCUMULATE = new AttributeName(ALL_NO_NS, "accumulate", "accumulate", "accumulate", "accumulate", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNSPAN = new AttributeName(ALL_NO_NS, "columnspan", "columnspan", "columnspan", "columnspan", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNLINES = new AttributeName(ALL_NO_NS, "columnlines", "columnlines", "columnlines", "columnlines", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNALIGN = new AttributeName(ALL_NO_NS, "columnalign", "columnalign", "columnalign", "columnalign", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNSPACING = new AttributeName(ALL_NO_NS, "columnspacing", "columnspacing", "columnspacing", "columnspacing", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNWIDTH = new AttributeName(ALL_NO_NS, "columnwidth", "columnwidth", "columnwidth", "columnwidth", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GROUPALIGN = new AttributeName(ALL_NO_NS, "groupalign", "groupalign", "groupalign", "groupalign", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INPUTMODE = new AttributeName(ALL_NO_NS, "inputmode", "inputmode", "inputmode", "inputmode", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSUBMIT = new AttributeName(ALL_NO_NS, "onsubmit", "onsubmit", "onsubmit", "onsubmit", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCUT = new AttributeName(ALL_NO_NS, "oncut", "oncut", "oncut", "oncut", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REQUIRED = new AttributeName(ALL_NO_NS, "required", "required", "required", "required", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName REQUIREDFEATURES = new AttributeName(ALL_NO_NS, "requiredfeatures", "requiredfeatures", "requiredFeatures", "requiredfeatures", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RESULT = new AttributeName(ALL_NO_NS, "result", "result", "result", "result", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REQUIREDEXTENSIONS = new AttributeName(ALL_NO_NS, "requiredextensions", "requiredextensions", "requiredExtensions", "requiredextensions", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VALUES = new AttributeName(ALL_NO_NS, "values", "values", "values", "values", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VALUETYPE = new AttributeName(ALL_NO_NS, "valuetype", "valuetype", "valuetype", "valuetype", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName VALUE = new AttributeName(ALL_NO_NS, "value", "value", "value", "value", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ELEVATION = new AttributeName(ALL_NO_NS, "elevation", "elevation", "elevation", "elevation", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VIEWTARGET = new AttributeName(ALL_NO_NS, "viewtarget", "viewtarget", "viewTarget", "viewtarget", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VIEWBOX = new AttributeName(ALL_NO_NS, "viewbox", "viewbox", "viewBox", "viewbox", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CX = new AttributeName(ALL_NO_NS, "cx", "cx", "cx", "cx", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DX = new AttributeName(ALL_NO_NS, "dx", "dx", "dx", "dx", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FX = new AttributeName(ALL_NO_NS, "fx", "fx", "fx", "fx", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RX = new AttributeName(ALL_NO_NS, "rx", "rx", "rx", "rx", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFX = new AttributeName(ALL_NO_NS, "refx", "refx", "refX", "refx", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BY = new AttributeName(ALL_NO_NS, "by", "by", "by", "by", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CY = new AttributeName(ALL_NO_NS, "cy", "cy", "cy", "cy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DY = new AttributeName(ALL_NO_NS, "dy", "dy", "dy", "dy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FY = new AttributeName(ALL_NO_NS, "fy", "fy", "fy", "fy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RY = new AttributeName(ALL_NO_NS, "ry", "ry", "ry", "ry", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFY = new AttributeName(ALL_NO_NS, "refy", "refy", "refY", "refy", ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    private final static @NoLength AttributeName[] ATTRIBUTE_NAMES = {
    DECLARE,
    CITE,
    CHAR,
    CLEAR,
    HEIGHT,
    COLOR_RENDERING,
    FONT_SIZE,
    ARIA_DISABLED,
    OPACITY,
    ONBEFOREPASTE,
    ADDITIVE,
    KERNELUNITLENGTH,
    STROKE_MITERLIMIT,
    KEYSPLINES,
    ONCUT,
    Y,
    ARIA_MULTISELECTABLE,
    ROTATE,
    SANDBOX,
    NORESIZE,
    SCHEME,
    ARCHIVE,
    VALIGN,
    FRAME,
    WHEN,
    ONCONTEXTMENU,
    KEYPOINTS,
    ONDRAGLEAVE,
    CONTENT,
    TEXT_RENDERING,
    RX,
    MIN,
    K3,
    ARIA_CHANNEL,
    ARIA_VALUENOW,
    LOCAL,
    ONABORT,
    HIDDEN,
    BASEFREQUENCY,
    INDEX,
    ONREADYSTATECHANGE,
    RULES,
    ONAFTERPRINT,
    LENGTHADJUST,
    NOSHADE,
    ONFINISH,
    MARKER_START,
    ROWLINES,
    USEMAP,
    POINTSATX,
    XLINK_SHOW,
    LQUOTE,
    ONFOCUSOUT,
    CLIP_PATH,
    SLOPE,
    ONDRAGOVER,
    CROSSORIGIN,
    ROWSPACING,
    FONTSTYLE,
    POSTER,
    COLUMNSPAN,
    ELEVATION,
    DY,
    END,
    SRC,
    Y1,
    ARIA_GRAB,
    ARIA_REQUIRED,
    ARIA_ATOMIC,
    ARIA_OWNS,
    ARIA_BUSY,
    EQUALCOLUMNS,
    ONDATAAVAILABLE,
    XCHANNELSELECTOR,
    FETCHPRIORITY,
    BORDER,
    RENDERING_INTENT,
    ACCESSKEY,
    BASE,
    EDGEMODE,
    LABEL,
    ONSELECT,
    ORIENT,
    ONKEYDOWN,
    SELECTED,
    TYPE,
    ALIGN,
    LANGUAGE,
    PING,
    METHOD,
    ALTIMG,
    DEFINITIONURL,
    SCRIPTLEVEL,
    MARKER_MID,
    MASKCONTENTUNITS,
    MAXLENGTH,
    TITLE,
    PROMPT,
    IN,
    ONEND,
    STANDBY,
    XLINK_ARCROLE,
    AUTOFOCUS,
    ENCODING,
    ONMOUSEWHEEL,
    ONMOUSEMOVE,
    STROKE_LINECAP,
    STROKE_OPACITY,
    GLYPH_ORIENTATION_VERTICAL,
    STEP,
    WRAP,
    NOWRAP,
    ONERROR,
    AXIS,
    CLOSE,
    OFFSET,
    VERSION,
    FONT_STRETCH,
    FONT_VARIANT,
    MULTIPLE,
    PATTERNCONTENTUNITS,
    TEXT,
    COLUMNWIDTH,
    REQUIREDEXTENSIONS,
    DX,
    BY,
    RY,
    DIR,
    IN2,
    REL,
    R,
    K1,
    X2,
    XML_SPACE,
    ARIA_LABELLEDBY,
    ARIA_SELECTED,
    ARIA_PRESSED,
    ARIA_SECRET,
    ARIA_TEMPLATEID,
    ARIA_MULTILINE,
    ARIA_RELEVANT,
    ARIA_AUTOCOMPLETE,
    ARIA_HASPOPUP,
    DEFAULT,
    HSPACE,
    MOVABLELIMITS,
    RSPACE,
    SEPARATORS,
    ENABLE_BACKGROUND,
    CHECKED,
    ONSCROLL,
    SPECULAREXPONENT,
    GRADIENTTRANSFORM,
    LOADING,
    SRCDOC,
    ACCENTUNDER,
    ACCEPT,
    BASEPROFILE,
    CODETYPE,
    DATETIME,
    ENTERKEYHINT,
    INTEGRITY,
    MODE,
    ONREPEAT,
    OTHER,
    ONBEGIN,
    ONBEFORECOPY,
    ONKEYUP,
    REPEAT,
    REPEATCOUNT,
    SUPERSCRIPTSHIFT,
    SELECTION,
    HREF,
    SURFACESCALE,
    ALIGNMENTSCOPE,
    IMAGESRCSET,
    LARGEOP,
    MARGINWIDTH,
    TARGETX,
    LIGHTING_COLOR,
    MATHCOLOR,
    PATHLENGTH,
    ACTION,
    DOMINANT_BASELINE,
    MEDIA,
    RADIOGROUP,
    SCRIPTMINSIZE,
    BACKGROUND,
    MARKER_END,
    MASKUNITS,
    CELLSPACING,
    FILL,
    ONBLUR,
    STYLE,
    FORMAT,
    FROM,
    SYMMETRIC,
    ASYNC,
    KERNELMATRIX,
    OPEN,
    POINTER_EVENTS,
    POINTSATZ,
    TRANSFORM,
    XLINK_TITLE,
    XMLNS,
    AUTOPLAY,
    BGCOLOR,
    COLOR,
    FLOOD_COLOR,
    NOMODULE,
    ONMOUSEOVER,
    ONCOPY,
    ONFOCUS,
    TO,
    STROKE_DASHOFFSET,
    SCROLLING,
    CLIP,
    DISPLAY,
    GLYPHREF,
    PROPERTY,
    SCOPE,
    STOP_OPACITY,
    ATTRIBUTENAME,
    CHAROFF,
    ONDRAG,
    ONDROP,
    OVERFLOW,
    START,
    COLSPAN,
    CURSOR,
    IS,
    MAXSIZE,
    PRESERVEASPECTRATIO,
    SRCSET,
    CONTENTEDITABLE,
    DEPTH,
    FONTWEIGHT,
    FONTFAMILY,
    FILTERUNITS,
    LETTER_SPACING,
    ONSTOP,
    PATTERN,
    STITCHTILES,
    TEXT_ANCHOR,
    WIDTH,
    COLUMNALIGN,
    INPUTMODE,
    REQUIREDFEATURES,
    VALUETYPE,
    VIEWBOX,
    FX,
    REFX,
    CY,
    FY,
    REFY,
    ALT,
    DUR,
    FOR,
    LOW,
    MAX,
    REV,
    D,
    X,
    Z,
    X1,
    K2,
    Y2,
    K4,
    XML_LANG,
    ARIA_VALUEMAX,
    ARIA_DESCRIBEDBY,
    ARIA_CHECKED,
    ARIA_DROPEFFECT,
    ARIA_EXPANDED,
    ARIA_LEVEL,
    ARIA_HIDDEN,
    ARIA_POSINSET,
    ARIA_INVALID,
    ARIA_VALUEMIN,
    ARIA_CONTROLS,
    ARIA_READONLY,
    ARIA_ACTIVEDESCENDANT,
    ARIA_DATATYPE,
    ARIA_SORT,
    ARIA_FLOWTO,
    ARIA_LIVE,
    ARIA_SETSIZE,
    DISABLED,
    DATA,
    EQUALROWS,
    ISMAP,
    LSPACE,
    NOTATION,
    ONPASTE,
    ROWALIGN,
    SEPARATOR,
    VSPACE,
    YCHANNELSELECTOR,
    ONDBLCLICK,
    CALCMODE,
    FENCE,
    NONCE,
    ONACTIVATE,
    SPACING,
    SPECULARCONSTANT,
    ID,
    GRADIENTUNITS,
    HEADERS,
    READONLY,
    SEED,
    STDDEVIATION,
    WORD_SPACING,
    ACCEPT_CHARSET,
    ACCENT,
    BEVELLED,
    BASELINE_SHIFT,
    BASELINE,
    CODE,
    CODEBASE,
    DEFER,
    DIRECTION,
    EDGE,
    FACE,
    INTERCEPT,
    LINEBREAK,
    LINETHICKNESS,
    NAME,
    ONBEFOREUNLOAD,
    OBJECT,
    ORDER,
    ONRESET,
    ONMESSAGE,
    ONBEFOREPRINT,
    ORIENTATION,
    ONSELECTSTART,
    ONKEYPRESS,
    ONBEFORECUT,
    ONRESIZE,
    REFERRERPOLICY,
    ROLE,
    REPEATDUR,
    SIZES,
    STRETCHY,
    SPREADMETHOD,
    SIZE,
    DIFFUSECONSTANT,
    HREFLANG,
    PROFILE,
    XREF,
    ALIGNMENT_BASELINE,
    DRAGGABLE,
    IMAGESIZES,
    IMAGE_RENDERING,
    LANG,
    LONGDESC,
    MARGINHEIGHT,
    ORIGIN,
    TARGET,
    TARGETY,
    HIGH,
    MATHBACKGROUND,
    MATHVARIANT,
    MATHSIZE,
    ONCHANGE,
    PATH,
    ACTIONTYPE,
    ACTIVE,
    BEGIN,
    DIVISOR,
    LIMITINGCONEANGLE,
    MANIFEST,
    OPTIMUM,
    RADIUS,
    SCRIPTSIZEMULTIPLIER,
    TABINDEX,
    VISIBILITY,
    LINK,
    MARKERHEIGHT,
    MASK,
    MARKERWIDTH,
    MARKERUNITS,
    AMPLITUDE,
    CELLPADDING,
    FILL_RULE,
    FILL_OPACITY,
    ONCLICK,
    REPLACE,
    SCALE,
    TABLEVALUES,
    AZIMUTH,
    FRAMEBORDER,
    FRAMESPACING,
    FORM,
    PRIMITIVEUNITS,
    SUMMARY,
    ZOOMANDPAN,
    ALINK,
    ICON,
    KERNING,
    ONUNLOAD,
    ONINVALID,
    ONINPUT,
    POINTS,
    POINTSATY,
    SPAN,
    TRANSFORM_ORIGIN,
    VLINK,
    XLINK_HREF,
    XLINK_ROLE,
    XMLNS_XLINK,
    XLINK_TYPE,
    XLINK_ACTUATE,
    AUTOCOMPLETE,
    AUTOCAPITALIZE,
    COLOR_PROFILE,
    COLOR_INTERPOLATION,
    COLOR_INTERPOLATION_FILTERS,
    EXPONENT,
    FLOOD_OPACITY,
    NUMOCTAVES,
    ONLOAD,
    ONMOUSEENTER,
    ONFOCUSIN,
    ONZOOM,
    ONMOUSELEAVE,
    ONMOUSEUP,
    ONMOUSEOUT,
    ONMOUSEDOWN,
    RQUOTE,
    STROKE_DASHARRAY,
    STROKE_LINEJOIN,
    STROKE,
    STROKE_WIDTH,
    COMPACT,
    CLIP_RULE,
    CLIPPATHUNITS,
    DISPLAYSTYLE,
    GLYPH_ORIENTATION_HORIZONTAL,
    HTTP_EQUIV,
    LOOP,
    SCOPED,
    SHAPE_RENDERING,
    SHAPE,
    STOP_COLOR,
    TEMPLATE,
    ABBR,
    ATTRIBUTETYPE,
    COORDS,
    CHARSET,
    NOHREF,
    ONDRAGENTER,
    ONDRAGEND,
    ONDRAGDROP,
    OPERATOR,
    ONDRAGSTART,
    STARTOFFSET,
    AS,
    BIAS,
    CLASSID,
    COLS,
    CLOSURE,
    CLASS,
    KEYSYSTEM,
    LOWSRC,
    MINSIZE,
    PRESERVEALPHA,
    ROWSPAN,
    ROWS,
    SUBSCRIPTSHIFT,
    ALTTEXT,
    CONTROLS,
    CONTEXTMENU,
    ENCTYPE,
    FILTER,
    FONT_WEIGHT,
    FONT_STYLE,
    FONT_FAMILY,
    FONT_SIZE_ADJUST,
    FONTSIZE,
    KEYTIMES,
    LIST,
    RT,
    ONSTART,
    PATTERNTRANSFORM,
    PATTERNUNITS,
    RESTART,
    SYSTEMLANGUAGE,
    TEXT_DECORATION,
    TEXTLENGTH,
    WRITING_MODE,
    ACCUMULATE,
    COLUMNLINES,
    COLUMNSPACING,
    GROUPALIGN,
    ONSUBMIT,
    REQUIRED,
    RESULT,
    VALUES,
    VALUE,
    VIEWTARGET,
    CX,
    };
    private final static int[] ATTRIBUTE_HASHES = {
    1866496199,
    1748566068,
    1966384692,
    1681174213,
    1784574102,
    1916247343,
    2001898809,
    1680165421,
    1721347639,
    1754860061,
    1814656840,
    1903759600,
    1924583073,
    1987422362,
    2023342821,
    71827457,
    1680282148,
    1689324870,
    1740130375,
    1754434872,
    1756836998,
    1797886599,
    1825437894,
    1884246821,
    1909819252,
    1922566877,
    1937336473,
    1972996699,
    2000160071,
    2009041198,
    2073034754,
    57205395,
    911736834,
    1680181996,
    1680368221,
    1685882101,
    1704526375,
    1734182982,
    1747800157,
    1751507685,
    1754647074,
    1756219733,
    1771569964,
    1786851500,
    1804405895,
    1821958888,
    1854466380,
    1873656984,
    1891937366,
    1906419001,
    1910527802,
    1921061206,
    1922679610,
    1933123337,
    1941440197,
    1972744954,
    1983290011,
    1991220282,
    2001669449,
    2006824246,
    2016711994,
    2034765641,
    2082471938,
    53006051,
    60345635,
    885522434,
    1680095865,
    1680165533,
    1680229115,
    1680343801,
    1680437801,
    1682440540,
    1687620127,
    1692408896,
    1716623661,
    1731048742,
    1739583824,
    1747309881,
    1748021284,
    1749350104,
    1753049109,
    1754612424,
    1754794646,
    1754927689,
    1756704824,
    1757421892,
    1780879045,
    1786622296,
    1788842244,
    1804054854,
    1814517574,
    1816178925,
    1823829083,
    1854285018,
    1854497008,
    1871251689,
    1874788501,
    1889569526,
    1900544002,
    1905754853,
    1907701479,
    1910441773,
    1915341049,
    1917295176,
    1922400908,
    1922665179,
    1924443742,
    1924773438,
    1934917290,
    1941286708,
    1943317364,
    1972151670,
    1972908839,
    1982254612,
    1983432389,
    1989522022,
    1993343287,
    2001527900,
    2001732764,
    2005342360,
    2007064819,
    2009231684,
    2017010843,
    2024794274,
    2065694722,
    2081423362,
    2089811970,
    52488851,
    55077603,
    59825747,
    68157441,
    878182402,
    901775362,
    1037879561,
    1680159327,
    1680165437,
    1680165692,
    1680198203,
    1680231247,
    1680315086,
    1680345965,
    1680413393,
    1680452349,
    1681879063,
    1683805446,
    1686731997,
    1689048326,
    1689839946,
    1699185409,
    1714763319,
    1721189160,
    1723336432,
    1733874289,
    1736416327,
    1740096054,
    1747295467,
    1747479606,
    1747906667,
    1748503880,
    1748971848,
    1749549708,
    1751755561,
    1753550036,
    1754579720,
    1754644293,
    1754698327,
    1754835516,
    1754899031,
    1756147974,
    1756360955,
    1756762256,
    1756889417,
    1767725700,
    1773606972,
    1781007934,
    1785053243,
    1786775671,
    1787365531,
    1791068279,
    1803561214,
    1804081401,
    1805715690,
    1814560070,
    1816104145,
    1820727381,
    1823574314,
    1824159037,
    1848600826,
    1854366938,
    1854497001,
    1865910331,
    1867462756,
    1872343590,
    1874270021,
    1884079398,
    1884295780,
    1890996553,
    1898415413,
    1903612236,
    1905628916,
    1906408542,
    1906423097,
    1908462185,
    1910441627,
    1910503637,
    1915025672,
    1915757815,
    1916286197,
    1917857531,
    1921977416,
    1922413307,
    1922607670,
    1922677495,
    1923088386,
    1924517489,
    1924629705,
    1932959284,
    1933369607,
    1934970504,
    1939976792,
    1941435445,
    1941550652,
    1965512429,
    1966442279,
    1972656710,
    1972904518,
    1972922984,
    1975062341,
    1983157559,
    1983398182,
    1984430082,
    1988784439,
    1990107683,
    1991625270,
    2000096287,
    2000752725,
    2001634458,
    2001710298,
    2001826027,
    2004846654,
    2006459190,
    2007021895,
    2008401563,
    2009079867,
    2010716309,
    2016810187,
    2019887833,
    2024647008,
    2026893641,
    2060474743,
    2066743298,
    2075005220,
    2081947650,
    2083520514,
    2091784484,
    50917059,
    52489043,
    53537523,
    56685811,
    57210387,
    59830867,
    60817409,
    71303169,
    72351745,
    884998146,
    894959618,
    902299650,
    928514050,
    1038063816,
    1680140893,
    1680159328,
    1680165436,
    1680165487,
    1680165613,
    1680181850,
    1680185931,
    1680198381,
    1680230940,
    1680251485,
    1680311085,
    1680323325,
    1680345685,
    1680347981,
    1680411449,
    1680433915,
    1680446153,
    1680511804,
    1681733672,
    1681969220,
    1682587945,
    1684319541,
    1685902598,
    1687164232,
    1687751191,
    1689130184,
    1689788441,
    1691145478,
    1692933184,
    1704262346,
    1714745560,
    1716303957,
    1720503541,
    1721305962,
    1723309623,
    1723336528,
    1732771842,
    1733919469,
    1734404167,
    1739561208,
    1739927860,
    1740119884,
    1742183484,
    1747299630,
    1747446838,
    1747792072,
    1747839118,
    1747939528,
    1748306996,
    1748552744,
    1748869205,
    1749027145,
    1749399124,
    1749856356,
    1751679545,
    1752985897,
    1753297133,
    1754214628,
    1754546894,
    1754606246,
    1754643237,
    1754645079,
    1754647353,
    1754792749,
    1754798923,
    1754858317,
    1754872618,
    1754907227,
    1754958648,
    1756190926,
    1756302628,
    1756471625,
    1756737685,
    1756804936,
    1756874572,
    1757053236,
    1765800271,
    1767875272,
    1772032615,
    1776114564,
    1780975314,
    1782518297,
    1785051290,
    1785174319,
    1786740932,
    1786821704,
    1787193500,
    1788254870,
    1790814502,
    1791070327,
    1801312388,
    1804036350,
    1804069019,
    1804235064,
    1804978712,
    1805715716,
    1814558026,
    1814656326,
    1814986837,
    1816144023,
    1820262641,
    1820928104,
    1822002839,
    1823580230,
    1823841492,
    1824377064,
    1825677514,
    1853862084,
    1854302364,
    1854464212,
    1854474395,
    1854497003,
    1864698185,
    1865910347,
    1867448617,
    1867620412,
    1872034503,
    1873590471,
    1874261045,
    1874698443,
    1881750231,
    1884142379,
    1884267068,
    1884343396,
    1889633006,
    1891186903,
    1894552650,
    1898428101,
    1902640276,
    1903659239,
    1905541832,
    1905672729,
    1905902311,
    1906408598,
    1906421049,
    1907660596,
    1908316832,
    1909438149,
    1910328970,
    1910441770,
    1910487243,
    1910507338,
    1910572893,
    1915295948,
    1915394254,
    1916210285,
    1916278099,
    1916337499,
    1917327080,
    1917953597,
    1921894426,
    1922319046,
    1922413292,
    1922470745,
    1922567078,
    1922665052,
    1922671417,
    1922679386,
    1922699851,
    1924206934,
    1924462384,
    1924570799,
    1924585254,
    1924738716,
    1932870919,
    1932986153,
    1933145837,
    1933508940,
    1934917372,
    1935597338,
    1937777860,
    1941253366,
    1941409583,
    1941438085,
    1941454586,
    1942026440,
    1965349396,
    1965561677,
    1966439670,
    1966454567,
    1972196486,
    1972744939,
    1972863609,
    1972904522,
    1972909592,
    1972962123,
    1974849131,
    1980235778,
    1982640164,
    1983266615,
    1983347764,
    1983416119,
    1983461061,
    1987410233,
    1988132214,
    1988788535,
    1990062797,
    1991021879,
    1991392548,
    1991643278,
    1999273799,
    2000125224,
    2000162011,
    2001210183,
    2001578182,
    2001634459,
    2001669450,
    2001710299,
    2001814704,
    2001898808,
    2004199576,
    2004957380,
    2005925890,
    2006516551,
    2007019632,
    2007064812,
    2008084807,
    2008408414,
    2009071951,
    2009141482,
    2010452700,
    2015950026,
    2016787611,
    2016910397,
    2018908874,
    2023146024,
    2024616088,
    2024763702,
    2026741958,
    2026975253,
    2060302634,
    2065170434,
    };
}
