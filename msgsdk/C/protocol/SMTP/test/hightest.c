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
  
/* USAGE: SMTPTest.exe server domain mailFrom rcptTo message */

#include "smtp.h"
#include "testsink.h"

int main( int argc, char *argv[ ] )
{
    int l_return;
    char l_errorBuffer[512];

    memset( &l_errorBuffer, 0, 512 );

    l_return = SMTP_sendMessage( "muxtest", "netscape.com", "derekt", "derekt,alterego", "HAHAHAAHHAHAH", (char*)&l_errorBuffer );
    printf( "%s\r\n", l_errorBuffer );
    return l_return;
}
