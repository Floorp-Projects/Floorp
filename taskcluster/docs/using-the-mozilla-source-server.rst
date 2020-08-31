Using The Mozilla Source Server
===============================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

Using the Mozilla source server is now even more feature-packed. The
nightly debug builds are now also Source Indexed so that by following a
couple of simple steps you can also have the source code served to you
for debugging without a local build

What you'll need
----------------

-  `WinDbg <https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/>`__
   or Visual Studio (Note: express editions will not work, but WinDbg is
   a free download)
-  A nightly build that was created after April 15, 2008; go to the
   `/pub/firefox/nightly/latest-mozilla-central/ <https://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central/>`__
   folder and grab the installer
-  For builds predating the switch to Mercurial, you'll need cvs.exe,
   added to your PATH (the cvs.exe from MozillaBuild has problems, `use
   this one
   instead <http://ftp.gnu.org/non-gnu/cvs/binary/stable/x86-woe/cvs-1-11-22.zip>`__)

Do *not* use the CVS from MozillaBuild, it will not work!

Set up symbols
--------------

Follow the instructions for :ref:`Using the Mozilla symbol
server <Using The Mozilla Symbol Server>`. Once
the symbol path is set you must now enable Source Server.

Using the source server in WinDbg
---------------------------------

In the WinDbg command line, type ``.srcfix`` and hit enter. This enables
source server support.

|Image:windbg-srcfix.png|

Now, when you click on a frame in the "Calls" window, WinDbg will prompt
you about running cvs to download the associated source code.

|Image:Windbg-Srcsrv-prompt.png|

If you click "Yes", WinDbg will display \*BUSY\* in the status bar while
it downloads the source, and then it will automatically open the file
and highlight the current line.

|Image:Windbg-Srcsrv-src.png|

Using the source server in Visual Studio
----------------------------------------

Source server support does not work correctly out of the
box in Visual Studio 2005. If you install WinDBG, and copy srcsrv.dll
from the WinDBG install dir to the Visual Studio install dir
(replacing the existing copy) it will work.

Enable source server support under Tools -> Options. Also, disable
(uncheck) the box that says "Require source files to exactly match the
original version".

|Image:enableSourceServer.png|

Start debugging your program. Symbols will load and then, if you're
using a CVS build, when you try to step into or step over a confirmation
window will appear asking if you trust the cvs command that Source
Server is trying to run.

|Image:cvsMessage.png|

After the command executes, the source file will load in the window.

|Image:firefoxDebugging.png|

.. |Image:windbg-srcfix.png| image:: https://developer.mozilla.org/@api/deki/files/969/=Windbg-srcfix.png
   :class: internal
.. |Image:Windbg-Srcsrv-prompt.png| image:: https://developer.mozilla.org/@api/deki/files/421/=Windbg-Srcsrv-prompt.png
   :class: internal
.. |Image:Windbg-Srcsrv-src.png| image:: https://developer.mozilla.org/@api/deki/files/422/=Windbg-Srcsrv-src.png
   :class: internal
.. |Image:enableSourceServer.png| image:: https://developer.mozilla.org/@api/deki/files/674/=EnableSourceServer.png
   :class: internal
.. |Image:cvsMessage.png| image:: https://developer.mozilla.org/@api/deki/files/636/=CvsMessage.png
   :class: internal
.. |Image:firefoxDebugging.png| image:: https://developer.mozilla.org/@api/deki/files/699/=FirefoxDebugging.png
   :class: internal
