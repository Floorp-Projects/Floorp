# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.


######################################################################
#
# This is a windows based makefile (currently) designed to pull
# and build all appropriate components and systems to generate
# a Trex distribution and products using such a distribution
#
######################################################################

DEPTH=.
IGNORE_MANIFEST=1

######################################################################
# General environment variables
######################################################################

!if !defined(MOZ_SRC)
MOZ_SRC=y:
!endif

!if !defined(MOZ_TOP)
MOZ_TOP=mozilla
!endif

CVS=cvs -q co -P
NMAKE=@nmake -nologo -$(MAKEFLAGS)
MMAKE=$(MOZ_SRC)\$(MOZ_TOP)\config\w95make.exe
GMAKE=gmake
GMAKEFILE=Makefile

CVTREX = $(CVSROOT:pub=src)
CVST=cvs -q -d $(CVTREX) co -P

######################################################################
# Branches and Modules - Trex Needs These
######################################################################

CONFIG_BRANCH           = 
CONFIG_DIR              = $(MOZ_TOP)/config

NSPR20_BRANCH           = 
NSPR20_DIR              = $(MOZ_TOP)/nsprpub

NETLIB_BRANCH           = -r MODULAR_NETLIB_BRANCH
NETLIB_DIR              = $(MOZ_TOP)/lib/libnet

IMGLIB_BRANCH           = -r MODULAR_IMGLIB_BRANCH
IMGLIB_DIR1             = $(MOZ_TOP)/jpeg
IMGLIB_DIR2             = $(MOZ_TOP)/modules/libutil
IMGLIB_DIR3             = $(MOZ_TOP)/modules/libimg

ZLIB_BRANCH             =
ZLIB_DIR                = $(MOZ_TOP)/modules/zlib

INCLUDE_BRANCH          = 
INCLUDE_DIR             = $(MOZ_TOP)/include
#
#LIBXP_BRANCH            = 
#LIBXP_DIR               = $(MOZ_TOP)/lib/xp

RAPTOR_XPCOM_BRANCH     = 
RAPTOR_XPCOM_DIR        = $(MOZ_TOP)/xpcom

RAPTOR_BASE_BRANCH      = 
RAPTOR_BASE_DIR         = $(MOZ_TOP)/base

RAPTOR_GFX_BRANCH       = 
RAPTOR_GFX_DIR          = $(MOZ_TOP)/gfx

RAPTOR_PARSER_BRANCH    = 
RAPTOR_PARSER_DIR       = $(MOZ_TOP)/htmlparser

RAPTOR_WIDGET_BRANCH    = 
RAPTOR_WIDGET_DIR       = $(MOZ_TOP)/widget

RAPTOR_VIEW_BRANCH      = 
RAPTOR_VIEW_DIR         = $(MOZ_TOP)/view

RAPTOR_LAYOUT_BRANCH    = 
RAPTOR_LAYOUT_DIR       = $(MOZ_TOP)/layout

TREX_GCONFIG_BRANCH     = 
TREX_GCONFIG_DIR        = $(MOZ_TOP)/gconfig

TREX_SHELL_BRANCH       = 
TREX_SHELL_DIR          = $(MOZ_TOP)/shell

TREX_TREX_BRANCH        = 
TREX_TREX_DIR           = ns/trex

LIBNLS_BRANCH           = -r libnls_v3_Normandy
LIBNLS_DIR              = ns/modules/libnls

JULIAN_BRANCH           = -r JULIAN_TREX_BRANCH
JULIAN_DIR              = ns/julian

# $(MOZ_TOP)/LICENSE
# $(MOZ_TOP)/LEGAL
# $(MOZ_TOP)/lib/liblayer
# $(MOZ_TOP)/modules/zlib
# $(MOZ_TOP)/modules/libutil
# $(MOZ_TOP)/sun-java
# $(MOZ_TOP)/nav-java
# $(MOZ_TOP)/js
# $(MOZ_TOP)/modules/security/freenav
# $(MOZ_TOP)/modules/libpref

######################################################################
# Modules - PLATFORM_DIRS are what Trex needs
######################################################################

PLATFORM_DIRS =		\
  nsprpub			\
  xpcom				\
  jpeg              \
  modules\zlib      \
  modules\libutil   \
  modules\libimg    \
  base              \
  htmlparser		\
  gfx				\
  view				\
  widget			

PLATFORM_EXPORT_DIRS = $(PLATFORM_DIRS) \
  lib\libnet        \
  layout			

TREX_DIRS =		\
  trex			

######################################################################
# Targets
######################################################################

default:: 		        build_all

pull_and_build_all::    pull_all    \
					    build_all

pull_clobber_build_all:: pull_all \
  						 clobber_all \
						 build_all

