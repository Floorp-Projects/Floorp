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

public class TestServer extends Thread {

	final static int DEFAULT_PORT = 4321;

    protected int port;
	protected ServerSocket listen_socket;

	public static void fail(Exception e, String msg) {
		System.err.println(msg + ":" + e);
		e.printStackTrace();
	}

	public TestServer() {
		this(DEFAULT_PORT);
	}

	public TestServer(int port) {
		this.port = port;
		try {
			listen_socket = new ServerSocket(port);
		}
		catch (IOException e) {
			fail(e, "Exception creating server socket");
		}
		System.out.println("Server: listening on port " + port);
		this.start();
	}

	public void run() {
		try {
			while (true) {
				Socket client_socket = listen_socket.accept();
				Connection c = new Connection(client_socket);
			}
		}
		catch (IOException e) {
			fail(e, "Exception while listening");
		}
	}

	public static void main(String args[]) {
		int port = DEFAULT_PORT;
		if (args.length == 1) {
			try {
				port = Integer.parseInt(args[0]);
			}
			catch (NumberFormatException e) {
				port = DEFAULT_PORT;
			}
		}
		new TestServer(port);
	}

}

