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

 
/*  Support class for the Demo Message Send Application using Netscape Messaging SDK.
 *  prasad@netscape.com
 */

public class Config extends Frame 
{
	
  	SendMsgClnt clnt = null;

  	//Choice updateChoice = new Choice();
  	Button okButton = new Button ("OK");
  	Button cancelButton = new Button ("Cancel");

  	TextField senderTextField = new TextField(30);
  	TextField hostTextField = new TextField(30);
  	TextField portTextField = new TextField(12);
  	TextField tmpTextField = new TextField(12);

  	Checkbox pipeline = new Checkbox("Pipeline", false);
  	Checkbox dsn 	  = new Checkbox("DSN", false);
  	Checkbox tracelog = new Checkbox("Tracing", false);

 	public Config (SendMsgClnt clnt) 
  	{
    		setTitle("SMTP Options");
    		this.clnt = clnt;
    		init();
  	}

  	public void display() 
	{
    		hostTextField.setText (clnt.host);           
    		portTextField.setText (String.valueOf(clnt.port));
    		tmpTextField.setText (String.valueOf(clnt.tmpdir));
    		senderTextField.setText (clnt.sender);           
    		pipeline.setState (clnt.b_pipeline);          
    		dsn.setState (clnt.b_dsn);                  
    		tracelog.setState (clnt.b_tracelog);       
    		setVisible(true);                         
  	}

  public void init() 
  {
	Panel sender = new Panel();                     
    	sender.add("West", new Label("Msg Sender: "));
    	sender.add("East", senderTextField);

    	Panel host = new Panel();                
    	host.add("West", new Label("SMTP Host:"));
    	host.add("East", hostTextField);

    	Panel north = new Panel();              
    	north.setLayout(new BorderLayout(0,0));
    	north.add("North", sender);
    	north.add("South", host);
    	//north.add("North", sender);
    	//north.add("West", host);

    	Panel port = new Panel();             
    	port.add("West", new Label("SMTP Port:"));
    	port.add("East", portTextField);

    	Panel tmp = new Panel();             
    	tmp.add("West", new Label("TEMP Dir:"));
    	tmp.add("East", tmpTextField);
	
    	Panel west = new Panel();              
    	west.setLayout(new BorderLayout(0,0));

    	west.add("North",  port);
    	west.add("West",  tmp);

    	Panel ckBox = new Panel();                     
    	Panel ckBoxwest = new Panel();
    	ckBoxwest.setLayout(new BorderLayout(0,0));
    	Panel ckBoxeast = new Panel();
    	ckBoxeast.setLayout(new BorderLayout(0,0));

    	ckBox.add(new Label("Options:"));
    	ckBoxeast.add("North", pipeline);
    	ckBoxeast.add("Center", dsn);
    	ckBoxeast.add("South", tracelog);

    	ckBox.add("West", ckBoxeast);
    	ckBox.add("East", ckBoxwest);
	
    	Panel east = new Panel();                     
    	east.setLayout(new BorderLayout(0,0));

    	east.add("South", ckBox);

    	Panel south = new Panel();                     // new south panel
    	south.add(okButton);
    	south.add(cancelButton);

    	add("North", north);
    	add("West", west);
    	add("East", east);
    	add("South", south);
    	pack();                                    

    	okButton.addActionListener(new ActionListener() 
	{
      		public void actionPerformed(ActionEvent e) 
		{
        		clnt.host = hostTextField.getText(); 
        		clnt.sender = senderTextField.getText(); 
        		clnt.tmpdir = tmpTextField.getText(); 
        		try 
			{
          			clnt.port = Integer.parseInt(portTextField.getText());
        		}
        		catch (NumberFormatException ee) {}        

        		clnt.b_pipeline = pipeline.getState(); 
        		clnt.b_dsn = dsn.getState();
        		clnt.b_tracelog = tracelog.getState();
        		clnt.setEnabled(true);

        		clnt.b_configured = true;

        		setVisible(false);
      		}
        });

        cancelButton.addActionListener(new ActionListener() 
	{
      		public void actionPerformed(ActionEvent e) 
		{
        		clnt.setEnabled(true);
        		setVisible(false);
      		}
    	});
    }
}
