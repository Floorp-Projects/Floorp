##########################################################################
# IMAP4 makefile.
# created 9/9/97 -- Prasad
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

# java flags
DEBUGJAVAFLAG =
OPTJAVAFLAG = -d $(CLASSDIR) 
JAVAFLAGS = $(OTHERJAVAFLAGS) $(OPTJAVAFLAG) $(DEBUGJAVAFLAG)

# files and directories
CLASSDIR = ../../../built/$(ARCH)/protocol

#CLASSPATH = .:$(CLASSDIR):$(JDKCLASSPATH)

SRCS = \
  IMAP4Exception.java \
  IMAP4ServerException.java \
  IGlobals.java \
  IIMAP4Sink.java \
  IMAP4Sink.java \
  SystemPreferences.java \
  Dispatcher.java \
  IMAP4Client.java

OBJS = ${SRCS:.java=.class}

TARGET = package

.SUFFIXES:      .java .class

all: $(CLASSDIR) $(TARGET) 

install: $(TARGET)
	foreach f ( $(OBJS) ) \
		mv -f $$f $(CLASSDIR)/$$f \
	end

$(TARGET): $(OBJS)
$(CLASSDIR): 
	echo mkdir $(CLASSDIR)
	- mkdir -p $(CLASSDIR)

.java.class: $(SRCS)
	$(JAVAC) $(JAVAFLAGS) $<

clean:
	echo $(ARCH)
	rm -f ../../../built/$(ARCH)/protocol/netscape/messaging/imap4/*.class

