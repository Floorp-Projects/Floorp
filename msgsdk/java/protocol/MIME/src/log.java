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

// log.java
// Carson Lee
// Log class
//

package netscape.messaging.mime;

//import java.io.*;
import java.io.FileWriter;
import java.io.BufferedWriter;
import java.util.Date;
import java.io.IOException;


public class log
{
    // defines
    final static String errorLogFilename = "nsmail.log";
    final static String spaces = "      ";
    static boolean bLogOn = true;
    static boolean bTimestampOn = true;
    static BufferedWriter errorLog;

    public static void init()
    {
        try
        {
            if ( errorLog == null )
                errorLog = new BufferedWriter( new FileWriter( errorLogFilename, true ), 1024 );
        }
        catch ( IOException e )
        {
        }
    }


    public static void close()
    {
        try
        {
            if ( errorLog != null )
            {
                errorLog.flush();
                errorLog.close();
                errorLog = null;
            }
        }
        catch ( IOException e )
        {
        }
    }


    protected void finalize()
    {
        close();
    }


    public static void errorLog( String message )
    {
        if ( !bLogOn || message == null )
            return;
            
        if ( errorLog == null )
            init();

        try
        {
            // option to turn off timestamp, it's expensive to use
            if ( bTimestampOn )
            {
                Date timeStamp = new Date();
                timeStamp.setTime( System.currentTimeMillis() );
                errorLog.write( timeStamp.toString(), 0, 28 );
                errorLog.write( spaces, 0, 6 );
            }
            
            errorLog.write( message, 0, message.length() );
            errorLog.newLine();
        }
        catch ( IOException e )
        {
        }
    }


    public static void timestampOn() { bTimestampOn = true; }
    public static void timestampOff() { bTimestampOn = false; }
    public static void LogOn() { bLogOn = true; }
    public static void LogOff() { bLogOn = false; }


}  // eof log.java
