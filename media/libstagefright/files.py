#!/usr/bin/python

import os
import sys

DEFINES={}
CONFIG={}
CXXFLAGS=[]
CONFIG['_MSC_VER'] = 0
CONFIG['OS_TARGET'] = 0

class Exports(object):
    def __init__(self):
        self.mp4_demuxer=[]

EXPORTS=Exports()

SOURCES=[]
UNIFIED_SOURCES=[]
LOCAL_INCLUDES=[]

try:
    execfile('moz.build')
except:
    sys.exit(1)

for f in SOURCES+UNIFIED_SOURCES:
    if not f.startswith('binding/'):
        print f