clobber_build_all:: 	clobber_all \
						build_all


#pull_all::
#    @echo +++ trex.mak: checking out trex with "$(CVS_BRANCH)"
#    cd $(MOZ_SRC)\.
#    -$(CVS) $(CONFIG_BRANCH)  $(CONFIG_DIR)
#    -$(CVS) $(INCLUDE_BRANCH) $(INCLUDE_DIR)
#    -$(CVS) $(NSPR20_BRANCH)  $(NSPR20_DIR)
#    -$(CVS) $(NETLIB_BRANCH)  $(NETLIB_DIR)
#    -$(CVS) $(IMGLIB_BRANCH)  $(IMGLIB_DIR1)
#    -$(CVS) $(IMGLIB_BRANCH)  $(IMGLIB_DIR2)
#    -$(CVS) $(IMGLIB_BRANCH)  $(IMGLIB_DIR3)
#    -$(CVS) $(ZLIB_BRANCH)    $(ZLIB_DIR)
#    -$(CVS) $(RAPTOR_XPCOM_BRANCH) $(RAPTOR_XPCOM_DIR)
#    -$(CVS) $(RAPTOR_BASE_BRANCH) $(RAPTOR_BASE_DIR)
#    -$(CVS) $(RAPTOR_GFX_BRANCH) $(RAPTOR_GFX_DIR)
#    -$(CVS) $(RAPTOR_PARSER_BRANCH) $(RAPTOR_PARSER_DIR)
#    -$(CVS) $(RAPTOR_WIDGET_BRANCH) $(RAPTOR_WIDGET_DIR)
#    -$(CVS) $(RAPTOR_VIEW_BRANCH) $(RAPTOR_VIEW_DIR)
#    -$(CVS) $(RAPTOR_LAYOUT_BRANCH) $(RAPTOR_LAYOUT_DIR)


pull_all:: pull_platform pull_julian pull_trex

pull_platform::
    @echo +++ trex.mak: checking out platform with "$(CVS_BRANCH)"
    cd $(MOZ_SRC)\.
    -$(CVS) $(CONFIG_BRANCH)  $(CONFIG_DIR)
    -$(CVS) $(MOZ_TOP)/nglayout.mak
    cd $(MOZ_SRC)\$(MOZ_TOP)
    nmake -f nglayout.mak pull_all
    cd $(MOZ_SRC)\.
    
pull_julian::
    @echo +++ trex.mak: checking out libnls with "$(CVTREX)"
    cd $(MOZ_SRC)\.
    -$(CVST) ns/client.mak
    cd $(MOZ_SRC)\ns\.
    -$(CVST) -d config ns/clientconfig
    cd $(MOZ_SRC)\.
    -$(CVST) $(JULIAN_BRANCH) $(JULIAN_DIR)
    -$(CVST) $(LIBNLS_BRANCH) $(LIBNLS_DIR)
    cd $(MOZ_SRC)\.

pull_trex::
    @echo +++ trex.mak: checking out trex with "$(CVTREX)"
    cd $(MOZ_SRC)\.
    -$(CVS)  $(TREX_GCONFIG_BRANCH)  $(TREX_GCONFIG_DIR)
    -$(CVS)  $(TREX_SHELL_BRANCH)    $(TREX_SHELL_DIR)
    cd $(MOZ_SRC)\$(MOZ_TOP)
    -$(CVST) $(TREX_TREX_BRANCH)  -d trex  $(TREX_TREX_DIR)
    cd $(MOZ_SRC)\.


build_all:: build_platform build_julian build_trex

# builds PLATFORM_DIRS
#build_platform:: 
#    cd $(MOZ_SRC)\$(MOZ_TOP)\.
#    @$(MMAKE) export  $(MAKEDIR) $(PLATFORM_EXPORT_DIRS)
#    @$(MMAKE) libs    $(MAKEDIR) $(PLATFORM_DIRS)
#    @$(MMAKE) install $(MAKEDIR) $(PLATFORM_DIRS)
build_platform:: 
    cd $(MOZ_SRC)\$(MOZ_TOP)\.
    nmake -f nglayout.mak all
    cd $(MOZ_SRC)\.

build_julian:: 
    cd $(MOZ_SRC)\ns\modules\libnls
    nmake -f makefile.win
    cd $(MOZ_SRC)\ns\julian
    nmake -f makefile.win
    cd $(MOZ_SRC)\.

build_trex:: 
    cd $(MOZ_SRC)\$(MOZ_TOP)\.
    @$(MMAKE) export  $(MAKEDIR) shell trex
    @$(MMAKE) libs    $(MAKEDIR) shell trex
    @$(MMAKE) install $(MAKEDIR) shell trex
    cd $(MOZ_SRC)\.


clobber_all::

