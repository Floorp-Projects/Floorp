/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/
  
/*
* Copyright (c) 1997 and 1998 Netscape Communications Corporation
* (http://home.netscape.com/misc/trademarks.html)
*/


package netscape.messaging.convenience;
import java.io.*;
import java.lang.*;
import java.util.*;


/**
 * IMSearchTerm
 * @author Prasad Yendluri
 * The Search Term
 */
public class IMSearchTerm 
{
	/**
         * Name of the field to search in. 
         * The special String "TextBody" will search all text bodyParts.
         */
	public String fieldName;
	/**
         * Actual string to search for. 
         */
	public String value;

	/**
         * This term must be present to satisfy the search.
         */
        public static final int AND	= 1;

	/**
         * This term may be present to satisfy the search. 
	 * It is valid to not have matched this term to satisfy this search, 
	 * if it is satisfied otherwise.
         */
        public static final int OR	= 0;


	/**
         * Constructor for the IMSearchTerm
	 * @param name Name of the header field to search in. 
	 * The String "TextBody" stands for all text bodyParts.
	 * @param value Actual string to search for (in the field specified above).
	 * @param AndOR Should the search for this be on OR / AND basis? 
	 * @see IMSearchTerm#AND 
	 * @see IMSearchTerm#OR 
         */
	public IMSearchTerm (String name, String value, int AndOR)
        {
            // Do any initialization
        }

}
