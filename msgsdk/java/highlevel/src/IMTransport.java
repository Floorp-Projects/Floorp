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


package netscape.messaging.convenience;
import java.io.*;
import java.lang.*;
import java.util.*;

import netscape.messaging.smtp.*;
import netscape.messaging.mime.*;

/**
 * The IMTransport class implements Java Convenience APIs.
 * <p>This class provides a convenient interface for building,
 * encoding, and sending messages. It combines the functionality
 * of MIME and SMTP.
 * @author Prasad Yendluri
 */
public class IMTransport
{
	/**
         * Content (primary) type Text.
         */
        public static final int TEXT            = 0;
        /**
         * Content (primary) type Audio.
         */
        public static final int AUDIO           = 1;
        /**
         * Content (primary) type Image.
         */
        public static final int IMAGE           = 2;
        /**
         * Content (primary) type Video.
         */
        public static final int VIDEO           = 3;
        /**
         * Content (primary) type Application.
         */
        public static final int APPLICATION     = 4;

	/**
         * Base64 Transfer Encoding.
         */
        public static final int BASE64          = 0;
        /**
         * Quoted Printable Transfer Encoding.
         */
        public static final int QP              = 1;
        /**
         * No Transfer Encoding.
         */
        public static final int NONE            = 2;

        /**
         * Content disposition INLINE
         */
        public static final int INLINE         = 1;

        /**
         * Content disposition ATTACHMENT
         */
        public static final int ATTACHMENT     = 2;

	/**
	 * Default constructor for the IMTransport class.
         */

	//public IMTransport ()
        //{
        //   // Do any initialization
        //}

	/**
     * Connects to the SMTP transport at the specified host and submits the message.
	 * <P>NOTE: The MIME Message itself can be created using the Netscape MIME API or
	 * by any other means.
         * @param host Name of the host to connect to.
         * @param sender Sender of the message.
         * @param recipients Email addresses of the recipients of the message, separated by commas or spaces.
         * @param MIMEMessageStream Input-Stream to the message in MIME canonical form.
         * @return List of recipients to whom the message could not be submitted.
	 * @exception IMException If the message could not be sent due to invalid host
         * or other causes.
         * @see #sendDocuments
         */
	public static String [] sendMessage (String host,
				      	     String sender,
				      	     String recipients,
				      	     InputStream MIMEMessageStream) throws IMException
        {
		Vector 			l_rejrecips;
		SMTPClient		l_SMTPclnt;
		IMSMTPSink		l_smtpSink;
		String[]                l_addrs=null;

		if (sender == null || sender.length() == 0)
                {
			throw new IMException ("Invalid null sender");
                }

                if (host == null || host.length() == 0)
                {
			throw new IMException ("Invalid null host");
                }

		if (MIMEMessageStream == null)
		{
			throw new IMException ("Invalid InputStream");
		}

		if (recipients != null)
			l_addrs = parseAddrs (recipients);

		if (l_addrs == null || l_addrs.length <= 0)
		{
			throw new IMException ("Invalid recipients");
		}

		if (recipients != null)
                        l_addrs = parseAddrs (recipients);

                if (l_addrs == null || l_addrs.length <= 0)
                {
                        throw new IMException ("Invalid recipients");
                }

		try {
		l_smtpSink = new IMSMTPSink();
		l_SMTPclnt = new SMTPClient (l_smtpSink);

		l_SMTPclnt.connect (host);

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("Unable to connect to SMTP host " + host);
		}

		l_SMTPclnt.mailFrom (sender, null);

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("sender rejected by server. Resp Code = " +
							l_smtpSink.m_failCode);
		}

		boolean someFailed = false;
		boolean allFailed = true;
		l_rejrecips = new Vector();

		for (int i = 0, len = l_addrs.length; i < len; i++)
		{
			l_SMTPclnt.rcptTo (l_addrs[i], null);
			if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
			{
				someFailed = true;
				l_rejrecips.addElement (l_addrs[i]);
			}
			else
				allFailed = false;
		}

		if (allFailed)
		{
			throw new IMException ("Server rejected all recipients");
		}

