/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.plugin.composer;
import java.util.Vector;
import java.util.Enumeration;

/* This class is private to the composer. */

class SortedStringTable {
    private Vector keys = new Vector();
    private Vector values_ = new Vector();

    public SortedStringTable(){}

    public int length() { return keys.size(); }
    public Object get(String key) {
        int index = keys.indexOf(key);
        if ( index < 0 ) return null;
        return values_.elementAt(index);
    }
    public String getKey(int index) { return (String) keys.elementAt(index);}
    public Object get(int index) {
        return values_.elementAt(index);
    }
    public void put(String key, Object value){
    // Insertion sort. Should use binary search.
        int length = length();
        for(int i = 0; i < length; i++ ){
            String k = (String) keys.elementAt(i);
            int comp = k.compareTo(key);
            if ( comp == 0 ) return; // Already have that key
            else if (comp > 0 ) {
                keys.insertElementAt(key, i);
                values_.insertElementAt(value, i);
                return;
            }
        }
        // append to end
        keys.addElement(key);
        values_.addElement(value);
    }

    /** Convenience method for nested StringTables.
     */
    public SortedStringTable getTable(String key){
        return (SortedStringTable) get(key);
    }

    /** Convenience method for nested StringTables.
     */
    public SortedStringTable getOrCreateTable(String key){
        SortedStringTable table = getTable(key);
        if ( table == null ) {
            table = new SortedStringTable();
            put(key, table);
        }
        return table;
    }

    public String toString(){
        return "keys: " + keys.toString() + "values: " + values_.toString();
    }

    public Enumeration values() {
        return values_.elements();
    }
}
