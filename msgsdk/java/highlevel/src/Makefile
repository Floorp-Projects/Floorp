##########################################################################
# HighLevel API makefile.
# created 12/15/97 -- Prasad
#
############################################################################
# environment
#SHELL = /usr/bin/ksh

# commands
JAVAC = javac
ARCH = $(shell uname -s)

ifeq ($(ARCH), SunOS)
        ARCH = SOLARIS
endif

ifeq ($(ARCH), HP-UX)
        ARCH = HPUX
endif

ifndef MMDD
MMDD = $(shell date +%m.%d)
endif

JAVABUILDDIR=release/$(MMDD)/java/$(ARCH)
JAVAHIFILES=IMException.java IMTransport.java IMAttachment.java

# java flags
DEBUGJAVAFLAG =
OPTJAVAFLAG = -d $(CLASSDIR) 
JAVAFLAGS = $(OTHERJAVAFLAGS) $(OPTJAVAFLAG) $(DEBUGJAVAFLAG)
RM = rm -f

# files and directories
#CLASSDIR = ./built
CLASSDIR = ../../built/$(ARCH)/highlevel
PROCLASSDIR = ../../built/$(ARCH)/protocol

#CLASSPATH = .:$(CLASSDIR):$(JDKCLASSPATH)
CLASSPATH = .:$(PROCLASSDIR):$(CLASSDIR)

SRCS = \
   IMException.java       \
   IMSMTPSink.java       \
   IMAttachment.java     \
   IMTransport.java \
   IMSearchTerm.java  \
   MMessageSummary.java \
   IMStore.java


OBJS = ${SRCS:.java=.class}

TARGET = package

.SUFFIXES:      .java .class

all: $(CLASSDIR) $(TARGET) 

install: $(TARGET)
	foreach f ( $(OBJS) ) \
		mv -f $$f $(CLASSDIR)/$$f \
	end

#$(TARGET): $(OBJS)
$(TARGET): 
	echo $(CLASSPATH)
	echo $(CLASSDIR)
	$(JAVAC) $(JAVAFLAGS) *.java
$(CLASSDIR): 
	echo mkdir $(CLASSDIR)
	- mkdir -p $(CLASSDIR)

GEN_JAVADOCS:
	echo $(CLASSPATH)
	javadoc -public -d ../../../$(JAVABUILDDIR)/doc/JavaDocs/ConvenienceAPI $(JAVAHIFILES)

#$(OBJS):
#	$(JAVAC) $(JAVAFLAGS) *.java
#.java.class: $(SRCS)
#	$(JAVAC) $(JAVAFLAGS) $<

clean:
	rm -f $(CLASSDIR)/netscape/messaging/highlevel/*.class
