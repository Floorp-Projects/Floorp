/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2008-2011 Mozilla Foundation
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

import nu.validator.htmlparser.annotation.Auto;
import nu.validator.htmlparser.annotation.IdType;
import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.NsUri;
import nu.validator.htmlparser.annotation.Prefix;
import nu.validator.htmlparser.annotation.QName;
import nu.validator.htmlparser.common.Interner;
import nu.validator.htmlparser.common.XmlViolationPolicy;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;

/**
 * Be careful with this class. QName is the name in from HTML tokenization.
 * Otherwise, please refer to the interface doc.
 * 
 * @version $Id: AttributesImpl.java 206 2008-03-20 14:09:29Z hsivonen $
 * @author hsivonen
 */
public final class HtmlAttributes implements Attributes {

    // [NOCPP[

    private static final AttributeName[] EMPTY_ATTRIBUTENAMES = new AttributeName[0];

    private static final String[] EMPTY_STRINGS = new String[0];

    // ]NOCPP]

    public static final HtmlAttributes EMPTY_ATTRIBUTES = new HtmlAttributes(
            AttributeName.HTML);

    private int mode;

    private int length;

    private @Auto AttributeName[] names;

    private @Auto String[] values; // XXX perhaps make this @NoLength?

    // [NOCPP[

    private String idValue;

    private int xmlnsLength;

    private AttributeName[] xmlnsNames;

    private String[] xmlnsValues;

    // ]NOCPP]

    public HtmlAttributes(int mode) {
        this.mode = mode;
        this.length = 0;
        /*
         * The length of 5 covers covers 98.3% of elements
         * according to Hixie
         */
        this.names = new AttributeName[5];
        this.values = new String[5];

        // [NOCPP[

        this.idValue = null;

        this.xmlnsLength = 0;

        this.xmlnsNames = HtmlAttributes.EMPTY_ATTRIBUTENAMES;

        this.xmlnsValues = HtmlAttributes.EMPTY_STRINGS;

        // ]NOCPP]
    }
    /*
    public HtmlAttributes(HtmlAttributes other) {
        this.mode = other.mode;
        this.length = other.length;
        this.names = new AttributeName[other.length];
        this.values = new String[other.length];
        // [NOCPP[
        this.idValue = other.idValue;
        this.xmlnsLength = other.xmlnsLength;
        this.xmlnsNames = new AttributeName[other.xmlnsLength];
        this.xmlnsValues = new String[other.xmlnsLength];
        // ]NOCPP]
    }
    */

    void destructor() {
        clear(0);
    }
    
    /**
     * Only use with a static argument
     * 
     * @param name
     * @return
     */
    public int getIndex(AttributeName name) {
        for (int i = 0; i < length; i++) {
            if (names[i] == name) {
                return i;
            }
        }
        return -1;
    }

    // [NOCPP[
    
    public int getIndex(String qName) {
        for (int i = 0; i < length; i++) {
            if (names[i].getQName(mode).equals(qName)) {
                return i;
            }
        }
        return -1;
    }
    
    public int getIndex(String uri, String localName) {
        for (int i = 0; i < length; i++) {
            if (names[i].getLocal(mode).equals(localName)
                    && names[i].getUri(mode).equals(uri)) {
                return i;
            }
        }
        return -1;
    }

    public @IdType String getType(String qName) {
        int index = getIndex(qName);
        if (index == -1) {
            return null;
        } else {
            return getType(index);
        }
    }

    public @IdType String getType(String uri, String localName) {
        int index = getIndex(uri, localName);
        if (index == -1) {
            return null;
        } else {
            return getType(index);
        }
    }
    
    public String getValue(String qName) {
        int index = getIndex(qName);
        if (index == -1) {
            return null;
        } else {
            return getValue(index);
        }
    }

    public String getValue(String uri, String localName) {
        int index = getIndex(uri, localName);
        if (index == -1) {
            return null;
        } else {
            return getValue(index);
        }
    }
    
    // ]NOCPP]
    
    public int getLength() {
        return length;
    }

    public @Local String getLocalName(int index) {
        if (index < length && index >= 0) {
            return names[index].getLocal(mode);
        } else {
            return null;
        }
    }

    // [NOCPP[
    
    public @QName String getQName(int index) {
        if (index < length && index >= 0) {
            return names[index].getQName(mode);
        } else {
            return null;
        }
    }

