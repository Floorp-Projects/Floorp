/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2011 Mozilla Foundation
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
import nu.validator.htmlparser.annotation.NsUri;

final class StackNode<T> {
    final int flags;

    final @Local String name;

    final @Local String popName;

    final @NsUri String ns;

    final T node;

    // Only used on the list of formatting elements
    HtmlAttributes attributes;

    private int refcount = 1;

    // [NOCPP[

    private final TaintableLocatorImpl locator;
    
    public TaintableLocatorImpl getLocator() {
        return locator;
    }

    // ]NOCPP]

    @Inline public int getFlags() {
        return flags;
    }

    public int getGroup() {
        return flags & ElementName.GROUP_MASK;
    }

    public boolean isScoping() {
        return (flags & ElementName.SCOPING) != 0;
    }

    public boolean isSpecial() {
        return (flags & ElementName.SPECIAL) != 0;
    }

    public boolean isFosterParenting() {
        return (flags & ElementName.FOSTER_PARENTING) != 0;
    }

    public boolean isHtmlIntegrationPoint() {
        return (flags & ElementName.HTML_INTEGRATION_POINT) != 0;
    }

    // [NOCPP[
    
    public boolean isOptionalEndTag() {
        return (flags & ElementName.OPTIONAL_END_TAG) != 0;
    }
    
    // ]NOCPP]

    /**
     * Constructor for copying. This doesn't take another <code>StackNode</code>
     * because in C++ the caller is reponsible for reobtaining the local names
     * from another interner.
     * 
     * @param flags
     * @param ns
     * @param name
     * @param node
     * @param popName
     * @param attributes
     */
    StackNode(int flags, @NsUri String ns, @Local String name, T node,
            @Local String popName, HtmlAttributes attributes
            // [NOCPP[
            , TaintableLocatorImpl locator
    // ]NOCPP]
    ) {
        this.flags = flags;
        this.name = name;
        this.popName = popName;
        this.ns = ns;
        this.node = node;
        this.attributes = attributes;
        this.refcount = 1;
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * Short hand for well-known HTML elements.
     * 
     * @param elementName
     * @param node
     */
    StackNode(ElementName elementName, T node
    // [NOCPP[
            , TaintableLocatorImpl locator
    // ]NOCPP]
    ) {
        this.flags = elementName.getFlags();
        this.name = elementName.name;
        this.popName = elementName.name;
        this.ns = "http://www.w3.org/1999/xhtml";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        assert !elementName.isCustom() : "Don't use this constructor for custom elements.";
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * Constructor for HTML formatting elements.
     * 
     * @param elementName
     * @param node
     * @param attributes
     */
    StackNode(ElementName elementName, T node, HtmlAttributes attributes
    // [NOCPP[
            , TaintableLocatorImpl locator
    // ]NOCPP]
    ) {
        this.flags = elementName.getFlags();
        this.name = elementName.name;
        this.popName = elementName.name;
        this.ns = "http://www.w3.org/1999/xhtml";
        this.node = node;
        this.attributes = attributes;
        this.refcount = 1;
        assert !elementName.isCustom() : "Don't use this constructor for custom elements.";
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * The common-case HTML constructor.
     * 
     * @param elementName
     * @param node
     * @param popName
     */
    StackNode(ElementName elementName, T node, @Local String popName
    // [NOCPP[
            , TaintableLocatorImpl locator
    // ]NOCPP]
    ) {
        this.flags = elementName.getFlags();
        this.name = elementName.name;
        this.popName = popName;
        this.ns = "http://www.w3.org/1999/xhtml";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * Constructor for SVG elements. Note that the order of the arguments is
     * what distinguishes this from the HTML constructor. This is ugly, but
     * AFAICT the least disruptive way to make this work with Java's generics
     * and without unnecessary branches. :-(
     * 
     * @param elementName
     * @param popName
     * @param node
     */
    StackNode(ElementName elementName, @Local String popName, T node
    // [NOCPP[
            , TaintableLocatorImpl locator
    // ]NOCPP]
    ) {
        this.flags = prepareSvgFlags(elementName.getFlags());
        this.name = elementName.name;
        this.popName = popName;
        this.ns = "http://www.w3.org/2000/svg";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * Constructor for MathML.
     * 
     * @param elementName
     * @param node
     * @param popName
     * @param markAsIntegrationPoint
     */
    StackNode(ElementName elementName, T node, @Local String popName,
            boolean markAsIntegrationPoint
            // [NOCPP[
            , TaintableLocatorImpl locator
    // ]NOCPP]
    ) {
        this.flags = prepareMathFlags(elementName.getFlags(),
                markAsIntegrationPoint);
        this.name = elementName.name;
        this.popName = popName;
        this.ns = "http://www.w3.org/1998/Math/MathML";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    private static int prepareSvgFlags(int flags) {
        flags &= ~(ElementName.FOSTER_PARENTING | ElementName.SCOPING
                | ElementName.SPECIAL | ElementName.OPTIONAL_END_TAG);
        if ((flags & ElementName.SCOPING_AS_SVG) != 0) {
            flags |= (ElementName.SCOPING | ElementName.SPECIAL | ElementName.HTML_INTEGRATION_POINT);
        }
        return flags;
    }

    private static int prepareMathFlags(int flags,
            boolean markAsIntegrationPoint) {
        flags &= ~(ElementName.FOSTER_PARENTING | ElementName.SCOPING
                | ElementName.SPECIAL | ElementName.OPTIONAL_END_TAG);
        if ((flags & ElementName.SCOPING_AS_MATHML) != 0) {
            flags |= (ElementName.SCOPING | ElementName.SPECIAL);
        }
        if (markAsIntegrationPoint) {
            flags |= ElementName.HTML_INTEGRATION_POINT;
        }
        return flags;
    }

    @SuppressWarnings("unused") private void destructor() {
        Portability.delete(attributes);
    }

    public void dropAttributes() {
        attributes = null;
    }

    // [NOCPP[
    /**
     * @see java.lang.Object#toString()
     */
    @Override public @Local String toString() {
        return name;
    }

    // ]NOCPP]

    public void retain() {
        refcount++;
    }

    public void release() {
        refcount--;
        if (refcount == 0) {
            Portability.delete(this);
        }
    }
}
