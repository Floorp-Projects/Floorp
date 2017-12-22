/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2008-2015 Mozilla Foundation
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

import java.io.IOException;

import org.xml.sax.SAXException;

import nu.validator.htmlparser.annotation.Auto;
import nu.validator.htmlparser.annotation.Inline;
import nu.validator.htmlparser.common.ByteReadable;

public abstract class MetaScanner {

    /**
     * Constant for "charset".
     */
    private static final char[] CHARSET = { 'h', 'a', 'r', 's', 'e', 't' };

    /**
     * Constant for "content".
     */
    private static final char[] CONTENT = { 'o', 'n', 't', 'e', 'n', 't' };

    /**
     * Constant for "http-equiv".
     */
    private static final char[] HTTP_EQUIV = { 't', 't', 'p', '-', 'e', 'q',
            'u', 'i', 'v' };

    /**
     * Constant for "content-type".
     */
    private static final char[] CONTENT_TYPE = { 'c', 'o', 'n', 't', 'e', 'n',
            't', '-', 't', 'y', 'p', 'e' };

    private static final int NO = 0;

    private static final int M = 1;

    private static final int E = 2;

    private static final int T = 3;

    private static final int A = 4;

    private static final int DATA = 0;

    private static final int TAG_OPEN = 1;

    private static final int SCAN_UNTIL_GT = 2;

    private static final int TAG_NAME = 3;

    private static final int BEFORE_ATTRIBUTE_NAME = 4;

    private static final int ATTRIBUTE_NAME = 5;

    private static final int AFTER_ATTRIBUTE_NAME = 6;

    private static final int BEFORE_ATTRIBUTE_VALUE = 7;

    private static final int ATTRIBUTE_VALUE_DOUBLE_QUOTED = 8;

    private static final int ATTRIBUTE_VALUE_SINGLE_QUOTED = 9;

    private static final int ATTRIBUTE_VALUE_UNQUOTED = 10;

    private static final int AFTER_ATTRIBUTE_VALUE_QUOTED = 11;

    private static final int MARKUP_DECLARATION_OPEN = 13;

    private static final int MARKUP_DECLARATION_HYPHEN = 14;

    private static final int COMMENT_START = 15;

    private static final int COMMENT_START_DASH = 16;

    private static final int COMMENT = 17;

    private static final int COMMENT_END_DASH = 18;

    private static final int COMMENT_END = 19;

    private static final int SELF_CLOSING_START_TAG = 20;

    private static final int HTTP_EQUIV_NOT_SEEN = 0;

    private static final int HTTP_EQUIV_CONTENT_TYPE = 1;

    private static final int HTTP_EQUIV_OTHER = 2;

    /**
     * The data source.
     */
    protected ByteReadable readable;

    /**
     * The state of the state machine that recognizes the tag name "meta".
     */
    private int metaState = NO;

    /**
     * The current position in recognizing the attribute name "content".
     */
    private int contentIndex = Integer.MAX_VALUE;

    /**
     * The current position in recognizing the attribute name "charset".
     */
    private int charsetIndex = Integer.MAX_VALUE;

    /**
     * The current position in recognizing the attribute name "http-equive".
     */
    private int httpEquivIndex = Integer.MAX_VALUE;

    /**
     * The current position in recognizing the attribute value "content-type".
     */
    private int contentTypeIndex = Integer.MAX_VALUE;

    /**
     * The tokenizer state.
     */
    protected int stateSave = DATA;

    /**
     * The currently filled length of strBuf.
     */
    private int strBufLen;

    /**
     * Accumulation buffer for attribute values.
     */
    private @Auto char[] strBuf;

    private String content;

    private String charset;

    private int httpEquivState;

    // CPPONLY: private TreeBuilder treeBuilder;

