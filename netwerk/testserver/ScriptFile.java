/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

import java.io.*;
import java.util.Date;
/*
	ScriptFile class reads off the script file and sets up the list
	of delimiters.
*/

class ScriptFile {

    public static final String URLMAP = new String("docs/urlmap");

	public ScriptFile(Connection c, String req, PrintWriter stream ) {
        con = c;
	    request = req;
	    out = stream;
	    if (out == null)
	        out = new PrintWriter(System.out);
	    file = URLMAP;
	    try {
	        Parse();
	    }
	    catch (IOException e)
	    {
	        out.println("HTTP/1.1 500 Server Error\n\n");
	        out.println("Failed with this exception-");
	        out.println(e);
	    }
	    out.flush();
	}

	public ScriptFile(String f) {
		file = f;
		out = new PrintWriter(System.out);
	    try {
	        Parse();
	    }
	    catch (FileNotFoundException e)
	    {
	        out.println("HTTP/1.1 404 File Not Found\n\n");
	        out.println("File not found!");
	        out.println(e);
	    }
	    catch (IOException e)
	    {
	        out.println("HTTP/1.1 500 Server Error\n\n");
	        out.println("Failed with this exception-");
	        out.println(e);
	    }
	    out.flush();
	}

	public void Parse () throws IOException {

	    if ((request == null) || (request.length() == 0) || request.startsWith("GET / "))
	    {
	        WriteDefaultResponse();
            return;
	    }

        boolean outDirty = false;
	    if (file != null)
	    {
              DataInputStream in =
                new DataInputStream(
                  new BufferedInputStream(
                    new FileInputStream(file)));

              String s = new String();
              while((s = in.readLine())!= null)
              {
                s.trim();
                /* Skip comments */
                if ((s.length() == 0) || (s.charAt(0) == '#'))
                    continue;
                if (s.startsWith("START")) {
                    // Check to see if this was in the requested URL
                    String filename = new String ("GET " + s.substring(6));
                    if (request.startsWith(filename))
                    {
                        continue;
                    }
                    else// Else skipto past the END
                    {
                        while (!s.startsWith("END"))
                        {
                            s = in.readLine();
                            if (s != null)
                            {
                                s.trim();
                            }
                            else
                                break;
                        }
                    }
                }
                else if (s.startsWith("INCLUDE")) {
                    outDirty = true;
                    WriteOutFile("docs/" + s.substring(8));
                }
                else if (s.startsWith("CRLF")) {
                    outDirty = true;
                    out.println();
                }
                else if (s.startsWith("END")) {
                    // ignore should never have appeared here though!
                    continue;
                }
              }
              in.close();

              if (outDirty)
              {
                out.flush();
              }
              else
                WriteDefaultResponse();
	    }
	}

	public static void main(String args[]) {
	    if (args.length >= 1) {
            ScriptFile sf = new ScriptFile(args[0]);
            /* Detect change stuff;
            File f = new File(args[0]);
            long lastMod = f.lastModified();
            while (f.lastModified() == lastMod) {
            }
            sf.Parse();
            */
	    }
	}

    protected void WriteOutFile(String filename) throws IOException {
        DataInputStream incl =
            new DataInputStream(
                new BufferedInputStream(
                    new FileInputStream(filename)));
        // This doesn't have to be line wise... change later TODO
        String s;
        while ((s = incl.readLine()) != null)
        {
            out.println(s);
        }
        incl.close();
    }

    protected void WriteDefaultResponse() throws IOException {
	        WriteOutFile("docs/generic.res");
            out.println("Date: " + (new Date()).toString());
            if (file != null)
            {
                File f = new File(file);
                out.println("Last-modified: " + (new Date(f.lastModified())).toString());
            }
            out.println("\n"); // prints 2
            if (con != null)
            {
                out.println("<H3>Rec'd. the following request-</H3>");
                out.println("<PRE>");
                out.println(con.request);
                out.println("</PRE>");
                out.println("From- " + con.client);
            }
    }

    String file = null;
    // The string associated with this script occurence

    String request;
    PrintWriter out;
    Connection con;
}
