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
import java.net.*;

class Connection extends Thread {

    public static final boolean DEBUG = true;
	public Socket client;
	protected BufferedReader in;
	protected PrintWriter out;

    final static String SERVER = "HTTP Test Server/1.1";

    public Connection(Socket client_socket) {
		client = client_socket;

		if (DEBUG)
		{
			System.out.println(
			    "---------------------------------------------------------");
			System.out.println(client);
		}
		try {
			in = new BufferedReader(new InputStreamReader(
				client.getInputStream()));
			out = new PrintWriter(client.getOutputStream());
			this.start();
		}
		catch (IOException e) {
			try {
				client.close();
			}
			catch (IOException another) {
			}
			System.out.println("Exception while getting socket streams."
				+ e);
		}
	}

	final public void run() {

		String line = null;
		String firstline = null;
		request = new StringBuffer();

		int len = 0;
		try {
			while(true) {
				line = in.readLine();
				if (line == null)
					break;
				if (firstline == null)
				    firstline = new String(line);
				len = line.length();
				if (len == 0)
				    break;
                request.append(line);
				request.append("\n");
				if (DEBUG)
				    System.out.println(line);
			}
            // This will appropriately write all the stuff based
            // on the first line and the script file
            ScriptFile sf = new ScriptFile(this, firstline, out);
    		out.flush();
    		// mark for garbage collect
    		sf = null;
		} // try
		catch (IOException e) {
			System.out.println("Some problem while communicating. " + e);
		}
		finally {
			try {
				client.close();
			}
			catch (IOException e2) {
			}
		}
	}
	StringBuffer request;
}
