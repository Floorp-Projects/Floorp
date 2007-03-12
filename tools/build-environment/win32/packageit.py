# Creates a mozilla-build installer.
#
# This script will taint your registry, cause hives, and otherwise screw up
# the system it's run on. Please do *not* run it on any machine you care about
# (a temporary VM would be perfect!)
#
# When clicking through installer dialogs, don't run any post-install steps.
# You won't need to change any paths.
#
# This script is python instead of shell because running the MSYS installer
# requires that no MSYS shells be currently running.
#
# The following tools must be set up in the environment:
#   makensis
#   msvc8
#   the Platform SDK version of ATL must be in INCLUDE before the VC8 version
#
# The following tools are prerequisites:
#   A mingw build toolchain, and an MSYS shell to build it. Use --msys= to
#   specify the installed location of MSYS (default c:\msys\1.0)
#   An MSYS build toolchain. This is described in:
#     http://www.mingw.org/MinGWiki/index.php/MSYSBuildEnvironment
#   The msysCORE package must match that provided here.

from subprocess import check_call
from os import getcwd, remove, environ
from os.path import dirname, join, split, abspath, exists
import optparse
from shutil import rmtree

sourcedir = join(split(abspath(__file__))[0])
stagedir = getcwd()
msysdir = "c:\\msys\\1.0"

oparser = optparse.OptionParser()
oparser.add_option("-s", "--source", dest="sourcedir")
oparser.add_option("-o", "--output", dest="stagedir")
oparser.add_option("-m", "--msys", dest="msysdir")
(options, args) = oparser.parse_args()

if len(args) != 0:
    raise Exception("Unexpected arguments passed to command line.")

if options.sourcedir:
    sourcedir = options.sourcedir
if options.stagedir:
    stagedir = options.stagedir
if options.msysdir:
    msysdir = option.msysdir

environ["MOZ_STAGEDIR"] = stagedir
environ["MOZ_SRCDIR"] = sourcedir

print("Source file location: " + sourcedir)
print("Output location: " + stagedir)

if exists(join(stagedir, "mozilla-build")):
    rmtree(join(stagedir, "mozilla-build"))

check_call([join(sourcedir, "7z442.exe"),
            "/D=" + join(stagedir, "mozilla-build", "7zip")])
check_call(["msiexec.exe", "/a",
            join(sourcedir, "python-2.5.msi"),
            "TARGETDIR=" + join(stagedir, "mozilla-build", "python25")])
check_call([join(sourcedir, "MSYS-1.0.10.exe"),
            "/DIR=" + join(stagedir, "mozilla-build", "msys"),
            # "/VERYSILENT", "/SUPRESSMSGBOXES",
            "/SP-", "/NOICONS"])
check_call([join(sourcedir, "msysDTK-1.0.1.exe"),
            "/DIR=" + join(stagedir, "mozilla-build", "msys"),
            # "/VERYSILENT", "/SUPRESSMSGBOXES",
            "/SP-", "/NOICONS"])
check_call([join(sourcedir, "XEmacs Setup 21.4.19.exe"),
            "/DIR=" + join(stagedir, "mozilla-build", "xemacs"),
            "/SP-", "/NOICONS"])
# Run an MSYS shell to perform the following tasks:
# * install make-3.81
# * install UPX
# * install blat
# * install SVN
# * build and install libiconv

check_call([join(msysdir, "bin", "sh.exe"), "--login",
            join(sourcedir, "packageit.sh")])

environ["MSYSTEM"] = "MSYS"
check_call([join(msysdir, "bin", "sh.exe"), "--login",
            join(sourcedir, "packageit-msys.sh")])

del environ["MSYSTEM"]

# Make an installer
check_call(["makensis", "/NOCD", "installit.nsi"])
