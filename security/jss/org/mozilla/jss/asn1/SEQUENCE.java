/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
package org.mozilla.jss.asn1;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.util.Vector;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

/**
 * An ASN.1 SEQUENCE.  This class is an ordered collection of ASN.1 values.
 * It has an interface similar to a Java <code>Vector</code>.
 * Null entries may be added; they will be skipped when encoded.
 */
public class SEQUENCE extends SET implements ASN1Value {

    public static final Tag TAG = new Tag(Tag.Class.UNIVERSAL, 16);
    public Tag getTag() {
        return TAG;
    }

    public static Template getTemplate() {
        return new Template();
    }

    /**
     * Writes the DER encoding to the given output stream,
     * using the given implicit tag.
     */
    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        BERencode(implicitTag, ostream);
    }

    // SET.Element and SEQUENCE.Element are identical types.  We could
    // have just reused SET.Element, but that would have been a bit
    // confusing for users.
    private static class Element extends SET.Element {
        public Element( ASN1Value val ) {
            super(val);
        }
        public Element( Tag implicitTag, ASN1Value val) {
            super(implicitTag, val);
        }
    }

/**
 * A class for constructing a <code>SEQUENCE</code> from its BER encoding.
 * It is an ordered collection of sub-templates.  Each sub-template can be
 * marked optional, or a default value can be given.
 */
public static class Template implements ASN1Template {

    private Vector elements = new Vector();

    private void addElement(Element el) {
        elements.addElement( el );
    }

    private void insertElementAt(Element e, int index) {
        elements.insertElementAt(e, index);
    }

    /**
     * Adds a sub-template to the end of this SEQUENCE template. For example,
     *  if the ASN.1 included:
     * <pre>
     * MySequence ::= SEQUENCE {
     *      item        SubType,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySequence template with:
     * <pre>
     *  mySequence.addElement( new SubType.Template() );
     * </pre>
     */
    public void addElement( ASN1Template t ) {
        addElement( new Element(null, t, false) );
    }

    /**
     * Inserts the template at the given index.
     */
    public void insertElementAt( ASN1Template t, int index )
    {
        insertElementAt( new Element(null, t, false), index );
    }

