/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package org.mozilla.jss.asn1;

import java.math.BigInteger;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Vector;
import org.mozilla.jss.util.Assert;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;

/**
 * An ASN.1 SET, which is an unordered collection of ASN.1 values.
 * It has an interface like a Java Vector, but the ordering is arbitrary.
 * Null entries may be added; they will be skipped when encoding.
 */
public class SET implements ASN1Value {

    public static final Tag TAG = new Tag(Tag.Class.UNIVERSAL, 17);

    public Tag getTag() {
        return TAG;
    }
    protected static final Form FORM = Form.CONSTRUCTED;

    // The elements of the set
    protected Vector elements = new Vector();

    private void addElement( Element e ) {
        elements.addElement(e);
    }

    private void insertElementAt( Element e, int index ) {
        elements.insertElementAt(e, index);
    }

    /**
     * Adds an element to this SET.
     */
    public void addElement( ASN1Value v ) {
        addElement( new Element(v) );
    }

    /**
     * Adds an element to this SET with the given implicit tag. For example,
     * if the ASN.1 were:
     * <pre>
     *  MyType ::= SET {
     *      item        [0] IMPLICIT INTEGER,
     *      ... }
     * </pre>
     * then the "item" element could be added (with a sample value of 45)
     *  to the SET with:
     * <pre>
     * myTypeInstance.addElement( new Tag(0), new INTEGER(45) );
     * </pre>
     */
    public void addElement( Tag implicitTag, ASN1Value v ) {
        addElement( new Element(implicitTag, v) );
    }

    /**
     * Inserts an element at the given index.
     */
    public void insertElementAt( ASN1Value v, int index ) {
        insertElementAt( new Element(v), index );
    }

    /**
     * Inserts an element with the given implicit tag at the given index.
     */
    public void insertElementAt( Tag implicitTag, ASN1Value v, int index ) {
        insertElementAt( new Element(implicitTag, v), index );
    }

    /**
     * Returns the element at the given index in the SET.
     */
    public ASN1Value elementAt( int index ) {
        return ((Element)elements.elementAt(index)).getValue();
    }

    /**
     * Returns the tag of the element at the given index. If the element
     * has an implicit tag, that is returned.  Otherwise, the tag of the
     * underlying type is returned.
     */
     public Tag tagAt( int index ) {
        Tag implicit = ((Element)elements.elementAt(index)).getImplicitTag();
        if( implicit != null ) {
            return implicit;
        } else {
            return elementAt(index).getTag();
        }
    }

    /**
     * Returns the element with the given Tag, or null if no element exists
     * with the given tag.
     */
    public ASN1Value elementWithTag( Tag tag ) {
        // hmmm...linear search for now, should use hashtable later

        int size = elements.size();
        for( int i=0; i < size; i++ ) {
            Element e = (Element) elements.elementAt(i);
            if( e.getTag().equals(tag) ) {
                return e.getValue();
            }
        }
        return null;
    }

    /**
     * Returns the number of elements in this SET.
     */
    public int size() {
        return elements.size();
    }

    /**
     * Removes all elements from this SET.
     */
    public void removeAllElements() {
        elements.removeAllElements();
    }

    /**
     * Removes the element from the specified index.
     */
    public void removeElementAt(int index) {
        elements.removeElementAt(index);
    }

    /**
     * Writes the DER encoding to the given output stream.
     */
    public void encode(OutputStream ostream)
        throws IOException
    {
        encode(getTag(), ostream);
    }

    /**
     * Writes the DER encoding to the given output stream,
     * using the given implicit tag. To satisfy DER encoding rules,
     * the elements will be re-ordered either by tag or lexicographically.
     */
    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        // what ordering method?
        boolean lexOrdering;
        if( elements.size() < 2 ) {
            // doesn't matter, only one element
            lexOrdering = true;
        } else if( tagAt(0).equals(tagAt(1)) ) {
            // tags are the same, lexicographic ordering
            lexOrdering = true;
        } else {
            // tags are different, order by tag
            lexOrdering = false;
        }

        // compute and order contents
        int numElements = elements.size();
        int totalBytes = 0;
        Vector encodings = new Vector(numElements);
        Vector tags = new Vector(numElements);
        int i;
        for(i = 0; i < numElements; i++ ) {

            // if an entry is null, just skip it
            if( elementAt(i) != null ) {
                byte[] enc = ASN1Util.encode(tagAt(i), elementAt(i));

                totalBytes += enc.length;

                if( lexOrdering ) {
                    insertInOrder(encodings, enc);
                } else {
                   insertInOrder(encodings, enc, tags, (int) tagAt(i).getNum());
                }
            }
        }

