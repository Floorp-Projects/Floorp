##########################################################################
# MIME makefile.
# created 9/26/97 -- Prasad
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
RM = rm -f

# files and directories
#CLASSDIR = ./built
CLASSDIR = ../../../built/$(ARCH)/protocol

#CLASSPATH = .:$(CLASSDIR):$(JDKCLASSPATH)

SRCS = \
  Header.java \
  fileMIMEType.java \
  MIMEHelper.java \
  MIMEException.java \
  MIMEBodyPart.java \
  MIMEBasicPart.java \
  MIMEMessage.java \
  MIMEMessagePart.java \
  MIMEMultiPart.java \
  MIMEDataSink.java \
  MIMEParser.java 

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
	$(JAVAC) $(JAVAFLAGS) *.java
$(CLASSDIR): 
	echo mkdir $(CLASSDIR)
	- mkdir -p $(CLASSDIR)

#$(OBJS):
#	$(JAVAC) $(JAVAFLAGS) *.java
#.java.class: $(SRCS)
#	$(JAVAC) $(JAVAFLAGS) $<

clean:
	rm -f $(CLASSDIR)/netscape/messaging/mime/*.class
	rm -f $(CLASSDIR)/netscape/mime/*.class
