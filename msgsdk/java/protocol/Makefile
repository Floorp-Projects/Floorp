#
# GNU Makefile for Messaging SDK
#

all: 
	cd Messaging/src; $(MAKE)  $(MFLAGS) 
	cd MIME/src; $(MAKE)  $(MFLAGS) 
	cd POP3/src; $(MAKE)  $(MFLAGS) 
	cd IMAP4/src; $(MAKE) $(MFLAGS) 
	cd SMTP/src; $(MAKE)  $(MFLAGS) 

javaDoc:
	- mkdir ../built/$(ARCH)/docs/mime
	- mkdir ../built/$(ARCH)/docs/pop3
	- mkdir ../built/$(ARCH)/docs/imap4
	- mkdir ../built/$(ARCH)/docs/smtp
	cd POP3/src; javadoc -d ../../../built/$(ARCH)/docs/POP3 *.java  
	cd smtp/src; javadoc -d ../../../built/$(ARCH)/docs/smtp *.java  
	cd imap4/src; javadoc -d ../../../built/$(ARCH)/docs/imap4 *.java  
	cd MIME/src; javadoc -d ../../../built/$(ARCH)/docs/mime *.java  
