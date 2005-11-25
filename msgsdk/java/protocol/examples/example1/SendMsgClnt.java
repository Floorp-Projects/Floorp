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
import java.io.*;
import java.net.*;
import java.util.*;

import netscape.messaging.smtp.*;
import netscape.messaging.mime.*;


/*  Demo Message Send Application using Netscape Messaging SDK.
 *  Demonstrates use of MIME and SMTP java APIs.
 *  Author: Prasad Yendluri <prasad@netscape.com>, Feb 1998.
 */

public class SendMsgClnt extends Frame
{
	///////////////////////////////////////////////////////////////////////////
	// Private data members.
	///////////////////////////////////////////////////////////////////////////

	private Button			sendOrNewButton;
	private Button			fileButton;
	private Button			cancelButton;
	private Button			configButton;
	private Button			exitButton;
	private Button			blankButton;
	private FileDialog		m_fd;
	private Config			config;
	private OKDialog 		configPopup;
	private OKDialog                errPopup;
	private TextField 		ToTextField = new TextField(30);
	private TextField 		CcTextField = new TextField(30);
	private TextArea 		textArea = new TextArea();
	private TextField 		SubjTextField = new TextField(30);
	private TextField 		FileTextField = new TextField(30);
	//private TextField 		InfoTextField = new TextField(30); // sending msg.. / sent
	private String 			blank = new String (" ");
	private String 			ToField = null;
	private String 			CcField = null;
	private String 			SubjField = null;

	protected SMTPClient    	m_SMTPclnt;
	protected SMTPTestSink    	m_smtpSink;

	private String 			filename = null;
	private String 			filename_lastnode = null;
	public  String 			sender = "user@domain.com";
	public  String 			host = "host.mcom.com";
	public  String 			tmpdir = null;
  	public  int    			port = 25;

	private boolean			b_sending = false;
	public boolean 			b_pipeline = false;
	public boolean 			b_tracelog = false;
	public boolean 			b_dsn = false;
	public boolean 			b_configured = false;


  	/////////////////////////////////////////////////////////////////////
	// Constructor
  	/////////////////////////////////////////////////////////////////////
	public SendMsgClnt ()
	{
		init ();
	}