        // write header
        ASN1Header header = new ASN1Header( implicitTag, FORM, totalBytes );
        header.encode(ostream);

        // write contents in order
        for(i=0; i < numElements; i++ ) {
            ostream.write( (byte[]) encodings.elementAt(i) );
        }
    }

    /**
     * Encodes this SET without re-ordering it.  This may violate
     * DER, but it is within BER.
     */
    public void BERencode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();

        // compute contents
        int size = elements.size();
        for(int i = 0; i < size; i++ ) {
            ASN1Value el = elementAt(i);
            if(el!=null) {
                el.encode(tagAt(i), bos);
            }
        }

        byte[] bytes = bos.toByteArray();

        // write header
        ASN1Header header = new ASN1Header( implicitTag, FORM, bytes.length );
        header.encode(ostream);

        // write contents
        ostream.write(bytes);
    }

    // performs ascending lexicographic ordering
    // linear search, but number of items is usually going to be small.
    private static void insertInOrder(Vector encs, byte[] enc) {
        int size = encs.size();

        // find the lowest item that we are less than or equal to
        int i;
        for(i=0; i < size; i++) {
            if( compare(enc, (byte[])encs.elementAt(i)) < 1 ) {
                break;
            }
        }

        // insert ourself before this item
        encs.insertElementAt(enc, i);
    }

    // performs ascending ordering by tag
    // linear search, but number of items is usually going to be small.
    private static void insertInOrder(Vector encs, byte[] enc, Vector tags,
                int tag)
    {
        int size = encs.size();

        // find the lowest item that we are less than or equal to
        int i;
        for(i = 0; i < size; i++) {
            if( tag <= ((Integer)tags.elementAt(i)).intValue() ) {
                break;
            }
        }

        // insert ourself before this item
        encs.insertElementAt(enc, i);
        tags.insertElementAt(new Integer(i), i );
    }


    // compares two byte arrays
    // returns 1 if left > right, -1 if left < right, 0 if left == right
    private static int compare(byte[] left, byte[] right) {
        int min = (left.length < right.length) ? left.length : right.length;

        for(int i=0; i < min; i++) {
            if( (left[i]&0xff) < (right[i]&0xff) ) {
                return -1;
            } else if( (left[i]&0xff) > (right[i]&0xff) ) {
                return 1;
            }
        }

        // equal up to the minimal endpoint
        if( left.length > min ) {
            Assert._assert(right.length==min);
            return 1;
        }
        if( right.length > min ) {
            Assert._assert(left.length==min);
            return -1;
        }
        return 0;
    }

    /**
     * An element of a SET
     */
    static class Element {

        /**
         * Makes a new SET element from the given value.
         */
        public Element( ASN1Value val ) {
            this.val = val;
        }

        /**
         * Makes a new SET element from the given value with the given
         * implicit tag.
         */
        public Element( Tag implicitTag, ASN1Value val )
        {
            this.val = val;
            this.implicitTag = implicitTag;
        }

        private ASN1Value val;
        /**
         * Returns the value of this SET element.
         */
        public ASN1Value getValue() {
            return val;
        }

        /**
         * Returns the tag that actually shows up in the encoding.
         * If there is an implicit tag, it will be used.  Otherwise,
         * it will be the base tag for the value.
         */
        public Tag getTag() {
            if(implicitTag!=null) {
                return implicitTag;
            } else {
                return val.getTag();
            }
        }

        private Tag implicitTag=null;
        /**
         * Returns the implicit tag for this value, if there is one.
         * If not, returns null.
         */
        public Tag getImplicitTag() {
            return implicitTag;
        }
    }

/**
 * SET.Template
 * This class is used for decoding DER-encoded SETs.
 */
public static class Template implements ASN1Template {

    private Vector elements = new Vector();

    private void addElement( Element e ) {
        elements.addElement(e);
    }

    private void insertElementAt( Element e, int index ) {
        elements.insertElementAt(e, index);
    }

    /**
     * Adds a sub-template to the end of this SET template. For example,
     *  if the ASN.1 included:
     * <pre>
     * MySet ::= SET {
     *      item        SubType,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySet template with:
     * <pre>
     *  mySet.addElement( new SubType.Template() );
     * </pre>
     */
    public void addElement( ASN1Template t ) {
        addElement( new Element(TAG, t, false) );
    }

    /**
     * Inserts the template at the given index.
     */
    public void insertElementAt( ASN1Template t, int index )
    {
        insertElementAt( new Element(TAG, t, false), index );
    }

