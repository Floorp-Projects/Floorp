/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
	MJRConsole.java
	
	Simple Java console for MRJ.
	
	All methods are called asynchronously, using JMExecJNIStaticMethodInContext(). After posting
	each call this way, MRJConsole.finish() is called, which waits on MRJConsole.class. Each
	public method calls MRJConsole.done(), which notifies. This should probably be changed
	to a pure Java approach.
	
	by Patrick C. Beard.
 */

package netscape.oji;

import java.io.*;
import java.awt.*;
import java.awt.event.*;

class ConsoleWindow extends Frame {
	TextArea text;

	ConsoleWindow(String title) {
		super("Java Console");
		
		addWindowListener(
			new WindowAdapter() {
				public void windowClosing(WindowEvent e) {
					hide();
				}
			});
		
		add(text = new TextArea());
		setSize(300, 200);

		ActionListener dumpThreadsListener =
			new ActionListener() {
				public void actionPerformed(ActionEvent e) {
					dumpThreads();
				}
			};

		// Create a console menu.
		MenuBar menuBar = new MenuBar();
		Menu consoleMenu = new Menu("Console");
		consoleMenu.add(newItem("Dump Threads", dumpThreadsListener));
		
		menuBar.add(consoleMenu);
		setMenuBar(menuBar);
	}

	private MenuItem newItem(String title, ActionListener listener) {
		MenuItem item = new MenuItem(title);
		item.addActionListener(listener);
		return item;
	}
	
	public void dumpThreads() {
		System.out.println("Dumping threads...");
	}
}

public class MRJConsole {
	// Save primordial System streams.
	private static InputStream in;
	private static PrintStream out;
	private static PrintStream err;
	private static ConsoleWindow window;

	native static int readLine(byte[] buffer, int offset, int length);
	native static void writeLine(byte[] buffer, int offset, int length);

	private static class Input extends InputStream {
		byte[] buffer = new byte[1024];
		int position = 0;
		int count = 0;
		
		private void fillBuffer() throws EOFException {
			// int length = readLine(buffer, 0, buffer.length);
			int length = 1024;
			if (length == -1)
				throw new EOFException();
			count = length;
			position = 0;
		}
		
	    public int read() throws IOException {
	    	synchronized(this) {
		    	if (position >= count)
		    		fillBuffer();
    			return buffer[position++];
    		}
	    }

	    public int read(byte[] b, int offset, int length) throws IOException {
			synchronized(this) {
				// only fill the buffer at the outset, always returns at most one line of data.
		    	if (position >= count)
		    		fillBuffer();
		    	int initialOffset = offset;
		    	while (offset < length && position < count) {
		    		b[offset++] = buffer[position++];
		    	}
		    	return (offset - initialOffset);
		    }
	    }
	}
	
	private static class Output extends OutputStream implements Runnable {
		StringBuffer buffer = new StringBuffer();
		
		public Output() {
			Thread flusher = new Thread(this, getClass().getName() + "-Flusher");
			flusher.setDaemon(true);
			flusher.start();
		}
	
		public synchronized void write(int b) throws IOException {
			this.buffer.append((char)b);
			notify();
	    }

		public synchronized void write(byte[] buffer, int offset, int count) throws IOException {
			this.buffer.append(new String(buffer, 0, offset, count));
			notify();
		}

	    public synchronized void flush() throws IOException {
	    	String value = this.buffer.toString();
	    	window.text.append(value);
	    	this.buffer.setLength(0);
    	}
		
    	/**
    	 * When I/O occurs, it is placed in a StringBuffer, which is flushed in a different thread.
    	 * This prevents deadlocks that could occur when the AWT itself is printing messages.
    	 */
    	public synchronized void run() {
   			for (;;) {
  		  		try {
    				wait();
    				flush();
	    		} catch (InterruptedException ie) {
	    		} catch (IOException ioe) {
    			}
    		}
    	}
	}
	
	private static class Error extends Output {}

	public static void init() {
		in = System.in;
		out = System.out;
		err = System.err;

		window = new ConsoleWindow("Java Console");

		System.setIn(new Input());
		System.setOut(new PrintStream(new Output()));
		System.setErr(new PrintStream(new Error()));

		done();
	}
	
	public static void dispose() {
		System.setIn(in);
		System.setOut(out);
		System.setErr(err);
		window.dispose();
		window = null;
		done();
	}
	
	public static void show() {
		window.show();
		done();
	}
	
	public static void hide() {
		window.hide();
		done();
	}
	
	public static void visible(boolean[] result) {
		result[0] = window.isVisible();
		done();
	}
	
	public static void print(String text) {
		System.out.print(text);
		done();
	}
	
	public static synchronized void finish() {
		try {
			MRJConsole.class.wait();
		} catch (InterruptedException ie) {
		}
	}
	
	private static synchronized void done() {
		MRJConsole.class.notify();
	}
}
