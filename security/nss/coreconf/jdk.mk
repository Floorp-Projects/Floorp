#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef NS_USE_JDK
#######################################################################
# [1] Define preliminary JDK "Core Components" toolset options        #
#######################################################################

# set default JDK java threading model
ifeq ($(JDK_THREADING_MODEL),)
	JDK_THREADING_MODEL     = native_threads
# no such thing as -native flag
	JDK_THREADING_MODEL_OPT =
endif

#######################################################################
# [2] Define platform-independent JDK "Core Components" options       #
#######################################################################

# set default location of the java classes repository
ifeq ($(JAVA_DESTPATH),)
ifdef BUILD_OPT
	JAVA_DESTPATH  = $(SOURCE_CLASSES_DIR)
else
	JAVA_DESTPATH  = $(SOURCE_CLASSES_DBG_DIR)
endif
endif

# set default location of the package under the java classes repository
# note that this overrides the default package value in ruleset.mk
ifeq ($(PACKAGE),)
	PACKAGE = .
endif

# set default location of the java source code repository
ifeq ($(JAVA_SOURCEPATH),)
	JAVA_SOURCEPATH	= .
endif

# add JNI directory to default include search path
ifneq ($(JNI_GEN),)
	ifdef NSBUILDROOT
		INCLUDES += -I$(JNI_GEN_DIR) -I$(SOURCE_XP_DIR)
	else
		INCLUDES += -I$(JNI_GEN_DIR)
	endif
endif

#######################################################################
# [3] Define platform-dependent JDK "Core Components" options         #
#######################################################################

# set [Microsoft Windows] platforms
ifeq ($(OS_ARCH), WINNT)
	JAVA_CLASSES = $(JAVA_HOME)/jre/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/rt.jar
		endif
	endif

	PATH_SEPARATOR = ;

	# (2) specify "header" information
	JAVA_ARCH = win32

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	#     currently, disable JIT option on this platform
	JDK_JIT_OPT = -nojit
endif

# set [Sun Solaris] platforms
ifeq ($(OS_ARCH), SunOS)
	JAVA_CLASSES = $(JAVA_HOME)/jre/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/rt.jar
		endif
	endif

	PATH_SEPARATOR = :

	# (2) specify "header" information
	JAVA_ARCH = solaris

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	#     currently, disable JIT option on this platform
	JDK_JIT_OPT =
endif

# set [Hewlett Packard HP-UX] platforms
ifeq ($(OS_ARCH), HP-UX)
	JAVA_CLASSES = $(JAVA_HOME)/jre/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/rt.jar
		endif
	endif

	PATH_SEPARATOR = :

	# (2) specify "header" information
	JAVA_ARCH = hp-ux

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [Redhat Linux] platforms
ifeq ($(OS_ARCH), Linux)
	JAVA_CLASSES = $(JAVA_HOME)/jre/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/rt.jar
		endif
	endif

	PATH_SEPARATOR = :

	# (2) specify "header" information
	JAVA_ARCH = linux

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [Mac OS X] platforms
ifeq ($(OS_ARCH), Darwin)
	JAVA_CLASSES = $(JAVA_HOME)/../Classes/classes.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/../Classes/classes.jar
		endif
	endif

	PATH_SEPARATOR = :

	# (2) specify "header" information
	JAVA_ARCH = darwin

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [IBM AIX] platforms
ifeq ($(OS_ARCH), AIX)
	JAVA_CLASSES = $(JAVA_HOME)/jre/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/rt.jar
		endif
	endif

	PATH_SEPARATOR = :

	# (2) specify "header" information
	JAVA_ARCH = aix

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [Digital UNIX] platforms
ifeq ($(OS_ARCH), OSF1)
	JAVA_CLASSES = $(JAVA_HOME)/jre/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/rt.jar
		endif
	endif

	PATH_SEPARATOR = :

	# (2) specify "header" information
	JAVA_ARCH = alpha

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [Silicon Graphics IRIX] platforms
ifeq ($(OS_ARCH), IRIX)
	JAVA_CLASSES = $(JAVA_HOME)/lib/dev.jar:$(JAVA_HOME)/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/dev.jar:$(JRE_HOME)/lib/rt.jar
		endif
	endif

	PATH_SEPARATOR = :

	# (2) specify "header" information
	JAVA_ARCH = irix

	INCLUDES += -I$(JAVA_HOME)/include
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