	public void init ()
	{
		try
		{
			Panel controlPanel;

			//setLayout (new BorderLayout (0,0));
			setLayout (new FlowLayout (FlowLayout.LEFT));

			////////////////////////////////////////////////////////////////////
			// Create the panels and set the layouts
			////////////////////////////////////////////////////////////////////

			////////////////////////////////////////////////////////////////////
			// 1.  Control Panel 
			////////////////////////////////////////////////////////////////////

			controlPanel = new Panel ();

			controlPanel.setLayout (new GridLayout (8,1));

			////////////////////////////////////////////////////////////////////
			// Create the buttons.
			////////////////////////////////////////////////////////////////////

			sendOrNewButton	= new Button ("NewMessage");
			fileButton	= new Button ("AttachFile");
			cancelButton	= new Button ("Cancel");
			blankButton	= new Button ("      ");
			configButton	= new Button ("SetUp");
			exitButton	= new Button ("Exit");

			////////////////////////////////////////////////////////////////////
			// Set Button Properties
			////////////////////////////////////////////////////////////////////

			sendOrNewButton.setBackground(Color.yellow);
    			sendOrNewButton.setForeground(Color.black);

			cancelButton.setBackground(Color.red);
    			cancelButton.setForeground(Color.black);

			fileButton.setBackground(Color.blue);
    			fileButton.setForeground(Color.white);

			configButton.setBackground(Color.magenta);
    			configButton.setForeground(Color.white);

			exitButton.setBackground(Color.black);
    			exitButton.setForeground(Color.white);

			////////////////////////////////////////////////////////////////////
			// Add the buttons and the images to the controlPanel
			////////////////////////////////////////////////////////////////////

			controlPanel.add (sendOrNewButton);
			controlPanel.add (cancelButton);
			controlPanel.add (fileButton);
			controlPanel.add (blankButton);
			controlPanel.add (configButton);
			controlPanel.add (exitButton);

			////////////////////////////////////////////////////////////////////
			// Add the panel to the frame
			////////////////////////////////////////////////////////////////////

			add ("South", controlPanel);

			cancelButton.setEnabled(false); // initially
			fileButton.setEnabled(false); // initially

			////////////////////////////////////////////////////////////////////
			// 2. To/Cc/Text/Staus  Panel 
			////////////////////////////////////////////////////////////////////

			Panel userPanel = new Panel ();
			Panel ToPanel = new Panel ();
			Panel CcPanel = new Panel ();
			Panel SubjPanel = new Panel ();
			Panel FilePanel = new Panel ();

			ToPanel.add("West", new Label("To: "));
        		ToPanel.add("East", ToTextField);

			CcPanel.add("West", new Label("Cc: "));
        		CcPanel.add("East", CcTextField);

			SubjPanel.add("West", new Label("Subject: "));
        		SubjPanel.add("East", SubjTextField);

			FilePanel.add("West", new Label("Attached: "));
        		FilePanel.add("East", FileTextField);

			Panel northPanel = new Panel ();
			northPanel.setLayout (new BorderLayout());

			northPanel.add ("North",  ToPanel);
			northPanel.add ("Center", CcPanel);
			northPanel.add ("South",  SubjPanel);

			// Intially Until New Message is pressed
		 	ToTextField.setEnabled(false);
		 	CcTextField.setEnabled(false);
		 	SubjTextField.setEnabled(false);
		 	FileTextField.setEditable(false);
		 	textArea.setEnabled(false);

			userPanel.setLayout (new BorderLayout());

			userPanel.add ("North",  northPanel);
			userPanel.add ("Center", textArea);
			userPanel.add ("South",  FilePanel);

			add ("West", userPanel);
			add ("East", controlPanel);
			//add ("South", InfoPanel);

			////////////////////////////////////////////////////////////////////
			// Action Handlers
			////////////////////////////////////////////////////////////////////
	
			// -- New_Or_Send Button was pressed --------
			sendOrNewButton.addActionListener(new ActionListener() 
			{
      				public void actionPerformed(ActionEvent e) 
				{
					if (b_configured == false)
					{
						// POP UP a Dialog BOX to configure 
						// and return without sending

						configPopup.display();
						return;
					}

					if (b_sending == true)
					{
						// Currently in "Send" Mode.
						// Send Message and Switch to "NewMessage" Mode

						// Try to send the message.
						// If it was successfull (i.e. all needed 
						// fields are present etc. only then do the
						// label change. If not Pop up a Dialog Box
						// asking user to check for the needed fields.

						if (sendIt ())
							return;

						sendOrNewButton.setLabel ("NewMessage");
						sendOrNewButton.setBackground(Color.yellow);
						cancelButton.setEnabled(false);
						fileButton.setEnabled(false);

		 				ToTextField.setEnabled(false);
		 				CcTextField.setEnabled(false);
		 				SubjTextField.setEnabled(false);
		 				textArea.setEnabled(false);

						b_sending = false;
					}
					else
					{
						// Currently in "NewMessage" Mode.
						// Switch to (Compose &) Send mode.
						sendOrNewButton.setLabel ("Send");
						sendOrNewButton.setBackground(Color.green);
						cancelButton.setEnabled(true);
						fileButton.setEnabled(true);
						filename = null; // ready for next round
						filename_lastnode = null; // ready for next round

						// Clear The Text
		 				ToTextField.setText (blank);
		 				CcTextField.setText (blank);
		 				SubjTextField.setText(blank);
		 				textArea.setText (blank);
					    	FileTextField.setText (blank);

						// Make them writable
		 				ToTextField.setEnabled(true);
		 				CcTextField.setEnabled(true);
		 				SubjTextField.setEnabled(true);
		 				textArea.setEnabled(true);

						b_sending = true;
					}
      				}
        		});
 
			// -- File Button was pressed --------
			fileButton.addActionListener(new ActionListener() 
			{
      				public void actionPerformed (ActionEvent e) 
				{
					filename = (selectFile()).trim();

					if (filename != null)
					{
					    fileButton.setEnabled(false);
					    FileTextField.setText (filename);
					}
					else
					     FileTextField.setText (null);
      				}
        		});
 
			// -- config (SetUP) Button was pressed --------
			configButton.addActionListener(new ActionListener() 
			{
      				public void actionPerformed (ActionEvent e) 
				{
					setEnabled(false);
        				config.display();
      				}
        		});

			// -- Cancel Button was pressed --------
			cancelButton.addActionListener(new ActionListener() 
			{
      				public void actionPerformed (ActionEvent e) 
				{
					sendOrNewButton.setLabel ("NewMessage");
					sendOrNewButton.setBackground(Color.yellow);
					b_sending = false;
					cancelButton.setEnabled(false);
					fileButton.setEnabled(false);

		 			ToTextField.setEnabled(false);
		 			CcTextField.setEnabled(false);
		 			SubjTextField.setEnabled(false);
		 			textArea.setEnabled(false);
      				}
        		});

			// -- Exit Button was pressed --------
			exitButton.addActionListener (new ActionListener() 
			{
      				public void actionPerformed (ActionEvent e) 
				{
					System.exit (0);
      				}
        		});

			////////////////////////////////////////////////////////////////////
			// Set the Frame's title etc.
			////////////////////////////////////////////////////////////////////

			setTitle ("Netscape Messaging SDK - Send Message Application");

			//setLocation(500,200);
			//setLocation(0,0);

			setResizable(false);
			pack();

			// Make Config stuff ready to display
			config = new Config (this);
			configPopup = new OKDialog (this, "Error!", "Please SetUp First!");

			// set up default tmpdir
			String sep = System.getProperty ("path.separator");
			if (sep.equals (";"))
				tmpdir = "C:\\temp"; // its windoz
			else
				tmpdir = "/tmp";

			m_fd = new FileDialog (this, "Pick a File", FileDialog.LOAD);

			m_smtpSink = new SMTPTestSink();
			m_SMTPclnt = new SMTPClient(m_smtpSink);
		}
		catch (Exception e)
		{
			System.out.println ("SendMsgClnt Constructor caught Exception>>> " + e );
		}

	} // init ()

