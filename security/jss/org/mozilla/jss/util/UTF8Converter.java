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

package org.mozilla.jss.util;

import java.io.*;

/**
 * Class for converting between char arrays and byte arrays.  The conversion
 * is guaranteed (in optimized mode only) not to leave any data hanging around
 * in memory unless there is a catastrophic VM error, which is useful for
 * encoding passwords.
 */
public class UTF8Converter {

	/**
	 * Creates a new UTF8-encoded byte array representing the
	 * char[] passed in. The output array will NOT be null-terminated.
	 *
	 * <p>This call is safe for passwords; all internal buffers are cleared.
	 * The output array is the owned by the caller, who has responsibility
	 * for clearing it.
     *
	 * <p>See http://www.stonehard.com/unicode/standard/ for the UTF-16
	 * and UTF-8 standards.
	 *
	 * @param unicode An array of Unicode characters, which may have UCS4
	 *	characters encoded in UTF-16.  This array must not be null.
	 * @exception CharConversionException If the input characters are invalid.
	 */
	public static byte[] UnicodeToUTF8(char[] unicode) 
		throws CharConversionException
	{
		return UnicodeToUTF8(unicode, false);
	}

	/**
	 * Creates a new null-terminated UTF8-encoded byte array representing the
	 * char[] passed in.
	 *
	 * <p>This call is safe for passwords; all internal buffers are cleared.
	 * The output array is the owned by the caller, who has responsibility
	 * for clearing it.
     *
	 * <p>See http://www.stonehard.com/unicode/standard/ for the UTF-16
	 * and UTF-8 standards.
	 *
	 * @param unicode An array of Unicode characters, which may have UCS4
	 *	characters encoded in UTF-16.  This array must not be null.
	 * @exception CharConversionException If the input characters are invalid.
	 */
	public static byte[] UnicodeToUTF8NullTerm(char [] unicode)
		throws CharConversionException
	{
		return UnicodeToUTF8(unicode, true);
	}