    /**
     * Adds a sub-template with the given implicit tag to the end of this
     *  SET template. For example, if the ASN.1 included:
     * <pre>
     * MySet ::= SET {
     *      item        [0] IMPLICIT SubType,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySet template with:
     * <pre>
     *  mySet.addElement( new Tag(0), new SubType.Template() );
     * </pre>
     */
    public void addElement( Tag implicit, ASN1Template t ) {
        addElement( new Element(implicit, t, false) );
    }

    /**
     * Inserts the template with the given implicit tag at the given index.
     */
    public void insertElementAt( Tag implicit, ASN1Template t,
        int index )
    {
        insertElementAt( new Element(implicit, t, false), index );
    }

    /**
     * Adds an optional sub-template to the end
     *  of this SET template. For example, if the ASN.1 included:
     * <pre>
     * MySet ::= SET {
     *      item        SubType OPTIONAL,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySet template with:
     * <pre>
     *  mySet.addOptionalElement( new SubType.Template() );
     * </pre>
     */
    public void addOptionalElement( ASN1Template t ) {
        addElement( new Element(TAG, t, true) );
    }

    /**
     * Inserts the optional template at the given index.
     */
    public void insertOptionalElementAt( ASN1Template t, int index )
    {
        insertElementAt( new Element(null, t, true), index );
    }

    /**
     * Adds an optional sub-template with the given implicit tag to the end
     *  of this SET template. For example, if the ASN.1 included:
     * <pre>
     * MySet ::= SET {
     *      item        [0] IMPLICIT SubType OPTIONAL,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySet template with:
     * <pre>
     *  mySet.addOptionalElement( new Tag(0), new SubType.Template() );
     * </pre>
     */
    public void addOptionalElement( Tag implicit, ASN1Template t ) {
        addElement( new Element(implicit, t, true) );
    }

    /**
     * Inserts the optional template with the given default
     * value at the given index.
     */
    public void insertOptionalElementAt( Tag implicit, ASN1Template t,
        int index )
    {
        insertElementAt( new Element(implicit, t, true), index );
    }


    /**
     * Adds a sub-template with the given default value to the end
     *  of this SET template. For example, if the ASN.1 included:
     * <pre>
     * MySet ::= SET {
     *      item        INTEGER DEFAULT (5),
     *      ... }
     * </pre>
     * the "item" element would be added to the MySet template with:
     * <pre>
     *  mySet.addElement( new SubType.Template(), new INTEGER(5) );
     * </pre>
     */
    public void addElement( ASN1Template t, ASN1Value def ) {
        addElement( new Element(TAG, t, def) );
    }

    /**
     * Inserts the template with the given default
     * value at the given index.
     */
    public void insertElementAt( ASN1Template t, ASN1Value def, int index )
    {
        insertElementAt( new Element(null, t, def), index );
    }

    /**
     * Adds a sub-template with the given default value and implicit tag to
     *  the end of this SET template. For example, if the ASN.1 included:
     * <pre>
     * MySet ::= SET {
     *      item        [0] IMPLICIT INTEGER DEFAULT (5),
     *      ... }
     * </pre>
     * the "item" element would be added to the MySet template with:
     * <pre>
     *  mySet.addElement( new Tag(0), new SubType.Template(), new INTEGER(5) );
     * </pre>
     */
    public void addElement( Tag implicit, ASN1Template t, ASN1Value def ) {
        addElement( new Element(implicit, t, def) );
    }

    /**
     * Inserts the template with the given implicit tag and given default
     * value at the given index.
     */
    public void insertElementAt( Tag implicit, ASN1Template t, ASN1Value def,
        int index )
    {
        insertElementAt( new Element(implicit, t, def), index );
    }

    /**
     * Returns the implicit tag of the item stored at the given index.
     * May be NULL if no implicit tag was specified.
     */
    public Tag implicitTagAt(int index) {
        return ((Element)elements.elementAt(index)).getImplicitTag();
    }

    /**
     * Returns the sub-template stored at the given index.
     */
    public ASN1Template templateAt(int index) {
        return ((Element)elements.elementAt(index)).getTemplate();
    }

    /**
     * Returns <code>true</code> if the sub-template at the given index
     * is optional.
     */
    public boolean isOptionalAt(int index) {
        return ((Element)elements.elementAt(index)).isOptional();
    }

    private boolean isRepeatableAt(int index) {
        return ((Element)elements.elementAt(index)).isRepeatable();
    }

