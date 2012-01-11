// Mozilla has modified this file - see http://hg.mozilla.org/ for details.
/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.apache.commons.codec;

/**
 * Character encoding names required of every implementation of the Java platform.
 * 
 * From the Java documentation <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard
 * charsets</a>:
 * <p>
 * <cite>Every implementation of the Java platform is required to support the following character encodings. Consult the
 * release documentation for your implementation to see if any other encodings are supported. Consult the release
 * documentation for your implementation to see if any other encodings are supported. </cite>
 * </p>
 * 
 * <ul>
 * <li><code>US-ASCII</code><br/>
 * Seven-bit ASCII, a.k.a. ISO646-US, a.k.a. the Basic Latin block of the Unicode character set.</li>
 * <li><code>ISO-8859-1</code><br/>
 * ISO Latin Alphabet No. 1, a.k.a. ISO-LATIN-1.</li>
 * <li><code>UTF-8</code><br/>
 * Eight-bit Unicode Transformation Format.</li>
 * <li><code>UTF-16BE</code><br/>
 * Sixteen-bit Unicode Transformation Format, big-endian byte order.</li>
 * <li><code>UTF-16LE</code><br/>
 * Sixteen-bit Unicode Transformation Format, little-endian byte order.</li>
 * <li><code>UTF-16</code><br/>
 * Sixteen-bit Unicode Transformation Format, byte order specified by a mandatory initial byte-order mark (either order
 * accepted on input, big-endian used on output.)</li>
 * </ul>
 * 
 * This perhaps would best belong in the [lang] project. Even if a similar interface is defined in [lang], it is not
 * forseen that [codec] would be made to depend on [lang].
 * 
 * @see <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard charsets</a>
 * @author Apache Software Foundation
 * @since 1.4
 * @version $Id: CharEncoding.java 797857 2009-07-25 23:43:33Z ggregory $
 */
public class CharEncoding {
    /**
     * CharEncodingISO Latin Alphabet No. 1, a.k.a. ISO-LATIN-1. </p>
     * <p>
     * Every implementation of the Java platform is required to support this character encoding.
     * </p>
     * 
     * @see <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard charsets</a>
     */
    public static final String ISO_8859_1 = "ISO-8859-1";

    /**
     * <p>
     * Seven-bit ASCII, also known as ISO646-US, also known as the Basic Latin block of the Unicode character set.
     * </p>
     * <p>
     * Every implementation of the Java platform is required to support this character encoding.
     * </p>
     * 
     * @see <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard charsets</a>
     */
    public static final String US_ASCII = "US-ASCII";

    /**
     * <p>
     * Sixteen-bit Unicode Transformation Format, The byte order specified by a mandatory initial byte-order mark
     * (either order accepted on input, big-endian used on output)
     * </p>
     * <p>
     * Every implementation of the Java platform is required to support this character encoding.
     * </p>
     * 
     * @see <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard charsets</a>
     */
    public static final String UTF_16 = "UTF-16";

    /**
     * <p>
     * Sixteen-bit Unicode Transformation Format, big-endian byte order.
     * </p>
     * <p>
     * Every implementation of the Java platform is required to support this character encoding.
     * </p>
     * 
     * @see <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard charsets</a>
     */
    public static final String UTF_16BE = "UTF-16BE";

    /**
     * <p>
     * Sixteen-bit Unicode Transformation Format, little-endian byte order.
     * </p>
     * <p>
     * Every implementation of the Java platform is required to support this character encoding.
     * </p>
     * 
     * @see <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard charsets</a>
     */
    public static final String UTF_16LE = "UTF-16LE";

    /**
     * <p>
     * Eight-bit Unicode Transformation Format.
     * </p>
     * <p>
     * Every implementation of the Java platform is required to support this character encoding.
     * </p>
     * 
     * @see <a href="http://java.sun.com/j2se/1.4.2/docs/api/java/nio/charset/Charset.html">Standard charsets</a>
     */
    public static final String UTF_8 = "UTF-8";
}