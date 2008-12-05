==========================================================================

= = = = = = = = = = = = =  SeaMonkey Read Me   = = = = = = = = = = = = = =

==========================================================================

SeaMonkey is subject to the terms detailed in the license agreement
accompanying it.

This Read Me file contains information about system requirements and
installation instructions for the OS/2 builds of SeaMonkey.

For more info on SeaMonkey, see http://www.seamonkey-project.org/
For more info on the OS/2 port see http://www.mozilla.org/ports/os2.
To submit bugs or other feedback, see
http://www.seamonkey-project.org/dev/get-involved and check out Bugzilla
at https://bugzilla.mozilla.org/ for links to known bugs, bug-writing
guidelines, and more. You can also get help with Bugzilla by pointing your
IRC client to #mozillazine at irc.mozilla.org, OS/2 specific problems are
discussed in #warpzilla and in the newsgroup mozilla.dev.ports.os2 on
news.mozilla.org.


==========================================================================

                          Getting SeaMonkey

==========================================================================

You can download OS/2 nightly builds of SeaMonkey from the mozilla.org FTP
site at

  http://ftp.mozilla.org/pub/mozilla.org/seamonkey/nightly/contrib/

For the very latest builds, see

  http://ftp.mozilla.org/pub/mozilla.org/seamonkey/nightly/contrib/latest-trunk/

Keep in mind that nightly builds, which are used by mozilla.org developers
for testing, may be buggy. If you are looking for a more polished version
of SeaMonkey, the SeaMonkey project releases builds of SeaMonkey regularly
that you can download from

  http://www.seamonkey-project.org/releases/

Be sure to read the SeaMonkey release notes for information on known
problems and installation issues with SeaMonkey.  The release notes can be
found at the preceding URL along with the releases themselves.

OS/2 releases are not created by SeaMonkey project or mozilla.org
themselves and may appear on the page http://www.mozilla.org/ports/os2
before the releases page. Be sure to read the SeaMonkey release notes
linked on the releases page for information on known problems and
installation issues with SeaMonkey.


==========================================================================

                         System Requirements

==========================================================================

* OS/2
   - This release requires the C runtime DLLs (libc-0.6.1) from
     ftp://ftp.netlabs.org/pub/gcc/libc-0.6.1-csd1.zip
     in order to run.  You can unpack them in the same directory as
     SeaMonkey's executable or somewhere else in your LIBPATH. The
     SeaMonkey installer will not install the C runtime DLLs for you but
     requires them to run.

   - Minimum hardware requirements
     + Pentium class processor
     + 64 MiB RAM plus 64 MiB free swap space
     + 35 MiB free harddisk space for installation
       plus storage space for disk cache and mail

   - Recommended hardware for acceptable performance
     + 500 MHz processor
     + 256 MiB RAM plus 64 MiB free swap space
       NOTE: SeaMonkey's performance and stability increases the more
       physical RAM is available. Especially for long browsing and IRC
       sessions 512 MiB of memory is recommended.

   - Software requirements
     + Installation on a file system supporting long file names
       (i.e. HPFS or JFS but not FAT)
     + OS/2 Warp 4 with Fixpack 15 or later
     + MPTS version 5.3
     + TCP/IP version 4.1
     + INETVER: SOCKETS.SYS=5.3007, AFOS2.SYS=5.3001, AFINET.SYS=5.3006
       NOTE: Do not attempt to use MPTS & TCP/IP versions below these
       INETVER levels. Although SeaMonkey may seem to start and run
       normally with older stacks, some features SeaMonkey needs are not
       implemented correctly in older MPTS versions, which may result in
       crashes and data loss.

     + Convenience Pack 2 or eComStation 1.0 or later meet these requirements
       out of the box.


==========================================================================

                      Installation Instructions

==========================================================================

It is strongly recommended that you exit all programs before running the
setup program. Also, you should temporarily disable virus-detection
software.

Install into a clean (new) directory. Installing on top of previously
released builds may cause problems.

Note: These instructions do not tell you how to build SeaMonkey.
For info on building SeaMonkey from the mozilla.org source code, see

  http://developer.mozilla.org/en/docs/Build_Documentation


