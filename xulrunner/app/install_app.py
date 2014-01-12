#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Min version of python is 2.7
import sys
if ((sys.version_info.major != 2) or (sys.version_info.minor < 7)):
    raise Exception("You need to use Python version 2.7 or higher")

import os, shutil, re, zipfile
from ConfigParser import SafeConfigParser

# Platform-specific support
# see https://developer.mozilla.org/en/XULRunner/Deploying_XULRunner_1.8
if sys.platform.startswith('linux') or sys.platform == "win32":
    def installApp(appLocation, installDir, appName, greDir):
        zipApp, iniParser, appName = validateArguments(appLocation, installDir, appName, greDir)
        if (zipApp):
            zipApp.extractAll(installDir)
        else:
            shutil.copytree(appLocation, installDir)
        shutil.copy2(os.path.join(greDir, xulrunnerStubName),
                     os.path.join(installDir, appName))
        copyGRE(greDir, os.path.join(installDir, "xulrunner"))

if sys.platform.startswith('linux'):
    xulrunnerStubName = "xulrunner-stub"

    def makeAppName(leafName):
        return leafName.lower()

elif sys.platform == "win32":
    xulrunnerStubName = "xulrunner-stub.exe"

    def makeAppName(leafName):
        return leafName + ".exe"

elif sys.platform == "darwin":
    xulrunnerStubName = "xulrunner"

    def installApp(appLocation, installDir, appName, greDir):
        zipApp, iniparser, appName = validateArguments(appLocation, installDir, appName, greDir)
        installDir += "/" + appName + ".app"
        resourcesDir = os.path.join(installDir, "Contents/Resources")
        if (zipApp):
            zipApp.extractAll(resourcesDir)
        else:
            shutil.copytree(appLocation, resourcesDir)
        MacOSDir = os.path.join(installDir, "Contents/MacOS")
        os.makedirs(MacOSDir)
        shutil.copy2(os.path.join(greDir, xulrunnerStubName), MacOSDir)
        copyGRE(greDir,
                os.path.join(installDir, "Contents/Frameworks/XUL.framework"))

        # Contents/Info.plist
        contents = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
<key>CFBundleInfoDictionaryVersion</key>
<string>6.0</string>
<key>CFBundlePackageType</key>
<string>APPL</string>
<key>CFBundleSignature</key>
<string>????</string>
<key>CFBundleExecutable</key>
<string>xulrunner</string>
<key>NSAppleScriptEnabled</key>
<true/>
<key>CFBundleGetInfoString</key>
<string>$infoString</string>
<key>CFBundleName</key>
<string>$appName</string>
<key>CFBundleShortVersionString</key>
<string>$version</string>
<key>CFBundleVersion</key>
<string>$version.$buildID</string>
<key>CFBundleIdentifier</key>
<string>$reverseVendor</string>
</dict>
</plist>
"""
        version = iniparser.get("App", "Version")
        buildID = iniparser.get("App", "BuildID")
        infoString = appName + " " + version
        reverseVendor = "com.vendor.unknown"
        appID = iniparser.get("App", "ID")
        colonIndex = appID.find("@") + 1
        if (colonIndex != 0):
            vendor = appID[colonIndex:]
            reverseVendor = ".".join(vendor.split(".")[::-1])
        contents = contents.replace("$infoString", infoString)
        contents = contents.replace("$appName", appName)
        contents = contents.replace("$version", version)
        contents = contents.replace("$buildID", buildID)
        contents = contents.replace("$reverseVendor", reverseVendor)
        infoPList = open(os.path.join(installDir, "Contents/Info.plist"), "w+b")
        infoPList.write(contents)
        infoPList.close()

    def makeAppName(leafName):
        return leafName

else:
    # Implement xulrunnerStubName, installApp and makeAppName as above.
    raise Exception("This operating system isn't supported for install_app.py yet!")
# End platform-specific support

def resolvePath(path):
    return os.path.realpath(path)

def requireINIOption(iniparser, section, option):
    if not (iniparser.has_option(section, option)):
        raise Exception("application.ini must have a " + option + " option under the " +  section + " section")

def checkAppINI(appLocation):
    if (os.path.isdir(appLocation)):
        zipApp = None
        appINIPath = os.path.join(appLocation, "application.ini")
        if not (os.path.isfile(appINIPath)):
            raise Exception(appINIPath + " does not exist")
        appINI = open(appINIPath)
    elif (zipfile.is_zipfile(appLocation)):
        zipApp = zipfile.ZipFile(appLocation)
        if not ("application.ini" in zipApp.namelist()):
            raise Exception("jar:" + appLocation + "!/application.ini does not exist")
        appINI = zipApp.open("application.ini")
    else:
        raise Exception("appLocation must be a directory containing application.ini or a zip file with application.ini at its root")

    # application.ini verification
    iniparser = SafeConfigParser()
    iniparser.readfp(appINI)
    if not (iniparser.has_section("App")):
        raise Exception("application.ini must have an App section")
    if not (iniparser.has_section("Gecko")):
        raise Exception("application.ini must have a Gecko section")
    requireINIOption(iniparser, "App", "Name")
    requireINIOption(iniparser, "App", "Version")
    requireINIOption(iniparser, "App", "BuildID")
    requireINIOption(iniparser, "App", "ID")
    requireINIOption(iniparser, "Gecko", "MinVersion")

    return zipApp, iniparser
    pass

def copyGRE(greDir, targetDir):
    shutil.copytree(greDir, targetDir, symlinks=True)

def validateArguments(appLocation, installDir, appName, greDir):
    # application directory / zip verification
    appLocation = resolvePath(appLocation)

    # target directory
    installDir = resolvePath(installDir)

    if (os.path.exists(installDir)):
        raise Exception("installDir must not exist: " + cmds.installDir)

    greDir = resolvePath(greDir)
    xulrunnerStubPath = os.path.isfile(os.path.join(greDir, xulrunnerStubName))
    if not xulrunnerStubPath:
        raise Exception("XULRunner stub executable not found: " + os.path.join(greDir, xulrunnerStubName))

    # appName
    zipApp, iniparser = checkAppINI(appLocation)
    if not appName:
        appName = iniparser.get("App", "Name")
    appName = makeAppName(appName)
    pattern = re.compile("[\\\/\:*?\"<>|\x00]")
    if pattern.search(appName):
        raise Exception("App name has illegal characters for at least one operating system")
    return zipApp, iniparser, appName

def handleCommandLine():
    import argparse

    # Argument parsing.
    parser = argparse.ArgumentParser(
        description="XULRunner application installer",
        usage="""install_app.py appLocation installDir greDir [--appName APPNAME]
           install_app.py -h
           install_app.py --version
    """
    )
    parser.add_argument(
        "appLocation",
        action="store",
        help="The directory or ZIP file containing application.ini as a top-level child file"
    )
    parser.add_argument(
        "installDir",
        action="store",
        help="The directory to install the application to"
    )
    parser.add_argument(
        "--greDir",
        action="store",
        help="The directory containing the Gecko SDK (usually where this Python script lives)",
        default=os.path.dirname(sys.argv[0])
    )
    parser.add_argument(
        "--appName",
        action="store",
        help="The name of the application to install"
    )
    parser.add_argument("--version", action="version", version="%(prog)s 1.0")

    # The command code.
    cmds = parser.parse_args()
    try:
        installApp(cmds.appLocation, cmds.installDir, cmds.appName, cmds.greDir)
    except exn:
        shutil.rmtree(cmds.installDir)
        raise exn
    print cmds.appName + " application installed to " + cmds.installDir

if __name__ == '__main__':
    handleCommandLine()