  	/////////////////////////////////////////////////////////////////////
	// Display and Let the user Select File
  	/////////////////////////////////////////////////////////////////////
	public String selectFile ()
        {
                m_fd.setDirectory (".");
		m_fd.setVisible (true);
                //m_fd.show ();
		String file =  m_fd.getFile();
		
		if (file == null || file.length() <= 0)
                        return null;

		filename_lastnode = file;

                return (new String (m_fd.getDirectory() + file));
        }


  	/////////////////////////////////////////////////////////////////////
	// Just the Destroy Event. The EXIT button takes care of that
  	/////////////////////////////////////////////////////////////////////
//	public boolean handleEvent (Event evt)
//	{
//		if (evt.id == Event.WINDOW_DESTROY && evt.target == this)
//		{
//			System.exit(0);
//		}
//		else
//		{
//			return super.handleEvent (evt);
//		}
//		return true;
//	}

	/////////////////////////////////////////////////////////////////////
	// Send the Message out. On success return false and true on failure.
	// On detection of any errors Pop up a Dialog Box asking user to check 
	// for the needed fields.
	/////////////////////////////////////////////////////////////////////
	public boolean sendIt ()
        {
		String[]		l_addrs;

		// make sure needed items are present

		if (sender == null || sender.length() == 0 || sender.equals ("user@domain.com"))
		{
			showFailPopUp("Error!",  "Please SetUp Msg Sender!");
			return true;
		}

		if (host == null || host.length() == 0 || host.equals ("host.mcom.com"))
		{
			showFailPopUp("Error!",  "Please SetUp SMTP Host!");
			return true;
		}

		ToField = ToTextField.getText();

		if (ToField == null || ToField.length() == 0 || ToField.equals (blank))
		{
			showFailPopUp("Error!",  "Please specify a recipient in To:");
			return true;
		}

		CcField = CcTextField.getText();
		SubjField = SubjTextField.getText();

		try
		{
			m_SMTPclnt.connect (host, port);

			if (!m_smtpSink.tryProcessResponses(m_SMTPclnt))
			{
				showFailPopUp("Error!", "Unable to connect to SMTP host: " + host);
				return true;
			}
		}
		catch ( Exception e )
                {
			showFailPopUp("Error!", "Unable to connect to SMTP host: " + host);
			return true;
                }


		try
		{
			m_SMTPclnt.mailFrom (sender, null);

			if (!m_smtpSink.tryProcessResponses(m_SMTPclnt))
			{
				showFailPopUp("Error!", "Invalid Msg Sender Configured!");
				return true;
			}


			boolean someFailed = false;
			boolean allFailed = true;

			l_addrs = parseAddrs (ToField);

			for (int i = 0, len = l_addrs.length; i < len; i++)
			{
			     m_SMTPclnt.rcptTo (l_addrs[i], null);
			     if (!m_smtpSink.tryProcessResponses(m_SMTPclnt))
					someFailed = true;
			     else
				  allFailed = false;
			}

			if (CcField != null && !CcField.equals (blank))
			{
				l_addrs = parseAddrs (CcField);
				for (int i = 0, len = l_addrs.length; i < len; i++)
				{
			     		m_SMTPclnt.rcptTo (l_addrs[i], null);

			     		if (!m_smtpSink.tryProcessResponses(m_SMTPclnt))
						someFailed = true;
			     		else
				  		allFailed = false;
				}
			}

			if (allFailed)
			{
				showFailPopUp("Error!",  "Server rejected all recipients");
				return true;
			}

			InputStream is = getMIMEMessage (sender, ToField, CcField, SubjField, 
							 filename);

			m_SMTPclnt.data ();

			if (!m_smtpSink.tryProcessResponses(m_SMTPclnt))
			{
				showFailPopUp("Error!", "Could not intiate Data transfer. Server Code =" 
							+ m_smtpSink.m_failCode);
				return true;
			}

			m_SMTPclnt.send (is);

			if (!m_smtpSink.tryProcessResponses(m_SMTPclnt))
			{
				showFailPopUp("Error!", "Data transfer failure! Server code = "
							+ m_smtpSink.m_failCode);
				return true;
			}

			m_SMTPclnt.quit();

			if (!m_smtpSink.tryProcessResponses(m_SMTPclnt))
			{
				showFailPopUp("Error!", "Unbale to quit from server! Server code = "
							+ m_smtpSink.m_failCode);
			}

			
		}
		catch ( Exception e )
                {
			showFailPopUp("Exception!", e.getMessage());
			return true;
                }

		// On success return false
		return false;
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Build a MIME Message and return an inputStream
	/////////////////////////////////////////////////////////////////////////////////

	public InputStream getMIMEMessage (String sender, String To, String Cc, String subject, 
						String filefullname)
	{
		if (filename == null)
			return TextMIMEMessage (sender, To, Cc, subject);
		else
			return MultiPartMIMEMessage (sender, To, Cc, subject, filefullname);
	}

	//////////////////////////////////////////////////////////////////////
	// Build and return an InputStream for MIME-Text-Message
	//////////////////////////////////////////////////////////////////////
	public InputStream TextMIMEMessage  (String sender, String To, String Cc, String subject)
	{
	    FileInputStream fis;
	    FileOutputStream fos;
	    ByteArrayInputStream bins;

	    MIMEMessage   mmsg;

	    try
	    {
		// >>>> Build and return a text MIME Message <<<<
		
		// Get an inputStream to user entered text 
                bins = new ByteArrayInputStream ((textArea.getText()).getBytes());

		// Create text MIMEMessage with the above text
                mmsg = new MIMEMessage(bins, null, 0);

		// set the user entered headers on the message
		mmsg.setHeader ("From", sender);
                mmsg.setHeader ("Reply-To", sender);
                mmsg.setHeader ("To", To);
 
                if (Cc != null && Cc.length() != 0 && !Cc.equals (blank))
                        mmsg.setHeader ("Cc", Cc);
 
                if (subject != null && subject.length() != 0 && !subject.equals (blank))
                        mmsg.setHeader ("Subject", subject);
 
		// Add any other desired headers.
                mmsg.setHeader ("X-MsgSdk-Header", "This is a Text Message");

		String mimefile = new String (tmpdir + "/SDKMIMEMsg.out");
                fos = new FileOutputStream (mimefile);

		// Encode the message in MIME Canaonical form for transmission
                mmsg.putByteStream (fos);
 
		// Return an inputStream to the encoded MIME message
                fis = new FileInputStream (mimefile);
                return fis;
	    }
	    catch (Exception e)
            {
                showFailPopUp ("Exception",  e.getMessage());
		return null;
            }
	}

	//////////////////////////////////////////////////////////////////////
	// Build and return an InputStream for MIME-Multi-Part-Message
	//////////////////////////////////////////////////////////////////////
	public InputStream MultiPartMIMEMessage  (String sender, String To, String Cc, 
						  String subject, String fullfilename)
	{
	    FileInputStream fis, fdis;
	    FileOutputStream fos;
	    ByteArrayInputStream bins;

	    MIMEMessage   mmsg;

	    try
	    {
		// >>>> Build and return a Multi-Part MIME Message <<<<

		// Get an inputStream to user entered text 
                bins = new ByteArrayInputStream ((textArea.getText()).getBytes());

		// Create a new Multi-part MIMEMessage with the above text and the file passed
                mmsg = new MIMEMessage(bins, fullfilename, -1);

		// set the user entered headers on the message
		mmsg.setHeader ("From", sender);
                mmsg.setHeader ("Reply-To", sender);
                mmsg.setHeader ("To", To);
 
                if (Cc != null && Cc.length() != 0 && !Cc.equals (blank))
                        mmsg.setHeader ("Cc", Cc);
 
                if (subject != null && subject.length() != 0 && !subject.equals (blank))
                        mmsg.setHeader ("Subject", subject);
 
		// Add any other desired headers.
                mmsg.setHeader ("X-MsgSdk-Header", "This is a Text Message");

		String mimefile = new String (tmpdir + "/SDKMIMEMsg.out");
                fos = new FileOutputStream (mimefile);

		// Encode the message in MIME Canaonical form for transmission
		mmsg.putByteStream (fos);

		// Return an inputStream to the encoded MIME message
		fis = new FileInputStream (mimefile);
		return fis;
	    }
	    catch (Exception e)
            {
                showFailPopUp ("Exception",  e.getMessage());
		return null;
            }
	}


	////////////////////////////////////////////////////////////////
	// Parse space or comma separated addresses
	///////////////////////////////////////////////////////////////
	String [] parseAddrs (String mailAddrs) 
	{
		String [] retAddrs;
		int count;

		String  inAddrs = mailAddrs.trim();
		
		StringTokenizer stz = new StringTokenizer (inAddrs, " ,");

		count = stz.countTokens(); 

		if (count <= 0)
			return null;

		retAddrs = new String [count];

		for (int i = 0; i < count; i++)
		{
			retAddrs [i] = stz.nextToken();
		}

		
		return retAddrs;
	}

	//////////////////////////////////////////////////////////////
	// Show the messages via PopUP
	//////////////////////////////////////////////////////////////
	public void showFailPopUp (String err, String msg)
	{
			errPopup = new OKDialog (this, err, msg);
			errPopup.display();
	}


  	/////////////////////////////////////////////////////////////////////
	// -----------------  MAIN -------------------------------
  	/////////////////////////////////////////////////////////////////////
	public static void main (String args[])
	{
		////////////////////////////////////////////////////////////////////////
		// Create the SendMsgClnt Frame, set the size, and display it.
		////////////////////////////////////////////////////////////////////////

		SendMsgClnt client = new SendMsgClnt();
		//client.resize (200, 300);
		client.setVisible (true);
		//client.show ();
	}

}
