This directory contains three example programs that demonstrate the use of 
Netscape Messaging SDK convenience API. In particular use of the IMTransport 
and IMAttachment classes is shown. The use of the netscape MIME API for
building MIME messages and MIME encoding the same, is also demonstrated
by one of the programs.

The example programs are:
-------------------------

		sendMessage.java 
		testsendMessage.java and
		testSendDocs.java

sendMessage.java 
----------------

		Demonstrates the use of IMTransport.sendMessage() method, that is
		used to send an existing MIME message. The MIME message to be sent
		is NOT built and assumed to be available.

testsendMessage.java
--------------------

		Demonstrates the use of IMTransport.sendMessage() method, that is
		used to send an existing MIME message. The MIME message to be sent 
		itself is built using the MIME API, which is also demonstrated.

testSendDocs.java
-----------------

		Demonstrates the use of IMTransport.sendDocuments() method that is
		used to send >=1 files (documents) as MIME message.

		This program also demonstrates the use of IMAttachment class that is
		passed to the sendDocuments() method.

To Compile:
-----------
		Set CLASSPATH to include the full path names of the proapi.jar file
		and the coapi.jar file and invoke javac compiler on the source files.
		
		The proapi.jar and coapi.jar files are the Java archive files that 
		contain the classes for the Netscape Messaging SDK protocol APIs and
		the Convenience	API respectively. These two jar files are copied to 
		the packages directory under the install root when you install the SDK.