    /**
     * Adds a sub-template to the end of this SEQUENCE template, with the
     *  given implicit tag. For example, if the ASN.1 were:
     * <pre>
     * MySequence ::= SEQUENCE {
     *      item        [0] IMPLICIT  SubType,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySequence template with:
     * <pre>
     *  mySequence.addElement( new Tag(0), new SubType.Template());
     * </pre>
     */
    public void addElement( Tag implicitTag, ASN1Template t ) {
        addElement( new Element(implicitTag, t, false) );
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
     * Adds an optional sub-template. For example, if the ASN.1 were:
     * <pre>
     * MySequence ::= SEQUENCE {
     *      item        SubType OPTIONAL,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySequence template with:
     * <pre>
     *  mySequence.addOptionalElement( new SubType.Template() );
     * </pre>
     */
    public void addOptionalElement( ASN1Template t ) {
        addElement( new Element(null, t, true) );
    }

    /**
     * Inserts the optional template at the given index.
     */
    public void insertOptionalElementAt( ASN1Template t, int index )
    {
        insertElementAt( new Element(null, t, true), index );
    }

    /**
     * Adds an optional sub-template with an implicit tag. For example,
     *  if the ASN.1 were:
     * <pre>
     * MySequence ::= SEQUENCE {
     *      item        [0] IMPLICIT SubType OPTIONAL,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySequence template with:
     * <pre>
     *  mySequence.addOptionalElement( new SubType.Template() );
     * </pre>
     */
    public void addOptionalElement( Tag implicitTag, ASN1Template t ) {
        addElement( new Element(implicitTag, t, true) );
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
     * Adds a sub-template with a default value. For example,
     *  if the ASN.1 were:
     * <pre>
     * MySequence ::= SEQUENCE {
     *      version     INTEGER DEFAULT 1,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySequence template with:
     * <pre>
     *  mySequence.addElement( new INTEGER.Template(), new INTEGER(1) );
     * </pre>
     * @param def The default value for this field, which will be used if
     *      no value is supplied by the encoded structure. It must be of
     *      the same type as what the template would produce.
     */
    public void addElement( ASN1Template t, ASN1Value def ) {
        addElement( new Element(null, t, def) );
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
     * Adds a sub-template with a default value and an implicit tag.
     *  For example, if the ASN.1 were:
     * <pre>
     * MySequence ::= SEQUENCE {
     *      version     [0] IMPLICIT INTEGER DEFAULT 1,
     *      ... }
     * </pre>
     * the "item" element would be added to the MySequence template with:
     * <pre>
     *  mySequence.addElement( new Tag(0), new INTEGER.Template(),
     *      new INTEGER(1) );
     * </pre>
     * @param def The default value for this field, which will be used if
     *      no value is supplied by the encoded structure. It must be of
     *      the same type as what the template would produce.
     */
    public void addElement( Tag implicitTag, ASN1Template t, ASN1Value def) {
        addElement( new Element(implicitTag, t, def) );
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
    public Tag implicitTagAt( int index ) {
        return ((Element)elements.elementAt(index)).getImplicitTag();
    }

    /**
     * Returns the sub-template stored at the given index.
     */
    public ASN1Template templateAt( int index ) {
        return ((Element)elements.elementAt(index)).getTemplate();
    }

    /**
     * Returns whether the sub-template at the given index is optional.
     */
    public boolean isOptionalAt( int index ) {
        return ((Element)elements.elementAt(index)).isOptional();
    }

    /**
     * Returns the default value for the sub-template at the given index.
     * May return NULL if no default value was specified.
     */
    public ASN1Value defaultAt( int index ) {
        return ((Element)elements.elementAt(index)).getDefault();
    }

    /**
     * Returns the number of elements in this SEQUENCE template.
     */
    public int size() {
        return elements.size();
    }

    /**
     * Removes all sub-templates from this SEQUENCE template.
     */
    public void removeAllElements() {
        elements.removeAllElements();
    }

    /**
     * Removes the sub-template at the given index.
     */
    public void removeElementAt(int index) {
        elements.removeElementAt(index);
    }

    Tag getTag() {
        return SEQUENCE.TAG;
    }

    public boolean tagMatch(Tag tag) {
        return( tag.equals(SEQUENCE.TAG) );
    }

    /**
     * Decodes a SEQUENCE from its BER encoding.
     */
    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return decode(getTag(), istream);
    }

    /**
     * Decodes a SEQUENCE from its BER encoding, where the SEQUENCE itself has
     * an implicit tag.
     */
    public ASN1Value decode(Tag tag, InputStream istream)
        throws IOException, InvalidBERException
    {
      int index = 0;

      try {
        ASN1Header header = new ASN1Header(istream);

        header.validate( tag, Form.CONSTRUCTED );

        // will be -1 for indefinite encoding
        long remainingContent = header.getContentLength();

        boolean repeatableElement=false;
        SEQUENCE seq = new SEQUENCE();
        ASN1Header lookAhead=null;

        // go through the whole template
        for( index = 0; index < size(); index++ ) {

            // find out about the next item
            if( remainingContent == 0 ) {
                lookAhead = null;
            } else {
                // remainingContent > 0 or remainingContent == -1, which means
                // indefinite encoding.
                lookAhead = ASN1Header.lookAhead(istream);
            }

            // skip over items that don't match.  Hopefully they are
            // optional or have a default.  Otherwise, it's an error.
            Element e = (Element) elements.elementAt(index);
            if( (lookAhead == null) || lookAhead.isEOC() ||
                    ! e.tagMatch( lookAhead.getTag() ) )
            {
                if( e.isRepeatable() ) {
                    repeatableElement = true;
                } else if( e.isOptional() ) {
                    // put an empty entry into the SEQUENCE
                    SEQUENCE.Element se = new SEQUENCE.Element(null, null );
                    seq.addElement( null );
                } else if( e.getDefault() != null ) {
                    // use the default
                    seq.addElement( e.getDefault() );
                } else {
                    String tagDesc;
                    if( lookAhead == null ) {
                        tagDesc = "(null)";
                    } else {
                        tagDesc = lookAhead.getTag().toString();
                    }
                    throw new InvalidBERException("Missing item #" + index +
                        ": found " + tagDesc );
                }
                continue;
            }

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
                    // this item went past the end of the SEQUENCE
                    throw new InvalidBERException("Item went "+
                        (len-remainingContent)+" bytes past the end of"+
                        " the SEQUENCE");
                }
                remainingContent -= len;
            }

            // Store this element in the SEQUENCE
            if( e.producesOutput() ) {
                if(  e.getImplicitTag() == null ) {
                    // no implicit tag
                    seq.addElement( val );
                } else {
                    // there is an implicit tag
                    seq.addElement( e.getImplicitTag(), val );
                }
            }

            // If this element is repeatable, don't go on to the next element
            if( e.isRepeatable() ) {
                repeatableElement = true;
                index--;
            }
        }

        if( remainingContent > 0 ) {
            throw new InvalidBERException("SEQUENCE is " + remainingContent +
                " bytes longer than expected");
        }
        Assert._assert( remainingContent == 0 || remainingContent == -1 );

        // If this was indefinite-length encoding, consume the end-of-contents
        if( remainingContent == -1 ) {
            lookAhead = new ASN1Header(istream);
            if( ! lookAhead.isEOC() ) {
                throw new InvalidBERException("No end-of-contents marker");
            }
        }

        // Make sure we stayed in sync
        if( ! repeatableElement ) {
            Assert._assert(index == seq.size());
        }

        return seq;

      } catch(InvalidBERException e) {
        e.append("SEQUENCE(item #" +index + ")");
        throw e;
      }
    }

    /**
     * An element of a SEQUENCE template. For each sub-template, contains the
     * template, its optionality, its implicit tag, and its default value.
     */
    static class Element {

        /**
         * Creates a new element, which may or may not be optional.
         */
        public Element(Tag implicitTag, ASN1Template type, boolean optional)
        {
            this(implicitTag, type, optional, true);
        }

        /**
         * Creates a new element, which may or may not be optional.
         */
        public Element(Tag implicitTag, ASN1Template type, boolean optional,
            boolean doesProduceOutput)
        {
            this.type = type;
            defaultVal = null;
            this.optional = optional;
            this.implicitTag = implicitTag;
            this.doesProduceOutput = doesProduceOutput;
        }

        /**
         * Creates a new element with a default value.
         */
        public Element(Tag implicitTag, ASN1Template type, ASN1Value defaultVal)
        {
            this.type = type;
            this.defaultVal = defaultVal;
            optional = false;
            this.implicitTag = implicitTag;
        }

        private boolean doesProduceOutput = true;
        boolean producesOutput() {
            return doesProduceOutput;
        }

        // repeatability is provided to allow for SEQUENCE OF SIZE
        // constructs.  It is package private.
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

        public boolean tagMatch(Tag tag) {
            if( implicitTag != null ) {
                return( implicitTag.equals(tag) );
            } else {
                return type.tagMatch(tag);
            }
        }

        private ASN1Template type;
        public ASN1Template getTemplate() {
            return type;
        }

        private ASN1Value defaultVal=null;
        public ASN1Value getDefault() {
            return defaultVal;
        }
    }
} // End of SEQUENCE.Template

/**
 * A Template for decoding SEQUENCE OF values. The main difference between
 *  a SEQUENCE.Template and a SEQUENCE.OF_Template is that a regular template
 *  specifies the exact ordering, number, and type of elements of the sequence,
 *  while
 *  an OF_Template has an indefinite number of elements, all the same type.
 * For example, given:
 * <pre>
 * MyType ::= SEQUENCE OF Extension
 * </pre>
 * a MyType could be decoded with:
 * <pre>
 *  SEQUENCE.OF_Template myTypeTemplate = new SEQUENCE.OF_Template( new
 *      Extension.Template) );
 *  SEQUENCE seq = (SEQUENCE) myTypeTemplate.decode(someInputStream);
 * </pre>
 * The number of <code>Extension</code>s actually decoded could be found
 * with <code>seq.size()</code>.
 */
public static class OF_Template implements ASN1Template {

    private OF_Template() { }

    Template template;  // a normal SEQUENCE template

    public OF_Template(ASN1Template type) {
        template = new Template();
        Template.Element el = new Template.Element(null, type, true); //optional
        el.makeRepeatable();
        template.addElement( el );
    }

    public static OF_Template makeOutputlessOFTemplate(ASN1Template type) {
        OF_Template t = new OF_Template();
        t.template = new Template();
        Template.Element el = new Template.Element(null, type, true, false);
        el.makeRepeatable();
        t.template.addElement(el);
        return t;
    }

    public boolean tagMatch(Tag tag) {
        return TAG.equals(tag);
    }

    /**
     * Decodes a SEQUENCE OF from an input stream.
     */
    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return template.decode(istream);
    }

    /**
     * Decodes a SEQUENCE OF with an implicit tag from an input stream.
     */
    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
        return template.decode(implicitTag, istream);
    }
}