#######################################################################
# [4] Define remaining JDK "Core Components" default toolset options  #
#######################################################################

# set JDK optimization model
ifeq ($(BUILD_OPT),1)
	JDK_OPTIMIZER_OPT = -O
else
	JDK_OPTIMIZER_OPT = -g
endif

# set minimal JDK debugging model
ifeq ($(JDK_DEBUG),1)
	JDK_DEBUG_OPT    = -debug
else
	JDK_DEBUG_OPT    =
endif

# set default path to repository for JDK classes
ifeq ($(JDK_CLASS_REPOSITORY_OPT),)
	JDK_CLASS_REPOSITORY_OPT = -d $(JAVA_DESTPATH)
endif

# define a default JDK classpath
ifeq ($(JDK_CLASSPATH),)
	JDK_CLASSPATH = '$(JAVA_DESTPATH)$(PATH_SEPARATOR)$(JAVA_SOURCEPATH)$(PATH_SEPARATOR)$(JAVA_CLASSES)'
endif

# by default, override CLASSPATH environment variable using the JDK classpath option with $(JDK_CLASSPATH)
ifeq ($(JDK_CLASSPATH_OPT),)
	JDK_CLASSPATH_OPT = -classpath $(JDK_CLASSPATH)
endif

ifeq ($(USE_64), 1)
	JDK_USE_64 = -d64
endif

endif


#######################################################################
# [5] Define JDK "Core Components" toolset;                           #
#     (always allow a user to override these values)                  #
#######################################################################

#
# (1) appletviewer
#

ifeq ($(APPLETVIEWER),)
	APPLETVIEWER_PROG   = $(JAVA_HOME)/bin/appletviewer$(PROG_SUFFIX)
	APPLETVIEWER_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	APPLETVIEWER_FLAGS += $(JDK_DEBUG_OPT)
	APPLETVIEWER_FLAGS += $(JDK_JIT_OPT)
	APPLETVIEWER        = $(APPLETVIEWER_PROG) $(APPLETVIEWER_FLAGS)
endif

#
# (2) jar
#

ifeq ($(JAR),)
	JAR_PROG   = $(JAVA_HOME)/bin/jar$(PROG_SUFFIX)
	JAR_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAR        = $(JAR_PROG) $(JAR_FLAGS)
endif

#
# (3) java
#

ifeq ($(JAVA),)
	JAVA_PROG   = $(JAVA_HOME)/bin/java$(PROG_SUFFIX)
	JAVA_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVA_FLAGS += $(JDK_DEBUG_OPT)
	JAVA_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVA_FLAGS += $(JDK_JIT_OPT)
	JAVA_FLAGS += $(JDK_USE_64)
	JAVA        = $(JAVA_PROG) $(JAVA_FLAGS) 
endif

#
# (4) javac
#

ifeq ($(JAVAC),)
	JAVAC_PROG   = $(JAVA_HOME)/bin/javac$(PROG_SUFFIX)
	JAVAC_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAC_FLAGS += $(JDK_OPTIMIZER_OPT)
	JAVAC_FLAGS += $(JDK_DEBUG_OPT)
	JAVAC_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVAC_FLAGS += $(JDK_CLASS_REPOSITORY_OPT)
	JAVAC_FLAGS += $(JDK_USE_64)
	JAVAC        = $(JAVAC_PROG) $(JAVAC_FLAGS)
endif

#
# (5) javadoc
#

ifeq ($(JAVADOC),)
	JAVADOC_PROG   = $(JAVA_HOME)/bin/javadoc$(PROG_SUFFIX)
	JAVADOC_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVADOC_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVADOC        = $(JAVADOC_PROG) $(JAVADOC_FLAGS)
endif

#
# (6) javah
#

ifeq ($(JAVAH),)
	JAVAH_PROG   = $(JAVA_HOME)/bin/javah$(PROG_SUFFIX)
	JAVAH_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAH_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVAH        = $(JAVAH_PROG) $(JAVAH_FLAGS)
endif

#
# (7) javakey
#

ifeq ($(JAVAKEY),)
	JAVAKEY_PROG   = $(JAVA_HOME)/bin/javakey$(PROG_SUFFIX)
	JAVAKEY_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAKEY        = $(JAVAKEY_PROG) $(JAVAKEY_FLAGS)
