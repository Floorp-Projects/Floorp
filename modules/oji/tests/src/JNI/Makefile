# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s): 
DIRS =  \
	getjni \
	AccessingFields \
	AccessingStaticFields \
	ArrayOperations \
	CallingInstanceMethods \
	CallingStaticMethods \
	ClassOperation \
	Exceptions \
	GlobalAndLocalRefs \
	JavaVMInterface \
	ObjectOperations \
	StringOperations \
	VersionInformation



TOP_DIR = ../..

OBJ_DIR = obj

JAVA =  java/Test1.java \
        java/Test2.java \
        java/Test3.java \
        java/Test4.java \
        java/Test5.java \
        java/Test6.java \
        java/Test7.java \
        java/Test8.java \
        java/Test9.java \
        java/Test10.java \
        java/Test11.java 

#here we say to link all obj files in OBJ_DIR directory
OBJS = $(OBJ_DIR)/*.o

DLL = ojiapijnitests

include ../../build/rules.mk

single_objdir:
	@if test ! -d $(OBJ_DIR); then mkdir $(OBJ_DIR); fi

$(TOP_DIR)/build/classes/test.jar: $(JAVA)
	@javac $? -d $(TOP_DIR)/build/classes
	@cd $(TOP_DIR)/build/classes; jar cvf $(TOP_DIR)/build/classes/test.jar *.class; cd $(TOP_DIR)/build

classes: $(TOP_DIR)/build/classes/test.jar

all: single_objdir $(DIRS) classes link 

$(DIRS)::
	@cd $@; \
		if test ! -f depend.mak; then >depend.mak; fi; \
		$(MAKE) make_depends; \
		$(MAKE) all;
