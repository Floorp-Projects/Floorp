This directory contains an example program that demonstrates the use of the Netscape
Messaging MIME Parser java API.

To compile and run:
-------------------

(1)     Make sure CLASSPATH includes the msg-sdk jar file (proapi.jar).
        When installed proapi.jar is located at <install-root>/packages/proapi.jar.
        Make sure CLASSPATH also includes the directory you are compiling in.

(2)     Compile: javac *.java
(3)     Run:    java testApp

	This shows the usage:  

	usage: java testApp <file-name> [D] 

	Explanation of parameters  above:
		<file-name>:  file with MIME message to parse
		[D]	For dynamic parsing (default). 
			Any other values performs static parsing
  
(4)	Run again with appropriate parameters. For example:

	java testApp mime1.txt D

NOTE:	The other two program testAppStr.java and  
        testAppStrLoop.java  have minor differences with 
        testApp.java. 
        
        testAppStr.java  passes the entire input stream to the parser in one shot.
        testAppStrLoop.java  Demonstrate parsing > 1 messages with the same parser object.
