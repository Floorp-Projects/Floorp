##### PACKAGING MAKEFILE for MESSAGING SDK############

MSGSDK_ROOT=./C
BUILD_MODULE=MSGSDK
CONFIG_ROOT=./CONFIG
include CONFIG/msgsdk.mk

ifndef MMDD
MMDD = $(shell date +%m.%d)
endif

JAVA_EXE=mjdk35_1
C_EXE=msdk35_1
JAVABUILDDIR=./release/$(MMDD)/java/$(ARCH)

## COMMON ####
DOCSDIR=./docs

### C #####
CBUILDDIR=./release/$(MMDD)/C/$(ARCH)
CINCLUDES=./C/include
CPROPOP3=./C/protocol/POP3/include
CPROSMTP=./C/protocol/SMTP/include
CPROIMAP4=./C/protocol/IMAP4/include
CPROMIME=./C/protocol/MIME/include
CABSINCLUDES=./C/abstract/include
CHIINCLUDES=./C/highlevel/include
### JAVA #######
JAVADIR=./java/built/$(ARCH)
IMAGEDIR=./images
JAVAMIMESRC=./java/protocol/MIME/src
JAVASMTPSRC=./java/protocol/SMTP/src
JAVAPOP3SRC=./java/protocol/POP3/src
JAVAIMAP4SRC=./java/protocol/IMAP4/src
JAVAHISRC=./java/highlevel/src
JAVAMIMEFILES=Header.java MIMEBasicPart.java MIMEBodyPart.java MIMEDataSink.java \
		MIMEDynamicParser.java MIMEException.java MIMEHelper.java \
		MIMEMessage.java MIMEMessagePart.java MIMEMultiPart.java \
		MIMEParser.java fileMIMEType.java
JAVAHIFILES=IMException.java IMTransport.java IMAttachment.java

ifeq ($(ARCH), WINNT)
MKDIR=mkdir
TYPE=.zip
ZIP = jar -cvf
COMPTYPE=.exe
else
MKDIR=mkdir -p
TYPE=.tar
ZIP = zip -r
COMPTYPE=.tar.gz
endif

LIBTYPE=.a
DLLTYPE=.so

ifeq ($(ARCH), WINNT)
LIBTYPE=.lib
DLLTYPE=.dll
endif

ifeq ($(ARCH), AIX)
LIBTYPE=.a
DLLTYPE=.a
endif

ifeq ($(ARCH), HPUX)
DLLTYPE=.sl
endif

all: 
	@echo ""
	@echo "Usage: gmake releaseJava "
	@echo "Usage: gmake buildJava "
	@echo ""
	@echo "You can specify date for the package"
	@echo "by setting MMDD=<some date> on the gmake line"
	@echo "although it is not necessary"

buildC:
	cd C/src; $(MAKE) $(MFLAGS)
	cd C/protocol/POP3/src; $(MAKE) $(MFLAGS)
	cd C/protocol/SMTP/src; $(MAKE) $(MFLAGS)
	cd C/protocol/IMAP4/src; $(MAKE) $(MFLAGS)
	cd C/protocol/MIME/src; $(MAKE) $(MFLAGS)
#cd C/highlevel/src; $(MAKE) $(MFLAGS)