	/**
	 * Do the work of the above functions.
	 */
	protected static byte[] UnicodeToUTF8(char[] unicode, boolean nullTerminate)
		throws CharConversionException
	{
		int uni; // unicode index
		int utf; // UTF8 index
		int maxsize; // maximum size of UTF8 output
		byte[] utf8 = null; // UTF8 output buffer
		byte[] temp = null; // used to create an array of the correct size
		char c; // Unicode character
		int ucs4; // UCS4 encoding of a character
		boolean failed = true;
		boolean orphaned_low = false;

		Assert._assert(unicode != null);
		if(unicode == null) {
			return null;
		}

		try {

			// Allocate worst-case size (UTF8 bytes == 1.5 times Unicode bytes)
			maxsize = unicode.length * 3; //chars are 2 bytes each
			if(nullTerminate) {
				maxsize++;
			}
			utf8 = new byte[maxsize];

			for(uni=0, utf=0; uni < unicode.length; uni++) {

				//
				// Convert UCS2 to UCS4
				//
				c = unicode[uni];
				if( c >= 0xd800 && c <= 0xdbff) {
					// This is the high half of a UTF-16 char
					ucs4 = (c-0xd800)<<10;
	
					// Now get the lower half
					if(uni == unicode.length-1) {
						//There is no lower half
						throw new CharConversionException();
					}
					c = unicode[++uni];
					if(c < 0xdc00 || c > 0xdfff) {
						// not in the low-half zone
						throw new CharConversionException();
					}
					ucs4 |= c-0xdc00;
					ucs4 += 0x00010000;
				} else if(c >=0xdc00 && c <=0xdfff) {
					// orphaned low-half char
					orphaned_low = true;
					throw new CharConversionException();
				} else {
					// UCS2 char to UCS4
					ucs4 = unicode[uni];
				}


				//
				// UCS4 to UTF8 conversion
				//
				if(ucs4 < 0x80) {
					// 0000 0000 - 0000 007f (ASCII)
					utf8[utf++] = (byte)ucs4;
				} else if(ucs4 < 0x800) {
					// 0000 0080 - 0000 07ff
					utf8[utf++] = (byte) (0xc0 | ucs4>>6);
					utf8[utf++] = (byte) (0x80 | (ucs4 & 0x3f) );
				} else if(ucs4 < 0x0010000) {
					// 0000 0800 - 0000 ffff
					utf8[utf++] = (byte) (0xe0 | ucs4>>12);
					utf8[utf++] = (byte) (0x80 | ((ucs4>>6) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | (ucs4 & 0x3f) );
				} else if(ucs4 < 0x00200000) {
					// 001 0000 - 001f ffff
					utf8[utf++] = (byte) (0xf0 | ucs4>>18);
					utf8[utf++] = (byte) (0x80 | ((ucs4>>12) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | ((ucs4>>6) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | (ucs4 & 0x3f) );
				} else if(ucs4 < 0x00200000) {
					// 0020 0000 - 03ff ffff
					utf8[utf++] = (byte) (0xf8 | ucs4>>24);
					utf8[utf++] = (byte) (0x80 | ((ucs4>>18) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | ((ucs4>>12) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | ((ucs4>>6) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | (ucs4 & 0x3f) );
				} else {
					// 0400 0000 - 7fff ffff
					utf8[utf++] = (byte) (0xfc | ucs4>>30);
					utf8[utf++] = (byte) (0x80 | ((ucs4>>24) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | ((ucs4>>18) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | ((ucs4>>12) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | ((ucs4>>6) & 0x3f) );
					utf8[utf++] = (byte) (0x80 | (ucs4 & 0x3f) );
				}
			}

			if(nullTerminate) {
				utf8[utf++] = 0;
			}

			//
			// Copy into a correct-sized array
			//
			try {
				int i;

				// last index is the size of the UTF8
				temp = new byte[utf];

				for(i=0; i < utf; i++) {
					temp[i] = utf8[i];
					utf8[i] = 0;
				}
				utf8 = temp;
				temp = null;
			} finally {
				if(temp != null) {
					wipeBytes(temp);
				}
			}

			failed = false;
			return utf8;

		} finally {
			// Cleanup data locations where the password was written
			if(failed && utf8 != null) {
					wipeBytes(utf8);
					utf8 = null;
			}
			ucs4 = 0;
			c = 0;

			if(Debug.DEBUG) {
				// Verify this operation by comparing its output
				// to that of the standard Java conversion routines.
				// WARNING: This means passwords will be left in memory,
				// so Ninja is not secure in debugging mode!
			  try {
				OutputStreamWriter writer;
				ByteArrayOutputStream barray;
				int i;
				byte[] output;

				barray = new ByteArrayOutputStream();

				writer = new OutputStreamWriter(barray, "UTF8");
				writer.write(unicode, 0, unicode.length);
				writer.close();

				output = barray.toByteArray();

				// My class rejects orphaned low-half characters, but the Java
				// class allows them.  I think they really are bogus, so
				// let's pretend the Java class rejects them too.
				if(orphaned_low) {
					throw new IOException();
				}

				// It worked with the Java class, so it should have worked
				// with my class
				Assert._assert(utf8 != null);

				// Compare the arrays
				if(nullTerminate) {
					Assert._assert(utf8.length-1 == output.length);
				} else {
					Assert._assert(utf8.length == output.length);
				}
				for(i=0; i<output.length; i++) {
					Assert._assert(utf8[i] == output[i]);
				}

			  } catch(UnsupportedEncodingException e) {
					// Huh? No UTF8?
					Assert.notReached("No UTF8 encoding conversion");
			  } catch(IOException e) {
					// If it failed with the Java class it should have failed
					// with mine
					Assert._assert(utf8 == null);
			  } catch(Exception e) {
					// Nothing else should be thrown here
					Assert.notReached("UTF8Converter validation threw an"+
						" unexpected exception");
			  }
			}
		}
	}

	/**
	 * Wipes a byte array by setting all of its bytes to zero.
	 * @param array The input array must not be null.
	 */
	public static void wipeBytes(byte[] array) {
		int i;

		Assert._assert(array != null);
		if(array == null) {
			return;
		}

		for(i=0; i < array.length; i++) {
			array[i] = 0;
		}
	}

	/**
	 * Testing method
	 */
	public static void main(String[] args) {

		char[] unicode;
		byte[] utf8;
		int i, j;

		Debug.setLevel(Debug.OBNOXIOUS);

		if(!Debug.DEBUG) {
			System.out.println("***WARNING***");
			System.out.println("Debugging mode is disabled. This code only"+
				" checks itself in debug mode. The test is almost worthless"+
				" in optimized mode."
			);
		}

	  try {

		//
		// Regular ascii
		//
		System.out.println("ASCII Test:");
		unicode = new char[128];
		for(i=0; i< 128; i++) {
			unicode[i] = (char) i;
		}
		utf8 = UnicodeToUTF8(unicode);
		i = 0;
		while( i < 128) {
			for(j = i; j < 128 && j < i+20; j++) {
				System.out.print((int)unicode[j] + " ");
			}
			System.out.println();
			for(j = i; j < 128 && j < i+20; j++) {
				System.out.print((int)utf8[j] + " ");
			}
			System.out.println("\n"); // two lines
			i = j;
		}

		//
		// UCS2
		//
		System.out.println("UCS2 test:");
		unicode = new char[] {'\u0000', '\u007f', '\u0080', '\u0400', '\u07ff',
			'\u0800', '\u3167', '\ud7ff', '\ue000', '\uffff'};
		utf8 = UnicodeToUTF8(unicode);
		for(i=0; i<10; i++) {
			System.out.print( Integer.toHexString((int)unicode[i]) + " ");
		}
		System.out.println();
		for(i=0; i<utf8.length; i++) {
			System.out.print( Integer.toHexString((byte)utf8[i]) + " ");
		}
		System.out.println();

		//
		// max buffer size
		//
		System.out.println("Maximum buffer size test:");
		unicode = new char[] {'\uffff', '\uffff', '\uffff', '\uffff'};
		utf8 = UnicodeToUTF8(unicode);
		Assert._assert(utf8.length == 12);
		System.out.println("8 bytes of unicode --> "+utf8.length+
			" bytes of utf8\n");

		//
		// empty array
		//
		System.out.println("Empty input test:");
		unicode = new char[0];
		utf8 = UnicodeToUTF8(unicode);
		Assert._assert(utf8 != null);
		Assert._assert(utf8.length == 0);
		System.out.println("given 0 bytes Unicode, produces 0 length utf8\n");

		//
		// UCS4
		//
		System.out.println("UCS4 Test:");
		unicode = new char[] {'\ud800', '\udc00', '\uda85', '\ude47',
			'\udbff', '\udfff'};
		utf8 = UnicodeToUTF8(unicode);
		for(i=0; i < 6; i++) {
			System.out.print( Integer.toHexString(unicode[i]) + " ");
		}
		System.out.println();
		for(i=0; i < utf8.length; i++) {
			System.out.print( Integer.toHexString(utf8[i]) + " ");
		}
		System.out.println("\n");

		//
		// high half with no low half
		//
		System.out.println("high half at end of input:");
		try {
			unicode = new char[] {'\ud800'};
			utf8 = UnicodeToUTF8(unicode);
			Assert.notReached("should have failed on bad UCS4");
		} catch (CharConversionException e) {
			System.out.println("Correctly caught bad UCS4\n");
		}

		//
		// high half with something else
		//
		System.out.println("high half with something other than low half:");
		try {
			unicode = new char[] {'\ud800', '\u007f'};
			utf8 = UnicodeToUTF8(unicode);
			Assert.notReached("should have failed on bad UCS4");
		} catch (CharConversionException e) {
			System.out.println("Correctly caught bad UCS4\n");
		}

		//
		// orphaned low half
		//
		System.out.println("orphaned low half test:");
		try {
			unicode = new char[] {'\u0032', '\udc01', '\u0033'};
			utf8 = UnicodeToUTF8(unicode);
			Assert.notReached("should have failed on bad UCS4");
		} catch (CharConversionException e) {
			System.out.println("Correctly caught bad UCS4\n");
		}

		//
		// null-terminated
		//
		System.out.println("null-terminating:");
		unicode = new char[] {'f', 'o', 'o', 'b', 'a', 'r'};
		utf8 = UnicodeToUTF8NullTerm(unicode);
		for(i=0; i < unicode.length; i++) {
			System.out.print(unicode[i] + " ");
		}
		System.out.println();
		for(i=0; i < utf8.length; i++) {
			System.out.print(utf8[i] + " ");
		}
		System.out.println("\n");


	  } catch(CharConversionException e) {
			System.out.println("Error converting Unicode "+e);
	  } finally {
		if(!Debug.DEBUG) {
			System.out.println("***WARNING***");
			System.out.println("Debugging mode is disabled. This code only"+
				" checks itself in debug mode. The test is almost worthless"+
				" in optimized mode."
			);
		}
	  }
	}
}
