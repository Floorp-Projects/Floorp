This directory contains example programs that demonstrate the use of 
the C MIME Encoder / Parser API.

The needed include files mime.h nsmail.h and nsStream.h etc. are installed at 

	<install-root>/include/nsStream.h and
	<install-root>/include/nsmail.h and
	<install-root>/include/protocol/mime.h

Where <install-root> is where you installed the Netscape Messaging-sdk.

How to compile and run:
-----------------------

When compiling for UNIX platforms specify -DXP_UNIX. 
For Windows do not specify -DXP_UNIX.

You would need to link with libmime.so library and libcomm.so.

libmime library implements the MIME functionality and
libcomm library contains the implementation of Stream I/O 
functions that are defined in nsStream.h

NOTE: The .so extension for libraries is used on Solaris and 
      other such platforms. The extension is different on other
      platforms. On NT it is .lib and .dll for example.

An example compile statement for Solaris is shown below:

	cc -DXP_UNIX -I./ testMessage.c -lmime -lcomm -lnsl -lsocket

To Run:
------

Be sure to set the LD_LIBRARY_PATH or the equivalents and
run the program. For example, if you complied testMessage.c to generate a.out 
run the program as follows: 

Usage:   a.out <file-name> [encoding]

Where encoding can be B (Base64), Q (QP). If the encoding parameter
is not supplied the program assumes a default based on filename extension.

For example:   a.out IMAGE.JPG B
               a.out xxx.txt   Q
               a.out xxx.txt  
