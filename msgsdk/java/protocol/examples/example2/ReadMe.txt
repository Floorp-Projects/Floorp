This directory contains an example program that demonstrates the use of the Netscape
Messaging MIME Encoder java API.

To compile and run:
-------------------

(1)     Make sure CLASSPATH includes the msg-sdk jar file (proapi.jar).
        When installed proapi.jar is located at <install-root>/packages/proapi.jar.
        Make sure CLASSPATH also includes the directory you are compiling in.

(2)     Compile: javac *.java
(3)     Run:    java MIMEEncodeTest

	This shows the usage:  

	java MIMEEncodeTest sender To subject <file-name> <B|Q>

	Explanation of parameters  above:
		sender:  Sender of the message
		To:	 recipient of the message.
		Subject: Subject of the message.
		<file-name>: Name of file to attach (e.g. /tmp/x.txt C:image.jpg)
		<B|Q>:	Type of MIME encoding desired. B = Base64. Q = QP.
			Assumes default based on filename extension if left out.
  
(4)	Run again with appropriate parameters. For example:

	java MIMEEncodeTest prasad@netscape.com prasad@netscape Test-Msg IMAG.JPG B