    public @IdType String getType(int index) {
        if (index < length && index >= 0) {
            return (names[index] == AttributeName.ID) ? "ID" : "CDATA";
        } else {
            return null;
        }
    }

    // ]NOCPP]
    
    public AttributeName getAttributeName(int index) {
        if (index < length && index >= 0) {
            return names[index];
        } else {
            return null;
        }
    }

    public @NsUri String getURI(int index) {
        if (index < length && index >= 0) {
            return names[index].getUri(mode);
        } else {
            return null;
        }
    }

    public @Prefix String getPrefix(int index) {
        if (index < length && index >= 0) {
            return names[index].getPrefix(mode);
        } else {
            return null;
        }
    }

    public String getValue(int index) {
        if (index < length && index >= 0) {
            return values[index];
        } else {
            return null;
        }
    }

    /**
     * Only use with static argument.
     * 
     * @see org.xml.sax.Attributes#getValue(java.lang.String)
     */
    public String getValue(AttributeName name) {
        int index = getIndex(name);
        if (index == -1) {
            return null;
        } else {
            return getValue(index);
        }
    }
    
    // [NOCPP[

    public String getId() {
        return idValue;
    }

    public int getXmlnsLength() {
        return xmlnsLength;
    }

    public @Local String getXmlnsLocalName(int index) {
        if (index < xmlnsLength && index >= 0) {
            return xmlnsNames[index].getLocal(mode);
        } else {
            return null;
        }
    }

    public @NsUri String getXmlnsURI(int index) {
        if (index < xmlnsLength && index >= 0) {
            return xmlnsNames[index].getUri(mode);
        } else {
            return null;
        }
    }

    public String getXmlnsValue(int index) {
        if (index < xmlnsLength && index >= 0) {
            return xmlnsValues[index];
        } else {
            return null;
        }
    }
    
    public int getXmlnsIndex(AttributeName name) {
        for (int i = 0; i < xmlnsLength; i++) {
            if (xmlnsNames[i] == name) {
                return i;
            }
        }
        return -1;
    }
    
    public String getXmlnsValue(AttributeName name) {
        int index = getXmlnsIndex(name);
        if (index == -1) {
            return null;
        } else {
            return getXmlnsValue(index);
        }
    }
    
    public AttributeName getXmlnsAttributeName(int index) {
        if (index < xmlnsLength && index >= 0) {
            return xmlnsNames[index];
        } else {
            return null;
        }
    }

    // ]NOCPP]

    void addAttribute(AttributeName name, String value
            // [NOCPP[
            , XmlViolationPolicy xmlnsPolicy
    // ]NOCPP]        
    ) throws SAXException {
        // [NOCPP[
        if (name == AttributeName.ID) {
            idValue = value;
        }

        if (name.isXmlns()) {
            if (xmlnsNames.length == xmlnsLength) {
                int newLen = xmlnsLength == 0 ? 2 : xmlnsLength << 1;
                AttributeName[] newNames = new AttributeName[newLen];
                System.arraycopy(xmlnsNames, 0, newNames, 0, xmlnsNames.length);
                xmlnsNames = newNames;
                String[] newValues = new String[newLen];
                System.arraycopy(xmlnsValues, 0, newValues, 0, xmlnsValues.length);
                xmlnsValues = newValues;
            }
            xmlnsNames[xmlnsLength] = name;
            xmlnsValues[xmlnsLength] = value;
            xmlnsLength++;
            switch (xmlnsPolicy) {
                case FATAL:
                    // this is ugly
                    throw new SAXException("Saw an xmlns attribute.");
                case ALTER_INFOSET:
                    return;
                case ALLOW:
                    // fall through
            }
        }

        // ]NOCPP]

        if (names.length == length) {
            int newLen = length << 1; // The first growth covers virtually
            // 100% of elements according to
            // Hixie
            AttributeName[] newNames = new AttributeName[newLen];
            System.arraycopy(names, 0, newNames, 0, names.length);
            names = newNames;
            String[] newValues = new String[newLen];
            System.arraycopy(values, 0, newValues, 0, values.length);
            values = newValues;
        }
        names[length] = name;
        values[length] = value;
        length++;
    }

