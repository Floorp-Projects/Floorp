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

import java.io.InputStream;
import java.io.ByteArrayInputStream;
import netscape.messaging.mime.log;
import netscape.messaging.mime.MIMEParser;

/**
 * The MIMEDynamicParser class defines the MIME Dynamic Parser.
 * @author Carson Lee
 * @version 1.0
 * Dec 17,97
 */

// @author Carson Lee
// @version %I%, %G%
public final class MIMEDynamicParser
{
    protected MIMEParser p = null;

    /**
    * Constructor for the Dynamic Parser.
    * @param  dataSink User's datasink for callbacks. Cannot be null.
    * @param  decodeData Whether the parser should decode message  body-part data; if
    * true, the parser decodes the data; if false, the parser stores or returns raw data.
    * @param  localStorage Whether the parser should save references to all callback data
    * and build up the MIMEMessage that is available as a whole after endParse().
    * If true, the parser manages data locally; if false, the does not save references to
    * data supplied through callbacks.
    * @return New MIMEParser object
    * @exception MIMEException If dataSink is null or an error occurs.
    */
    private MIMEDynamicParser( MIMEDataSink dataSink,
			      boolean decodeData,
			      boolean localStorage ) throws MIMEException
    {
        p = new MIMEParser( dataSink, decodeData, localStorage );
    }

    /**
    * Constructor for the Dynamic Parser.
    * @param  dataSink User's datasink for callbacks. Cannot be null.
    * @return New MIMEParser object
    * @exception MIMEException If dataSink is null or an error occurs.
    */
    public MIMEDynamicParser (MIMEDataSink dataSink) throws MIMEException
    {
        p = new MIMEParser (dataSink, false, false);
    }


    /**
     * Begins a new parse cycle and resets parser internal data-structures.
     * This method is called when the user wants to initiate message parsing.
     * @exception MIMEException If the parser object was not properly set-up.
     */
    public void beginParse() throws MIMEException
    {
        p.beginParse();
    }




    /**
    * Parse more incoming data.
    * This method can be called several times between beginParse() and endParse().
    * @param  input User's input-stream. Source of data for parse operation.
    * @exception MIMEException If parser detects MIME format errors.
    */
    public void parse( InputStream input ) throws MIMEException
    {
        p.parse( input );
    }


    /**
    * Parses more incoming data.
    * This method can be called several times between beginParse() and endParse().
    * @param  inputData User's input. Source of data for parse operation.
    * @exception MIMEException If parser detects MIME format errors.
    */
    public void parse (byte [] inputData) throws MIMEException
    {
        parse (new ByteArrayInputStream (inputData));
    }


    /**
    * Ends parse. Tells parser there is no more data to parse.
    * Parser winds up parse operation of the message being parsed.
    * @exception MIMEException If parser detects MIME format errors.
    */
    public void endParse() throws MIMEException
    {
        p.endParse();
    }
}
