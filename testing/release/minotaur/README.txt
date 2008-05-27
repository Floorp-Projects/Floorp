 == Steps to Install Minotaur ==
 1. Drop all the Minotaur files into a directory - this is the MINOTAUR_DIR folder
 2. Change the tests.manifest file to also point to this directory
    DO NOT FORGET TRAILING SLASH!!:
    Windows: content minotaur file:///c:/code/minotaur/
    Mac/Linux: content minotaur file:///Users/clint/code/Minotaur/workspace/
 3. If you will be using mozDownload.py - ensure that wget is installed!
 4. Run Minotaur - run it without commands to get a usage statement
 5. Run Minotaur - results will be in <buildname>/<version>/<locale> directory in the
    MINOTAUR_DIR folder

 == To get verification files for a first run of a build ==
 1. Run Minotaur without the "verification" args, this means without -b -o and
    -c options
 2. The output.xml and bookmarks.html will be placed in <buildname>/<version>/<locale>
    directory in the MINOTAUR_DIR folder
 3. View the http.log that was generated (or partner specs) and create a
    release.txt file where the first (and only) line is the release channel name
 4. Edit these files if needed, and copy them to a verification directory.
 5. Run Minotaur again with using these as your verification files for the
    -b -o and -c options
 6. Submit those files for review and check-in so that future runs will use
    those files to diff against.

== Known Issues and Troubleshooting ==
* No Firefox windows appear when I run Minotaur, and the Firefox process (may)
  hang out in the system process list after the test finishes:  If you see this
  then Firefox is trying to tell you that your tests.manifest file is messed up.
  Either it has the wrong path, or you forgot the trailing slash.

* On Mac, you should use the following style paths:
<path to firefox.app>/Contents/MacOS This is the proper -f parameter path for a
mac.  So, for example, if firefox is installed in applications, this becomes:
/Applications/Firefox.app/Contents/MacOS/
