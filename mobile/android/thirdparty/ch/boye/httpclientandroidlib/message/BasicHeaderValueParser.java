/*
 * ====================================================================
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.message;

import java.util.ArrayList;
import java.util.List;

import ch.boye.httpclientandroidlib.HeaderElement;
import ch.boye.httpclientandroidlib.NameValuePair;
import ch.boye.httpclientandroidlib.ParseException;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.protocol.HTTP;
import ch.boye.httpclientandroidlib.util.Args;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;

/**
 * Basic implementation for parsing header values into elements.
 * Instances of this class are stateless and thread-safe.
 * Derived classes are expected to maintain these properties.
 *
 * @since 4.0
 */
@Immutable
public class BasicHeaderValueParser implements HeaderValueParser {

    /**
     * A default instance of this class, for use as default or fallback.
     * Note that {@link BasicHeaderValueParser} is not a singleton, there
     * can be many instances of the class itself and of derived classes.
     * The instance here provides non-customized, default behavior.
     *
     * @deprecated (4.3) use {@link #INSTANCE}
     */
    @Deprecated
    public final static
        BasicHeaderValueParser DEFAULT = new BasicHeaderValueParser();

    public final static BasicHeaderValueParser INSTANCE = new BasicHeaderValueParser();

    private final static char PARAM_DELIMITER                = ';';
    private final static char ELEM_DELIMITER                 = ',';
    private final static char[] ALL_DELIMITERS               = new char[] {
                                                                PARAM_DELIMITER,
                                                                ELEM_DELIMITER
                                                                };

    public BasicHeaderValueParser() {
        super();
    }

    /**
     * Parses elements with the given parser.
     *
     * @param value     the header value to parse
     * @param parser    the parser to use, or <code>null</code> for default
     *
     * @return  array holding the header elements, never <code>null</code>
     */
    public static
        HeaderElement[] parseElements(final String value,
                                      final HeaderValueParser parser) throws ParseException {
        Args.notNull(value, "Value");

        final CharArrayBuffer buffer = new CharArrayBuffer(value.length());
        buffer.append(value);
        final ParserCursor cursor = new ParserCursor(0, value.length());
        return (parser != null ? parser : BasicHeaderValueParser.INSTANCE)
            .parseElements(buffer, cursor);
    }


    // non-javadoc, see interface HeaderValueParser
    public HeaderElement[] parseElements(final CharArrayBuffer buffer,
                                         final ParserCursor cursor) {
        Args.notNull(buffer, "Char array buffer");
        Args.notNull(cursor, "Parser cursor");
        final List<HeaderElement> elements = new ArrayList<HeaderElement>();
        while (!cursor.atEnd()) {
            final HeaderElement element = parseHeaderElement(buffer, cursor);
            if (!(element.getName().length() == 0 && element.getValue() == null)) {
                elements.add(element);
            }
        }
        return elements.toArray(new HeaderElement[elements.size()]);
    }


    /**
     * Parses an element with the given parser.
     *
     * @param value     the header element to parse
     * @param parser    the parser to use, or <code>null</code> for default
     *
     * @return  the parsed header element
     */
    public static
        HeaderElement parseHeaderElement(final String value,
                                         final HeaderValueParser parser) throws ParseException {
        Args.notNull(value, "Value");

        final CharArrayBuffer buffer = new CharArrayBuffer(value.length());
        buffer.append(value);
        final ParserCursor cursor = new ParserCursor(0, value.length());
        return (parser != null ? parser : BasicHeaderValueParser.INSTANCE)
                .parseHeaderElement(buffer, cursor);
    }


    // non-javadoc, see interface HeaderValueParser
    public HeaderElement parseHeaderElement(final CharArrayBuffer buffer,
                                            final ParserCursor cursor) {
        Args.notNull(buffer, "Char array buffer");
        Args.notNull(cursor, "Parser cursor");
        final NameValuePair nvp = parseNameValuePair(buffer, cursor);
        NameValuePair[] params = null;
        if (!cursor.atEnd()) {
            final char ch = buffer.charAt(cursor.getPos() - 1);
            if (ch != ELEM_DELIMITER) {
                params = parseParameters(buffer, cursor);
            }
        }
        return createHeaderElement(nvp.getName(), nvp.getValue(), params);
    }


    /**
     * Creates a header element.
     * Called from {@link #parseHeaderElement}.
     *
     * @return  a header element representing the argument
     */
    protected HeaderElement createHeaderElement(
            final String name,
            final String value,
            final NameValuePair[] params) {
        return new BasicHeaderElement(name, value, params);
    }


    /**
     * Parses parameters with the given parser.
     *
     * @param value     the parameter list to parse
     * @param parser    the parser to use, or <code>null</code> for default
     *
     * @return  array holding the parameters, never <code>null</code>
     */
    public static
        NameValuePair[] parseParameters(final String value,
                                        final HeaderValueParser parser) throws ParseException {
        Args.notNull(value, "Value");

        final CharArrayBuffer buffer = new CharArrayBuffer(value.length());
        buffer.append(value);
        final ParserCursor cursor = new ParserCursor(0, value.length());
        return (parser != null ? parser : BasicHeaderValueParser.INSTANCE)
                .parseParameters(buffer, cursor);
    }



