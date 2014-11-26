#!/usr/bin/env python

from __future__ import print_function
import sys
import os.path

DEFAULT_OBJDIR = "/home/worker/object-folder"

gecko_dir = sys.argv[1]

base_path = os.path.join(gecko_dir, 'python')
sys.path.append(os.path.join(base_path, 'mozbuild'))
sys.path.append(os.path.join(base_path, 'mach'))
sys.path.append(os.path.join(gecko_dir, 'testing', 'mozbase', 'mozprocess'))

from mozbuild.mozconfig import MozconfigLoader

loader = MozconfigLoader(gecko_dir)
result = loader.read_mozconfig()

topobjdir = result['topobjdir']
print(topobjdir if topobjdir else DEFAULT_OBJDIR)