    void clear(int m) {
        for (int i = 0; i < length; i++) {
            names[i].release();
            names[i] = null;
            Portability.releaseString(values[i]);
            values[i] = null;
        }
        length = 0;
        mode = m;
        // [NOCPP[
        idValue = null;
        for (int i = 0; i < xmlnsLength; i++) {
            xmlnsNames[i] = null;
            xmlnsValues[i] = null;
        }
        xmlnsLength = 0;
        // ]NOCPP]
    }
    
    /**
     * This is used in C++ to release special <code>isindex</code>
     * attribute values whose ownership is not transferred.
     */
    void releaseValue(int i) {
        Portability.releaseString(values[i]);        
    }
    
    /**
     * This is only used for <code>AttributeName</code> ownership transfer
     * in the isindex case to avoid freeing custom names twice in C++.
     */
    void clearWithoutReleasingContents() {
        for (int i = 0; i < length; i++) {
            names[i] = null;
            values[i] = null;
        }
        length = 0;
    }

    boolean contains(AttributeName name) {
        for (int i = 0; i < length; i++) {
            if (name.equalsAnother(names[i])) {
                return true;
            }
        }
        // [NOCPP[
        for (int i = 0; i < xmlnsLength; i++) {
            if (name.equalsAnother(xmlnsNames[i])) {
                return true;
            }
        }
        // ]NOCPP]
        return false;
    }

    public void adjustForMath() {
        mode = AttributeName.MATHML;
    }

    public void adjustForSvg() {
        mode = AttributeName.SVG;
    }

    public HtmlAttributes cloneAttributes(Interner interner) throws SAXException {
        assert (length == 0 && xmlnsLength == 0) || mode == 0 || mode == 3;
        HtmlAttributes clone = new HtmlAttributes(0);
        for (int i = 0; i < length; i++) {
            clone.addAttribute(names[i].cloneAttributeName(interner), Portability.newStringFromString(values[i])
            // [NOCPP[
                   , XmlViolationPolicy.ALLOW
            // ]NOCPP]
            );
        }
        // [NOCPP[
        for (int i = 0; i < xmlnsLength; i++) {
            clone.addAttribute(xmlnsNames[i],
                    xmlnsValues[i], XmlViolationPolicy.ALLOW);
        }
        // ]NOCPP]
        return clone; // XXX!!!
    }
    
    public boolean equalsAnother(HtmlAttributes other) {
        assert mode == 0 || mode == 3 : "Trying to compare attributes in foreign content.";
        int otherLength = other.getLength();
        if (length != otherLength) {
            return false;
        }
        for (int i = 0; i < length; i++) {
            // Work around the limitations of C++
            boolean found = false;
            // The comparing just the local names is OK, since these attribute
            // holders are both supposed to belong to HTML formatting elements
            @Local String ownLocal = names[i].getLocal(AttributeName.HTML);
            for (int j = 0; j < otherLength; j++) {
                if (ownLocal == other.names[j].getLocal(AttributeName.HTML)) {
                    found = true;
                    if (!Portability.stringEqualsString(values[i], other.values[j])) {
                        return false;
                    }
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }
    
    // [NOCPP[
    
    void processNonNcNames(TreeBuilder<?> treeBuilder, XmlViolationPolicy namePolicy) throws SAXException {
        for (int i = 0; i < length; i++) {
            AttributeName attName = names[i];
            if (!attName.isNcName(mode)) {
                String name = attName.getLocal(mode);
                switch (namePolicy) {
                    case ALTER_INFOSET:
                        names[i] = AttributeName.create(NCName.escapeName(name));
                        // fall through
                    case ALLOW:
                        if (attName != AttributeName.XML_LANG) {
                            treeBuilder.warn("Attribute \u201C" + name + "\u201D is not serializable as XML 1.0.");
                        }
                        break;
                    case FATAL:
                        treeBuilder.fatal("Attribute \u201C" + name + "\u201D is not serializable as XML 1.0.");
                        break;
                }
            }
        }
    }
    
    public void merge(HtmlAttributes attributes) throws SAXException {
        int len = attributes.getLength();
        for (int i = 0; i < len; i++) {
            AttributeName name = attributes.getAttributeName(i);
            if (!contains(name)) {
                addAttribute(name, attributes.getValue(i), XmlViolationPolicy.ALLOW);
            }
        }
    }


    // ]NOCPP]
    
}