OS/2 Installation Instructions
------------------------------

   To install SeaMonkey by downloading the SeaMonkey installer, follow
   these steps:

   1. Click the "Installer" link on the site you're downloading SeaMonkey
      from to download the installer file to your machine. This file is
      typically called seamonkey-x.xx.en-US.os2.installer.exe where the
      "x.xx" is replaced by the SeaMonkey version.

   2. Navigate to where you downloaded the file, make sure that the C
      library DLLs are copied to the same directory or installed in the
      LIBPATH, and double-click on the the SeaMonkey installer object to
      start the Setup program.

   3. Follow the on-screen instructions in the setup program. The program 
      automatically ends any running SeaMonkey sessions and creates a
      SeaMonkey folder on the Desktop. To start SeaMonkey the first time,
      again make sure that the C library DLLs are copied to the
      installation directory or installed in the LIBPATH and then
      double-click the SeaMonkey program object inside this folder.

   To install SeaMonkey by downloading the .zip file and installing
   manually, follow these steps:

   1. Click the "Zip" link on the site you're downloading SeaMonkey from
      to download the ZIP package to your machine. This file is typically
      called seamonkey-x.x.x.en-US.os2.zip where the "x.x.x" is replaced
      by the SeaMonkey version.

   2. Navigate to where you downloaded the file and unpack it using your
      favorite unzip tool.

   3. Keep in mind that the unzip process creates a directory "seamonkey"
      below the location you point it to, e.g.
        unzip seamonkey-1.7.5.en-US.os2.zip -d c:\seamonkey-1.7.5
      will unpack SeaMonkey into c:\seamonkey-1.7.5\seamonkey.

   4. Make sure that you are _not_ unpacking over an old installation.
      This is known to cause problems.

   5. To start SeaMonkey, navigate to the directory you extracted
      SeaMonkey to, make sure that the C library DLLs are copied to the
      installation directory or installed in the LIBPATH, and then
      double-click the seamonkey.exe object.


Running multiple versions of SeaMonkey concurrently
-------------------------------------------------

Because various members of the Mozilla family (i.e. SeaMonkey, Mozilla,
Firefox, Thunderbird, IBM Web Browser) may use different, incompatible
versions of the same DLL, some extra steps may be required to run them
concurrently.

One workaround is the LIBPATHSTRICT variable. To run Mozilla-based
applications one can create a CMD script like the following example
(where an installation of SeaMonkey exists in the directory
d:\internet\seamonkey is assumed):

   set LIBPATHSTRICT=T
   rem The next line may be needed when a different Mozilla program is listed in LIBPATH
   rem set BEGINLIBPATH=d:\internet\seamonkey
   rem The next line is only needed to run two versions of the same program
   rem set MOZ_NO_REMOTE=1
   d:
   cd d:\internet\seamonkey
   seamonkey.exe %1 %2 %3 %4 %5 %6 %7 %8 %9

Similarly, one can create a program object to start SeaMonkey using the
following settings:

   Path and file name: *
   Parameters:         /c set LIBPATHSTRICT=T & .\seamonkey.exe "%*"
   Working directory:  d:\internet\seamonkey

(One might need to add MOZ_NO_REMOTE and/or BEGINLIBPATH as in the CMD
script above depending on the system configuration.)

Finally, the simplest method is to use the Run! utility by Rich Walsh that
can be found in the Hobbes Software Archive:

   http://hobbes.nmsu.edu/cgi-bin/h-search?key=Run!

Read its documentation for more information.


Separating profiles from installation directory
-----------------------------------------------

To separate the locations of the user profile(s) (containing the bookmarks
and all customizations) from the installation directory to keep your
preferences in the case of an update even when using ZIP packages, set the
variable MOZILLA_HOME to a directory of your choice. You can do this
either in Config.sys or in a script or using a program object as listed
above. If you add

   set MOZILLA_HOME=f:\Data

the user profile(s) will be created in "f:\Data\Mozilla\Profiles".


Other important environment variables
-------------------------------------

There are a few enviroment variables that can be used to control special
behavior of SeaMonkey on OS/2:

- set NSPR_OS2_NO_HIRES_TIMER=1
  This causes SeaMonkey not to use OS/2's high resolution timer. Set this
  if other applications using the high resolution timer (multimedia apps)
  act strangely.

- set MOZ_NO_REMOTE=1
  Use this to run two instances of SeaMonkey simultaneously (e.g. debug
  and optimized version).

Find more information on this topic and other tips on
   http://www.os2bbs.com/os2news/Warpzilla.html


Known Problems of the OS/2 version
----------------------------------

Cross-platform problems are usually listed in the release notes of each
milestone release.

- SeaMonkey will beep when copying more than 64 kB of text to the clipboard.
  This is to alert users that many applications (most notably the system
  editor, EPM, and applications running in VIO windows) cannot paste more
  than this.

- SeaMonkey cannot make use of OS/2 fonts like WarpSans and others which
  are not available in Type1 or TrueType format. It is therefore
  recommended to install the "Workplace Sans" font from
     http://users.socis.ca/~ataylo00/creative/fonts/
  or
     http://hobbes.nmsu.edu/cgi-bin/h-search?key=wpsu_ttf
  which SeaMonkey will use as a replacement of WarpSans.

Other known problems can be found by following the link "Current Open
Warpzilla Bugs" on the OS/2 Mozilla page <http://www.mozilla.org/ports/os2/>.
