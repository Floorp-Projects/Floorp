#!/usr/bin/env python
# encoding: utf-8
"""
PerfConfigurator.py

Created by Rob Campbell on 2007-03-02.
Modified by Rob Campbell on 2007-05-30
Modified by Rob Campbell on 2007-06-26 - added -i buildid option
Modified by Rob Campbell on 2007-07-06 - added -d testDate option
Modified by Rob Campbell on 2007-08-22 - fixed errors in main()
"""

import sys
import getopt
import re
import time
from datetime import datetime

executablePath = "C:\\cygwin\\tmp\\test\\"
configFilePath = "C:\\mozilla\\testing\\performance\\talos\\"
masterIniSubpath = 'firefox\\extensions\\talkback@mozilla.org\\components\\master.ini'
defaultTitle = "qm-pxp01"

help_message = '''
This is the buildbot performance runner's YAML configurator.bean

USAGE: python PerfConfigurator.py -e executablePath -c configFilePath 
            -b branchid -t title -o output -i buildid -d
'''

class PerfConfigurator:
    exePath = ""
    configPath = ""
    outputName = ""
    title = ""
    branch = ""
    buildid = ""
    currentDate = ""
    verbose = False
    testDateFromBuildId = False
    
    def _dumpConfiguration(self):
        """dump class configuration for convenient pickup or perusal"""
        print "Writing configuration:"
        print " - title = " + self.title
        print " - executablePath = " + self.exePath
        print " - configPath = " + self.configPath
        print " - outputName = " + self.outputName
        print " - branch = " + self.branch
        print " - buildid = " + self.buildid
        print " - currentDate = " + self.currentDate
    
    def _getCurrentDateString(self):
        currentDateTime = datetime.now()
        return currentDateTime.strftime("%Y%m%d_%H%M")
    
    def _getCurrentBuildId(self):
        master = open(self.exePath + masterIniSubpath)
        if not master:
            raise Configuration("Unable to open " + self.exePath + masterIniSubpath)
        masterContents = master.readlines()
        master.close()
        reBuildid = re.compile('BuildID\s*=\s*"(\d{10})"')
        for line in masterContents:
            match = re.match(reBuildid, line)
            if match:
                return match.group(1)
        raise Configuration("BuildID not found in " + self.exePath + masterIniSubpath)
    
    def _getTimeFromBuildId(self):
        buildIdTime = time.strptime(self.buildid, "%Y%m%d%H")
        return time.strftime("%a, %d %b %Y %H:%M:%S GMT", buildIdTime)
    
    def writeConfigFile(self):
        configFile = open(self.configPath + "sample.config")
        self.currentDate = self._getCurrentDateString()
        if not self.buildid:
            self.buildid = self._getCurrentBuildId()
        if not self.outputName:
            self.outputName = self.currentDate + "_config.yml"
        destination = open(self.outputName, "w")
        config = configFile.readlines()
        configFile.close()
        buildidString = "'" + str(self.buildid) + "'"
        for line in config:
            newline = line
            if 'firefox:' in line:
                newline = '  firefox: ' + self.exePath + 'firefox\\firefox.exe'
            if 'testtitle' in line:
                newline = line.replace('testtitle', self.title)
                if self.testDateFromBuildId:
                    newline += '\n'
                    newline += 'testdate: "%s"\n' % self._getTimeFromBuildId()
            if 'testfilename' in line:
                newline = line.replace('testfilename', self.outputName)
            if 'testbranchid' in line:
                newline = line.replace('testbranchid', buildidString)
            else:
                if 'testbranch' in line:
                    newline = line.replace('testbranch', self.branch)
            destination.write(newline)
        destination.close()
        if self.verbose:
            self._dumpConfiguration()
    
    def __init__(self, **kwargs):
        if 'title' in kwargs:
            self.title = kwargs['title']
        if 'branch' in kwargs:
            self.branch = kwargs['branch']
        if 'executablePath' in kwargs:
            self.exePath = kwargs['executablePath']
        if 'configFilePath' in kwargs:
            self.configPath = kwargs['configFilePath']
        if 'outputName' in kwargs:
            self.outputName = kwargs['outputName']
        if 'buildid' in kwargs:
            self.buildid = kwargs['buildid']
        if 'verbose' in kwargs:
            self.verbose = kwargs['verbose']
        if 'testDate' in kwargs:
            self.testDateFromBuildId = kwargs['testDate']


class Configuration(Exception):
    def __init__(self, msg):
        self.msg = "ERROR: " + msg

class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg


def main(argv=None):
    exePath = executablePath
    configPath = configFilePath
    output = ""
    title = defaultTitle
    branch = ""
    buildid = ""
    testDate = False
    verbose = False
    
    if argv is None:
        argv = sys.argv
    try:
        try:
            opts, args = getopt.getopt(argv[1:], "hve:c:t:b:o:i:d", 
                ["help", "verbose", "executablePath=", "configFilePath=", "title=", 
                "branch=", "output=", "id=", "testDate"])
        except getopt.error, msg:
            raise Usage(msg)
        
        # option processing
        for option, value in opts:
            if option in ("-v", "--verbose"):
                verbose = True
            if option in ("-h", "--help"):
                raise Usage(help_message)
            if option in ("-e", "--executablePath"):
                exePath = value
            if option in ("-c", "--configFilePath"):
                configPath = value
            if option in ("-t", "--title"):
                title = value
            if option in ("-b", "--branch"):
                branch = value
            if option in ("-o", "--output"):
                output = value
            if option in ("-i", "--id"):
                buildid = value
            if option in ("-d", "--testDate"):
                testDate = True
        
    except Usage, err:
        print >> sys.stderr, sys.argv[0].split("/")[-1] + ": " + str(err.msg)
        print >> sys.stderr, "\t for help use --help"
        return 2
    
    configurator = PerfConfigurator(title=title,
                                    executablePath=exePath,
                                    configFilePath=configPath,
                                    buildid=buildid,
                                    branch=branch,
                                    verbose=verbose,
                                    testDate=testDate,
                                    outputName=output)
    try:
        configurator.writeConfigFile()
    except Configuration, err:
        print >> sys.stderr, sys.argv[0].split("/")[-1] + ": " + str(err.msg)
        return 5
    return 0


if __name__ == "__main__":
    sys.exit(main())
