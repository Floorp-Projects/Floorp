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

import java.awt.*;
import java.awt.event.*;
import java.lang.*;

/*  Support class for the Demo Message Send Application using Netscape Messaging SDK.
 *  prasad@netscape.com
 */
public class OKDialog extends Dialog 
{
  	public OKDialog(Frame parent, String title, String message) 
	{
    		super(parent, title, true);

    		Panel north = new Panel();
    		setLayout(new BorderLayout(0,0));
    		north.add("Center", new Label(message));

    		Panel south = new Panel();
    		setLayout(new BorderLayout(0,0));
    		Button okButton = new Button("OK");
    		south.add(okButton);

    		add("North", north);
    		add("South", south);

    		pack();

    		okButton.addActionListener(new ActionListener() 
		{
      			public void actionPerformed(ActionEvent e) 
			{
        			setVisible(false);
      			}
    		});
  	}

  	public void display() 
  	{
		for (int i = 1; i <= 2; i++)
                     Toolkit.getDefaultToolkit().beep();
    		setVisible(true);
  	}

}