packageC: FRESH DIRS_LIB DIRS_INCLUDE \
	DIRS_EXAMPLES  \
	PROTOCOL_INCLUDE \
	ROOT_INCLUDE \
	CDOCS 
	cp $(COMMON_OBJDIR)/lib/*$(DLLTYPE)  $(NS_RELEASE)/lib
ifeq ($(ARCH), AIX)
	cp $(COMMON_OBJDIR)/lib/*$(LIBTYPE)  $(NS_RELEASE)/lib
endif
ifeq ($(ARCH), WINNT)
	cp $(COMMON_OBJDIR)/lib/*$(LIBTYPE)  $(NS_RELEASE)/lib
endif

ifeq ($(ARCH), WINNT)
	cd $(NS_RELEASE); winzip32 -a -r -p $(C_EXE)
	mv $(NS_RELEASE)/$(C_EXE).zip $(CBUILDDIR)
ifdef BUILD_SHIP
	cp $(NS_RELEASE)/$(C_EXE)$(COMPTYPE) $(BUILD_SHIP) 
	chmod g+w $(BUILD_SHIP) 
endif
else
	cd $(NS_RELEASE); tar cvf ../$(NS_BUILD_FLAVOR)$(TYPE) .  
	gzip -f $(NS_RELEASE)/../$(NS_BUILD_FLAVOR)$(TYPE) ;
	mv $(NS_RELEASE)/../$(NS_BUILD_FLAVOR)$(COMPTYPE) $(CBUILDDIR)
ifdef BUILD_SHIP
	cp $(CBUILDDIR)/$(NS_BUILD_FLAVOR)$(COMPTYPE) $(BUILD_SHIP) 
endif
endif

buildJava:
	cd java/protocol; $(MAKE) $(MFLAGS) 
	cd java/highlevel/src; $(MAKE) $(MFLAGS) 

packageJava: FRESH_JAVA \
	JAVA_DOCS \
	JAVA_EXAMPLES
	cd $(JAVADIR)/protocol; $(ZIP) ../proapi.jar * 
	cd $(JAVADIR)/highlevel; $(ZIP) ../coapi.jar * 
	cp $(JAVADIR)/proapi.jar $(JAVABUILDDIR)/packages
	cp $(JAVADIR)/coapi.jar $(JAVABUILDDIR)/packages
ifeq ($(ARCH), WINNT)
	cd $(JAVABUILDDIR); winzip32 -a -r -p ../../../../$(JAVA_EXE) 
	mv $(JAVA_EXE).zip $(JAVABUILDDIR)
#       Taken out. Add back if you want winzipse 
#	cmd /c 'echo $(JAVA_EXE).zip -d C:\Netscape\MessagingJavaSDK -y -win32 > import'
#	winzipse @import
#	rm -f $(JAVA_EXE).zip
#	rm -f import 
#	mv $(JAVA_EXE)$(COMPTYPE) $(JAVABUILDDIR)
else
	cd $(JAVABUILDDIR); tar cvf ../$(JAVA_EXE)$(TYPE) . 
	cd $(JAVABUILDDIR); mv ../$(JAVA_EXE)$(TYPE) . 
	cd $(JAVABUILDDIR); gzip -f  ./$(JAVA_EXE)$(TYPE) 
endif
ifdef BUILD_SHIP
	mv $(JAVABUILDDIR)/../$(JAVA_EXE)$(COMPTYPE)  $(BUILD_SHIP)
endif
	

test:
	cmd /c 'echo $(C_EXE) -d C:\\Netscape\MessagingSDK -y -win32 > import'
	echo $(NS_RELEASE)
	echo $(INSTALL)
	echo $(MKDIR) $(RM)
	ls $(JAVADIR)
	ls $(JAVABUILDDIR)

### ACTUAL BUILD / PACKAGE RULES FOR JAVA SDK ####
FRESH_JAVA:
	rm -rf $(JAVABUILDDIR)
	mkdir -p $(JAVABUILDDIR)/packages
	mkdir -p $(JAVABUILDDIR)/examples
	mkdir -p $(JAVABUILDDIR)/examples/example1
	mkdir -p $(JAVABUILDDIR)/examples/MIME/example2
	mkdir -p $(JAVABUILDDIR)/examples/MIME/example3
	mkdir -p $(JAVABUILDDIR)/examples/MIME/example4
	mkdir -p $(JAVABUILDDIR)/examples/MIME/example5
	mkdir -p $(JAVABUILDDIR)/examples/MIME/example6
	mkdir -p $(JAVABUILDDIR)/examples/SMTP
	mkdir -p $(JAVABUILDDIR)/examples/SMTP/advanced
	mkdir -p $(JAVABUILDDIR)/examples/POP3
	mkdir -p $(JAVABUILDDIR)/examples/IMAP4
	mkdir -p $(JAVABUILDDIR)/examples/IMAP4/advanced
	mkdir -p $(JAVABUILDDIR)/examples/ConvenienceAPI
	mkdir -p $(JAVABUILDDIR)/doc
	mkdir -p $(JAVABUILDDIR)/doc/JavaDocs
	mkdir -p $(JAVABUILDDIR)/doc/JavaDocs/MIME/images
	mkdir -p $(JAVABUILDDIR)/doc/JavaDocs/SMTP/images
	mkdir -p $(JAVABUILDDIR)/doc/JavaDocs/POP3/images
	mkdir -p $(JAVABUILDDIR)/doc/JavaDocs/IMAP4/images
	mkdir -p $(JAVABUILDDIR)/doc/JavaDocs/ConvenienceAPI/images
#mkdir -p $(JAVABUILDDIR)/classes
#mkdir -p $(JAVABUILDDIR)/doc/DevGuide

JAVA_DOCS: \
	GEN_JAVADOCS \
	$(JAVABUILDDIR)/ReadMeJ.htm \
	$(JAVABUILDDIR)/readme.gif

#	$(JAVABUILDDIR)/readme.txt \
#	$(JAVABUILDDIR)/license.txt \
#	$(JAVABUILDDIR)/relnotes.htm 
	
GEN_JAVADOCS:
	cd $(JAVAMIMESRC); javadoc -public -d ../../../../$(JAVABUILDDIR)/doc/JavaDocs/MIME $(JAVAMIMEFILES)
	cp $(IMAGEDIR)/*.gif $(JAVABUILDDIR)/doc/JavaDocs/MIME/images/
	cd $(JAVASMTPSRC); javadoc -public -d ../../../../$(JAVABUILDDIR)/doc/JavaDocs/SMTP *.java
	cp $(IMAGEDIR)/*.gif $(JAVABUILDDIR)/doc/JavaDocs/SMTP/images/
	cd $(JAVAPOP3SRC); javadoc -public -d ../../../../$(JAVABUILDDIR)/doc/JavaDocs/POP3 *.java
	cp $(IMAGEDIR)/*.gif $(JAVABUILDDIR)/doc/JavaDocs/POP3/images/
	cd $(JAVAIMAP4SRC); javadoc -public -d ../../../../$(JAVABUILDDIR)/doc/JavaDocs/IMAP4 *.java
	cp $(IMAGEDIR)/*.gif $(JAVABUILDDIR)/doc/JavaDocs/IMAP4/images/
###	cd $(JAVAHISRC); javadoc -public -d ../../../$(JAVABUILDDIR)/doc/JavaDocs/ConvenienceAPI $(JAVAHIFILES)
	cd $(JAVAHISRC);  $(MAKE) GEN_JAVADOCS
	cp $(IMAGEDIR)/*.gif $(JAVABUILDDIR)/doc/JavaDocs/ConvenienceAPI/images/

$(JAVABUILDDIR)/ReadMeJ.htm: $(DOCSDIR)/ReadMeJ.htm
	cp $< $@
$(JAVABUILDDIR)/readme.gif: $(DOCSDIR)/readme.gif
	cp $< $@
$(JAVABUILDDIR)/readme.txt: $(DOCSDIR)/javareadme.txt
	cp $< $@
$(JAVABUILDDIR)/license.txt: $(DOCSDIR)/license.txt
	cp $< $@
$(JAVABUILDDIR)/relnotes.htm: $(DOCSDIR)/javarelnotes.htm
	cp $< $@

### ACTUAL BUILD / PACKAGE RULES FOR C SDK ####

FRESH:
	rm -rf $(NS_RELEASE)
	$(MKDIR) $(CBUILDDIR)
CDOCS: \
	$(NS_RELEASE)/ReadMeC.htm \
	$(NS_RELEASE)/readme.gif

#	$(NS_RELEASE)/readme.txt \
#	$(NS_RELEASE)/license.txt \
#	$(NS_RELEASE)/relnotes.htm 

DIRS_LIB:
	$(MKDIR) $(NS_RELEASE)/lib
DIRS_INCLUDE:
	$(MKDIR) $(NS_RELEASE)/include
	$(MKDIR) $(NS_RELEASE)/include/protocol
#	$(MKDIR) $(NS_RELEASE)/include/highlevel
#$(MKDIR) $(NS_RELEASE)/include/abstract
#DIRS_DOCS:
#	$(MKDIR) $(NS_RELEASE)/docs
#	$(MKDIR) $(NS_RELEASE)/docs/devGuide
DIRS_EXAMPLES:
	$(MKDIR) $(NS_RELEASE)/examples
	$(MKDIR) $(NS_RELEASE)/examples/MIME
	$(MKDIR) $(NS_RELEASE)/examples/POP3
	$(MKDIR) $(NS_RELEASE)/examples/SMTP
	$(MKDIR) $(NS_RELEASE)/examples/SMTP/advanced
	$(MKDIR) $(NS_RELEASE)/examples/IMAP4
	$(MKDIR) $(NS_RELEASE)/examples/IMAP4/advanced
ifeq ($(ARCH), WINNT)
	cp ./C/protocol/MIME/test/testMessage.c $(NS_RELEASE)/examples/MIME/
	cp ./C/protocol/MIME/test/multiPartmsgPart.c $(NS_RELEASE)/examples/MIME/
	cp ./C/protocol/MIME/test/testPasrePlus.c $(NS_RELEASE)/examples/MIME/
	cp ./C/protocol/MIME/test/ReadMe.txt  $(NS_RELEASE)/examples/MIME/
	cp ./C/protocol/MIME/test/dynamic/* $(NS_RELEASE)/examples/MIME/
endif
ifneq ($(ARCH), WINNT)
	cp ./C/protocol/MIME/test/*.c $(NS_RELEASE)/examples/MIME/
	cp ./C/protocol/MIME/test/ReadMe.txt $(NS_RELEASE)/examples/MIME/
	cp ./C/protocol/MIME/test/dynamic/*.[ch] $(NS_RELEASE)/examples/MIME/
endif
	cp ./C/protocol/SMTP/test/smtptest.c $(NS_RELEASE)/examples/SMTP/
	cp ./C/protocol/SMTP/test/testsink.c $(NS_RELEASE)/examples/SMTP/
	cp ./C/protocol/SMTP/test/testsink.h $(NS_RELEASE)/examples/SMTP/
	cp ./C/advancedExamples/SMTPAdvanced_1/include/testsink.h $(NS_RELEASE)/examples/SMTP/advanced
	cp ./C/advancedExamples/SMTPAdvanced_1/src/testsink.c $(NS_RELEASE)/examples/SMTP/advanced
	cp ./C/advancedExamples/SMTPAdvanced_1/src/smtptest.c $(NS_RELEASE)/examples/SMTP/advanced
	cp ./C/protocol/POP3/test/pop3test.c $(NS_RELEASE)/examples/POP3/
	cp ./C/protocol/POP3/test/testsink.c $(NS_RELEASE)/examples/POP3/
	cp ./C/protocol/POP3/test/testsink.h $(NS_RELEASE)/examples/POP3/
	cp ./C/protocol/IMAP4/test/*.c       $(NS_RELEASE)/examples/IMAP4/
	cp ./C/protocol/IMAP4/test/*.h       $(NS_RELEASE)/examples/IMAP4/
	cp ./C/protocol/IMAP4/test/*.out     $(NS_RELEASE)/examples/IMAP4/
	cp ./C/advancedExamples/IMAP4Advanced_1/include/testsink.h $(NS_RELEASE)/examples/IMAP4/advanced
	cp ./C/advancedExamples/IMAP4Advanced_1/src/testsink.c $(NS_RELEASE)/examples/IMAP4/advanced
	cp ./C/advancedExamples/IMAP4Advanced_1/src/imaptest.c $(NS_RELEASE)/examples/IMAP4/advanced

JAVA_EXAMPLES: 
	cp java/protocol/examples/example1/*.java $(JAVABUILDDIR)/examples/example1
	cp java/protocol/examples/example1/ReadMe.txt $(JAVABUILDDIR)/examples/example1
	cp java/protocol/examples/example2/*.java $(JAVABUILDDIR)/examples/MIME/example2
	cp java/protocol/examples/example2/ReadMe.txt $(JAVABUILDDIR)/examples/MIME/example2
	cp java/protocol/MIME/testapp/*.java $(JAVABUILDDIR)/examples/MIME/example3
	cp java/protocol/MIME/testapp/ReadMe.txt $(JAVABUILDDIR)/examples/MIME/example3
	cp java/protocol/MIME/examples/msg_extern_body/*.java $(JAVABUILDDIR)/examples/MIME/example4
	cp java/protocol/MIME/examples/msg_extern_body/ReadMe.txt $(JAVABUILDDIR)/examples/MIME/example4
	cp java/protocol/MIME/examples/multi_part/*.java $(JAVABUILDDIR)/examples/MIME/example5
	cp java/protocol/MIME/examples/multi_part/ReadMe.txt $(JAVABUILDDIR)/examples/MIME/example5
	cp java/protocol/MIME/examples/message_part/*.java $(JAVABUILDDIR)/examples/MIME/example6
	cp java/protocol/MIME/examples/message_part/ReadMe.txt $(JAVABUILDDIR)/examples/MIME/example6
	cp java/protocol/SMTP/test/*.java       $(JAVABUILDDIR)/examples/SMTP/
	cp java/advancedExamples/SMTPAdvanced_1/src/*.java       $(JAVABUILDDIR)/examples/SMTP/advanced
	cp java/protocol/POP3/test/*.java       $(JAVABUILDDIR)/examples/POP3/
	cp java/protocol/IMAP4/test/*.java      $(JAVABUILDDIR)/examples/IMAP4/
	cp java/protocol/IMAP4/test/*.out       $(JAVABUILDDIR)/examples/IMAP4/
	cp java/advancedExamples/IMAP4Advanced_1/src/*.java       $(JAVABUILDDIR)/examples/IMAP4/advanced
	cp java/highlevel/examples/*.java $(JAVABUILDDIR)/examples/ConvenienceAPI
	cp java/highlevel/examples/ReadMe.txt $(JAVABUILDDIR)/examples/ConvenienceAPI

PROTOCOL_INCLUDE: \
	$(NS_RELEASE)/include/protocol/mime.h \
	$(NS_RELEASE)/include/protocol/mimeparser.h \
	$(NS_RELEASE)/include/protocol/imap4.h \
	$(NS_RELEASE)/include/protocol/pop3.h \
	$(NS_RELEASE)/include/protocol/smtp.h 

ABSTRACT_INCLUDE:
	cp $(CABSINCLUDES)/*.h $(NS_RELEASE)/include/abstract 

HI_INCLUDE:
	cp $(CHIINCLUDES)/*.h $(NS_RELEASE)/include/highlevel 

ROOT_INCLUDE: \
	$(NS_RELEASE)/include/nsmail.h \
	$(NS_RELEASE)/include/nsStream.h

$(NS_RELEASE)/include/protocol/mime.h: $(CPROMIME)/mime.h
	cp $< $@  
$(NS_RELEASE)/include/protocol/mimeparser.h: $(CPROMIME)/mimeparser.h
	cp $< $@  
$(NS_RELEASE)/include/protocol/imap4.h: $(CPROIMAP4)/imap4.h
	cp $< $@  
$(NS_RELEASE)/include/protocol/pop3.h: $(CPROPOP3)/pop3.h
	cp $< $@  
$(NS_RELEASE)/include/protocol/smtp.h: $(CPROSMTP)/smtp.h
	cp $< $@ 
$(NS_RELEASE)/include/nsmail.h: $(CINCLUDES)/nsmail.h  
	cp $< $@
$(NS_RELEASE)/include/nsStream.h: $(CINCLUDES)/nsStream.h  
	cp $< $@

$(NS_RELEASE)/ReadMeC.htm: $(DOCSDIR)/ReadMeC.htm
	cp $< $@
$(NS_RELEASE)/readme.gif: $(DOCSDIR)/readme.gif
	cp $< $@
$(NS_RELEASE)/readme.txt: $(DOCSDIR)/creadme.txt
	cp $< $@
$(NS_RELEASE)/license.txt: $(DOCSDIR)/license.txt
	cp $< $@
$(NS_RELEASE)/relnotes.htm: $(DOCSDIR)/crelnotes.htm
	cp $< $@


 
