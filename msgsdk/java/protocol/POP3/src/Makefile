##########################################################################
# POP3 makefile.
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
RM = rm -f

ifeq ($(ARCH), WINNT)
	RM = del /q
endif

# files and directories
CLASSDIR = ../../../built/$(ARCH)/protocol

#CLASSPATH = .:$(CLASSDIR):$(JDKCLASSPATH)

SRCS = \
  POP3Exception.java \
  POP3ServerException.java \
  IPOP3Sink.java \
  POP3Sink.java \
  POP3Client.java

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
ifneq ($(ARCH), WINNT)
	cd ../../../built/$(ARCH)/netscape/messaging/pop3; \
	$(RM) $(OBJS) $(TARGET)
else
	rm -f $(CLASSDIR)/netscape/messaging/pop3/*.class
endif
#	cd ../../../built/$(ARCH)/SASL; \
#	$(RM) $(OBJS) $(TARGET)
