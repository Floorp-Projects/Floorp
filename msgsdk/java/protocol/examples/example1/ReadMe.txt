This directory contains files that implement an example Send Message Program using 
Netscape Messaging SDK. This is a fairly complex example showing the use of SMTP
and MIME java APIs.

To compile and run:
-------------------

(1)	Make sure CLASSPATH includes the msg-sdk jar file (proapi.jar).
	When installed proapi.jar is located at <install-root>/packages/proapi.jar.

	Make sure CLASSPATH includes the directory you are compiling in also.

(2)	Compile: javac *.java

(3)	Run:	java SendMsgClnt		(No parameters needed).

	This brings up a GUI screen that lets one compose and send a message, 
	optionally attaching a file.