    public static void main(String args[]) {

      try {

        if(args.length > 0) {
            // input

            Template type = new Template();
            type.addOptionalElement( new Tag(15), new INTEGER.Template() );
            type.addElement( new Tag(16), new INTEGER.Template(),
                    new INTEGER(42) );
            type.addElement( new INTEGER.Template() );
            type.addElement( new BOOLEAN.Template() );
            type.addElement( new INTEGER.Template() );
            type.addOptionalElement( new Tag(12), new INTEGER.Template() );
            type.addElement( new BOOLEAN.Template() );
            type.addElement( new Tag(13), new INTEGER.Template(),
                new INTEGER(53) );
            type.addElement( new INTEGER.Template() );
            type.addElement( new INTEGER.Template() );
            type.addOptionalElement( new Tag(14), new INTEGER.Template() );
            type.addElement( new OBJECT_IDENTIFIER.Template() );
            type.addElement( new NULL.Template() );
            type.addElement( new EXPLICIT.Template(
                                    new Tag(27), new INTEGER.Template()));
            type.addElement( new ENUMERATED.Template() );
            type.addElement( new OCTET_STRING.Template() );
            type.addElement( new IA5String.Template() );

            CHOICE.Template choice = new CHOICE.Template();
            choice.addElement( new Tag(23), new INTEGER.Template() );
            choice.addElement( new BOOLEAN.Template() );
            type.addElement( choice );
            type.addElement( new BIT_STRING.Template() );
            type.addElement( new ANY.Template() );
            type.addElement( new PrintableString.Template() );
            type.addElement( new OF_Template( new INTEGER.Template() ) );
            type.addElement( new OF_Template( new INTEGER.Template() ) );

            FileInputStream fin = new FileInputStream(args[0]);
            System.out.println("Available: "+fin.available());
            byte[] stuff = new byte[ fin.available() ];
            ASN1Util.readFully(stuff, fin);
            SEQUENCE s=null;
            for( int i = 0; i < 1; i++) {
                s = (SEQUENCE) type.decode( new ByteArrayInputStream(stuff) );
            }

            for(int i=0; i < s.size(); i ++ ) {
                ASN1Value v = s.elementAt(i);
                if(v instanceof ENUMERATED) {
                    ENUMERATED en = (ENUMERATED) v;
                    System.out.println("ENUMERATED: "+en);
                } else if( v instanceof INTEGER ) {
                    INTEGER in = (INTEGER) v;
                    System.out.println("INTEGER: "+in);
                } else if(v instanceof BOOLEAN ) {
                    BOOLEAN bo = (BOOLEAN) v;
                    System.out.println("BOOLEAN: "+bo);
                } else if(v instanceof OBJECT_IDENTIFIER) {
                    OBJECT_IDENTIFIER oid = (OBJECT_IDENTIFIER) v;
                    System.out.println("OID: "+oid);
                } else if(v instanceof NULL) {
                    NULL n = (NULL) v;
                    System.out.println("NULL");
                } else if(v instanceof EXPLICIT) {
                    EXPLICIT ex = (EXPLICIT) v;
                    INTEGER in = (INTEGER) ex.getContent();
                    System.out.println("EXPLICIT ["+ex.getTag()+"]: "+ 
                        "INTEGER: "+in);
                } else if(v instanceof OCTET_STRING) {
                    OCTET_STRING os = (OCTET_STRING) v;
                    byte[] bytes = os.toByteArray();
                    System.out.print("OCTET_STRING: ");
                    for(int j = 0; j < bytes.length; j++) {
                        System.out.print(bytes[j]+" ");
                    }
                    System.out.println("");
                } else if( v instanceof CharacterString ) {
                    CharacterString cs = (CharacterString) v;
                    System.out.println("String: "+cs);
                } else if( v instanceof BIT_STRING ) {
                    BIT_STRING bs = (BIT_STRING) v;
                    System.out.print("BIT_STRING: padCount="+
                        bs.getPadCount()+" : ");
                    byte[] bits = bs.getBits();
                    for(int j = 0; j < bits.length; j++) {
                        System.out.print(bits[j]+" ");
                    }
                    System.out.println("");
                } else if( v instanceof ANY ) {
                    ANY any = (ANY) v;
                    Tag tag = any.getTag();
                    System.out.println("Got ANY, tag is "+tag);
                    ByteArrayInputStream bos =
                        new ByteArrayInputStream( any.getEncoded() );
                    INTEGER in = (INTEGER) new INTEGER.Template().decode(bos);
                    System.out.println("    INTEGER: "+in);
                } else if(v instanceof SEQUENCE ) {
                    SEQUENCE seq = (SEQUENCE)v;
                    System.out.println("SEQUENCE: ");
                    for(int j=0; j < seq.size(); j++ ) {
                        INTEGER in = (INTEGER) seq.elementAt(j);
                        System.out.println("    INTEGER: "+in);
                    }
                } else {
                    System.out.println("Unknown value");
                }
            }


        } else {
            // output

            SEQUENCE seq = new SEQUENCE();
            seq.addElement( new INTEGER(5) );
            seq.addElement( new BOOLEAN(true) );
            seq.addElement( new INTEGER(-322) );
            seq.addElement( new BOOLEAN(false) );
            seq.addElement( new INTEGER(0) );
            seq.addElement( new INTEGER("2934293834242") );
            seq.addElement( new OBJECT_IDENTIFIER(
                new long[] { 1, 2, 127, 563, 1231982 } ) );
            seq.addElement( new NULL() );
            seq.addElement( new EXPLICIT( new Tag(27), new INTEGER(39) ));
            seq.addElement( new ENUMERATED(983) );
            seq.addElement( new OCTET_STRING( new byte[] {
                    (byte)0x0, (byte)0xff, (byte)0xcc} ) );
            seq.addElement( new IA5String("foobar") );
            seq.addElement( new Tag(23), new INTEGER(234) );
            //seq.addElement( new BOOLEAN(false) );
            byte[] bits = new byte[]{ (byte)0x80, (byte)0xff, (byte)0x0f };
            seq.addElement( new BIT_STRING( bits, 3 ) );
            seq.addElement( new INTEGER(82734) );
            seq.addElement( new PrintableString("I'm printable??") );

            SEQUENCE nested = new SEQUENCE();
            nested.addElement( new INTEGER( 5 ) );
            nested.addElement( new INTEGER( 6 ) );
            seq.addElement( nested );

            nested = new SEQUENCE();
            seq.addElement( nested );
            

            seq.encode(System.out);
            System.out.flush();
        }
      } catch( Exception e) {
            e.printStackTrace();
      }
    }

}