		l_SMTPclnt.setChunkSize (8192);
		l_SMTPclnt.data ();

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("Unable to intiate Data transfer with server.  Resp Code = " +
						l_smtpSink.m_failCode);
		}

		l_SMTPclnt.send (MIMEMessageStream);

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("Data transfer failure! Resp Code = " +
						l_smtpSink.m_failCode);
		}

		l_SMTPclnt.quit();

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			// Ignore!
                        //showFailPopUp("Error!", "Unbale to quit from server! Server code = "
                        //                       + l_smtpSink.m_failCode);
		}

		if (someFailed == true)
		{
			if (l_rejrecips == null)
				return null;

			String [] rejrecips = new String [l_rejrecips.size()];

			for (int i=0; i < l_rejrecips.size(); i++)
			{
				rejrecips [i] = (String) l_rejrecips.elementAt (i);
			}

                	return (rejrecips);
		}
		else
		{
			return null;
		}
	    }
	    catch (IOException e)
            {
                  throw new IMException (e.getMessage());
            }
        }

	protected static String [] sendMessage2 (String host,
				      	     String sender,
				      	     String recipients,
				      	     PipedInputStream MIMEMessageStream) throws IMException
        {
		Vector 			l_rejrecips;
		SMTPClient		l_SMTPclnt;
		IMSMTPSink		l_smtpSink;
		String[]                l_addrs=null;

		if (sender == null || sender.length() == 0)
                {
			throw new IMException ("Invalid null sender");
                }

                if (host == null || host.length() == 0)
                {
			throw new IMException ("Invalid null host");
                }

		if (MIMEMessageStream == null)
		{
			throw new IMException ("Invalid InputStream");
		}

		if (recipients != null)
			l_addrs = parseAddrs (recipients);

		if (l_addrs == null || l_addrs.length <= 0)
		{
			throw new IMException ("Invalid recipients");
		}

		if (recipients != null)
                        l_addrs = parseAddrs (recipients);

                if (l_addrs == null || l_addrs.length <= 0)
                {
                        throw new IMException ("Invalid recipients");
                }

		try {
		l_smtpSink = new IMSMTPSink();
		l_SMTPclnt = new SMTPClient (l_smtpSink);

		l_SMTPclnt.connect (host);

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("Unable to connect to SMTP host " + host);
		}

		l_SMTPclnt.mailFrom (sender, null);

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("sender rejected by server. Resp Code = " +
							l_smtpSink.m_failCode);
		}

		boolean someFailed = false;
		boolean allFailed = true;
		l_rejrecips = new Vector();

		for (int i = 0, len = l_addrs.length; i < len; i++)
		{
			l_SMTPclnt.rcptTo (l_addrs[i], null);
			if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
			{
				someFailed = true;
				l_rejrecips.addElement (l_addrs[i]);
			}
			else
				allFailed = false;
		}

		if (allFailed)
		{
			throw new IMException ("Server rejected all recipients");
		}


		l_SMTPclnt.data ();

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("Unable to intiate Data transfer with server.  Resp Code = " +
						l_smtpSink.m_failCode);
		}

		l_SMTPclnt.send (MIMEMessageStream);

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			throw new IMException ("Data transfer failure! Resp Code = " +
						l_smtpSink.m_failCode);
		}

		l_SMTPclnt.quit();

		if (!l_smtpSink.tryProcessResponses(l_SMTPclnt))
		{
			// Ignore!
                        //showFailPopUp("Error!", "Unbale to quit from server! Server code = "
                        //                       + l_smtpSink.m_failCode);
		}

		if (someFailed == true)
		{
			if (l_rejrecips == null)
				return null;

			String [] rejrecips = new String [l_rejrecips.size()];

			for (int i=0; i < l_rejrecips.size(); i++)
			{
				rejrecips [i] = (String) l_rejrecips.elementAt (i);
			}

                	return (rejrecips);
		}
		else
		{
			return null;
		}
	    }
	    catch (IOException e)
            {
                  throw new IMException (e.getMessage());
            }
        }

	/**
	 * Builds a MIME message with the specified parameters. Connects to the SMTP transport
	 * at the specified host and submits the message. If the message has more than one
	 * attachment, it is sent as MIME multipart/mixed type message.
	 * <P>NOTE: This method facilitates mailing documents by mail-enabling an otherwise
         * mail-ignorant application. For any other uses or more sophisticated needs,
         * use the sendMessage() method in association with the Netscape MIME API
         * or the other Messaging APIs provided by Netscape.
	 * <P>NOTE: If the fUseTempFiles parameter is set to true, sendDocuments uses
	 * temporary intermediate files for some internal processing. This mode provides better
	 * performance. If temporary files should not be created, set this flag to false.
         * @param host Name of the host to connect to.
         * @param sender Sender of the message.
         * @param recipients Email addresses of the recipients of the message, separated by commas or spaces.
         * @param subject Subject of the Message. Can be null.
         * @param msgHeaderNames Additional RFC-822 header names.
         * @param msgHeaderValues Values corresponding to the headers in the msgHeaderNames parameter.
         * @param fUseTempFiles Whether to use temporary intermediate files for some
         * internal processing. Values:
         * <ul><li>true: use temp files (better performance).
         * <li>false: do not use temp files. </ul>
         * @param fUseTempFiles If true, uses temporary intermediate files (for better performance).
         * @return List of recipients to whom the message could not be submitted.
	 * @exception IMException If the message could not be sent due to invalid host or other
	 * causes, or if there are inconsistencies in the parameter specifications.
         */
	public static String [] sendDocuments (String host,
				        String sender,
					String recipients,
					String subject,
					String[] msgHeaderNames,
					String[] msgHeaderValues,
					IMAttachment [] attachments,
					boolean fUseTempFiles) throws IMException
        {
		MIMEMultiPart l_mmp;
		MIMEBasicPart l_mbp;
		MIMEMessage l_mmsg;
		boolean fMultiPart = false;
		//boolean fUseTempFiles = true;
		int attach_count = 0, hdrs_count = 0;
		//FileInputStream l_fis;
		String [] rejrecips;
		String m_mimefile;
		Vector l_rejects;

		if (sender == null || sender.length() == 0)
                {
			throw new IMException ("Invalid null sender");
                }

                if (host == null || host.length() == 0)
                {
			throw new IMException ("Invalid null host");
                }

		if (recipients == null)
		{
			throw new IMException ("Invalid null recipients");
		}

		if (msgHeaderNames != null)
		{
			hdrs_count = msgHeaderNames.length;

			if (msgHeaderValues == null || hdrs_count != msgHeaderValues.length)
			{
				throw new IMException ("msgHeaderNames and msgHeaderValues not equal in number");
			}
		}
		else
			hdrs_count = 0;

		if (attachments == null || attachments.length == 0)
		{
			throw new IMException ("Invalid null attachments");
		}

		attach_count = attachments.length;

		if (attach_count > 1)
			fMultiPart = true;
		try
		{
			l_mmp = new MIMEMultiPart ();
			l_mmp.setContentSubType ("mixed");

			for (int i = 0; i < attach_count; i++)
			{
				// build and add each attach to the message

				if (attachments [i] != null)
				{
				        //System.out.println("sendDocuments()> doing buildBasicPart. i= " + i);
					l_mbp = buildBasicPart (attachments[i]);
					l_mmp.addBodyPart (l_mbp, false);
				}
			}

			l_mmsg = new MIMEMessage ();

			l_mmsg.setBody (l_mmp, false);

			if (hdrs_count > 0)
			for (int i = 0; i < hdrs_count; i++)
			{
				l_mmsg.setHeader (msgHeaderNames[i], msgHeaderValues [i]);
			}

			if (subject != null)
				l_mmsg.setHeader ("Subject", subject);

			if (fUseTempFiles == true)
			{
				// set up default tmpdir
				String sep, tmpdir;
	                        sep = System.getProperty ("path.separator");
	                        if (sep.equals (";"))
	                                tmpdir = "C:\\temp\\"; // its windoz
	                        else
	                                tmpdir = "/tmp/";

				Random l_rand = new Random (System.currentTimeMillis());
				long l_decrand = l_rand.nextLong();

				m_mimefile = new String (tmpdir + Long.toHexString(l_decrand));

				removeFile (m_mimefile);
				FileOutputStream fos = new FileOutputStream (m_mimefile);
				l_mmsg.putByteStream (fos);

				FileInputStream l_fis = new FileInputStream (m_mimefile);

				rejrecips = sendMessage (host, sender, recipients, l_fis);
                		removeFile (m_mimefile);

                		return rejrecips;
			}
			else
			{
				PipedOutputStream pos = new PipedOutputStream();
				PipedInputStream pins = new PipedInputStream(pos);

				l_rejects = new Vector();

				IMSendThr sendThr = new IMSendThr (pins, host, sender,
								   recipients, l_rejects);
				sendThr.start();

				//System.out.println("SendDocs> l_mmsg.putByteStream()");

				l_mmsg.putByteStream (pos);


				//System.out.println("SendDocs> pos.close()");
				pos.close();

				//System.out.println("SendDocs> doinigIMSendThr.join()!");
				sendThr.join();

				if (l_rejects == null)
					return null;

				rejrecips = new String [l_rejects.size()];

				for (int i=0; i < l_rejects.size(); i++)
				{
					rejrecips [i] = (String) l_rejects.elementAt (i);
				}

				return rejrecips;
			}

		}
		catch (Exception e)
		{
			e.printStackTrace();
			throw new IMException (e.getMessage());
		}

        }

	protected static void removeFile(String in_file)
        {
		if (in_file != null)
		{
                	File file = new File(in_file);
        		if (file.exists())
                		file.delete();
		}
        }


        protected static MIMEBasicPart buildBasicPart (IMAttachment attachment) throws MIMEException, IMException, IOException
	{
		fileMIMEType    l_fmt;
		MIMEBasicPart 	l_basicPart;
		int 		content_type;
		String        	content_subtype;
        	String      	content_params;
		int           	content_disposition;
		int 		mime_encoding = NONE;
		InputStream	dataIS;

		if (attachment.attach_file != null)
		{
			if (attachment.attach_stream != null)
			{
				throw new IMException ("Invalid attachment");
			}

			l_fmt  = MIMEHelper.getFileMIMEType (attachment.attach_file);

			if (l_fmt == null)
				throw new IMException ("Invalid attachment");

			if (attachment.content_type >= TEXT && attachment.content_type <= APPLICATION)
			{
				//System.out.println("buildBasicPart()> setting content-type from passed in");
				content_type = attachment.content_type;
				content_subtype = attachment.content_subtype;
				content_params = attachment.content_params;
			}
			else
			{
				//System.out.println("buildBasicPart()> setting content-type from getFileMIMEType() = " + l_fmt.content_type + " sub-type" + l_fmt.content_subtype + " params=" + l_fmt.content_params);

				content_type = l_fmt.content_type;
				content_subtype = l_fmt.content_subtype;
				content_params = l_fmt.content_params;
			}

			if (l_fmt.mime_encoding != BASE64)
			{
				if (attachment.mime_encoding >= BASE64 && attachment.mime_encoding < NONE)
					mime_encoding = attachment.mime_encoding;
				else
					mime_encoding = l_fmt.mime_encoding;
			}
			else
				mime_encoding = l_fmt.mime_encoding;

			FileInputStream fis  = new FileInputStream (attachment.attach_file);
			dataIS = fis;
		}
		else
		{
			dataIS = attachment.attach_stream;

			if (attachment.content_type >= TEXT && attachment.content_type <= APPLICATION)
			{
				content_type = attachment.content_type;
				content_subtype = attachment.content_subtype;
				content_params = attachment.content_params;

				if (attachment.mime_encoding >= BASE64 && attachment.mime_encoding < NONE)
					mime_encoding = attachment.mime_encoding;
				else
					mime_encoding = BASE64;
			}
			else
			{
				content_type = APPLICATION;
				content_subtype = new String ("octet-stream");
				content_params = null;
				mime_encoding = BASE64;
			}
		}

		if (attachment.content_disposition == INLINE ||
		    attachment.content_disposition == ATTACHMENT)
		{
			content_disposition = attachment.content_disposition;
		}
		else
			content_disposition = INLINE;

		l_basicPart = new MIMEBasicPart(content_type);

		if (content_subtype != null)
			l_basicPart.setContentSubType (content_subtype);

		if (content_params != null)
			l_basicPart.setContentTypeParams (content_params);

		if (attachment.content_disposition > -1)
			l_basicPart.setContentDisposition (attachment.content_disposition);

		l_basicPart.setBodyData (dataIS);

		//new
		if (mime_encoding == BASE64)
		    l_basicPart.setContentEncoding (MIMEBodyPart.BASE64);
		else if (mime_encoding == QP)
		    l_basicPart.setContentEncoding (MIMEBodyPart.QP);

                return l_basicPart;
	}

	////////////////////////////////////////////////////////////////
        // Parse space or comma separated addresses
        ///////////////////////////////////////////////////////////////
        protected static String [] parseAddrs (String mailAddrs)
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
}