    /**
     * Returns the default value for the sub-template at the given index.
     * May return NULL if no default value was specified.
     */
    public ASN1Value defaultAt(int index) {
        return ((Element)elements.elementAt(index)).getDefault();
    }

    /**
     * Returns the number of elements in the SET.
     */
    public int size() {
        return elements.size();
    }

    public void removeAllElements() {
        elements.removeAllElements();
    }

    public void removeElementAt(int index) {
        elements.removeElementAt(index);
    }

    private Tag getTag() {
        return SET.TAG;
    }

    /**
     * Determines whether the given tag satisfies this template.
     */
    public boolean tagMatch(Tag tag) {
        return( tag.equals(SET.TAG) );
    }

    /**
     * Decodes the input stream into a SET value.
     */
    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return decode(getTag(), istream);
    }

    /**
     * Decodes the input stream into a SET value with the given implicit
     *  tag.
     */
    public ASN1Value decode(Tag tag, InputStream istream)
        throws IOException, InvalidBERException
    {
      try {
        ASN1Header header = new ASN1Header(istream);

        header.validate( tag, Form.CONSTRUCTED );

        // remainingContent will be -1 for indefinite length encoding
        long remainingContent = header.getContentLength();
        SET set = new SET();
        ASN1Header lookAhead;
        boolean[] found = new boolean[ elements.size() ];
        
        // while content remains, try to decode it
        while( remainingContent > 0  || remainingContent == -1) {

            // find out about the next item
            lookAhead = ASN1Header.lookAhead(istream);

            // if we found the end-of-content marker, we're done
            if( lookAhead.isEOC() ) {
                if( remainingContent != -1 ) {
                    throw new InvalidBERException("Unexpected end-of-content"+
                        "marker");
                }
                lookAhead = new ASN1Header(istream);
                break;
            }

            // Find the element with the matching tag
            int index = findElementByTag( lookAhead.getTag() );
            if( index == -1 ) {
                // element not found
                throw new InvalidBERException("Unexpected Tag in SET: "+
                    lookAhead.getTag() );
            }
            Element e = (Element) elements.elementAt(index);
            if( found[index] && ! e.isRepeatable() ) {
                // element already found, and it's not repeatable
                throw new InvalidBERException("Duplicate Tag in SET: "+
                    lookAhead.getTag() );
            }

            // mark this element as found
            found[index] = true;

            // Decode this element
            ASN1Template t = e.getTemplate();
            ASN1Value val;
            CountingStream countstream = new CountingStream(istream);
            if( e.getImplicitTag() == null ) {
                val = t.decode(countstream);
            } else {
                val = t.decode(e.getImplicitTag(), countstream);
            }

            // Decrement remaining count
            long len = countstream.getNumRead();
            if( remainingContent != -1 ) {
                if( remainingContent < len ) {
                    // this item went past the end of the SET
                    throw new InvalidBERException("Item went "+
                        (len-remainingContent)+" bytes past the end of"+
                        " the SET");
                }
                remainingContent -= len;
            }

            // Store this element in the SET
            SET.Element se;
            if( e.getImplicitTag() == null ) {
                // no implicit tag
                se = new SET.Element(val);
            } else {
                // there is an implicit tag
                se = new SET.Element( e.getImplicitTag(), val );
            }
            set.addElement(se);
        }

        // We check for this after we read in each item, so this shouldn't
        // happen
        Assert._assert( remainingContent == 0 || remainingContent == -1);

        // Deal with elements that weren't present.
        int size = elements.size();
        for(int i = 0; i < size; i++) {
            if( !found[i] ) {
                if( isOptionalAt(i) || isRepeatableAt(i) ) {
                    // no problem
                } else if( defaultAt(i) != null ) {
                    set.addElement( new SET.Element(defaultAt(i)) );
                } else {
                    throw new InvalidBERException("Field not found in SET");
                }
            }
        }

        return set;

      } catch(InvalidBERException e) {
        throw new InvalidBERException(e, "SET");
      }
    }


    /**
     * Returns the index in the vector of the type with this tag and class,
     * or -1 if not found.
     * lame linear search - but we're dealing with small numbers of elements,
     * so it's probably not worth it to use a hashtable
     */
    private int findElementByTag(Tag tag) {
        int size = elements.size();

        for( int i = 0; i < size ; i++ ) {
            Element e = (Element) elements.elementAt(i);
            if( e.tagMatch( tag ) ) {
                // match!
                return i;
            }
        }

        // no match
        return -1;
    }

    /**
     * An element of a SET template.
     */
    public static class Element {

        public Element(Tag implicitTag, ASN1Template type, boolean optional)
        {
            this.type = type;
            defaultVal = null;
            this.optional = optional;
            this.implicitTag = implicitTag;
        }

        public Element(Tag implicitTag, ASN1Template type, ASN1Value defaultVal)
        {
            this.type = type;
            this.defaultVal = defaultVal;
            optional = false;
            this.implicitTag = implicitTag;
        }

        // Repeatability is used for SET OF.  It is package private.
        private boolean repeatable;
        void makeRepeatable() {
            repeatable = true;
        }
        boolean isRepeatable() {
            return repeatable;
        }

        private boolean optional;
        public boolean isOptional() {
            return optional;
        }

        private Tag implicitTag=null;
        public Tag getImplicitTag() {
            return implicitTag;
        }

        /**
         * Determines whether the given tag satisfies this SET element.
         */
        public boolean tagMatch(Tag tag) {
            if( implicitTag != null ) {
                return( implicitTag.equals(tag) );
            } else {
                return type.tagMatch(tag);
            }
        }

        private ASN1Template type;
        /**
         * Returns the template for this element.
         */
        public ASN1Template getTemplate() {
            return type;
        }

        private ASN1Value defaultVal=null;
        /**
         * Returns the default value for this element, if one exists.
         * Otherwise, returns null.
         */
        public ASN1Value getDefault() {
            return defaultVal;
        }
    }
} // End of SET.Template

