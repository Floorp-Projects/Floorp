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

import nu.validator.htmlparser.annotation.Inline;
import nu.validator.htmlparser.annotation.Local;
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

    /**
     * An initialization helper for having a one name in the SVG mode and
     * another name in the other modes.
     *
     * @param name
     *            the name for the non-SVG modes
     * @param camel
     *            the name for the SVG mode
     * @return the initialized name array
     */
    private static @NoLength @Local String[] SVG_DIFFERENT(@Local String name,
            @Local String camel) {
        @NoLength @Local String[] arr = new String[4];
        arr[0] = name;
        arr[1] = name;
        arr[2] = camel;
        // [NOCPP[
        arr[3] = name;
        // ]NOCPP]
        return arr;
    }

    /**
     * An initialization helper for having a one name in the MathML mode and
     * another name in the other modes.
     *
     * @param name
     *            the name for the non-MathML modes
     * @param camel
     *            the name for the MathML mode
     * @return the initialized name array
     */
    private static @NoLength @Local String[] MATH_DIFFERENT(@Local String name,
            @Local String camel) {
        @NoLength @Local String[] arr = new String[4];
        arr[0] = name;
        arr[1] = camel;
        arr[2] = name;
        // [NOCPP[
        arr[3] = name;
        // ]NOCPP]
        return arr;
    }

    /**
     * An initialization helper for having a different local name in the HTML
     * mode and the SVG and MathML modes.
     *
     * @param name
     *            the name for the HTML mode
     * @param suffix
     *            the name for the SVG and MathML modes
     * @return the initialized name array
     */
    private static @NoLength @Local String[] COLONIFIED_LOCAL(
            @Local String name, @Local String suffix) {
        @NoLength @Local String[] arr = new String[4];
        arr[0] = name;
        arr[1] = suffix;
        arr[2] = suffix;
        // [NOCPP[
        arr[3] = name;
        // ]NOCPP]
        return arr;
    }

    /**
     * An initialization helper for having the same local name in all modes.
     *
     * @param name
     *            the name
     * @return the initialized name array
     */
    static @NoLength @Local String[] SAME_LOCAL(@Local String name) {
        @NoLength @Local String[] arr = new String[4];
        arr[0] = name;
        arr[1] = name;
        arr[2] = name;
        // [NOCPP[
        arr[3] = name;
        // ]NOCPP]
        return arr;
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
    private final @NsUri @NoLength String[] uri;

    /**
     * The local names indexable by mode.
     */
    private final @Local @NoLength String[] local;

    /**
     * The prefixes indexably by mode.
     */
    private final @Prefix @NoLength String[] prefix;

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
            @Local @NoLength String[] local, @Prefix @NoLength String[] prefix
            // [NOCPP[
            , int flags
    // ]NOCPP]
    ) {
        this.uri = uri;
        this.local = local;
        this.prefix = prefix;
        // [NOCPP[
        this.qName = COMPUTE_QNAME(local, prefix);
        this.flags = flags;
        // ]NOCPP]
        // CPPONLY: this.custom = false;
    }

    // CPPONLY: public AttributeName() {
    // CPPONLY:     this.uri = AttributeName.ALL_NO_NS;
    // CPPONLY:     this.local = AttributeName.SAME_LOCAL(null);
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
    static AttributeName createAttributeName(@Local String name
    // [NOCPP[
            , boolean checkNcName
    // ]NOCPP]
    ) {
        // [NOCPP[
        int flags = NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG;
        if (name.startsWith("xmlns:")) {
            flags = IS_XMLNS;
        } else if (checkNcName && !NCName.isNCName(name)) {
            flags = 0;
        }
        // ]NOCPP]
        return new AttributeName(AttributeName.ALL_NO_NS,
                AttributeName.SAME_LOCAL(name), ALL_NO_PREFIX, flags);
    }

    /**
     * The C++ destructor.
     */
    @SuppressWarnings("unused") private void destructor() {
        Portability.deleteArray(local);
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
                AttributeName.SAME_LOCAL(name), ALL_NO_PREFIX,
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
//        if (local[0] == local[1] && local[0] == local[3]
//                && local[0] != local[2]) {
//            return "SVG_DIFFERENT(\"" + local[0] + "\", \"" + local[2] + "\")";
//        }
//        if (local[0] == local[2] && local[0] == local[3]
//                && local[0] != local[1]) {
//            return "MATH_DIFFERENT(\"" + local[0] + "\", \"" + local[1] + "\")";
//        }
//        if (local[0] == local[3] && local[1] == local[2]
//                && local[0] != local[1]) {
//            return "COLONIFIED_LOCAL(\"" + local[0] + "\", \"" + local[1]
//                    + "\")";
//        }
//        for (int i = 1; i < local.length; i++) {
//            if (local[0] != local[i]) {
//                throw new IllegalStateException();
//            }
//        }
//        return "SAME_LOCAL(\"" + local[0] + "\")";
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
//     * Regenerate self
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
    public static final AttributeName ALT = new AttributeName(ALL_NO_NS, SAME_LOCAL("alt"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DIR = new AttributeName(ALL_NO_NS, SAME_LOCAL("dir"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName DUR = new AttributeName(ALL_NO_NS, SAME_LOCAL("dur"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName END = new AttributeName(ALL_NO_NS, SAME_LOCAL("end"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("for"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IN2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("in2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOW = new AttributeName(ALL_NO_NS, SAME_LOCAL("low"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("min"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MAX = new AttributeName(ALL_NO_NS, SAME_LOCAL("max"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REL = new AttributeName(ALL_NO_NS, SAME_LOCAL("rel"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REV = new AttributeName(ALL_NO_NS, SAME_LOCAL("rev"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SRC = new AttributeName(ALL_NO_NS, SAME_LOCAL("src"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName D = new AttributeName(ALL_NO_NS, SAME_LOCAL("d"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName R = new AttributeName(ALL_NO_NS, SAME_LOCAL("r"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X = new AttributeName(ALL_NO_NS, SAME_LOCAL("x"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y = new AttributeName(ALL_NO_NS, SAME_LOCAL("y"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Z = new AttributeName(ALL_NO_NS, SAME_LOCAL("z"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("x1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("y1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("x2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("y2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K3 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k3"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K4 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k4"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XML_SPACE = new AttributeName(XML_NS, COLONIFIED_LOCAL("xml:space", "space"), XML_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XML_LANG = new AttributeName(XML_NS, COLONIFIED_LOCAL("xml:lang", "lang"), XML_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName ARIA_GRAB = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-grab"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_VALUEMAX = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-valuemax"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_LABELLEDBY = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-labelledby"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DESCRIBEDBY = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-describedby"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DISABLED = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-disabled"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_CHECKED = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-checked"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SELECTED = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-selected"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DROPEFFECT = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-dropeffect"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_REQUIRED = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-required"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_EXPANDED = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-expanded"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_PRESSED = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-pressed"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_LEVEL = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-level"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_CHANNEL = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-channel"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_HIDDEN = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-hidden"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SECRET = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-secret"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_POSINSET = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-posinset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_ATOMIC = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-atomic"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_INVALID = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-invalid"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_TEMPLATEID = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-templateid"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_VALUEMIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-valuemin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_MULTISELECTABLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-multiselectable"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_CONTROLS = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-controls"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_MULTILINE = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-multiline"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_READONLY = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-readonly"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_OWNS = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-owns"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_ACTIVEDESCENDANT = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-activedescendant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_RELEVANT = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-relevant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_DATATYPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-datatype"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_VALUENOW = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-valuenow"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SORT = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-sort"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_AUTOCOMPLETE = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-autocomplete"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_FLOWTO = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-flowto"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_BUSY = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-busy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_LIVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-live"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_HASPOPUP = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-haspopup"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARIA_SETSIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("aria-setsize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLEAR = new AttributeName(ALL_NO_NS, SAME_LOCAL("clear"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName DISABLED = new AttributeName(ALL_NO_NS, SAME_LOCAL("disabled"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DEFAULT = new AttributeName(ALL_NO_NS, SAME_LOCAL("default"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DATA = new AttributeName(ALL_NO_NS, SAME_LOCAL("data"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EQUALCOLUMNS = new AttributeName(ALL_NO_NS, SAME_LOCAL("equalcolumns"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EQUALROWS = new AttributeName(ALL_NO_NS, SAME_LOCAL("equalrows"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("hspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ISMAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("ismap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName LOCAL = new AttributeName(ALL_NO_NS, SAME_LOCAL("local"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("lspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MOVABLELIMITS = new AttributeName(ALL_NO_NS, SAME_LOCAL("movablelimits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOTATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("notation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDATAAVAILABLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondataavailable"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONPASTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onpaste"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("rspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("rowalign"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROTATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("rotate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEPARATOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("separator"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEPARATORS = new AttributeName(ALL_NO_NS, SAME_LOCAL("separators"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("vspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XCHANNELSELECTOR = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("xchannelselector", "xChannelSelector"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName YCHANNELSELECTOR = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("ychannelselector", "yChannelSelector"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENABLE_BACKGROUND = new AttributeName(ALL_NO_NS, SAME_LOCAL("enable-background"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDBLCLICK = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondblclick"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONABORT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onabort"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CALCMODE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("calcmode", "calcMode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHECKED = new AttributeName(ALL_NO_NS, SAME_LOCAL("checked"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName FENCE = new AttributeName(ALL_NO_NS, SAME_LOCAL("fence"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSCROLL = new AttributeName(ALL_NO_NS, SAME_LOCAL("onscroll"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONACTIVATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onactivate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPACITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("opacity"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("spacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPECULAREXPONENT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("specularexponent", "specularExponent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPECULARCONSTANT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("specularconstant", "specularConstant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BORDER = new AttributeName(ALL_NO_NS, SAME_LOCAL("border"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ID = new AttributeName(ALL_NO_NS, SAME_LOCAL("id"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GRADIENTTRANSFORM = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("gradienttransform", "gradientTransform"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GRADIENTUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("gradientunits", "gradientUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HIDDEN = new AttributeName(ALL_NO_NS, SAME_LOCAL("hidden"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HEADERS = new AttributeName(ALL_NO_NS, SAME_LOCAL("headers"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOADING = new AttributeName(ALL_NO_NS, SAME_LOCAL("loading"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName READONLY = new AttributeName(ALL_NO_NS, SAME_LOCAL("readonly"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName RENDERING_INTENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("rendering-intent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEED = new AttributeName(ALL_NO_NS, SAME_LOCAL("seed"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SRCDOC = new AttributeName(ALL_NO_NS, SAME_LOCAL("srcdoc"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STDDEVIATION = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("stddeviation", "stdDeviation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SANDBOX = new AttributeName(ALL_NO_NS, SAME_LOCAL("sandbox"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WORD_SPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("word-spacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCENTUNDER = new AttributeName(ALL_NO_NS, SAME_LOCAL("accentunder"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCEPT_CHARSET = new AttributeName(ALL_NO_NS, SAME_LOCAL("accept-charset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCESSKEY = new AttributeName(ALL_NO_NS, SAME_LOCAL("accesskey"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("accent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCEPT = new AttributeName(ALL_NO_NS, SAME_LOCAL("accept"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BEVELLED = new AttributeName(ALL_NO_NS, SAME_LOCAL("bevelled"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASEFREQUENCY = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("basefrequency", "baseFrequency"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASELINE_SHIFT = new AttributeName(ALL_NO_NS, SAME_LOCAL("baseline-shift"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASEPROFILE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("baseprofile", "baseProfile"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASELINE = new AttributeName(ALL_NO_NS, SAME_LOCAL("baseline"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BASE = new AttributeName(ALL_NO_NS, SAME_LOCAL("base"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CODE = new AttributeName(ALL_NO_NS, SAME_LOCAL("code"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CODETYPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("codetype"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CODEBASE = new AttributeName(ALL_NO_NS, SAME_LOCAL("codebase"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CITE = new AttributeName(ALL_NO_NS, SAME_LOCAL("cite"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DEFER = new AttributeName(ALL_NO_NS, SAME_LOCAL("defer"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DATETIME = new AttributeName(ALL_NO_NS, SAME_LOCAL("datetime"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DIRECTION = new AttributeName(ALL_NO_NS, SAME_LOCAL("direction"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EDGEMODE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("edgemode", "edgeMode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EDGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("edge"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("face"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INDEX = new AttributeName(ALL_NO_NS, SAME_LOCAL("index"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INTERCEPT = new AttributeName(ALL_NO_NS, SAME_LOCAL("intercept"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INTEGRITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("integrity"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LINEBREAK = new AttributeName(ALL_NO_NS, SAME_LOCAL("linebreak"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LABEL = new AttributeName(ALL_NO_NS, SAME_LOCAL("label"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LINETHICKNESS = new AttributeName(ALL_NO_NS, SAME_LOCAL("linethickness"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MODE = new AttributeName(ALL_NO_NS, SAME_LOCAL("mode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NAME = new AttributeName(ALL_NO_NS, SAME_LOCAL("name"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NORESIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("noresize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONBEFOREUNLOAD = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforeunload"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONREPEAT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onrepeat"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OBJECT = new AttributeName(ALL_NO_NS, SAME_LOCAL("object"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSELECT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onselect"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORDER = new AttributeName(ALL_NO_NS, SAME_LOCAL("order"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OTHER = new AttributeName(ALL_NO_NS, SAME_LOCAL("other"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONRESET = new AttributeName(ALL_NO_NS, SAME_LOCAL("onreset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONREADYSTATECHANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onreadystatechange"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMESSAGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmessage"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEGIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbegin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREPRINT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforeprint"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("orient"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIENTATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("orientation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFORECOPY = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforecopy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSELECTSTART = new AttributeName(ALL_NO_NS, SAME_LOCAL("onselectstart"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREPASTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforepaste"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYPRESS = new AttributeName(ALL_NO_NS, SAME_LOCAL("onkeypress"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYUP = new AttributeName(ALL_NO_NS, SAME_LOCAL("onkeyup"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFORECUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforecut"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYDOWN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onkeydown"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONRESIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onresize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEAT = new AttributeName(ALL_NO_NS, SAME_LOCAL("repeat"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFERRERPOLICY = new AttributeName(ALL_NO_NS, SAME_LOCAL("referrerpolicy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RULES = new AttributeName(ALL_NO_NS, SAME_LOCAL("rules"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName ROLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("role"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEATCOUNT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("repeatcount", "repeatCount"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEATDUR = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("repeatdur", "repeatDur"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SELECTED = new AttributeName(ALL_NO_NS, SAME_LOCAL("selected"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName SIZES = new AttributeName(ALL_NO_NS, SAME_LOCAL("sizes"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SUPERSCRIPTSHIFT = new AttributeName(ALL_NO_NS, SAME_LOCAL("superscriptshift"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STRETCHY = new AttributeName(ALL_NO_NS, SAME_LOCAL("stretchy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCHEME = new AttributeName(ALL_NO_NS, SAME_LOCAL("scheme"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPREADMETHOD = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("spreadmethod", "spreadMethod"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SELECTION = new AttributeName(ALL_NO_NS, SAME_LOCAL("selection"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("size"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TYPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("type"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName DIFFUSECONSTANT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("diffuseconstant", "diffuseConstant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HREF = new AttributeName(ALL_NO_NS, SAME_LOCAL("href"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HREFLANG = new AttributeName(ALL_NO_NS, SAME_LOCAL("hreflang"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONAFTERPRINT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onafterprint"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PROFILE = new AttributeName(ALL_NO_NS, SAME_LOCAL("profile"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SURFACESCALE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("surfacescale", "surfaceScale"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XREF = new AttributeName(ALL_NO_NS, SAME_LOCAL("xref"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("align"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName ALIGNMENT_BASELINE = new AttributeName(ALL_NO_NS, SAME_LOCAL("alignment-baseline"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALIGNMENTSCOPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("alignmentscope"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DRAGGABLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("draggable"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("height"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IMAGE_RENDERING = new AttributeName(ALL_NO_NS, SAME_LOCAL("image-rendering"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LANGUAGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("language"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LANG = new AttributeName(LANG_NS, SAME_LOCAL("lang"), LANG_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LARGEOP = new AttributeName(ALL_NO_NS, SAME_LOCAL("largeop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LONGDESC = new AttributeName(ALL_NO_NS, SAME_LOCAL("longdesc"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LENGTHADJUST = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("lengthadjust", "lengthAdjust"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARGINHEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("marginheight"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARGINWIDTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("marginwidth"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIGIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("origin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PING = new AttributeName(ALL_NO_NS, SAME_LOCAL("ping"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGET = new AttributeName(ALL_NO_NS, SAME_LOCAL("target"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGETX = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("targetx", "targetX"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGETY = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("targety", "targetY"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARCHIVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("archive"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HIGH = new AttributeName(ALL_NO_NS, SAME_LOCAL("high"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIGHTING_COLOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("lighting-color"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MATHBACKGROUND = new AttributeName(ALL_NO_NS, SAME_LOCAL("mathbackground"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName METHOD = new AttributeName(ALL_NO_NS, SAME_LOCAL("method"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName MATHVARIANT = new AttributeName(ALL_NO_NS, SAME_LOCAL("mathvariant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MATHCOLOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("mathcolor"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MATHSIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("mathsize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOSHADE = new AttributeName(ALL_NO_NS, SAME_LOCAL("noshade"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONCHANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onchange"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATHLENGTH = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("pathlength", "pathLength"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATH = new AttributeName(ALL_NO_NS, SAME_LOCAL("path"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALTIMG = new AttributeName(ALL_NO_NS, SAME_LOCAL("altimg"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACTIONTYPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("actiontype"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACTION = new AttributeName(ALL_NO_NS, SAME_LOCAL("action"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACTIVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("active"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ADDITIVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("additive"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BEGIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("begin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DOMINANT_BASELINE = new AttributeName(ALL_NO_NS, SAME_LOCAL("dominant-baseline"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DIVISOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("divisor"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DEFINITIONURL = new AttributeName(ALL_NO_NS, MATH_DIFFERENT("definitionurl", "definitionURL"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIMITINGCONEANGLE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("limitingconeangle", "limitingConeAngle"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MEDIA = new AttributeName(ALL_NO_NS, SAME_LOCAL("media"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MANIFEST = new AttributeName(ALL_NO_NS, SAME_LOCAL("manifest"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFINISH = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfinish"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPTIMUM = new AttributeName(ALL_NO_NS, SAME_LOCAL("optimum"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RADIOGROUP = new AttributeName(ALL_NO_NS, SAME_LOCAL("radiogroup"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RADIUS = new AttributeName(ALL_NO_NS, SAME_LOCAL("radius"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTLEVEL = new AttributeName(ALL_NO_NS, SAME_LOCAL("scriptlevel"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTSIZEMULTIPLIER = new AttributeName(ALL_NO_NS, SAME_LOCAL("scriptsizemultiplier"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTMINSIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("scriptminsize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TABINDEX = new AttributeName(ALL_NO_NS, SAME_LOCAL("tabindex"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("valign"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName VISIBILITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("visibility"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BACKGROUND = new AttributeName(ALL_NO_NS, SAME_LOCAL("background"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LINK = new AttributeName(ALL_NO_NS, SAME_LOCAL("link"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKER_MID = new AttributeName(ALL_NO_NS, SAME_LOCAL("marker-mid"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKERHEIGHT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("markerheight", "markerHeight"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKER_END = new AttributeName(ALL_NO_NS, SAME_LOCAL("marker-end"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MASK = new AttributeName(ALL_NO_NS, SAME_LOCAL("mask"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKER_START = new AttributeName(ALL_NO_NS, SAME_LOCAL("marker-start"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKERWIDTH = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("markerwidth", "markerWidth"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MASKUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("maskunits", "maskUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARKERUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("markerunits", "markerUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MASKCONTENTUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("maskcontentunits", "maskContentUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AMPLITUDE = new AttributeName(ALL_NO_NS, SAME_LOCAL("amplitude"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CELLSPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("cellspacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CELLPADDING = new AttributeName(ALL_NO_NS, SAME_LOCAL("cellpadding"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DECLARE = new AttributeName(ALL_NO_NS, SAME_LOCAL("declare"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName FILL_RULE = new AttributeName(ALL_NO_NS, SAME_LOCAL("fill-rule"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILL = new AttributeName(ALL_NO_NS, SAME_LOCAL("fill"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILL_OPACITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("fill-opacity"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MAXLENGTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("maxlength"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCLICK = new AttributeName(ALL_NO_NS, SAME_LOCAL("onclick"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBLUR = new AttributeName(ALL_NO_NS, SAME_LOCAL("onblur"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPLACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("replace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName ROWLINES = new AttributeName(ALL_NO_NS, SAME_LOCAL("rowlines"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCALE = new AttributeName(ALL_NO_NS, SAME_LOCAL("scale"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STYLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("style"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TABLEVALUES = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("tablevalues", "tableValues"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TITLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("title"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AZIMUTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("azimuth"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FORMAT = new AttributeName(ALL_NO_NS, SAME_LOCAL("format"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FRAMEBORDER = new AttributeName(ALL_NO_NS, SAME_LOCAL("frameborder"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FRAME = new AttributeName(ALL_NO_NS, SAME_LOCAL("frame"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName FRAMESPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("framespacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FROM = new AttributeName(ALL_NO_NS, SAME_LOCAL("from"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FORM = new AttributeName(ALL_NO_NS, SAME_LOCAL("form"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PROMPT = new AttributeName(ALL_NO_NS, SAME_LOCAL("prompt"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PRIMITIVEUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("primitiveunits", "primitiveUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SYMMETRIC = new AttributeName(ALL_NO_NS, SAME_LOCAL("symmetric"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SUMMARY = new AttributeName(ALL_NO_NS, SAME_LOCAL("summary"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName USEMAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("usemap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ZOOMANDPAN = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("zoomandpan", "zoomAndPan"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ASYNC = new AttributeName(ALL_NO_NS, SAME_LOCAL("async"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ALINK = new AttributeName(ALL_NO_NS, SAME_LOCAL("alink"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IN = new AttributeName(ALL_NO_NS, SAME_LOCAL("in"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ICON = new AttributeName(ALL_NO_NS, SAME_LOCAL("icon"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KERNELMATRIX = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("kernelmatrix", "kernelMatrix"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KERNING = new AttributeName(ALL_NO_NS, SAME_LOCAL("kerning"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KERNELUNITLENGTH = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("kernelunitlength", "kernelUnitLength"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONUNLOAD = new AttributeName(ALL_NO_NS, SAME_LOCAL("onunload"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPEN = new AttributeName(ALL_NO_NS, SAME_LOCAL("open"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONINVALID = new AttributeName(ALL_NO_NS, SAME_LOCAL("oninvalid"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONEND = new AttributeName(ALL_NO_NS, SAME_LOCAL("onend"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONINPUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("oninput"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTER_EVENTS = new AttributeName(ALL_NO_NS, SAME_LOCAL("pointer-events"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTS = new AttributeName(ALL_NO_NS, SAME_LOCAL("points"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTSATX = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("pointsatx", "pointsAtX"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTSATY = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("pointsaty", "pointsAtY"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POINTSATZ = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("pointsatz", "pointsAtZ"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPAN = new AttributeName(ALL_NO_NS, SAME_LOCAL("span"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STANDBY = new AttributeName(ALL_NO_NS, SAME_LOCAL("standby"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TRANSFORM = new AttributeName(ALL_NO_NS, SAME_LOCAL("transform"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VLINK = new AttributeName(ALL_NO_NS, SAME_LOCAL("vlink"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WHEN = new AttributeName(ALL_NO_NS, SAME_LOCAL("when"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XLINK_HREF = new AttributeName(XLINK_NS, COLONIFIED_LOCAL("xlink:href", "href"), XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_TITLE = new AttributeName(XLINK_NS, COLONIFIED_LOCAL("xlink:title", "title"), XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_ROLE = new AttributeName(XLINK_NS, COLONIFIED_LOCAL("xlink:role", "role"), XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_ARCROLE = new AttributeName(XLINK_NS, COLONIFIED_LOCAL("xlink:arcrole", "arcrole"), XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XMLNS_XLINK = new AttributeName(XMLNS_NS, COLONIFIED_LOCAL("xmlns:xlink", "xlink"), XMLNS_PREFIX, IS_XMLNS);
    public static final AttributeName XMLNS = new AttributeName(XMLNS_NS, SAME_LOCAL("xmlns"), ALL_NO_PREFIX, IS_XMLNS);
    public static final AttributeName XLINK_TYPE = new AttributeName(XLINK_NS, COLONIFIED_LOCAL("xlink:type", "type"), XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_SHOW = new AttributeName(XLINK_NS, COLONIFIED_LOCAL("xlink:show", "show"), XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XLINK_ACTUATE = new AttributeName(XLINK_NS, COLONIFIED_LOCAL("xlink:actuate", "actuate"), XLINK_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName AUTOPLAY = new AttributeName(ALL_NO_NS, SAME_LOCAL("autoplay"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AUTOCOMPLETE = new AttributeName(ALL_NO_NS, SAME_LOCAL("autocomplete"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName AUTOFOCUS = new AttributeName(ALL_NO_NS, SAME_LOCAL("autofocus"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName BGCOLOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("bgcolor"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_PROFILE = new AttributeName(ALL_NO_NS, SAME_LOCAL("color-profile"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_RENDERING = new AttributeName(ALL_NO_NS, SAME_LOCAL("color-rendering"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_INTERPOLATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("color-interpolation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("color"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLOR_INTERPOLATION_FILTERS = new AttributeName(ALL_NO_NS, SAME_LOCAL("color-interpolation-filters"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENCODING = new AttributeName(ALL_NO_NS, SAME_LOCAL("encoding"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EXPONENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("exponent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FLOOD_COLOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("flood-color"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FLOOD_OPACITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("flood-opacity"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LQUOTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("lquote"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NUMOCTAVES = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("numoctaves", "numOctaves"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOMODULE = new AttributeName(ALL_NO_NS, SAME_LOCAL("nomodule"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONLOAD = new AttributeName(ALL_NO_NS, SAME_LOCAL("onload"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEWHEEL = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmousewheel"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEENTER = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseenter"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEOVER = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseover"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUSIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfocusin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCONTEXTMENU = new AttributeName(ALL_NO_NS, SAME_LOCAL("oncontextmenu"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONZOOM = new AttributeName(ALL_NO_NS, SAME_LOCAL("onzoom"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCOPY = new AttributeName(ALL_NO_NS, SAME_LOCAL("oncopy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSELEAVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseleave"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEMOVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmousemove"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEUP = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseup"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUS = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfocus"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEOUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseout"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUSOUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfocusout"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEDOWN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmousedown"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TO = new AttributeName(ALL_NO_NS, SAME_LOCAL("to"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RQUOTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("rquote"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_LINECAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-linecap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_DASHARRAY = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-dasharray"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_DASHOFFSET = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-dashoffset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_LINEJOIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-linejoin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_MITERLIMIT = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-miterlimit"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCROLLING = new AttributeName(ALL_NO_NS, SAME_LOCAL("scrolling"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName STROKE_WIDTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-width"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_OPACITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-opacity"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COMPACT = new AttributeName(ALL_NO_NS, SAME_LOCAL("compact"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName CLIP = new AttributeName(ALL_NO_NS, SAME_LOCAL("clip"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLIP_RULE = new AttributeName(ALL_NO_NS, SAME_LOCAL("clip-rule"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLIP_PATH = new AttributeName(ALL_NO_NS, SAME_LOCAL("clip-path"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLIPPATHUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("clippathunits", "clipPathUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DISPLAY = new AttributeName(ALL_NO_NS, SAME_LOCAL("display"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DISPLAYSTYLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("displaystyle"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GLYPH_ORIENTATION_VERTICAL = new AttributeName(ALL_NO_NS, SAME_LOCAL("glyph-orientation-vertical"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GLYPH_ORIENTATION_HORIZONTAL = new AttributeName(ALL_NO_NS, SAME_LOCAL("glyph-orientation-horizontal"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GLYPHREF = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("glyphref", "glyphRef"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HTTP_EQUIV = new AttributeName(ALL_NO_NS, SAME_LOCAL("http-equiv"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYPOINTS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("keypoints", "keyPoints"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOOP = new AttributeName(ALL_NO_NS, SAME_LOCAL("loop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PROPERTY = new AttributeName(ALL_NO_NS, SAME_LOCAL("property"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCOPED = new AttributeName(ALL_NO_NS, SAME_LOCAL("scoped"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STEP = new AttributeName(ALL_NO_NS, SAME_LOCAL("step"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName SHAPE_RENDERING = new AttributeName(ALL_NO_NS, SAME_LOCAL("shape-rendering"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCOPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("scope"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName SHAPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("shape"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName SLOPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("slope"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STOP_COLOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("stop-color"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STOP_OPACITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("stop-opacity"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEMPLATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("template"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WRAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("wrap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ABBR = new AttributeName(ALL_NO_NS, SAME_LOCAL("abbr"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ATTRIBUTENAME = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("attributename", "attributeName"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ATTRIBUTETYPE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("attributetype", "attributeType"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHAR = new AttributeName(ALL_NO_NS, SAME_LOCAL("char"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COORDS = new AttributeName(ALL_NO_NS, SAME_LOCAL("coords"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHAROFF = new AttributeName(ALL_NO_NS, SAME_LOCAL("charoff"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHARSET = new AttributeName(ALL_NO_NS, SAME_LOCAL("charset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOWRAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("nowrap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName NOHREF = new AttributeName(ALL_NO_NS, SAME_LOCAL("nohref"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONDRAG = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondrag"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGENTER = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragenter"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGOVER = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragover"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGEND = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragend"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDROP = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondrop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGDROP = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragdrop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONERROR = new AttributeName(ALL_NO_NS, SAME_LOCAL("onerror"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPERATOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("operator"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OVERFLOW = new AttributeName(ALL_NO_NS, SAME_LOCAL("overflow"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGSTART = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragstart"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGLEAVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragleave"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STARTOFFSET = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("startoffset", "startOffset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName START = new AttributeName(ALL_NO_NS, SAME_LOCAL("start"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AS = new AttributeName(ALL_NO_NS, SAME_LOCAL("as"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName AXIS = new AttributeName(ALL_NO_NS, SAME_LOCAL("axis"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BIAS = new AttributeName(ALL_NO_NS, SAME_LOCAL("bias"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLSPAN = new AttributeName(ALL_NO_NS, SAME_LOCAL("colspan"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLASSID = new AttributeName(ALL_NO_NS, SAME_LOCAL("classid"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CROSSORIGIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("crossorigin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLS = new AttributeName(ALL_NO_NS, SAME_LOCAL("cols"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CURSOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("cursor"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLOSURE = new AttributeName(ALL_NO_NS, SAME_LOCAL("closure"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLOSE = new AttributeName(ALL_NO_NS, SAME_LOCAL("close"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CLASS = new AttributeName(ALL_NO_NS, SAME_LOCAL("class"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IS = new AttributeName(ALL_NO_NS, SAME_LOCAL("is"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYSYSTEM = new AttributeName(ALL_NO_NS, SAME_LOCAL("keysystem"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYSPLINES = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("keysplines", "keySplines"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LOWSRC = new AttributeName(ALL_NO_NS, SAME_LOCAL("lowsrc"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MAXSIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("maxsize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MINSIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("minsize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OFFSET = new AttributeName(ALL_NO_NS, SAME_LOCAL("offset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PRESERVEALPHA = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("preservealpha", "preserveAlpha"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PRESERVEASPECTRATIO = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("preserveaspectratio", "preserveAspectRatio"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWSPAN = new AttributeName(ALL_NO_NS, SAME_LOCAL("rowspan"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWSPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("rowspacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWS = new AttributeName(ALL_NO_NS, SAME_LOCAL("rows"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SRCSET = new AttributeName(ALL_NO_NS, SAME_LOCAL("srcset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SUBSCRIPTSHIFT = new AttributeName(ALL_NO_NS, SAME_LOCAL("subscriptshift"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERSION = new AttributeName(ALL_NO_NS, SAME_LOCAL("version"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALTTEXT = new AttributeName(ALL_NO_NS, SAME_LOCAL("alttext"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTENTEDITABLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("contenteditable"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTROLS = new AttributeName(ALL_NO_NS, SAME_LOCAL("controls"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("content"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CONTEXTMENU = new AttributeName(ALL_NO_NS, SAME_LOCAL("contextmenu"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DEPTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("depth"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENCTYPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("enctype"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName FONT_STRETCH = new AttributeName(ALL_NO_NS, SAME_LOCAL("font-stretch"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILTER = new AttributeName(ALL_NO_NS, SAME_LOCAL("filter"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTWEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("fontweight"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_WEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("font-weight"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTSTYLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("fontstyle"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_STYLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("font-style"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTFAMILY = new AttributeName(ALL_NO_NS, SAME_LOCAL("fontfamily"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_FAMILY = new AttributeName(ALL_NO_NS, SAME_LOCAL("font-family"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_VARIANT = new AttributeName(ALL_NO_NS, SAME_LOCAL("font-variant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_SIZE_ADJUST = new AttributeName(ALL_NO_NS, SAME_LOCAL("font-size-adjust"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FILTERUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("filterunits", "filterUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONTSIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("fontsize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FONT_SIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("font-size"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName KEYTIMES = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("keytimes", "keyTimes"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LETTER_SPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("letter-spacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIST = new AttributeName(ALL_NO_NS, SAME_LOCAL("list"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MULTIPLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("multiple"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName RT = new AttributeName(ALL_NO_NS, SAME_LOCAL("rt"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSTOP = new AttributeName(ALL_NO_NS, SAME_LOCAL("onstop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSTART = new AttributeName(ALL_NO_NS, SAME_LOCAL("onstart"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName POSTER = new AttributeName(ALL_NO_NS, SAME_LOCAL("poster"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERNTRANSFORM = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("patterntransform", "patternTransform"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERN = new AttributeName(ALL_NO_NS, SAME_LOCAL("pattern"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERNUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("patternunits", "patternUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PATTERNCONTENTUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("patterncontentunits", "patternContentUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RESTART = new AttributeName(ALL_NO_NS, SAME_LOCAL("restart"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STITCHTILES = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("stitchtiles", "stitchTiles"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SYSTEMLANGUAGE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("systemlanguage", "systemLanguage"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_RENDERING = new AttributeName(ALL_NO_NS, SAME_LOCAL("text-rendering"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_DECORATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("text-decoration"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_ANCHOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("text-anchor"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXTLENGTH = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("textlength", "textLength"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT = new AttributeName(ALL_NO_NS, SAME_LOCAL("text"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WRITING_MODE = new AttributeName(ALL_NO_NS, SAME_LOCAL("writing-mode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WIDTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("width"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCUMULATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("accumulate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNSPAN = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnspan"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNLINES = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnlines"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnalign"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNSPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnspacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNWIDTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnwidth"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GROUPALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("groupalign"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INPUTMODE = new AttributeName(ALL_NO_NS, SAME_LOCAL("inputmode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSUBMIT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onsubmit"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("oncut"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REQUIRED = new AttributeName(ALL_NO_NS, SAME_LOCAL("required"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName REQUIREDFEATURES = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("requiredfeatures", "requiredFeatures"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RESULT = new AttributeName(ALL_NO_NS, SAME_LOCAL("result"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REQUIREDEXTENSIONS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("requiredextensions", "requiredExtensions"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VALUES = new AttributeName(ALL_NO_NS, SAME_LOCAL("values"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VALUETYPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("valuetype"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName VALUE = new AttributeName(ALL_NO_NS, SAME_LOCAL("value"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ELEVATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("elevation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VIEWTARGET = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("viewtarget", "viewTarget"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VIEWBOX = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("viewbox", "viewBox"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CX = new AttributeName(ALL_NO_NS, SAME_LOCAL("cx"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DX = new AttributeName(ALL_NO_NS, SAME_LOCAL("dx"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FX = new AttributeName(ALL_NO_NS, SAME_LOCAL("fx"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RX = new AttributeName(ALL_NO_NS, SAME_LOCAL("rx"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFX = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("refx", "refX"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BY = new AttributeName(ALL_NO_NS, SAME_LOCAL("by"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CY = new AttributeName(ALL_NO_NS, SAME_LOCAL("cy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DY = new AttributeName(ALL_NO_NS, SAME_LOCAL("dy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FY = new AttributeName(ALL_NO_NS, SAME_LOCAL("fy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RY = new AttributeName(ALL_NO_NS, SAME_LOCAL("ry"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFY = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("refy", "refY"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    private final static @NoLength AttributeName[] ATTRIBUTE_NAMES = {
    ONCLICK,
    DATETIME,
    ONDRAGENTER,
    CLEAR,
    LANG,
    FLOOD_OPACITY,
    ONSTART,
    ARIA_DISABLED,
    SPECULAREXPONENT,
    ONBEFORECUT,
    LIMITINGCONEANGLE,
    ONINPUT,
    CLIP_RULE,
    ROWSPAN,
    VALUETYPE,
    Y,
    ARIA_MULTISELECTABLE,
    ROTATE,
    ACCENTUNDER,
    OBJECT,
    SIZE,
    MATHVARIANT,
    MARKERHEIGHT,
    PRIMITIVEUNITS,
    XMLNS,
    ONMOUSEOUT,
    SHAPE,
    CLASSID,
    FONT_WEIGHT,
    ACCUMULATE,
    RX,
    MIN,
    K3,
    ARIA_CHANNEL,
    ARIA_VALUENOW,
    LOCAL,
    ONABORT,
    LOADING,
    BASEPROFILE,
    LINEBREAK,
    ONBEFOREPRINT,
    REPEATDUR,
    XREF,
    TARGET,
    ACTIONTYPE,
    SCRIPTSIZEMULTIPLIER,
    AMPLITUDE,
    AZIMUTH,
    ICON,
    TRANSFORM,
    COLOR_PROFILE,
    ONFOCUSIN,
    STROKE_LINEJOIN,
    HTTP_EQUIV,
    ATTRIBUTETYPE,
    ONDRAGSTART,
    KEYSYSTEM,
    CONTROLS,
    FONTSIZE,
    SYSTEMLANGUAGE,
    ONSUBMIT,
    VIEWBOX,
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
    ONSCROLL,
    GRADIENTTRANSFORM,
    SRCDOC,
    ACCEPT,
    CODETYPE,
    FACE,
    NAME,
    ONRESET,
    ONSELECTSTART,
    REFERRERPOLICY,
    STRETCHY,
    HREFLANG,
    DRAGGABLE,
    MARGINHEIGHT,
    HIGH,
    ONCHANGE,
    BEGIN,
    OPTIMUM,
    VISIBILITY,
    MARKERWIDTH,
    FILL_RULE,
    SCALE,
    FRAMESPACING,
    ZOOMANDPAN,
    ONUNLOAD,
    POINTSATY,
    XLINK_TITLE,
    AUTOPLAY,
    COLOR_INTERPOLATION_FILTERS,
    ONLOAD,
    ONMOUSELEAVE,
    RQUOTE,
    STROKE_WIDTH,
    DISPLAYSTYLE,
    SCOPED,
    TEMPLATE,
    CHARSET,
    ONDRAGDROP,
    AS,
    CLOSURE,
    MINSIZE,
    SUBSCRIPTSHIFT,
    ENCTYPE,
    FONT_FAMILY,
    LIST,
    PATTERNUNITS,
    TEXTLENGTH,
    COLUMNSPACING,
    RESULT,
    ELEVATION,
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
    OPACITY,
    BORDER,
    HIDDEN,
    RENDERING_INTENT,
    SANDBOX,
    ACCESSKEY,
    BASEFREQUENCY,
    BASE,
    CITE,
    EDGEMODE,
    INTERCEPT,
    LINETHICKNESS,
    ONBEFOREUNLOAD,
    ORDER,
    ONMESSAGE,
    ORIENTATION,
    ONKEYPRESS,
    ONRESIZE,
    ROLE,
    SIZES,
    SPREADMETHOD,
    DIFFUSECONSTANT,
    PROFILE,
    ALIGNMENT_BASELINE,
    IMAGE_RENDERING,
    LONGDESC,
    ORIGIN,
    TARGETY,
    MATHBACKGROUND,
    MATHSIZE,
    PATH,
    ACTIVE,
    DIVISOR,
    MANIFEST,
    RADIUS,
    TABINDEX,
    LINK,
    MASK,
    MARKERUNITS,
    CELLPADDING,
    FILL_OPACITY,
    REPLACE,
    TABLEVALUES,
    FRAMEBORDER,
    FORM,
    SUMMARY,
    ALINK,
    KERNING,
    ONINVALID,
    POINTS,
    SPAN,
    WHEN,
    XLINK_ARCROLE,
    XLINK_SHOW,
    AUTOFOCUS,
    COLOR_INTERPOLATION,
    EXPONENT,
    NUMOCTAVES,
    ONMOUSEENTER,
    ONZOOM,
    ONMOUSEUP,
    ONMOUSEDOWN,
    STROKE_DASHARRAY,
    STROKE,
    COMPACT,
    CLIPPATHUNITS,
    GLYPH_ORIENTATION_HORIZONTAL,
    LOOP,
    SHAPE_RENDERING,
    STOP_COLOR,
    ABBR,
    COORDS,
    NOHREF,
    ONDRAGEND,
    OPERATOR,
    STARTOFFSET,
    BIAS,
    COLS,
    CLASS,
    LOWSRC,
    PRESERVEALPHA,
    ROWS,
    ALTTEXT,
    CONTEXTMENU,
    FILTER,
    FONT_STYLE,
    FONT_SIZE_ADJUST,
    KEYTIMES,
    RT,
    PATTERNTRANSFORM,
    RESTART,
    TEXT_DECORATION,
    WRITING_MODE,
    COLUMNLINES,
    GROUPALIGN,
    REQUIRED,
    VALUES,
    VALUE,
    VIEWTARGET,
    CX,
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
    INDEX,
    INTEGRITY,
    LABEL,
    MODE,
    NORESIZE,
    ONREPEAT,
    ONSELECT,
    OTHER,
    ONREADYSTATECHANGE,
    ONBEGIN,
    ORIENT,
    ONBEFORECOPY,
    ONBEFOREPASTE,
    ONKEYUP,
    ONKEYDOWN,
    REPEAT,
    RULES,
    REPEATCOUNT,
    SELECTED,
    SUPERSCRIPTSHIFT,
    SCHEME,
    SELECTION,
    TYPE,
    HREF,
    ONAFTERPRINT,
    SURFACESCALE,
    ALIGN,
    ALIGNMENTSCOPE,
    HEIGHT,
    LANGUAGE,
    LARGEOP,
    LENGTHADJUST,
    MARGINWIDTH,
    PING,
    TARGETX,
    ARCHIVE,
    LIGHTING_COLOR,
    METHOD,
    MATHCOLOR,
    NOSHADE,
    PATHLENGTH,
    ALTIMG,
    ACTION,
    ADDITIVE,
    DOMINANT_BASELINE,
    DEFINITIONURL,
    MEDIA,
    ONFINISH,
    RADIOGROUP,
    SCRIPTLEVEL,
    SCRIPTMINSIZE,
    VALIGN,
    BACKGROUND,
    MARKER_MID,
    MARKER_END,
    MARKER_START,
    MASKUNITS,
    MASKCONTENTUNITS,
    CELLSPACING,
    DECLARE,
    FILL,
    MAXLENGTH,
    ONBLUR,
    ROWLINES,
    STYLE,
    TITLE,
    FORMAT,
    FRAME,
    FROM,
    PROMPT,
    SYMMETRIC,
    USEMAP,
    ASYNC,
    IN,
    KERNELMATRIX,
    KERNELUNITLENGTH,
    OPEN,
    ONEND,
    POINTER_EVENTS,
    POINTSATX,
    POINTSATZ,
    STANDBY,
    VLINK,
    XLINK_HREF,
    XLINK_ROLE,
    XMLNS_XLINK,
    XLINK_TYPE,
    XLINK_ACTUATE,
    AUTOCOMPLETE,
    BGCOLOR,
    COLOR_RENDERING,
    COLOR,
    ENCODING,
    FLOOD_COLOR,
    LQUOTE,
    NOMODULE,
    ONMOUSEWHEEL,
    ONMOUSEOVER,
    ONCONTEXTMENU,
    ONCOPY,
    ONMOUSEMOVE,
    ONFOCUS,
    ONFOCUSOUT,
    TO,
    STROKE_LINECAP,
    STROKE_DASHOFFSET,
    STROKE_MITERLIMIT,
    SCROLLING,
    STROKE_OPACITY,
    CLIP,
    CLIP_PATH,
    DISPLAY,
    GLYPH_ORIENTATION_VERTICAL,
    GLYPHREF,
    KEYPOINTS,
    PROPERTY,
    STEP,
    SCOPE,
    SLOPE,
    STOP_OPACITY,
    WRAP,
    ATTRIBUTENAME,
    CHAR,
    CHAROFF,
    NOWRAP,
    ONDRAG,
    ONDRAGOVER,
    ONDROP,
    ONERROR,
    OVERFLOW,
    ONDRAGLEAVE,
    START,
    AXIS,
    COLSPAN,
    CROSSORIGIN,
    CURSOR,
    CLOSE,
    IS,
    KEYSPLINES,
    MAXSIZE,
    OFFSET,
    PRESERVEASPECTRATIO,
    ROWSPACING,
    SRCSET,
    VERSION,
    CONTENTEDITABLE,
    CONTENT,
    DEPTH,
    FONT_STRETCH,
    FONTWEIGHT,
    FONTSTYLE,
    FONTFAMILY,
    FONT_VARIANT,
    FILTERUNITS,
    FONT_SIZE,
    LETTER_SPACING,
    MULTIPLE,
    ONSTOP,
    POSTER,
    PATTERN,
    PATTERNCONTENTUNITS,
    STITCHTILES,
    TEXT_RENDERING,
    TEXT_ANCHOR,
    TEXT,
    WIDTH,
    COLUMNSPAN,
    COLUMNALIGN,
    COLUMNWIDTH,
    INPUTMODE,
    ONCUT,
    REQUIREDFEATURES,
    REQUIREDEXTENSIONS,
    };
    private final static int[] ATTRIBUTE_HASHES = {
    1872034503,
    1748971848,
    1972744939,
    1681174213,
    1786740932,
    1917953597,
    2006516551,
    1680165421,
    1723336432,
    1754907227,
    1820262641,
    1905902311,
    1932986153,
    1991021879,
    2026893641,
    71827457,
    1680282148,
    1689324870,
    1747295467,
    1754606246,
    1757053236,
    1804069019,
    1854302364,
    1889633006,
    1910503637,
    1922679386,
    1941438085,
    1983266615,
    2001634459,
    2015950026,
    2073034754,
    57205395,
    911736834,
    1680181996,
    1680368221,
    1685882101,
    1704526375,
    1736416327,
    1747906667,
    1752985897,
    1754792749,
    1756471625,
    1776114564,
    1790814502,
    1814558026,
    1823841492,
    1864698185,
    1881750231,
    1902640276,
    1908462185,
    1916210285,
    1922470745,
    1924570799,
    1935597338,
    1965561677,
    1972962123,
    1987410233,
    2000125224,
    2001898808,
    2008408414,
    2023146024,
    2060474743,
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
    1721189160,
    1733874289,
    1740096054,
    1747479606,
    1748503880,
    1749856356,
    1754214628,
    1754645079,
    1754858317,
    1756190926,
    1756804936,
    1767875272,
    1782518297,
    1787193500,
    1801312388,
    1804978712,
    1814986837,
    1822002839,
    1825677514,
    1854474395,
    1867448617,
    1874261045,
    1884267068,
    1894552650,
    1905541832,
    1906421049,
    1910441627,
    1915025672,
    1916337499,
    1922319046,
    1922665052,
    1924206934,
    1924738716,
    1933508940,
    1941253366,
    1942026440,
    1966454567,
    1972904522,
    1980235778,
    1983416119,
    1988788535,
    1991643278,
    2001210183,
    2001710299,
    2004957380,
    2007064812,
    2009141482,
    2016910397,
    2024763702,
    2034765641,
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
    1721347639,
    1731048742,
    1734182982,
    1739583824,
    1740130375,
    1747309881,
    1747800157,
    1748021284,
    1748566068,
    1749350104,
    1751679545,
    1753297133,
    1754546894,
    1754643237,
    1754647353,
    1754798923,
    1754872618,
    1754958648,
    1756302628,
    1756737685,
    1756874572,
    1765800271,
    1772032615,
    1780975314,
    1785174319,
    1786821704,
    1788254870,
    1791070327,
    1804036350,
    1804235064,
    1805715716,
    1814656326,
    1816144023,
    1820928104,
    1823580230,
    1824377064,
    1853862084,
    1854464212,
    1854497003,
    1865910347,
    1867620412,
    1873590471,
    1874698443,
    1884142379,
    1884343396,
    1891186903,
    1898428101,
    1903659239,
    1905672729,
    1906408598,
    1907660596,
    1909819252,
    1910441773,
    1910527802,
    1915341049,
    1916278099,
    1917327080,
    1921894426,
    1922413292,
    1922567078,
    1922671417,
    1922699851,
    1924462384,
    1924585254,
    1932870919,
    1933145837,
    1934917372,
    1937777860,
    1941409583,
    1941454586,
    1965349396,
    1966439670,
    1972196486,
    1972863609,
    1972909592,
    1974849131,
    1982640164,
    1983347764,
    1983461061,
    1988132214,
    1990062797,
    1991392548,
    1999273799,
    2000162011,
    2001578182,
    2001669450,
    2001814704,
    2004199576,
    2005925890,
    2007019632,
    2008084807,
    2009071951,
    2010452700,
    2016787611,
    2018908874,
    2024616088,
    2026741958,
    2026975253,
    2060302634,
    2065170434,
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
    1751507685,
    1751755561,
    1753049109,
    1753550036,
    1754434872,
    1754579720,
    1754612424,
    1754644293,
    1754647074,
    1754698327,
    1754794646,
    1754835516,
    1754860061,
    1754899031,
    1754927689,
    1756147974,
    1756219733,
    1756360955,
    1756704824,
    1756762256,
    1756836998,
    1756889417,
    1757421892,
    1767725700,
    1771569964,
    1773606972,
    1780879045,
    1781007934,
    1784574102,
    1786622296,
    1786775671,
    1786851500,
    1787365531,
    1788842244,
    1791068279,
    1797886599,
    1803561214,
    1804054854,
    1804081401,
    1804405895,
    1805715690,
    1814517574,
    1814560070,
    1814656840,
    1816104145,
    1816178925,
    1820727381,
    1821958888,
    1823574314,
    1823829083,
    1824159037,
    1825437894,
    1848600826,
    1854285018,
    1854366938,
    1854466380,
    1854497001,
    1854497008,
    1865910331,
    1866496199,
    1867462756,
    1871251689,
    1872343590,
    1873656984,
    1874270021,
    1874788501,
    1884079398,
    1884246821,
    1884295780,
    1889569526,
    1890996553,
    1891937366,
    1898415413,
    1900544002,
    1903612236,
    1903759600,
    1905628916,
    1905754853,
    1906408542,
    1906419001,
    1906423097,
    1907701479,
    1909438149,
    1910328970,
    1910441770,
    1910487243,
    1910507338,
    1910572893,
    1915295948,
    1915757815,
    1916247343,
    1916286197,
    1917295176,
    1917857531,
    1921061206,
    1921977416,
    1922400908,
    1922413307,
    1922566877,
    1922607670,
    1922665179,
    1922677495,
    1922679610,
    1923088386,
    1924443742,
    1924517489,
    1924583073,
    1924629705,
    1924773438,
    1932959284,
    1933123337,
    1933369607,
    1934917290,
    1934970504,
    1937336473,
    1939976792,
    1941286708,
    1941435445,
    1941440197,
    1941550652,
    1943317364,
    1965512429,
    1966384692,
    1966442279,
    1972151670,
    1972656710,
    1972744954,
    1972904518,
    1972908839,
    1972922984,
    1972996699,
    1975062341,
    1982254612,
    1983157559,
    1983290011,
    1983398182,
    1983432389,
    1984430082,
    1987422362,
    1988784439,
    1989522022,
    1990107683,
    1991220282,
    1991625270,
    1993343287,
    2000096287,
    2000160071,
    2000752725,
    2001527900,
    2001634458,
    2001669449,
    2001710298,
    2001732764,
    2001826027,
    2001898809,
    2004846654,
    2005342360,
    2006459190,
    2006824246,
    2007021895,
    2007064819,
    2008401563,
    2009041198,
    2009079867,
    2009231684,
    2010716309,
    2016711994,
    2016810187,
    2017010843,
    2019887833,
    2023342821,
    2024647008,
    2024794274,
    };
}