endif

#
# (8) javap
#

ifeq ($(JAVAP),)
	JAVAP_PROG   = $(JAVA_HOME)/bin/javap$(PROG_SUFFIX)
	JAVAP_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAP_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVAP        = $(JAVAP_PROG) $(JAVAP_FLAGS)
endif

#
# (9) javat
#

ifeq ($(JAVAT),)
	JAVAT_PROG   = $(JAVA_HOME)/bin/javat$(PROG_SUFFIX)
	JAVAT_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAT        = $(JAVAT_PROG) $(JAVAT_FLAGS)
endif

#
# (10) javaverify
#

ifeq ($(JAVAVERIFY),)
	JAVAVERIFY_PROG   = $(JAVA_HOME)/bin/javaverify$(PROG_SUFFIX)
	JAVAVERIFY_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAVERIFY        = $(JAVAVERIFY_PROG) $(JAVAVERIFY_FLAGS)
endif

#
# (11) javaw
#

ifeq ($(JAVAW),)
	jJAVAW_PROG   = $(JAVA_HOME)/bin/javaw$(PROG_SUFFIX)
	jJAVAW_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	jJAVAW_FLAGS += $(JDK_DEBUG_OPT)
	jJAVAW_FLAGS += $(JDK_CLASSPATH_OPT)
	jJAVAW_FLAGS += $(JDK_JIT_OPT)
	jJAVAW        = $(JAVAW_PROG) $(JAVAW_FLAGS)
endif

#
# (12) jdb
#

ifeq ($(JDB),)
	JDB_PROG   = $(JAVA_HOME)/bin/jdb$(PROG_SUFFIX)
	JDB_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JDB_FLAGS += $(JDK_DEBUG_OPT)
	JDB_FLAGS += $(JDK_CLASSPATH_OPT)
	JDB_FLAGS += $(JDK_JIT_OPT)
	JDB        = $(JDB_PROG) $(JDB_FLAGS)
endif

#
# (13) jre
#

ifeq ($(JRE),)
	JRE_PROG   = $(JAVA_HOME)/bin/jre$(PROG_SUFFIX)
	JRE_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JRE_FLAGS += $(JDK_CLASSPATH_OPT)
	JRE_FLAGS += $(JDK_JIT_OPT)
	JRE        = $(JRE_PROG) $(JRE_FLAGS) 
endif

#
# (14) jrew
#

ifeq ($(JREW),)
	JREW_PROG   = $(JAVA_HOME)/bin/jrew$(PROG_SUFFIX)
	JREW_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JREW_FLAGS += $(JDK_CLASSPATH_OPT)
	JREW_FLAGS += $(JDK_JIT_OPT)
	JREW        = $(JREW_PROG) $(JREW_FLAGS) 
endif

#
# (15) native2ascii
#

ifeq ($(NATIVE2ASCII),)
	NATIVE2ASCII_PROG   = $(JAVA_HOME)/bin/native2ascii$(PROG_SUFFIX)
	NATIVE2ASCII_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	NATIVE2ASCII        = $(NATIVE2ASCII_PROG) $(NATIVE2ASCII_FLAGS) 
endif

#
# (16) rmic
#

ifeq ($(RMIC),)
	RMIC_PROG   = $(JAVA_HOME)/bin/rmic$(PROG_SUFFIX)
	RMIC_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	RMIC_FLAGS += $(JDK_OPTIMIZER_OPT)
	RMIC_FLAGS += $(JDK_CLASSPATH_OPT)
	RMIC        = $(RMIC_PROG) $(RMIC_FLAGS) 
endif

#
# (17) rmiregistry
#

ifeq ($(RMIREGISTRY),)
	RMIREGISTRY_PROG   = $(JAVA_HOME)/bin/rmiregistry$(PROG_SUFFIX)
	RMIREGISTRY_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	RMIREGISTRY        = $(RMIREGISTRY_PROG) $(RMIREGISTRY_FLAGS) 
endif

#
# (18) serialver
#

ifeq ($(SERIALVER),)
	SERIALVER_PROG   = $(JAVA_HOME)/bin/serialver$(PROG_SUFFIX)
	SERIALVER_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	SERIALVER        = $(SERIALVER_PROG) $(SERIALVER_FLAGS) 
endif
