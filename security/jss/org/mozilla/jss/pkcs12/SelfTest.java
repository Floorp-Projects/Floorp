/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

package com.netscape.jss.pkcs12;

import java.io.*;
import java.text.*;
import java.util.*;
import com.netscape.jss.util.*;

/**
 * A PKCS #12 "virtual token".  Currently, these extend
 * tokens found in the PK11Token class.
 *
 * @author mharmsen
 * @version $Revision: 1.1 $ $Date: 2000/12/15 20:50:38 $ 
 * @see com.netscape.jss.pkcs11.PK11Token
 */
public class SelfTest
{
    ////////////////////////////////////////////////////
    //  exceptions
    ////////////////////////////////////////////////////


    ////////////////////////////////////////////////////
    //  public methods
    ////////////////////////////////////////////////////

    public static void TestPK12TokenConstructor()
    {
	PK12Token p1 = PK12Token.makePK12Token( "test0.p12", PK12Token.Flag.FILE_EXISTS );
	PK12Token p2 = PK12Token.makePK12Token( "test1.p12", PK12Token.Flag.CREATE_FILE );
        PK12Token p3 = PK12Token.makePK12Token( "test2.p12", PK12Token.Flag.CREATE_FILE );
	PK12Token p5 = PK12Token.makePK12Token( "", PK12Token.Flag.FILE_EXISTS );
    }

    public static void main(String[] args)
    {
	TestPK12TokenConstructor();
    }

    ////////////////////////////////////////////////////
    //  private methods
    ////////////////////////////////////////////////////


    ////////////////////////////////////////////////////
    // construction and finalization
    ////////////////////////////////////////////////////


    //////////////////////////////////////////////////
    // Public Data
    //////////////////////////////////////////////////


    //////////////////////////////////////////////////
    // Private Data
    //////////////////////////////////////////////////
}
