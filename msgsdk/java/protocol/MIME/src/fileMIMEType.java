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

package netscape.messaging.mime;

import java.io.*;
import java.net.*;
import java.util.*;

/**
 * The fileMIMEType class contains
 * File MIME Content type and MIME encoding information.
 * @author Prasad Yendluri
 */
public class fileMIMEType
{
	public int 	content_type;
	public String 	content_subtype;
	public String 	content_params;
	public String 	file_extn;
	public String 	file_shortname;
	public int	mime_encoding = MIMEBodyPart.BASE64;

    	/**
    	 * Creates a fileMIMEType object.
    	 * Default constructor for the fileMIMEType class.
    	 */
	public fileMIMEType ()
        {
		content_type = 0;
        }
}
