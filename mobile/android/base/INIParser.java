/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Enumeration;
import java.util.Hashtable;

public final class INIParser extends INISection {
    // default file to read and write to
    private File mFile = null;

    // List of sections in the current iniFile. null if the file has not been parsed yet
    private Hashtable<String, INISection> mSections = null;

    // create a parser. The file will not be read until you attempt to
    // access sections or properties inside it. At that point its read synchronously
    public INIParser(File iniFile) {
        super("");
        mFile = iniFile;
    }

    // write ini data to the default file. Will overwrite anything current inside
    public void write() {
        writeTo(mFile);
    }

    // write to the specified file. Will overwrite anything current inside
    public void writeTo(File f) {
        if (f == null)
            return;
  
        FileWriter outputStream = null;
        try {
            outputStream = new FileWriter(f);
        } catch (IOException e1) {
            e1.printStackTrace();
        }
  
        BufferedWriter writer = new BufferedWriter(outputStream);
        try {
            write(writer);
            writer.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void write(BufferedWriter writer) throws IOException {
        super.write(writer);

        if (mSections != null) {
            for (Enumeration<INISection> e = mSections.elements(); e.hasMoreElements();) {
                INISection section = e.nextElement();
                section.write(writer);
                writer.newLine();
            }
        }
    }

    // return all of the sections inside this file
    public Hashtable<String, INISection> getSections() {
        if (mSections == null) {
            try {
                parse();
            } catch (IOException e) {
                debug("Error parsing: " + e);
            }
        }
        return mSections;
    }

    // parse the default file
    @Override
    protected void parse() throws IOException {
        super.parse();
        parse(mFile);
    }
   
    // parse a passed in file
    private void parse(File f) throws IOException {
        // Set up internal data members
        mSections = new Hashtable<String, INISection>();
  
        if (f == null || !f.exists())
            return;
  
        FileReader inputStream = null;
        try {
            inputStream = new FileReader(f);
        } catch (FileNotFoundException e1) {
            // If the file doesn't exist. Just return;
            return;
        }
  
        BufferedReader buf = new BufferedReader(inputStream);
        String line = null;            // current line of text we are parsing
        INISection currentSection = null; // section we are currently parsing
  
        while ((line = buf.readLine()) != null) {
  
            if (line != null)
                line = line.trim();
  
            // blank line or a comment. ignore it
            if (line == null || line.length() == 0 || line.charAt(0) == ';') {
                debug("Ignore line: " + line);
            } else if (line.charAt(0) == '[') {
                debug("Parse as section: " + line);
                currentSection = new INISection(line.substring(1, line.length()-1));
                mSections.put(currentSection.getName(), currentSection);
            } else {
                debug("Parse as property: " + line);
  
                String[] pieces = line.split("=");
                if (pieces.length != 2)
                    continue;
  
                String key = pieces[0].trim();
                String value = pieces[1].trim();
                if (currentSection != null) {
                    currentSection.setProperty(key, value);
                } else {
                    mProperties.put(key, value);
                }
            }
        }
        buf.close();
    }

    // add a section to the file
    public void addSection(INISection sect) {
        // ensure that we have parsed the file
        getSections();
        mSections.put(sect.getName(), sect);
    }

    // get a section from the file. will return null if the section doesn't exist
    public INISection getSection(String key) {
        // ensure that we have parsed the file
        getSections();
        return mSections.get(key);
    }
 
    // remove an entire section from the file
    public void removeSection(String name) {
        // ensure that we have parsed the file
        getSections();
        mSections.remove(name);
    }

    // rename a section; nuking any previous section with the new
    // name in the process
    public void renameSection(String oldName, String newName) {
        // ensure that we have parsed the file
        getSections();

        mSections.remove(newName);
        INISection section = mSections.get(oldName);
        if (section == null)
            return;

        mSections.remove(oldName);
        mSections.put(newName, section);
    }
}
