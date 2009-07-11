# profiledir needs to be an absolute path on Mac OS X (FIXME: file bug).
profiledir = $(abspath ../profile)
sys := $(shell uname -s)

# OS detection

ifeq ($(sys), Darwin)
  os = Darwin
else
ifeq ($(sys), Linux)
  os = Linux
else
ifeq ($(sys), MINGW32_NT-6.0)
  os = WINNT
else
ifeq ($(sys), MINGW32_NT-5.1)
  os = WINNT
else
  $(error Sorry, your os is unknown/unsupported: $(sys))
endif
endif
endif
endif

topsrcdir ?= ${TOPSRCDIR}
native_topsrcdir ?= ${NATIVE_TOPSRCDIR}

ifeq ($(topsrcdir),)
topsrcdir = ../..
endif

ifeq ($(native_topsrcdir),)
ifeq ($(os), WINNT)
native_topsrcdir = ..\..
else
native_topsrcdir = ../..
endif
endif

# The path to the extension, in the native format, as required by the extension
# manager when it installs an extension via a file in the <profile>/extensions/
# directory that contains the path to the extension.
ifeq ($(os), WINNT)
extensiondir = $(native_topsrcdir)\dist\stage
else
extensiondir = $(topsrcdir)/dist/stage
endif

# A command that installs the extension into the profile when the harness
# creates a new profile.

configure_profile = echo "$(extensiondir)" > $(profiledir)/extensions/\{340c2bbc-ce74-4362-90b5-7c26312808ef\};

include ../harness/Makefile