    // non-javadoc, see interface HeaderValueParser
    public NameValuePair[] parseParameters(final CharArrayBuffer buffer,
                                           final ParserCursor cursor) {
        Args.notNull(buffer, "Char array buffer");
        Args.notNull(cursor, "Parser cursor");
        int pos = cursor.getPos();
        final int indexTo = cursor.getUpperBound();

        while (pos < indexTo) {
            final char ch = buffer.charAt(pos);
            if (HTTP.isWhitespace(ch)) {
                pos++;
            } else {
                break;
            }
        }
        cursor.updatePos(pos);
        if (cursor.atEnd()) {
            return new NameValuePair[] {};
        }

        final List<NameValuePair> params = new ArrayList<NameValuePair>();
        while (!cursor.atEnd()) {
            final NameValuePair param = parseNameValuePair(buffer, cursor);
            params.add(param);
            final char ch = buffer.charAt(cursor.getPos() - 1);
            if (ch == ELEM_DELIMITER) {
                break;
            }
        }

        return params.toArray(new NameValuePair[params.size()]);
    }

    /**
     * Parses a name-value-pair with the given parser.
     *
     * @param value     the NVP to parse
     * @param parser    the parser to use, or <code>null</code> for default
     *
     * @return  the parsed name-value pair
     */
    public static
       NameValuePair parseNameValuePair(final String value,
                                        final HeaderValueParser parser) throws ParseException {
        Args.notNull(value, "Value");

        final CharArrayBuffer buffer = new CharArrayBuffer(value.length());
        buffer.append(value);
        final ParserCursor cursor = new ParserCursor(0, value.length());
        return (parser != null ? parser : BasicHeaderValueParser.INSTANCE)
                .parseNameValuePair(buffer, cursor);
    }


    // non-javadoc, see interface HeaderValueParser
    public NameValuePair parseNameValuePair(final CharArrayBuffer buffer,
                                            final ParserCursor cursor) {
        return parseNameValuePair(buffer, cursor, ALL_DELIMITERS);
    }

    private static boolean isOneOf(final char ch, final char[] chs) {
        if (chs != null) {
            for (final char ch2 : chs) {
                if (ch == ch2) {
                    return true;
                }
            }
        }
        return false;
    }

    public NameValuePair parseNameValuePair(final CharArrayBuffer buffer,
                                            final ParserCursor cursor,
                                            final char[] delimiters) {
        Args.notNull(buffer, "Char array buffer");
        Args.notNull(cursor, "Parser cursor");

        boolean terminated = false;

        int pos = cursor.getPos();
        final int indexFrom = cursor.getPos();
        final int indexTo = cursor.getUpperBound();

        // Find name
        final String name;
        while (pos < indexTo) {
            final char ch = buffer.charAt(pos);
            if (ch == '=') {
                break;
            }
            if (isOneOf(ch, delimiters)) {
                terminated = true;
                break;
            }
            pos++;
        }

        if (pos == indexTo) {
            terminated = true;
            name = buffer.substringTrimmed(indexFrom, indexTo);
        } else {
            name = buffer.substringTrimmed(indexFrom, pos);
            pos++;
        }

        if (terminated) {
            cursor.updatePos(pos);
            return createNameValuePair(name, null);
        }

        // Find value
        final String value;
        int i1 = pos;

        boolean qouted = false;
        boolean escaped = false;
        while (pos < indexTo) {
            final char ch = buffer.charAt(pos);
            if (ch == '"' && !escaped) {
                qouted = !qouted;
            }
            if (!qouted && !escaped && isOneOf(ch, delimiters)) {
                terminated = true;
                break;
            }
            if (escaped) {
                escaped = false;
            } else {
                escaped = qouted && ch == '\\';
            }
            pos++;
        }

        int i2 = pos;
        // Trim leading white spaces
        while (i1 < i2 && (HTTP.isWhitespace(buffer.charAt(i1)))) {
            i1++;
        }
        // Trim trailing white spaces
        while ((i2 > i1) && (HTTP.isWhitespace(buffer.charAt(i2 - 1)))) {
            i2--;
        }
        // Strip away quotes if necessary
        if (((i2 - i1) >= 2)
            && (buffer.charAt(i1) == '"')
            && (buffer.charAt(i2 - 1) == '"')) {
            i1++;
            i2--;
        }
        value = buffer.substring(i1, i2);
        if (terminated) {
            pos++;
        }
        cursor.updatePos(pos);
        return createNameValuePair(name, value);
    }

    /**
     * Creates a name-value pair.
     * Called from {@link #parseNameValuePair}.
     *
     * @param name      the name
     * @param value     the value, or <code>null</code>
     *
     * @return  a name-value pair representing the arguments
     */
    protected NameValuePair createNameValuePair(final String name, final String value) {
        return new BasicNameValuePair(name, value);
    }

}

