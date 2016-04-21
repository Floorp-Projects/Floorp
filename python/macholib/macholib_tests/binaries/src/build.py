#!/usr/bin/env python

import os, sys


class Builder (object):
    def __init__(self, args):
        self.output_dir = args[1]


    def run(self):
        for nm in dir(type(self)):
            if nm.startswith('build_'):
                getattr(self, nm)()

    def build_executable(self):
        print "Building plain executable"
        pass


builder = Builder(sys.argv)
builder.run()
