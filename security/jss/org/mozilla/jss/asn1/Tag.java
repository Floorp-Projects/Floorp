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

/**
 * Represents an ASN.1 Tag.  A tag consists of a class and a number.
 */
public class Tag {

    private long num;
    /**
     * Returns the tag number.
     */
    public long getNum() {
        return num;
    }

    private Class tClass;
    /**
     * Returns the tag class.
     */
    public Class getTagClass() {
        return tClass;
    }

    private Tag() { }

    /**
     * A tag class.
     */
    public static final Class UNIVERSAL = Class.UNIVERSAL;
    /**
     * A tag class.
     */
    public static final Class APPLICATION = Class.APPLICATION;
    /**
     * A tag class.
     */
    public static final Class CONTEXT_SPECIFIC = Class.CONTEXT_SPECIFIC;
    /**
     * A tag class.
     */
    public static final Class PRIVATE = Class.PRIVATE;

    /**
     * The end-of-contents marker for indefinite length encoding.
     * It is encoded the same as an ASN.1 header whose tag is [UNIVERSAL 0].
     */
    public static final Tag END_OF_CONTENTS = new Tag( UNIVERSAL, 0 );

    /**
     * An alias for END_OF_CONTENTS.
     */
    public static final Tag EOC = END_OF_CONTENTS;

    /**
     * Creates a tag with the given class and number.
     * @param clazz The class of the tag.
     * @param num The tag number.
     */
    public Tag(Class clazz, long num) {
        tClass = clazz;
        this.num = num;
    }

    /**
     * Creates a CONTEXT-SPECIFIC tag with the given tag number.
     * @param num The tag number.
     */
    public Tag(long num) {
        this(Class.CONTEXT_SPECIFIC, num);
    }

    ///////////////////////////////////////////////////////////////////////
    // Tag Instances
    //
    // Since grabbing a context-specific tag is a very common operation,
    // let's make singletons of the most frequently used tags.
    ///////////////////////////////////////////////////////////////////////
    private static final int numTagInstances = 10;
    private static Tag tagInstances[] = new Tag[numTagInstances];
    static {
        for(int i=0; i < numTagInstances; i++) {
            tagInstances[i] = new Tag(i);
        }
    }

    /**
     * Returns an instance of a context-specific tag with the given number.
     * The returned instance may be singleton.  It is usually more efficient to
     * call this method than create your own context-specific tag.
     */
    public static Tag get(long num) {
        if( num >= 0 && num < numTagInstances ) {
            return tagInstances[(int)num];
        } else {
            return new Tag(num);
        }
    }

    public int hashCode() {
        return (tClass.toInt() * 131) + (int)num;
    }

    /**
     * Compares two tags for equality.  Tags are equal if they have
     * the same class and tag number.
     */
    public boolean equals(Object obj) {
        if(obj == null) {
            return false;
        }
        if(! (obj instanceof Tag) ) {
            return false;
        }

        Tag t = (Tag) obj;
        if( num == t.num && tClass == t.tClass ) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * Returns a String representation of the tag. For example, a tag
     * whose class was UNIVERSAL and whose number was 16 would return
     * "UNIVERSAL 16".
     */
    public String toString() {
        return tClass+" "+num;
    }

    /**
     * An enumeration of the ASN.1 tag classes.
     */
    public static class Class {

        private Class() { }
        private Class(int enc, String name) {
            encoding = enc;
            this.name = name;
        }
        private int encoding;
        private String name;

        public static final Class UNIVERSAL = new Class(0, "UNIVERSAL");
        public static final Class APPLICATION = new Class(1, "APPLICATION");
        public static final Class CONTEXT_SPECIFIC =
            new Class(2, "CONTEXT-SPECIFIC");
        public static final Class PRIVATE = new Class(3, "PRIVATE");

        public int toInt() {
            return encoding;
        }

        public String toString() {
            return name;
        }

        /**
         * @exception InvalidBERException If the given int does not correspond
         *      to any tag class.
         */
        public static Class fromInt(int i) throws InvalidBERException {
            if( i == 0 ) {
                return UNIVERSAL;
            } else if(i == 1) {
                return APPLICATION;
            } else if(i == 2) {
                return CONTEXT_SPECIFIC;
            } else if(i == 3) {
                return PRIVATE;
            } else {
                throw new InvalidBERException("Invalid tag class: " + i);
            }
        }
    }
}
