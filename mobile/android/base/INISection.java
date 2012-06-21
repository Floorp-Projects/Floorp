/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.text.TextUtils;
import android.util.Log;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Enumeration;
import java.util.Hashtable;

class INISection {
    private static final String LOGTAG = "INIParser";

    // default file to read and write to
    private String mName = null;
    public String getName() { return mName; }

    // show or hide debug logging
    private  boolean mDebug = false;

    // Global properties that aren't inside a section in the file
    protected Hashtable<String, Object> mProperties = null;

    // create a parser. The file will not be read until you attempt to
    // access sections or properties inside it. At that point its read synchronously
    public INISection(String name) {
        mName = name;
    }

    // log a debug string to the console
    protected void debug(String msg) {
        if (mDebug) {
            Log.i(LOGTAG, msg);
        }
    }
  
    // get a global property out of the hash table. will return null if the property doesn't exist
    public Object getProperty(String key) {
        getProperties(); // ensure that we have parsed the file
        return mProperties.get(key);
    }

    // get a global property out of the hash table. will return null if the property doesn't exist
    public int getIntProperty(String key) {
        Object val = getProperty(key);
        if (val == null)
          return -1;

        Integer i = new Integer(val.toString());
        return i.intValue();
    }

    // get a global property out of the hash table. will return null if the property doesn't exist
    public String getStringProperty(String key) {
        Object val = getProperty(key);
        if (val == null)
          return null;

        return val.toString();
    }

    // get a hashtable of all the global properties in this file
    public Hashtable<String, Object> getProperties() {
        if (mProperties == null) {
            try {
                parse();
            } catch (IOException e) {
                debug("Error parsing: " + e);
            }
        }
        return mProperties;
    }

    // do nothing for generic sections
    protected void parse() throws IOException {
        mProperties = new Hashtable<String, Object>();
    }

    // set a property. Will erase the property if value = null
    public void setProperty(String key, Object value) {
        getProperties(); // ensure that we have parsed the file
        if (value == null)
            removeProperty(key);
        else
            mProperties.put(key.trim(), value);     
    }   
 
    // remove a property
    public void removeProperty(String name) {
        // ensure that we have parsed the file
        getProperties();
        mProperties.remove(name);
    }

    public void write(BufferedWriter writer) throws IOException {
        if (!TextUtils.isEmpty(mName)) {
            writer.write("[" + mName + "]");
            writer.newLine();
        }

        if (mProperties != null) {
            for (Enumeration<String> e = mProperties.keys(); e.hasMoreElements();) {
                String key = e.nextElement();
                writeProperty(writer, key, mProperties.get(key));
            }
        }
        writer.newLine();
    }

    // Helper function to write out a property
    private void writeProperty(BufferedWriter writer, String key, Object value) {
        try {
            writer.write(key + "=" + value);
            writer.newLine();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}