    public MetaScanner(
        // CPPONLY: TreeBuilder tb
    ) {
        this.readable = null;
        this.metaState = NO;
        this.contentIndex = Integer.MAX_VALUE;
        this.charsetIndex = Integer.MAX_VALUE;
        this.httpEquivIndex = Integer.MAX_VALUE;
        this.contentTypeIndex = Integer.MAX_VALUE;
        this.stateSave = DATA;
        this.strBufLen = 0;
        this.strBuf = new char[36];
        this.content = null;
        this.charset = null;
        this.httpEquivState = HTTP_EQUIV_NOT_SEEN;
        // CPPONLY: this.treeBuilder = tb;
        // CPPONLY: this.mEncoding = null;
    }

    @SuppressWarnings("unused") private void destructor() {
        Portability.releaseString(content);
        Portability.releaseString(charset);
    }

    // [NOCPP[

    /**
     * Reads a byte from the data source.
     *
     * -1 means end.
     * @return
     * @throws IOException
     */
    protected int read() throws IOException {
        return readable.readByte();
    }

    // ]NOCPP]

    // WARNING When editing this, makes sure the bytecode length shown by javap
    // stays under 8000 bytes!
    /**
     * The runs the meta scanning algorithm.
     */
    protected final void stateLoop(int state)
            throws SAXException, IOException {
        int c = -1;
        boolean reconsume = false;
        stateloop: for (;;) {
            switch (state) {
                case DATA:
                    dataloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            c = read();
                        }
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '<':
                                state = MetaScanner.TAG_OPEN;
                                break dataloop; // FALL THROUGH continue
                            // stateloop;
                            default:
                                continue;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case TAG_OPEN:
                    tagopenloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case 'm':
                            case 'M':
                                metaState = M;
                                state = MetaScanner.TAG_NAME;
                                break tagopenloop;
                                // continue stateloop;
                            case '!':
                                state = MetaScanner.MARKUP_DECLARATION_OPEN;
                                continue stateloop;
                            case '?':
                            case '/':
                                state = MetaScanner.SCAN_UNTIL_GT;
                                continue stateloop;
                            case '>':
                                state = MetaScanner.DATA;
                                continue stateloop;
                            default:
                                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                                    metaState = NO;
                                    state = MetaScanner.TAG_NAME;
                                    break tagopenloop;
                                    // continue stateloop;
                                }
                                state = MetaScanner.DATA;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case TAG_NAME:
                    tagnameloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\u000C':
                                state = MetaScanner.BEFORE_ATTRIBUTE_NAME;
                                break tagnameloop;
                            // continue stateloop;
                            case '/':
                                state = MetaScanner.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '>':
                                state = MetaScanner.DATA;
                                continue stateloop;
                            case 'e':
                            case 'E':
                                if (metaState == M) {
                                    metaState = E;
                                } else {
                                    metaState = NO;
                                }
                                continue;
                            case 't':
                            case 'T':
                                if (metaState == E) {
                                    metaState = T;
                                } else {
                                    metaState = NO;
                                }
                                continue;
                            case 'a':
                            case 'A':
                                if (metaState == T) {
                                    metaState = A;
                                } else {
                                    metaState = NO;
                                }
                                continue;
                            default:
                                metaState = NO;
                                continue;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case BEFORE_ATTRIBUTE_NAME:
                    beforeattributenameloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            c = read();
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case -1:
                                break stateloop;
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\u000C':
                                continue;
                            case '/':
                                state = MetaScanner.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '>':
                                if (handleTag()) {
                                    break stateloop;
                                }
                                state = DATA;
                                continue stateloop;
                            case 'c':
                            case 'C':
                                contentIndex = 0;
                                charsetIndex = 0;
                                httpEquivIndex = Integer.MAX_VALUE;
                                contentTypeIndex = Integer.MAX_VALUE;
                                state = MetaScanner.ATTRIBUTE_NAME;
                                break beforeattributenameloop;
                            case 'h':
                            case 'H':
                                contentIndex = Integer.MAX_VALUE;
                                charsetIndex = Integer.MAX_VALUE;
                                httpEquivIndex = 0;
                                contentTypeIndex = Integer.MAX_VALUE;
                                state = MetaScanner.ATTRIBUTE_NAME;
                                break beforeattributenameloop;
                            default:
                                contentIndex = Integer.MAX_VALUE;
                                charsetIndex = Integer.MAX_VALUE;
                                httpEquivIndex = Integer.MAX_VALUE;
                                contentTypeIndex = Integer.MAX_VALUE;
                                state = MetaScanner.ATTRIBUTE_NAME;
                                break beforeattributenameloop;
                            // continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case ATTRIBUTE_NAME:
                    attributenameloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\u000C':
                                state = MetaScanner.AFTER_ATTRIBUTE_NAME;
                                continue stateloop;
                            case '/':
                                state = MetaScanner.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '=':
                                strBufLen = 0;
                                contentTypeIndex = 0;
                                state = MetaScanner.BEFORE_ATTRIBUTE_VALUE;
                                break attributenameloop;
                            // continue stateloop;
                            case '>':
                                if (handleTag()) {
                                    break stateloop;
                                }
                                state = MetaScanner.DATA;
                                continue stateloop;
                            default:
                                if (metaState == A) {
                                    if (c >= 'A' && c <= 'Z') {
                                        c += 0x20;
                                    }
                                    if (contentIndex < CONTENT.length && c == CONTENT[contentIndex]) {
                                        ++contentIndex;
                                    } else {
                                        contentIndex = Integer.MAX_VALUE;
                                    }
                                    if (charsetIndex < CHARSET.length && c == CHARSET[charsetIndex]) {
                                        ++charsetIndex;
                                    } else {
                                        charsetIndex = Integer.MAX_VALUE;
                                    }
                                    if (httpEquivIndex < HTTP_EQUIV.length && c == HTTP_EQUIV[httpEquivIndex]) {
                                        ++httpEquivIndex;
                                    } else {
                                        httpEquivIndex = Integer.MAX_VALUE;
                                    }
                                }
                                continue;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case BEFORE_ATTRIBUTE_VALUE:
                    beforeattributevalueloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\u000C':
                                continue;
                            case '"':
                                state = MetaScanner.ATTRIBUTE_VALUE_DOUBLE_QUOTED;
                                break beforeattributevalueloop;
                            // continue stateloop;
                            case '\'':
                                state = MetaScanner.ATTRIBUTE_VALUE_SINGLE_QUOTED;
                                continue stateloop;
                            case '>':
                                if (handleTag()) {
                                    break stateloop;
                                }
                                state = MetaScanner.DATA;
                                continue stateloop;
                            default:
                                handleCharInAttributeValue(c);
                                state = MetaScanner.ATTRIBUTE_VALUE_UNQUOTED;
                                continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case ATTRIBUTE_VALUE_DOUBLE_QUOTED:
                    attributevaluedoublequotedloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            c = read();
                        }
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '"':
                                handleAttributeValue();
                                state = MetaScanner.AFTER_ATTRIBUTE_VALUE_QUOTED;
                                break attributevaluedoublequotedloop;
                            // continue stateloop;
                            default:
                                handleCharInAttributeValue(c);
                                continue;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case AFTER_ATTRIBUTE_VALUE_QUOTED:
                    afterattributevaluequotedloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\u000C':
                                state = MetaScanner.BEFORE_ATTRIBUTE_NAME;
                                continue stateloop;
                            case '/':
                                state = MetaScanner.SELF_CLOSING_START_TAG;
                                break afterattributevaluequotedloop;
                            // continue stateloop;
                            case '>':
                                if (handleTag()) {
                                    break stateloop;
                                }
                                state = MetaScanner.DATA;
                                continue stateloop;
                            default:
                                state = MetaScanner.BEFORE_ATTRIBUTE_NAME;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case SELF_CLOSING_START_TAG:
                    c = read();
                    switch (c) {
                        case -1:
                            break stateloop;
                        case '>':
                            if (handleTag()) {
                                break stateloop;
                            }
                            state = MetaScanner.DATA;
                            continue stateloop;
                        default:
                            state = MetaScanner.BEFORE_ATTRIBUTE_NAME;
                            reconsume = true;
                            continue stateloop;
                    }
                case ATTRIBUTE_VALUE_UNQUOTED:
                    for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            c = read();
                        }
                        switch (c) {
                            case -1:
                                break stateloop;
                            case ' ':
                            case '\t':
                            case '\n':

                            case '\u000C':
                                handleAttributeValue();
                                state = MetaScanner.BEFORE_ATTRIBUTE_NAME;
                                continue stateloop;
                            case '>':
                                handleAttributeValue();
                                if (handleTag()) {
                                    break stateloop;
                                }
                                state = MetaScanner.DATA;
                                continue stateloop;
                            default:
                                handleCharInAttributeValue(c);
                                continue;
                        }
                    }
                case AFTER_ATTRIBUTE_NAME:
                    for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\u000C':
                                continue;
                            case '/':
                                handleAttributeValue();
                                state = MetaScanner.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '=':
                                strBufLen = 0;
                                contentTypeIndex = 0;
                                state = MetaScanner.BEFORE_ATTRIBUTE_VALUE;
                                continue stateloop;
                            case '>':
                                handleAttributeValue();
                                if (handleTag()) {
                                    break stateloop;
                                }
                                state = MetaScanner.DATA;
                                continue stateloop;
                            case 'c':
                            case 'C':
                                contentIndex = 0;
                                charsetIndex = 0;
                                state = MetaScanner.ATTRIBUTE_NAME;
                                continue stateloop;
                            default:
                                contentIndex = Integer.MAX_VALUE;
                                charsetIndex = Integer.MAX_VALUE;
                                state = MetaScanner.ATTRIBUTE_NAME;
                                continue stateloop;
                        }
                    }
                case MARKUP_DECLARATION_OPEN:
                    markupdeclarationopenloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '-':
                                state = MetaScanner.MARKUP_DECLARATION_HYPHEN;
                                break markupdeclarationopenloop;
                            // continue stateloop;
                            default:
                                state = MetaScanner.SCAN_UNTIL_GT;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case MARKUP_DECLARATION_HYPHEN:
                    markupdeclarationhyphenloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '-':
                                state = MetaScanner.COMMENT_START;
                                break markupdeclarationhyphenloop;
                            // continue stateloop;
                            default:
                                state = MetaScanner.SCAN_UNTIL_GT;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case COMMENT_START:
                    commentstartloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '-':
                                state = MetaScanner.COMMENT_START_DASH;
                                continue stateloop;
                            case '>':
                                state = MetaScanner.DATA;
                                continue stateloop;
                            default:
                                state = MetaScanner.COMMENT;
                                break commentstartloop;
                            // continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case COMMENT:
                    commentloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '-':
                                state = MetaScanner.COMMENT_END_DASH;
                                break commentloop;
                            // continue stateloop;
                            default:
                                continue;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case COMMENT_END_DASH:
                    commentenddashloop: for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '-':
                                state = MetaScanner.COMMENT_END;
                                break commentenddashloop;
                            // continue stateloop;
                            default:
                                state = MetaScanner.COMMENT;
                                continue stateloop;
                        }
                    }
                    // CPPONLY: MOZ_FALLTHROUGH;
                case COMMENT_END:
                    for (;;) {
                        c = read();
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '>':
                                state = MetaScanner.DATA;
                                continue stateloop;
                            case '-':
                                continue;
                            default:
                                state = MetaScanner.COMMENT;
                                continue stateloop;
                        }
                    }
                case COMMENT_START_DASH:
                    c = read();
                    switch (c) {
                        case -1:
                            break stateloop;
                        case '-':
                            state = MetaScanner.COMMENT_END;
                            continue stateloop;
                        case '>':
                            state = MetaScanner.DATA;
                            continue stateloop;
                        default:
                            state = MetaScanner.COMMENT;
                            continue stateloop;
                    }
                case ATTRIBUTE_VALUE_SINGLE_QUOTED:
                    for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            c = read();
                        }
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '\'':
                                handleAttributeValue();
                                state = MetaScanner.AFTER_ATTRIBUTE_VALUE_QUOTED;
                                continue stateloop;
                            default:
                                handleCharInAttributeValue(c);
                                continue;
                        }
                    }
                case SCAN_UNTIL_GT:
                    for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            c = read();
                        }
                        switch (c) {
                            case -1:
                                break stateloop;
                            case '>':
                                state = MetaScanner.DATA;
                                continue stateloop;
                            default:
                                continue;
                        }
                    }
            }
        }
        stateSave  = state;
    }

    private void handleCharInAttributeValue(int c) {
        if (metaState == A) {
            if (contentIndex == CONTENT.length || charsetIndex == CHARSET.length) {
                addToBuffer(c);
            } else if (httpEquivIndex == HTTP_EQUIV.length) {
                if (contentTypeIndex < CONTENT_TYPE.length && toAsciiLowerCase(c) == CONTENT_TYPE[contentTypeIndex]) {
                    ++contentTypeIndex;
                } else {
                    contentTypeIndex = Integer.MAX_VALUE;
                }
            }
        }
    }

    @Inline private int toAsciiLowerCase(int c) {
        if (c >= 'A' && c <= 'Z') {
            return c + 0x20;
        }
        return c;
    }

    /**
     * Adds a character to the accumulation buffer.
     * @param c the character to add
     */
    private void addToBuffer(int c) {
        if (strBufLen == strBuf.length) {
            char[] newBuf = new char[strBuf.length + (strBuf.length << 1)];
            System.arraycopy(strBuf, 0, newBuf, 0, strBuf.length);
            strBuf = newBuf;
        }
        strBuf[strBufLen++] = (char)c;
    }

    /**
     * Attempts to extract a charset name from the accumulation buffer.
     * @return <code>true</code> if successful
     * @throws SAXException
     */
    private void handleAttributeValue() throws SAXException {
        if (metaState != A) {
            return;
        }
        if (contentIndex == CONTENT.length && content == null) {
            content = Portability.newStringFromBuffer(strBuf, 0, strBufLen
                 // CPPONLY: , treeBuilder, false
            );
            return;
        }
        if (charsetIndex == CHARSET.length && charset == null) {
            charset = Portability.newStringFromBuffer(strBuf, 0, strBufLen
                 // CPPONLY: , treeBuilder, false
            );
            return;
        }
        if (httpEquivIndex == HTTP_EQUIV.length
                && httpEquivState == HTTP_EQUIV_NOT_SEEN) {
            httpEquivState = (contentTypeIndex == CONTENT_TYPE.length) ? HTTP_EQUIV_CONTENT_TYPE
                    : HTTP_EQUIV_OTHER;
            return;
        }
    }

    private boolean handleTag() throws SAXException {
        boolean stop = handleTagInner();
        Portability.releaseString(content);
        content = null;
        Portability.releaseString(charset);
        charset = null;
        httpEquivState = HTTP_EQUIV_NOT_SEEN;
        return stop;
    }

    private boolean handleTagInner() throws SAXException {
        if (charset != null && tryCharset(charset)) {
                return true;
        }
        if (content != null && httpEquivState == HTTP_EQUIV_CONTENT_TYPE) {
            String extract = TreeBuilder.extractCharsetFromContent(content
                // CPPONLY: , treeBuilder
            );
            if (extract == null) {
                return false;
            }
            boolean success = tryCharset(extract);
            Portability.releaseString(extract);
            return success;
        }
        return false;
    }

    /**
     * Tries to switch to an encoding.
     *
     * @param encoding
     * @return <code>true</code> if successful
     * @throws SAXException
     */
    protected abstract boolean tryCharset(String encoding) throws SAXException;

}