/**
 * A Template for decoding SET OF values.
 * Use this if you have a SIZE qualifier on your SET OF.
 * The SET will consume as many instances of type as it can, rather than
 * stopping after the first one. This is equivalent to SIZE (0..MAX).
 * If you need something more restrictive, you can look at what gets parsed
 * and decide whether it's OK or not yourself.
 */
public static class OF_Template implements ASN1Template {

    private OF_Template() { }

    private Template template;  // a normal SET template

    /**
     * Creates an OF_Template with the given type. For example:
     * <pre>
     * MySet ::= SET OF INTEGER;
     * </pre>
     * A <code>MySet</code> template would be constructed with:
     * <pre>
     * SET.OF_Template mySetTemplate = new SET.OF_Template( new
     *                                          INTEGER.Template() );
     * </pre>
     */
    public OF_Template(ASN1Template type) {
        template = new Template();
        Template.Element el = new Template.Element( null, type, false );
        el.makeRepeatable();
        template.addElement( el );
    }

    public boolean tagMatch(Tag tag) {
        return TAG.equals(tag);
    }

    /**
     * Decodes a <code>SET OF</code> from its BER encoding.
     */
    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return template.decode(istream);
    }

    /**
     * Decodes a <code>SET OF</code> with an implicit tag from its BER
     * encoding.
     */
    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
        return template.decode(implicitTag, istream);
    }
}

    // Test driver for SET
    public static void main(String args[]) {

      try {

        if(args.length > 0) {

        FileInputStream fin = new FileInputStream( args[0] );

        Template t = new SET.Template();

        t.addElement(new Tag(0), new INTEGER.Template() );
        t.addElement( new Tag(3), new INTEGER.Template() );
        t.addOptionalElement( new Tag(4), new INTEGER.Template() );
        t.addElement( new Tag(5), new INTEGER.Template(), new INTEGER(67) );
        t.addElement( new Tag(29), new BOOLEAN.Template() );
        t.addElement( new Tag(30), new BOOLEAN.Template(), new BOOLEAN(false) );
        t.addElement( new Tag(1), new INTEGER.Template() );
        t.addElement( new Tag(2), new INTEGER.Template() );

        SET st = (SET) t.decode(new BufferedInputStream(fin) );

        for(int i=0; i < st.size(); i++) {
            ASN1Value v = st.elementAt(i);
            if( v instanceof INTEGER ) {
                INTEGER in = (INTEGER) st.elementAt(i);
                System.out.println("INTEGER: "+in);
            } else if( v instanceof BOOLEAN ) {
                BOOLEAN bo = (BOOLEAN) st.elementAt(i);
                System.out.println("BOOLEAN: "+bo);
            } else {
                System.out.println("Unknown value");
            }
        }

        } else {


        SET s = new SET();
        s.addElement( new Tag(0), new INTEGER(255) );
        s.addElement( new Tag(29), new BOOLEAN(true) );
        s.addElement( new Tag(1), new INTEGER(-322) );
        s.addElement( new Tag(2), new INTEGER(0) );
        s.addElement( new Tag(3), new INTEGER("623423948273") );

        s.encode(System.out);

        }

      } catch( Exception e ) {
        e.printStackTrace();
      }

    }

}
