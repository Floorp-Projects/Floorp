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

package netscape.messaging;

/**
*Helper class for doing C-runtime equivalent operations.
*@author derekt@netscape.com
*@version 1.0
*/

public class Common
{
    /**
    *Converts a byte array of specified length into an int.
    */
    public final int atoi( byte[] in_string )
    {
        int i;
        int n;
        int sign;

        ///////////////////////////////////////////////////////////////////////
        // Move past any leading spaces.
        ///////////////////////////////////////////////////////////////////////

        for ( i = 0; Character.isSpaceChar((char)in_string[i]) &&
                                        i < in_string.length; i++ )
        {
        }

        sign = (in_string[i] == '-') ? -1 : 1;

        ///////////////////////////////////////////////////////////////////////
        // Store the sign of the number.
        ///////////////////////////////////////////////////////////////////////

        if ( in_string[i] == '+' || in_string[i] == '-' )
        {
            i++;
        }

        for ( n = 0; Character.isDigit((char)in_string[i]) &&
                                        i < in_string.length; i++ )
        {
            n = 10 * n + ( in_string[i] - '0');
        }

        return sign * n;
    }

    /**
    *Converts an integer into its ASCII representation.
    */
    public final int itoa( int in_number, byte[] io_string )
    {
        int i;
        int sign;

        if ((sign = in_number) < 0)
        {
            in_number = -in_number;
        }

        i = 0;

        do
        {
            io_string[i++] = (byte)(in_number % 10 + '0');
        }
        while ((in_number /= 10) > 0);

        if ( sign < 0 )
        {
            io_string[i++] = '-';
        }
        reverse(io_string, i );
        return i;
    }

    /**
    *Helper used by itoa.
    */
    protected final void reverse( byte[] io_string, int in_length )
    {
        int c, i, j;

        for ( i = 0, j = in_length - 1; i < j; i++, j-- )
        {
            c = io_string[i];
            io_string[i] = io_string[j];
            io_string[j] = (byte)c;
        }
    }
}
