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
    static AttributeName nameByBuffer(@NoLength char[] buf, int offset,
            int length
            , Interner interner) {
        // XXX deal with offset
        @Unsigned int hash = AttributeName.bufToHash(buf, length);
        int index = Arrays.binarySearch(AttributeName.ATTRIBUTE_HASHES, hash);
        if (index < 0) {
            return null;
        }
        AttributeName attributeName = AttributeName.ATTRIBUTE_NAMES[index];
        @Local String name = attributeName.getLocal(AttributeName.HTML);
        if (!Portability.localEqualsBuffer(name, buf, offset, length)) {
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
    // CPPONLY: public boolean isInterned() {
    // CPPONLY:     return !custom;
    // CPPONLY: }
    // CPPONLY:
    // CPPONLY: public void setNameForNonInterned(@Local String name) {
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
//        System.out.println("private final static @NoLength AttributeName[] ATTRIBUTE_NAMES = {");
//        for (int i = 0; i < ATTRIBUTE_NAMES.length; i++) {
//            AttributeName att = ATTRIBUTE_NAMES[i];
//            System.out.println(att.constName() + ",");
//        }
//        System.out.println("};");
//        System.out.println("private final static int[] ATTRIBUTE_HASHES = {");
//        for (int i = 0; i < ATTRIBUTE_NAMES.length; i++) {
//            AttributeName att = ATTRIBUTE_NAMES[i];
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
    public static final AttributeName K = new AttributeName(ALL_NO_NS, SAME_LOCAL("k"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName R = new AttributeName(ALL_NO_NS, SAME_LOCAL("r"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X = new AttributeName(ALL_NO_NS, SAME_LOCAL("x"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y = new AttributeName(ALL_NO_NS, SAME_LOCAL("y"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Z = new AttributeName(ALL_NO_NS, SAME_LOCAL("z"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CAP_HEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("cap-height"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName G1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("g1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName U1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("u1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("x1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("y1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName G2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("g2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName U2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("u2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("x2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName Y2 = new AttributeName(ALL_NO_NS, SAME_LOCAL("y2"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K3 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k3"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName K4 = new AttributeName(ALL_NO_NS, SAME_LOCAL("k4"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XML_SPACE = new AttributeName(XML_NS, COLONIFIED_LOCAL("xml:space", "space"), XML_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XML_LANG = new AttributeName(XML_NS, COLONIFIED_LOCAL("xml:lang", "lang"), XML_PREFIX, NCNAME_FOREIGN);
    public static final AttributeName XML_BASE = new AttributeName(XML_NS, COLONIFIED_LOCAL("xml:base", "base"), XML_PREFIX, NCNAME_FOREIGN);
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
    public static final AttributeName DATAFORMATAS = new AttributeName(ALL_NO_NS, SAME_LOCAL("dataformatas"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName DISABLED = new AttributeName(ALL_NO_NS, SAME_LOCAL("disabled"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DATAFLD = new AttributeName(ALL_NO_NS, SAME_LOCAL("datafld"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DEFAULT = new AttributeName(ALL_NO_NS, SAME_LOCAL("default"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DATASRC = new AttributeName(ALL_NO_NS, SAME_LOCAL("datasrc"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DATA = new AttributeName(ALL_NO_NS, SAME_LOCAL("data"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EQUALCOLUMNS = new AttributeName(ALL_NO_NS, SAME_LOCAL("equalcolumns"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName EQUALROWS = new AttributeName(ALL_NO_NS, SAME_LOCAL("equalrows"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("hspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ISMAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("ismap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName LOCAL = new AttributeName(ALL_NO_NS, SAME_LOCAL("local"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("lspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MOVABLELIMITS = new AttributeName(ALL_NO_NS, SAME_LOCAL("movablelimits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOTATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("notation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDATASETCHANGED = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondatasetchanged"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDATAAVAILABLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondataavailable"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONPASTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onpaste"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDATASETCOMPLETE = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondatasetcomplete"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("rspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROWALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("rowalign"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROTATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("rotate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEPARATOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("separator"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEPARATORS = new AttributeName(ALL_NO_NS, SAME_LOCAL("separators"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName V_MATHEMATICAL = new AttributeName(ALL_NO_NS, SAME_LOCAL("v-mathematical"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("vspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName V_HANGING = new AttributeName(ALL_NO_NS, SAME_LOCAL("v-hanging"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XCHANNELSELECTOR = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("xchannelselector", "xChannelSelector"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName YCHANNELSELECTOR = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("ychannelselector", "yChannelSelector"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARABIC_FORM = new AttributeName(ALL_NO_NS, SAME_LOCAL("arabic-form"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ENABLE_BACKGROUND = new AttributeName(ALL_NO_NS, SAME_LOCAL("enable-background"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDBLCLICK = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondblclick"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONABORT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onabort"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CALCMODE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("calcmode", "calcMode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CHECKED = new AttributeName(ALL_NO_NS, SAME_LOCAL("checked"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName DESCENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("descent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FENCE = new AttributeName(ALL_NO_NS, SAME_LOCAL("fence"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSCROLL = new AttributeName(ALL_NO_NS, SAME_LOCAL("onscroll"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONACTIVATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onactivate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPACITY = new AttributeName(ALL_NO_NS, SAME_LOCAL("opacity"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("spacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPECULAREXPONENT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("specularexponent", "specularExponent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPECULARCONSTANT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("specularconstant", "specularConstant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPECIFICATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("specification"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName THICKMATHSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("thickmathspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName UNICODE = new AttributeName(ALL_NO_NS, SAME_LOCAL("unicode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName UNICODE_BIDI = new AttributeName(ALL_NO_NS, SAME_LOCAL("unicode-bidi"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName UNICODE_RANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("unicode-range"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BORDER = new AttributeName(ALL_NO_NS, SAME_LOCAL("border"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ID = new AttributeName(ALL_NO_NS, SAME_LOCAL("id"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GRADIENTTRANSFORM = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("gradienttransform", "gradientTransform"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GRADIENTUNITS = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("gradientunits", "gradientUnits"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HIDDEN = new AttributeName(ALL_NO_NS, SAME_LOCAL("hidden"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HEADERS = new AttributeName(ALL_NO_NS, SAME_LOCAL("headers"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName READONLY = new AttributeName(ALL_NO_NS, SAME_LOCAL("readonly"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName RENDERING_INTENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("rendering-intent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEED = new AttributeName(ALL_NO_NS, SAME_LOCAL("seed"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SRCDOC = new AttributeName(ALL_NO_NS, SAME_LOCAL("srcdoc"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STDDEVIATION = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("stddeviation", "stdDeviation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SANDBOX = new AttributeName(ALL_NO_NS, SAME_LOCAL("sandbox"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName V_IDEOGRAPHIC = new AttributeName(ALL_NO_NS, SAME_LOCAL("v-ideographic"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WORD_SPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("word-spacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCENTUNDER = new AttributeName(ALL_NO_NS, SAME_LOCAL("accentunder"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCEPT_CHARSET = new AttributeName(ALL_NO_NS, SAME_LOCAL("accept-charset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCESSKEY = new AttributeName(ALL_NO_NS, SAME_LOCAL("accesskey"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCENT_HEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("accent-height"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("accent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ASCENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("ascent"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName HIDEFOCUS = new AttributeName(ALL_NO_NS, SAME_LOCAL("hidefocus"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INDEX = new AttributeName(ALL_NO_NS, SAME_LOCAL("index"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IRRELEVANT = new AttributeName(ALL_NO_NS, SAME_LOCAL("irrelevant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName ONCELLCHANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("oncellchange"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONREADYSTATECHANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onreadystatechange"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMESSAGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmessage"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEGIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbegin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONHELP = new AttributeName(ALL_NO_NS, SAME_LOCAL("onhelp"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREPRINT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforeprint"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIENT = new AttributeName(ALL_NO_NS, SAME_LOCAL("orient"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIENTATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("orientation"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFORECOPY = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforecopy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONSELECTSTART = new AttributeName(ALL_NO_NS, SAME_LOCAL("onselectstart"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREPASTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforepaste"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREUPDATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforeupdate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDEACTIVATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondeactivate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREACTIVATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforeactivate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFORDEACTIVATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbefordeactivate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYPRESS = new AttributeName(ALL_NO_NS, SAME_LOCAL("onkeypress"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYUP = new AttributeName(ALL_NO_NS, SAME_LOCAL("onkeyup"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFOREEDITFOCUS = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforeeditfocus"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBEFORECUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbeforecut"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONKEYDOWN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onkeydown"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONRESIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onresize"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEAT = new AttributeName(ALL_NO_NS, SAME_LOCAL("repeat"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEAT_MAX = new AttributeName(ALL_NO_NS, SAME_LOCAL("repeat-max"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFERRERPOLICY = new AttributeName(ALL_NO_NS, SAME_LOCAL("referrerpolicy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RULES = new AttributeName(ALL_NO_NS, SAME_LOCAL("rules"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName REPEAT_MIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("repeat-min"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ROLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("role"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEATCOUNT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("repeatcount", "repeatCount"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEAT_START = new AttributeName(ALL_NO_NS, SAME_LOCAL("repeat-start"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEAT_TEMPLATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("repeat-template"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REPEATDUR = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("repeatdur", "repeatDur"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SELECTED = new AttributeName(ALL_NO_NS, SAME_LOCAL("selected"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName SPEED = new AttributeName(ALL_NO_NS, SAME_LOCAL("speed"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SIZES = new AttributeName(ALL_NO_NS, SAME_LOCAL("sizes"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SUPERSCRIPTSHIFT = new AttributeName(ALL_NO_NS, SAME_LOCAL("superscriptshift"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STRETCHY = new AttributeName(ALL_NO_NS, SAME_LOCAL("stretchy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCHEME = new AttributeName(ALL_NO_NS, SAME_LOCAL("scheme"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SPREADMETHOD = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("spreadmethod", "spreadMethod"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SELECTION = new AttributeName(ALL_NO_NS, SAME_LOCAL("selection"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SIZE = new AttributeName(ALL_NO_NS, SAME_LOCAL("size"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TYPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("type"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName UNSELECTABLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("unselectable"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName UNDERLINE_POSITION = new AttributeName(ALL_NO_NS, SAME_LOCAL("underline-position"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName UNDERLINE_THICKNESS = new AttributeName(ALL_NO_NS, SAME_LOCAL("underline-thickness"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName X_HEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("x-height"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DIFFUSECONSTANT = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("diffuseconstant", "diffuseConstant"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HREF = new AttributeName(ALL_NO_NS, SAME_LOCAL("href"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HREFLANG = new AttributeName(ALL_NO_NS, SAME_LOCAL("hreflang"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONAFTERPRINT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onafterprint"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONAFTERUPDATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onafterupdate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PROFILE = new AttributeName(ALL_NO_NS, SAME_LOCAL("profile"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SURFACESCALE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("surfacescale", "surfaceScale"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName XREF = new AttributeName(ALL_NO_NS, SAME_LOCAL("xref"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("align"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED);
    public static final AttributeName ALIGNMENT_BASELINE = new AttributeName(ALL_NO_NS, SAME_LOCAL("alignment-baseline"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALIGNMENTSCOPE = new AttributeName(ALL_NO_NS, SAME_LOCAL("alignmentscope"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DRAGGABLE = new AttributeName(ALL_NO_NS, SAME_LOCAL("draggable"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("height"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HANGING = new AttributeName(ALL_NO_NS, SAME_LOCAL("hanging"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName IMAGE_RENDERING = new AttributeName(ALL_NO_NS, SAME_LOCAL("image-rendering"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LANGUAGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("language"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LANG = new AttributeName(LANG_NS, SAME_LOCAL("lang"), LANG_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LARGEOP = new AttributeName(ALL_NO_NS, SAME_LOCAL("largeop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LONGDESC = new AttributeName(ALL_NO_NS, SAME_LOCAL("longdesc"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LENGTHADJUST = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("lengthadjust", "lengthAdjust"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARGINHEIGHT = new AttributeName(ALL_NO_NS, SAME_LOCAL("marginheight"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MARGINWIDTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("marginwidth"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NARGS = new AttributeName(ALL_NO_NS, SAME_LOCAL("nargs"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ORIGIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("origin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PING = new AttributeName(ALL_NO_NS, SAME_LOCAL("ping"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGET = new AttributeName(ALL_NO_NS, SAME_LOCAL("target"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGETX = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("targetx", "targetX"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TARGETY = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("targety", "targetY"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ALPHABETIC = new AttributeName(ALL_NO_NS, SAME_LOCAL("alphabetic"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ARCHIVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("archive"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HIGH = new AttributeName(ALL_NO_NS, SAME_LOCAL("high"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIGHTING_COLOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("lighting-color"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MATHEMATICAL = new AttributeName(ALL_NO_NS, SAME_LOCAL("mathematical"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName HORIZ_ADV_X = new AttributeName(ALL_NO_NS, SAME_LOCAL("horiz-adv-x"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HORIZ_ORIGIN_X = new AttributeName(ALL_NO_NS, SAME_LOCAL("horiz-origin-x"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName HORIZ_ORIGIN_Y = new AttributeName(ALL_NO_NS, SAME_LOCAL("horiz-origin-y"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LIMITINGCONEANGLE = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("limitingconeangle", "limitingConeAngle"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MEDIUMMATHSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("mediummathspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MEDIA = new AttributeName(ALL_NO_NS, SAME_LOCAL("media"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName MANIFEST = new AttributeName(ALL_NO_NS, SAME_LOCAL("manifest"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFILTERCHANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfilterchange"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFINISH = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfinish"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPTIMUM = new AttributeName(ALL_NO_NS, SAME_LOCAL("optimum"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RADIOGROUP = new AttributeName(ALL_NO_NS, SAME_LOCAL("radiogroup"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RADIUS = new AttributeName(ALL_NO_NS, SAME_LOCAL("radius"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTLEVEL = new AttributeName(ALL_NO_NS, SAME_LOCAL("scriptlevel"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCRIPTSIZEMULTIPLIER = new AttributeName(ALL_NO_NS, SAME_LOCAL("scriptsizemultiplier"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STRING = new AttributeName(ALL_NO_NS, SAME_LOCAL("string"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STRIKETHROUGH_POSITION = new AttributeName(ALL_NO_NS, SAME_LOCAL("strikethrough-position"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STRIKETHROUGH_THICKNESS = new AttributeName(ALL_NO_NS, SAME_LOCAL("strikethrough-thickness"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName V_ALPHABETIC = new AttributeName(ALL_NO_NS, SAME_LOCAL("v-alphabetic"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName STEMH = new AttributeName(ALL_NO_NS, SAME_LOCAL("stemh"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STEMV = new AttributeName(ALL_NO_NS, SAME_LOCAL("stemv"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SEAMLESS = new AttributeName(ALL_NO_NS, SAME_LOCAL("seamless"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName THINMATHSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("thinmathspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName AUTOSUBMIT = new AttributeName(ALL_NO_NS, SAME_LOCAL("autosubmit"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
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
    public static final AttributeName IDEOGRAPHIC = new AttributeName(ALL_NO_NS, SAME_LOCAL("ideographic"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName LQUOTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("lquote"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName PANOSE_1 = new AttributeName(ALL_NO_NS, SAME_LOCAL("panose-1"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NUMOCTAVES = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("numoctaves", "numOctaves"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONLOAD = new AttributeName(ALL_NO_NS, SAME_LOCAL("onload"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONBOUNCE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onbounce"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCONTROLSELECT = new AttributeName(ALL_NO_NS, SAME_LOCAL("oncontrolselect"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONROWSINSERTED = new AttributeName(ALL_NO_NS, SAME_LOCAL("onrowsinserted"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEWHEEL = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmousewheel"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONROWENTER = new AttributeName(ALL_NO_NS, SAME_LOCAL("onrowenter"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEENTER = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseenter"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEOVER = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseover"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFORMCHANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onformchange"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUSIN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfocusin"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONROWEXIT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onrowexit"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOVEEND = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmoveend"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCONTEXTMENU = new AttributeName(ALL_NO_NS, SAME_LOCAL("oncontextmenu"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONZOOM = new AttributeName(ALL_NO_NS, SAME_LOCAL("onzoom"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONLOSECAPTURE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onlosecapture"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONCOPY = new AttributeName(ALL_NO_NS, SAME_LOCAL("oncopy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOVESTART = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmovestart"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONROWSDELETE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onrowsdelete"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSELEAVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseleave"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmove"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEMOVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmousemove"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEUP = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseup"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUS = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfocus"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEOUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmouseout"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFORMINPUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onforminput"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONFOCUSOUT = new AttributeName(ALL_NO_NS, SAME_LOCAL("onfocusout"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONMOUSEDOWN = new AttributeName(ALL_NO_NS, SAME_LOCAL("onmousedown"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TO = new AttributeName(ALL_NO_NS, SAME_LOCAL("to"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RQUOTE = new AttributeName(ALL_NO_NS, SAME_LOCAL("rquote"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STROKE_LINECAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("stroke-linecap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName SCROLLDELAY = new AttributeName(ALL_NO_NS, SAME_LOCAL("scrolldelay"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName GLYPH_NAME = new AttributeName(ALL_NO_NS, SAME_LOCAL("glyph-name"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName MACROS = new AttributeName(ALL_NO_NS, SAME_LOCAL("macros"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName NOWRAP = new AttributeName(ALL_NO_NS, SAME_LOCAL("nowrap"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName NOHREF = new AttributeName(ALL_NO_NS, SAME_LOCAL("nohref"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG | CASE_FOLDED | BOOLEAN);
    public static final AttributeName ONDRAG = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondrag"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGENTER = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragenter"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGOVER = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragover"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONPROPERTYCHANGE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onpropertychange"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGEND = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragend"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDROP = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondrop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGDROP = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragdrop"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OVERLINE_POSITION = new AttributeName(ALL_NO_NS, SAME_LOCAL("overline-position"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONERROR = new AttributeName(ALL_NO_NS, SAME_LOCAL("onerror"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OPERATOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("operator"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OVERFLOW = new AttributeName(ALL_NO_NS, SAME_LOCAL("overflow"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGSTART = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragstart"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONERRORUPDATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("onerrorupdate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OVERLINE_THICKNESS = new AttributeName(ALL_NO_NS, SAME_LOCAL("overline-thickness"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ONDRAGLEAVE = new AttributeName(ALL_NO_NS, SAME_LOCAL("ondragleave"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName STARTOFFSET = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("startoffset", "startOffset"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName START = new AttributeName(ALL_NO_NS, SAME_LOCAL("start"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName VERT_ORIGIN_X = new AttributeName(ALL_NO_NS, SAME_LOCAL("vert-origin-x"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERT_ADV_Y = new AttributeName(ALL_NO_NS, SAME_LOCAL("vert-adv-y"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERT_ORIGIN_Y = new AttributeName(ALL_NO_NS, SAME_LOCAL("vert-origin-y"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_DECORATION = new AttributeName(ALL_NO_NS, SAME_LOCAL("text-decoration"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT_ANCHOR = new AttributeName(ALL_NO_NS, SAME_LOCAL("text-anchor"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXTLENGTH = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("textlength", "textLength"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName TEXT = new AttributeName(ALL_NO_NS, SAME_LOCAL("text"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName UNITS_PER_EM = new AttributeName(ALL_NO_NS, SAME_LOCAL("units-per-em"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WRITING_MODE = new AttributeName(ALL_NO_NS, SAME_LOCAL("writing-mode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WIDTHS = new AttributeName(ALL_NO_NS, SAME_LOCAL("widths"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName WIDTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("width"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName ACCUMULATE = new AttributeName(ALL_NO_NS, SAME_LOCAL("accumulate"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNSPAN = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnspan"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNLINES = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnlines"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnalign"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNSPACING = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnspacing"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName COLUMNWIDTH = new AttributeName(ALL_NO_NS, SAME_LOCAL("columnwidth"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName GROUPALIGN = new AttributeName(ALL_NO_NS, SAME_LOCAL("groupalign"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName INPUTMODE = new AttributeName(ALL_NO_NS, SAME_LOCAL("inputmode"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName OCCURRENCE = new AttributeName(ALL_NO_NS, SAME_LOCAL("occurrence"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
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
    public static final AttributeName BBOX = new AttributeName(ALL_NO_NS, SAME_LOCAL("bbox"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RX = new AttributeName(ALL_NO_NS, SAME_LOCAL("rx"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFX = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("refx", "refX"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName BY = new AttributeName(ALL_NO_NS, SAME_LOCAL("by"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName CY = new AttributeName(ALL_NO_NS, SAME_LOCAL("cy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName DY = new AttributeName(ALL_NO_NS, SAME_LOCAL("dy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName FY = new AttributeName(ALL_NO_NS, SAME_LOCAL("fy"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName RY = new AttributeName(ALL_NO_NS, SAME_LOCAL("ry"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName REFY = new AttributeName(ALL_NO_NS, SVG_DIFFERENT("refy", "refY"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERYTHINMATHSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("verythinmathspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERYTHICKMATHSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("verythickmathspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERYVERYTHINMATHSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("veryverythinmathspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    public static final AttributeName VERYVERYTHICKMATHSPACE = new AttributeName(ALL_NO_NS, SAME_LOCAL("veryverythickmathspace"), ALL_NO_PREFIX, NCNAME_HTML | NCNAME_FOREIGN | NCNAME_LANG);
    private final static @NoLength AttributeName[] ATTRIBUTE_NAMES = {
    ALT,
    DIR,
    DUR,
    END,
    FOR,
    IN2,
    LOW,
    MIN,
    MAX,
    REL,
    REV,
    SRC,
    D,
    K,
    R,
    X,
    Y,
    Z,
    CAP_HEIGHT,
    G1,
    K1,
    U1,
    X1,
    Y1,
    G2,
    K2,
    U2,
    X2,
    Y2,
    K3,
    K4,
    XML_SPACE,
    XML_LANG,
    XML_BASE,
    ARIA_GRAB,
    ARIA_VALUEMAX,
    ARIA_LABELLEDBY,
    ARIA_DESCRIBEDBY,
    ARIA_DISABLED,
    ARIA_CHECKED,
    ARIA_SELECTED,
    ARIA_DROPEFFECT,
    ARIA_REQUIRED,
    ARIA_EXPANDED,
    ARIA_PRESSED,
    ARIA_LEVEL,
    ARIA_CHANNEL,
    ARIA_HIDDEN,
    ARIA_SECRET,
    ARIA_POSINSET,
    ARIA_ATOMIC,
    ARIA_INVALID,
    ARIA_TEMPLATEID,
    ARIA_VALUEMIN,
    ARIA_MULTISELECTABLE,
    ARIA_CONTROLS,
    ARIA_MULTILINE,
    ARIA_READONLY,
    ARIA_OWNS,
    ARIA_ACTIVEDESCENDANT,
    ARIA_RELEVANT,
    ARIA_DATATYPE,
    ARIA_VALUENOW,
    ARIA_SORT,
    ARIA_AUTOCOMPLETE,
    ARIA_FLOWTO,
    ARIA_BUSY,
    ARIA_LIVE,
    ARIA_HASPOPUP,
    ARIA_SETSIZE,
    CLEAR,
    DATAFORMATAS,
    DISABLED,
    DATAFLD,
    DEFAULT,
    DATASRC,
    DATA,
    EQUALCOLUMNS,
    EQUALROWS,
    HSPACE,
    ISMAP,
    LOCAL,
    LSPACE,
    MOVABLELIMITS,
    NOTATION,
    ONDATASETCHANGED,
    ONDATAAVAILABLE,
    ONPASTE,
    ONDATASETCOMPLETE,
    RSPACE,
    ROWALIGN,
    ROTATE,
    SEPARATOR,
    SEPARATORS,
    V_MATHEMATICAL,
    VSPACE,
    V_HANGING,
    XCHANNELSELECTOR,
    YCHANNELSELECTOR,
    ARABIC_FORM,
    ENABLE_BACKGROUND,
    ONDBLCLICK,
    ONABORT,
    CALCMODE,
    CHECKED,
    DESCENT,
    FENCE,
    ONSCROLL,
    ONACTIVATE,
    OPACITY,
    SPACING,
    SPECULAREXPONENT,
    SPECULARCONSTANT,
    SPECIFICATION,
    THICKMATHSPACE,
    UNICODE,
    UNICODE_BIDI,
    UNICODE_RANGE,
    BORDER,
    ID,
    GRADIENTTRANSFORM,
    GRADIENTUNITS,
    HIDDEN,
    HEADERS,
    READONLY,
    RENDERING_INTENT,
    SEED,
    SRCDOC,
    STDDEVIATION,
    SANDBOX,
    V_IDEOGRAPHIC,
    WORD_SPACING,
    ACCENTUNDER,
    ACCEPT_CHARSET,
    ACCESSKEY,
    ACCENT_HEIGHT,
    ACCENT,
    ASCENT,
    ACCEPT,
    BEVELLED,
    BASEFREQUENCY,
    BASELINE_SHIFT,
    BASEPROFILE,
    BASELINE,
    BASE,
    CODE,
    CODETYPE,
    CODEBASE,
    CITE,
    DEFER,
    DATETIME,
    DIRECTION,
    EDGEMODE,
    EDGE,
    FACE,
    HIDEFOCUS,
    INDEX,
    IRRELEVANT,
    INTERCEPT,
    INTEGRITY,
    LINEBREAK,
    LABEL,
    LINETHICKNESS,
    MODE,
    NAME,
    NORESIZE,
    ONBEFOREUNLOAD,
    ONREPEAT,
    OBJECT,
    ONSELECT,
    ORDER,
    OTHER,
    ONRESET,
    ONCELLCHANGE,
    ONREADYSTATECHANGE,
    ONMESSAGE,
    ONBEGIN,
    ONHELP,
    ONBEFOREPRINT,
    ORIENT,
    ORIENTATION,
    ONBEFORECOPY,
    ONSELECTSTART,
    ONBEFOREPASTE,
    ONBEFOREUPDATE,
    ONDEACTIVATE,
    ONBEFOREACTIVATE,
    ONBEFORDEACTIVATE,
    ONKEYPRESS,
    ONKEYUP,
    ONBEFOREEDITFOCUS,
    ONBEFORECUT,
    ONKEYDOWN,
    ONRESIZE,
    REPEAT,
    REPEAT_MAX,
    REFERRERPOLICY,
    RULES,
    REPEAT_MIN,
    ROLE,
    REPEATCOUNT,
    REPEAT_START,
    REPEAT_TEMPLATE,
    REPEATDUR,
    SELECTED,
    SPEED,
    SIZES,
    SUPERSCRIPTSHIFT,
    STRETCHY,
    SCHEME,
    SPREADMETHOD,
    SELECTION,
    SIZE,
    TYPE,
    UNSELECTABLE,
    UNDERLINE_POSITION,
    UNDERLINE_THICKNESS,
    X_HEIGHT,
    DIFFUSECONSTANT,
    HREF,
    HREFLANG,
    ONAFTERPRINT,
    ONAFTERUPDATE,
    PROFILE,
    SURFACESCALE,
    XREF,
    ALIGN,
    ALIGNMENT_BASELINE,
    ALIGNMENTSCOPE,
    DRAGGABLE,
    HEIGHT,
    HANGING,
    IMAGE_RENDERING,
    LANGUAGE,
    LANG,
    LARGEOP,
    LONGDESC,
    LENGTHADJUST,
    MARGINHEIGHT,
    MARGINWIDTH,
    NARGS,
    ORIGIN,
    PING,
    TARGET,
    TARGETX,
    TARGETY,
    ALPHABETIC,
    ARCHIVE,
    HIGH,
    LIGHTING_COLOR,
    MATHEMATICAL,
    MATHBACKGROUND,
    METHOD,
    MATHVARIANT,
    MATHCOLOR,
    MATHSIZE,
    NOSHADE,
    ONCHANGE,
    PATHLENGTH,
    PATH,
    ALTIMG,
    ACTIONTYPE,
    ACTION,
    ACTIVE,
    ADDITIVE,
    BEGIN,
    DOMINANT_BASELINE,
    DIVISOR,
    DEFINITIONURL,
    HORIZ_ADV_X,
    HORIZ_ORIGIN_X,
    HORIZ_ORIGIN_Y,
    LIMITINGCONEANGLE,
    MEDIUMMATHSPACE,
    MEDIA,
    MANIFEST,
    ONFILTERCHANGE,
    ONFINISH,
    OPTIMUM,
    RADIOGROUP,
    RADIUS,
    SCRIPTLEVEL,
    SCRIPTSIZEMULTIPLIER,
    STRING,
    STRIKETHROUGH_POSITION,
    STRIKETHROUGH_THICKNESS,
    SCRIPTMINSIZE,
    TABINDEX,
    VALIGN,
    VISIBILITY,
    BACKGROUND,
    LINK,
    MARKER_MID,
    MARKERHEIGHT,
    MARKER_END,
    MASK,
    MARKER_START,
    MARKERWIDTH,
    MASKUNITS,
    MARKERUNITS,
    MASKCONTENTUNITS,
    AMPLITUDE,
    CELLSPACING,
    CELLPADDING,
    DECLARE,
    FILL_RULE,
    FILL,
    FILL_OPACITY,
    MAXLENGTH,
    ONCLICK,
    ONBLUR,
    REPLACE,
    ROWLINES,
    SCALE,
    STYLE,
    TABLEVALUES,
    TITLE,
    V_ALPHABETIC,
    AZIMUTH,
    FORMAT,
    FRAMEBORDER,
    FRAME,
    FRAMESPACING,
    FROM,
    FORM,
    PROMPT,
    PRIMITIVEUNITS,
    SYMMETRIC,
    STEMH,
    STEMV,
    SEAMLESS,
    SUMMARY,
    USEMAP,
    ZOOMANDPAN,
    ASYNC,
    ALINK,
    IN,
    ICON,
    KERNELMATRIX,
    KERNING,
    KERNELUNITLENGTH,
    ONUNLOAD,
    OPEN,
    ONINVALID,
    ONEND,
    ONINPUT,
    POINTER_EVENTS,
    POINTS,
    POINTSATX,
    POINTSATY,
    POINTSATZ,
    SPAN,
    STANDBY,
    THINMATHSPACE,
    TRANSFORM,
    VLINK,
    WHEN,
    XLINK_HREF,
    XLINK_TITLE,
    XLINK_ROLE,
    XLINK_ARCROLE,
    XMLNS_XLINK,
    XMLNS,
    XLINK_TYPE,
    XLINK_SHOW,
    XLINK_ACTUATE,
    AUTOPLAY,
    AUTOSUBMIT,
    AUTOCOMPLETE,
    AUTOFOCUS,
    BGCOLOR,
    COLOR_PROFILE,
    COLOR_RENDERING,
    COLOR_INTERPOLATION,
    COLOR,
    COLOR_INTERPOLATION_FILTERS,
    ENCODING,
    EXPONENT,
    FLOOD_COLOR,
    FLOOD_OPACITY,
    IDEOGRAPHIC,
    LQUOTE,
    PANOSE_1,
    NUMOCTAVES,
    ONLOAD,
    ONBOUNCE,
    ONCONTROLSELECT,
    ONROWSINSERTED,
    ONMOUSEWHEEL,
    ONROWENTER,
    ONMOUSEENTER,
    ONMOUSEOVER,
    ONFORMCHANGE,
    ONFOCUSIN,
    ONROWEXIT,
    ONMOVEEND,
    ONCONTEXTMENU,
    ONZOOM,
    ONLOSECAPTURE,
    ONCOPY,
    ONMOVESTART,
    ONROWSDELETE,
    ONMOUSELEAVE,
    ONMOVE,
    ONMOUSEMOVE,
    ONMOUSEUP,
    ONFOCUS,
    ONMOUSEOUT,
    ONFORMINPUT,
    ONFOCUSOUT,
    ONMOUSEDOWN,
    TO,
    RQUOTE,
    STROKE_LINECAP,
    SCROLLDELAY,
    STROKE_DASHARRAY,
    STROKE_DASHOFFSET,
    STROKE_LINEJOIN,
    STROKE_MITERLIMIT,
    STROKE,
    SCROLLING,
    STROKE_WIDTH,
    STROKE_OPACITY,
    COMPACT,
    CLIP,
    CLIP_RULE,
    CLIP_PATH,
    CLIPPATHUNITS,
    DISPLAY,
    DISPLAYSTYLE,
    GLYPH_ORIENTATION_VERTICAL,
    GLYPH_ORIENTATION_HORIZONTAL,
    GLYPHREF,
    GLYPH_NAME,
    HTTP_EQUIV,
    KEYPOINTS,
    LOOP,
    PROPERTY,
    SCOPED,
    STEP,
    SHAPE_RENDERING,
    SCOPE,
    SHAPE,
    SLOPE,
    STOP_COLOR,
    STOP_OPACITY,
    TEMPLATE,
    WRAP,
    ABBR,
    ATTRIBUTENAME,
    ATTRIBUTETYPE,
    CHAR,
    COORDS,
    CHAROFF,
    CHARSET,
    MACROS,
    NOWRAP,
    NOHREF,
    ONDRAG,
    ONDRAGENTER,
    ONDRAGOVER,
    ONPROPERTYCHANGE,
    ONDRAGEND,
    ONDROP,
    ONDRAGDROP,
    OVERLINE_POSITION,
    ONERROR,
    OPERATOR,
    OVERFLOW,
    ONDRAGSTART,
    ONERRORUPDATE,
    OVERLINE_THICKNESS,
    ONDRAGLEAVE,
    STARTOFFSET,
    START,
    AXIS,
    BIAS,
    COLSPAN,
    CLASSID,
    CROSSORIGIN,
    COLS,
    CURSOR,
    CLOSURE,
    CLOSE,
    CLASS,
    KEYSYSTEM,
    KEYSPLINES,
    LOWSRC,
    MAXSIZE,
    MINSIZE,
    OFFSET,
    PRESERVEALPHA,
    PRESERVEASPECTRATIO,
    ROWSPAN,
    ROWSPACING,
    ROWS,
    SRCSET,
    SUBSCRIPTSHIFT,
    VERSION,
    ALTTEXT,
    CONTENTEDITABLE,
    CONTROLS,
    CONTENT,
    CONTEXTMENU,
    DEPTH,
    ENCTYPE,
    FONT_STRETCH,
    FILTER,
    FONTWEIGHT,
    FONT_WEIGHT,
    FONTSTYLE,
    FONT_STYLE,
    FONTFAMILY,
    FONT_FAMILY,
    FONT_VARIANT,
    FONT_SIZE_ADJUST,
    FILTERUNITS,
    FONTSIZE,
    FONT_SIZE,
    KEYTIMES,
    LETTER_SPACING,
    LIST,
    MULTIPLE,
    RT,
    ONSTOP,
    ONSTART,
    POSTER,
    PATTERNTRANSFORM,
    PATTERN,
    PATTERNUNITS,
    PATTERNCONTENTUNITS,
    RESTART,
    STITCHTILES,
    SYSTEMLANGUAGE,
    TEXT_RENDERING,
    VERT_ORIGIN_X,
    VERT_ADV_Y,
    VERT_ORIGIN_Y,
    TEXT_DECORATION,
    TEXT_ANCHOR,
    TEXTLENGTH,
    TEXT,
    UNITS_PER_EM,
    WRITING_MODE,
    WIDTHS,
    WIDTH,
    ACCUMULATE,
    COLUMNSPAN,
    COLUMNLINES,
    COLUMNALIGN,
    COLUMNSPACING,
    COLUMNWIDTH,
    GROUPALIGN,
    INPUTMODE,
    OCCURRENCE,
    ONSUBMIT,
    ONCUT,
    REQUIRED,
    REQUIREDFEATURES,
    RESULT,
    REQUIREDEXTENSIONS,
    VALUES,
    VALUETYPE,
    VALUE,
    ELEVATION,
    VIEWTARGET,
    VIEWBOX,
    CX,
    DX,
    FX,
    BBOX,
    RX,
    REFX,
    BY,
    CY,
    DY,
    FY,
    RY,
    REFY,
    VERYTHINMATHSPACE,
    VERYTHICKMATHSPACE,
    VERYVERYTHINMATHSPACE,
    VERYVERYTHICKMATHSPACE,
    };
    private final static int[] ATTRIBUTE_HASHES = {
    50917059,
    52488851,
    52489043,
    53006051,
    53537523,
    55077603,
    56685811,
    57205395,
    57210387,
    59825747,
    59830867,
    60345635,
    60817409,
    64487425,
    68157441,
    71303169,
    71827457,
    72351745,
    808872090,
    876085250,
    878182402,
    883425282,
    884998146,
    885522434,
    892862466,
    894959618,
    900202498,
    901775362,
    902299650,
    911736834,
    928514050,
    1037879561,
    1038063816,
    1038141480,
    1680095865,
    1680140893,
    1680159327,
    1680159328,
    1680165421,
    1680165436,
    1680165437,
    1680165487,
    1680165533,
    1680165613,
    1680165692,
    1680181850,
    1680181996,
    1680185931,
    1680198203,
    1680198381,
    1680229115,
    1680230940,
    1680231247,
    1680251485,
    1680282148,
    1680311085,
    1680315086,
    1680323325,
    1680343801,
    1680345685,
    1680345965,
    1680347981,
    1680368221,
    1680411449,
    1680413393,
    1680433915,
    1680437801,
    1680446153,
    1680452349,
    1680511804,
    1681174213,
    1681694748,
    1681733672,
    1681844247,
    1681879063,
    1681940503,
    1681969220,
    1682440540,
    1682587945,
    1683805446,
    1684319541,
    1685882101,
    1685902598,
    1686731997,
    1687164232,
    1687503600,
    1687620127,
    1687751191,
    1687751377,
    1689048326,
    1689130184,
    1689324870,
    1689788441,
    1689839946,
    1691091102,
    1691145478,
    1691293817,
    1692408896,
    1692933184,
    1697174123,
    1699185409,
    1704262346,
    1704526375,
    1714745560,
    1714763319,
    1715466295,
    1716303957,
    1721189160,
    1721305962,
    1721347639,
    1723309623,
    1723336432,
    1723336528,
    1723340621,
    1723645710,
    1724189239,
    1724197420,
    1724238365,
    1731048742,
    1732771842,
    1733874289,
    1733919469,
    1734182982,
    1734404167,
    1739561208,
    1739583824,
    1739927860,
    1740096054,
    1740119884,
    1740130375,
    1741535501,
    1742183484,
    1747295467,
    1747299630,
    1747309881,
    1747348637,
    1747446838,
    1747455030,
    1747479606,
    1747792072,
    1747800157,
    1747839118,
    1747906667,
    1747939528,
    1748021284,
    1748306996,
    1748503880,
    1748552744,
    1748566068,
    1748869205,
    1748971848,
    1749027145,
    1749350104,
    1749399124,
    1749856356,
    1751232761,
    1751507685,
    1751649130,
    1751679545,
    1751755561,
    1752985897,
    1753049109,
    1753297133,
    1753550036,
    1754214628,
    1754434872,
    1754546894,
    1754579720,
    1754606246,
    1754612424,
    1754643237,
    1754644293,
    1754645079,
    1754647068,
    1754647074,
    1754647353,
    1754698327,
    1754751622,
    1754792749,
    1754794646,
    1754798923,
    1754835516,
    1754858317,
    1754860061,
    1754860110,
    1754860396,
    1754860400,
    1754860401,
    1754872618,
    1754899031,
    1754905345,
    1754907227,
    1754927689,
    1754958648,
    1756147974,
    1756155098,
    1756190926,
    1756219733,
    1756265690,
    1756302628,
    1756360955,
    1756426572,
    1756428495,
    1756471625,
    1756704824,
    1756710661,
    1756737685,
    1756762256,
    1756804936,
    1756836998,
    1756874572,
    1756889417,
    1757053236,
    1757421892,
    1757874716,
    1757942610,
    1758018291,
    1759379608,
    1765800271,
    1767725700,
    1767875272,
    1771569964,
    1771637325,
    1772032615,
    1773606972,
    1776114564,
    1780879045,
    1780975314,
    1781007934,
    1782518297,
    1784574102,
    1784643703,
    1785174319,
    1786622296,
    1786740932,
    1786775671,
    1786821704,
    1786851500,
    1787193500,
    1787365531,
    1787699221,
    1788254870,
    1788842244,
    1790814502,
    1791068279,
    1791070327,
    1797666394,
    1797886599,
    1801312388,
    1803561214,
    1803839644,
    1804036350,
    1804054854,
    1804069019,
    1804081401,
    1804235064,
    1804405895,
    1804978712,
    1805715690,
    1805715716,
    1814517574,
    1814558026,
    1814560070,
    1814656326,
    1814656840,
    1814986837,
    1816104145,
    1816144023,
    1816178925,
    1817175115,
    1817175198,
    1817177246,
    1820262641,
    1820637455,
    1820727381,
    1820928104,
    1821755934,
    1821958888,
    1822002839,
    1823574314,
    1823580230,
    1823829083,
    1823841492,
    1823975206,
    1824005974,
    1824081655,
    1824159037,
    1824377064,
    1825437894,
    1825677514,
    1848600826,
    1853862084,
    1854285018,
    1854302364,
    1854366938,
    1854464212,
    1854466380,
    1854474395,
    1854497001,
    1854497003,
    1854497008,
    1864698185,
    1865910331,
    1865910347,
    1866496199,
    1867448617,
    1867462756,
    1867620412,
    1871251689,
    1872034503,
    1872343590,
    1873590471,
    1873656984,
    1874261045,
    1874270021,
    1874698443,
    1874788501,
    1875753052,
    1881750231,
    1884079398,
    1884142379,
    1884246821,
    1884267068,
    1884295780,
    1884343396,
    1889569526,
    1889633006,
    1890996553,
    1891069765,
    1891098437,
    1891182792,
    1891186903,
    1891937366,
    1894552650,
    1898415413,
    1898428101,
    1900544002,
    1902640276,
    1903612236,
    1903659239,
    1903759600,
    1905541832,
    1905628916,
    1905672729,
    1905754853,
    1905902311,
    1906408542,
    1906408598,
    1906419001,
    1906421049,
    1906423097,
    1907660596,
    1907701479,
    1908195085,
    1908462185,
    1909438149,
    1909819252,
    1910328970,
    1910441627,
    1910441770,
    1910441773,
    1910487243,
    1910503637,
    1910507338,
    1910527802,
    1910572893,
    1915025672,
    1915146282,
    1915295948,
    1915341049,
    1915757815,
    1916210285,
    1916247343,
    1916278099,
    1916286197,
    1916337499,
    1917295176,
    1917327080,
    1917857531,
    1917953597,
    1919297291,
    1921061206,
    1921880376,
    1921894426,
    1922319046,
    1922354008,
    1922384591,
    1922384686,
    1922400908,
    1922413290,
    1922413292,
    1922413307,
    1922419228,
    1922470745,
    1922482777,
    1922531929,
    1922566877,
    1922567078,
    1922599757,
    1922607670,
    1922630475,
    1922632396,
    1922665052,
    1922665174,
    1922665179,
    1922671417,
    1922677495,
    1922679386,
    1922679531,
    1922679610,
    1922699851,
    1923088386,
    1924206934,
    1924443742,
    1924453467,
    1924462384,
    1924517489,
    1924570799,
    1924583073,
    1924585254,
    1924629705,
    1924738716,
    1924773438,
    1932870919,
    1932959284,
    1932986153,
    1933123337,
    1933145837,
    1933369607,
    1933508940,
    1934917290,
    1934917372,
    1934970504,
    1935099626,
    1935597338,
    1937336473,
    1937777860,
    1939976792,
    1941253366,
    1941286708,
    1941409583,
    1941435445,
    1941438085,
    1941440197,
    1941454586,
    1941550652,
    1942026440,
    1943317364,
    1965349396,
    1965512429,
    1965561677,
    1966384692,
    1966439670,
    1966442279,
    1966454567,
    1971855414,
    1972151670,
    1972196486,
    1972656710,
    1972744939,
    1972744954,
    1972750880,
    1972863609,
    1972904518,
    1972904522,
    1972904785,
    1972908839,
    1972909592,
    1972922984,
    1972962123,
    1972963917,
    1972980466,
    1972996699,
    1974849131,
    1975062341,
    1982254612,
    1982640164,
    1983157559,
    1983266615,
    1983290011,
    1983347764,
    1983398182,
    1983416119,
    1983432389,
    1983461061,
    1987410233,
    1987422362,
    1988132214,
    1988784439,
    1988788535,
    1989522022,
    1990062797,
    1990107683,
    1991021879,
    1991220282,
    1991392548,
    1991625270,
    1991643278,
    1993343287,
    1999273799,
    2000096287,
    2000125224,
    2000160071,
    2000162011,
    2000752725,
    2001210183,
    2001527900,
    2001578182,
    2001634458,
    2001634459,
    2001669449,
    2001669450,
    2001710298,
    2001710299,
    2001732764,
    2001814704,
    2001826027,
    2001898808,
    2001898809,
    2004199576,
    2004846654,
    2004957380,
    2005342360,
    2005925890,
    2006459190,
    2006516551,
    2006824246,
    2007019632,
    2007021895,
    2007064812,
    2007064819,
    2008084807,
    2008401563,
    2008408414,
    2009041198,
    2009059485,
    2009061450,
    2009061533,
    2009071951,
    2009079867,
    2009141482,
    2009231684,
    2009434924,
    2010452700,
    2010542150,
    2010716309,
    2015950026,
    2016711994,
    2016787611,
    2016810187,
    2016910397,
    2017010843,
    2018908874,
    2019887833,
    2023011418,
    2023146024,
    2023342821,
    2024616088,
    2024647008,
    2024763702,
    2024794274,
    2026741958,
    2026893641,
    2026975253,
    2034765641,
    2060302634,
    2060474743,
    2065170434,
    2065694722,
    2066743298,
    2066762276,
    2073034754,
    2075005220,
    2081423362,
    2081947650,
    2082471938,
    2083520514,
    2089811970,
    2091784484,
    2093791505,
    2093791506,
    2093791509,
    2093791510,
    };
}
