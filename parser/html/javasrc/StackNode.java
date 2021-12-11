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
    // Index where this stack node is stored in the tree builder's list of stack nodes.
    // A value of -1 indicates that the stack node is not owned by a tree builder and
    // must delete itself when its refcount reaches 0.
    final int idxInTreeBuilder;

    int flags;

    @Local String name;

    @Local String popName;

    @NsUri String ns;

    T node;

    // Only used on the list of formatting elements
    HtmlAttributes attributes;

    private int refcount = 0;

    /*
     *  Only valid for formatting elements
     */
    // CPPONLY: private @HtmlCreator Object htmlCreator;

    // [NOCPP[

    private TaintableLocatorImpl locator;

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

    StackNode(int idxInTreeBuilder) {
        this.idxInTreeBuilder = idxInTreeBuilder;
        this.flags = 0;
        this.name = null;
        this.popName = null;
        // CPPONLY: this.ns = 0;
        this.node = null;
        this.attributes = null;
        this.refcount = 0;
        // CPPONLY: this.htmlCreator = null;
    }

    // CPPONLY: public @HtmlCreator Object getHtmlCreator() {
    // CPPONLY:     return htmlCreator;
    // CPPONLY: }

    /**
     * Setter for copying. This doesn't take another <code>StackNode</code>
     * because in C++ the caller is responsible for reobtaining the local names
     * from another interner.
     *
     * @param flags
     * @param ns
     * @param name
     * @param node
     * @param popName
     * @param attributes
     */
    void setValues(int flags, @NsUri String ns, @Local String name, T node,
            @Local String popName, HtmlAttributes attributes,
            // CPPONLY: @HtmlCreator Object htmlCreator
            // [NOCPP[
            TaintableLocatorImpl locator
            // ]NOCPP]
    ) {
        assert isUnused();
        this.flags = flags;
        this.name = name;
        this.popName = popName;
        this.ns = ns;
        this.node = node;
        this.attributes = attributes;
        this.refcount = 1;
        /*
         * Need to track creator for formatting elements when copying.
         */
        // CPPONLY: this.htmlCreator = htmlCreator;
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
    void setValues(ElementName elementName, T node
            // [NOCPP[
            , TaintableLocatorImpl locator
            // ]NOCPP]
    ) {
        assert isUnused();
        this.flags = elementName.getFlags();
        this.name = elementName.getName();
        this.popName = elementName.getName();
        this.ns = "http://www.w3.org/1999/xhtml";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        assert elementName.isInterned() : "Don't use this constructor for custom elements.";
        /*
         * Not used for formatting elements, so no need to track creator.
         */
        // CPPONLY: this.htmlCreator = null;
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * Setter for HTML formatting elements.
     *
     * @param elementName
     * @param node
     * @param attributes
     */
    void setValues(ElementName elementName, T node, HtmlAttributes attributes
            // [NOCPP[
            , TaintableLocatorImpl locator
            // ]NOCPP]
    ) {
        assert isUnused();
        this.flags = elementName.getFlags();
        this.name = elementName.getName();
        this.popName = elementName.getName();
        this.ns = "http://www.w3.org/1999/xhtml";
        this.node = node;
        this.attributes = attributes;
        this.refcount = 1;
        assert elementName.isInterned() : "Don't use this constructor for custom elements.";
        /*
         * Need to track creator for formatting elements in order to be able
         * to clone them.
         */
        // CPPONLY: this.htmlCreator = elementName.getHtmlCreator();
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * The common-case HTML setter.
     *
     * @param elementName
     * @param node
     * @param popName
     */
    void setValues(ElementName elementName, T node, @Local String popName
            // [NOCPP[
            , TaintableLocatorImpl locator
            // ]NOCPP]
    ) {
        assert isUnused();
        this.flags = elementName.getFlags();
        this.name = elementName.getName();
        this.popName = popName;
        this.ns = "http://www.w3.org/1999/xhtml";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        /*
         * Not used for formatting elements, so no need to track creator.
         */
        // CPPONLY: this.htmlCreator = null;
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * Setter for SVG elements. Note that the order of the arguments is
     * what distinguishes this from the HTML setter. This is ugly, but
     * AFAICT the least disruptive way to make this work with Java's generics
     * and without unnecessary branches. :-(
     *
     * @param elementName
     * @param popName
     * @param node
     */
    void setValues(ElementName elementName, @Local String popName, T node
            // [NOCPP[
            , TaintableLocatorImpl locator
            // ]NOCPP]
    ) {
        assert isUnused();
        this.flags = prepareSvgFlags(elementName.getFlags());
        this.name = elementName.getName();
        this.popName = popName;
        this.ns = "http://www.w3.org/2000/svg";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        /*
         * Not used for formatting elements, so no need to track creator.
         */
        // CPPONLY: this.htmlCreator = null;
        // [NOCPP[
        this.locator = locator;
        // ]NOCPP]
    }

    /**
     * Setter for MathML.
     *
     * @param elementName
     * @param node
     * @param popName
     * @param markAsIntegrationPoint
     */
    void setValues(ElementName elementName, T node, @Local String popName,
            boolean markAsIntegrationPoint
            // [NOCPP[
            , TaintableLocatorImpl locator
            // ]NOCPP]
    ) {
        assert isUnused();
        this.flags = prepareMathFlags(elementName.getFlags(),
                markAsIntegrationPoint);
        this.name = elementName.getName();
        this.popName = popName;
        this.ns = "http://www.w3.org/1998/Math/MathML";
        this.node = node;
        this.attributes = null;
        this.refcount = 1;
        /*
         * Not used for formatting elements, so no need to track creator.
         */
        // CPPONLY: this.htmlCreator = null;
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
        // The translator adds refcount debug code here.
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

    public void release(TreeBuilder<T> owningTreeBuilder) {
        refcount--;
        assert refcount >= 0;
        if (refcount == 0) {
            Portability.delete(attributes);
            if (idxInTreeBuilder >= 0) {
                owningTreeBuilder.notifyUnusedStackNode(idxInTreeBuilder);
            } else {
                assert owningTreeBuilder == null;
                Portability.delete(this);
            }
        }
    }

    boolean isUnused() {
        return refcount == 0;
    }
}
