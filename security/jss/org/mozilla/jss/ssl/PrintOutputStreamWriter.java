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
// 
// 
//
// $Id: PrintOutputStreamWriter.java,v 1.1 2000/12/21 22:58:55 nicolson%netscape.com Exp $


package org.mozilla.jss.ssl;

import java.io.*;
import java.util.*;



class PrintOutputStreamWriter 
    extends java.io.OutputStreamWriter
{

    public PrintOutputStreamWriter(OutputStream out)
    {
    	super(out);
    }

    public void print(String x) 
	throws  java.io.IOException
    {
    	write(x, 0, x.length());
    }

    public void println(String x)
	throws  java.io.IOException
    {
//	String line = new String(x + "\n");
	String line = x + "\n";
	write(line, 0, line.length());
	flush();
    }

}
