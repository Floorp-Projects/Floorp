PREREQUISITES:
You'll need to download some software onto your Windows machine before running
these performance tests:

  * Python 2.4
    The scripts all run from Python 2.4.  You will need the windows version
    (not the cygwin version).  You can download it here:
    http://www.python.org/ftp/python/2.4/python-2.4.msi

    Make sure to correctly set the path to python in the paths.py file.

    After you download and install Python 2.4, Windows users will need to install
    some extensions:

      * Python Win32 Extensions
        These extensions provide some support for process management and
        performance monitoring.
        http://prdownloads.sourceforge.net/pywin32/pywin32-208.win32-py2.4.exe?download
     
    Mac users may use Python 2.3.5, included with Mac OS X, but they will need 
    to download the 'subprocess.py' file included with more recent Python distributions
    and install it in the library path (/Library/Python/2.3/site-packages/). 

  * Apache HTTP Server
    Found at http://httpd.apache.org/
    The page cycler works on a local Apache server.  After installing Apache simply place
    the page_load_test/ directory into htdocs/ directory of Apache (found on most systems
    at c:\Program Files\Apache Software Foundation\Apache2.2\htdocs)
    
  * PyYAML YAML Parser
  	You'll need to download and install PyYAML from http://pyyaml.org/wiki/PyYAML
        (or via your OS's package installation system on Linux).

  1. Make sure the prerequisites, above, are installed.
  2. Copy this entire directory and all subdirectories onto your local disk
  3. Edit the config.py file to set the paths to Cygwin, Firefox, etc. on your 
     machine.
  4. Create a YAML config file with info about the profiles you want to test.
     NOTE: You should set the preferences network.proxy.type:1, network.proxy.http:localhost and 
     network.proxy.http_port:80 - these settings ensure that the browser will only
     contact local web pages and will not attempt to pull information from the live web,
     this is important for collecting consistant testing results.
     Your config file should look something like the sample.config file in the 
     Talos distribution.

  5. Provide a pages/ directory
     The page_load_test/ relies upon having a pages directory that includes the web pages
     to be cycled through.  Each directory in pages/ should be a given web page.  
     The parray.js file needs to be edited to reflect the list of index pages of the web pages 
     that are to be tested - it is currently full of a sample list.

  6. Run "python run_tests.py" with the name of your config file as an argument. You can use
     a space-separated list of config files, to generate a report of startup and page load times. 

DIRECTORY STRUCTURE:
   page_load_test/
     This directory contains the JavaScript files and html data files for the
     page load test. The page load test opens a new window and cycles through
     loading each html file, timing each load.
   startup_test/
     This directory contains the JavaScript to run the startup test.  It
     measures how long it takes Firefox to start up.
   base_profile/
     This directory contains the base profile used for testing.  A copy of
     this profile is made for each testing profile, and extensions or prefs
     are added according to the test_configs array in run_tests.py.  For the
     page load test to run correctly, the hostperm.1 file must be set to allow
     scheme:file uris to open in new windows, and the pref to force a window
     to open in a tab must not be set.  The dom.allow_scripts_to_close_windows
     pref should also be set to true.  The browser.shell.checkDefaultBrowser
     pref should be set to false.
   config.py
     This file should be configured to run the test on different machines,
     with different extensions or preferences.  See setup above.
     
