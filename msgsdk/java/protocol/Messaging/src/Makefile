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
  IO.java \
  Common.java

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
	rm -f ../../../built/$(ARCH)/netscape/messaging/*.class

