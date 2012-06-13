#! /bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ------------------------------------------------------------------
# repackage.sh -- Repackage NSPR from /s/b/c to mozilla.org format
#
# syntax: repackage.sh
#
# Description:
# repackage.sh creates NSPR binary distributions for mozilla.org from
# the internal binary distributions in /share/builds/components/nspr20.
# There are reasons why we can't just push the internal binary distributions
# to mozilla.org. External developers prefer to use the common archive 
# file format for their platforms, rather than the jar files we use internally.
#
# On Unix, we create a tar.gz file.  On Windows, we create a zip file.
# For example: NSPR 4.1.1, these would be nspr-4.1.1.tar.gz and nspr-4.1.1.zip.
#
# When unpacked, nspr-4.1.1.tar.gz or nspr-4.1.1.zip should expand to a
# nspr-4.1.1 directory that contains three subdirectories: include, lib,
# and bin.  The header files, with the correct line endings for the
# platform, are in nspr-4.1.1/include.  The libraries are in nspr-4.1.1/lib.
# The executable programs are in nspr-4.1.1/bin.
# 
# Note! Files written with Gnu tar are not readable by some non-Gnu
# versions. Sun, in particular.
# 
# 
# 
# 
# ------------------------------------------------------------------

FROMTOP=/share/builds/components/nspr20/v4.9.2
TOTOP=./v4.9.2
NSPRDIR=nspr-4.9.2
SOURCETAG=NSPR_4_9_2_RTM

#
# enumerate Unix object directories on /s/b/c
UNIX_OBJDIRS="
HP-UXB.11.11_64_DBG.OBJ
HP-UXB.11.11_64_OPT.OBJ
HP-UXB.11.11_DBG.OBJ
HP-UXB.11.11_OPT.OBJ
HP-UXB.11.23_ia64_32_DBG.OBJ
HP-UXB.11.23_ia64_32_OPT.OBJ
HP-UXB.11.23_ia64_64_DBG.OBJ
HP-UXB.11.23_ia64_64_OPT.OBJ
Linux2.4_x86_glibc_PTH_DBG.OBJ
Linux2.4_x86_glibc_PTH_OPT.OBJ
Linux2.6_x86_64_glibc_PTH_DBG.OBJ
Linux2.6_x86_64_glibc_PTH_OPT.OBJ
Linux2.6_x86_glibc_PTH_DBG.OBJ
Linux2.6_x86_glibc_PTH_OPT.OBJ
SunOS5.9_64_DBG.OBJ
SunOS5.9_64_OPT.OBJ
SunOS5.9_DBG.OBJ
SunOS5.9_OPT.OBJ
"
#
# enumerate Windows object directories on /s/b/c
WIN_OBJDIRS="
WIN954.0_DBG.OBJ
WIN954.0_DBG.OBJD
WIN954.0_OPT.OBJ
WINNT5.0_DBG.OBJ
WINNT5.0_DBG.OBJD
WINNT5.0_OPT.OBJ
"

#
# Create the destination directory.
#
echo "removing directory $TOTOP"
rm -rf $TOTOP
echo "creating directory $TOTOP"
mkdir -p $TOTOP

#
# Generate the tar.gz files for Unix platforms.
#
for OBJDIR in $UNIX_OBJDIRS; do
    echo "removing directory $NSPRDIR"
    rm -rf $NSPRDIR
    echo "creating directory $NSPRDIR"
    mkdir $NSPRDIR

    echo "creating directory $NSPRDIR/include"
    mkdir $NSPRDIR/include
    echo "copying $FROMTOP/$OBJDIR/include"
    cp -r $FROMTOP/$OBJDIR/include $NSPRDIR

    echo "copying $FROMTOP/$OBJDIR/lib"
    cp -r $FROMTOP/$OBJDIR/lib $NSPRDIR

    echo "copying $FROMTOP/$OBJDIR/bin"
    cp -r $FROMTOP/$OBJDIR/bin $NSPRDIR

    echo "creating directory $TOTOP/$OBJDIR"
    mkdir $TOTOP/$OBJDIR
    echo "creating $TOTOP/$OBJDIR/$NSPRDIR.tar"
    tar cvf $TOTOP/$OBJDIR/$NSPRDIR.tar $NSPRDIR
    echo "gzipping $TOTOP/$OBJDIR/$NSPRDIR.tar"
    gzip $TOTOP/$OBJDIR/$NSPRDIR.tar
done

#
# Generate the zip files for Windows platforms.
#
for OBJDIR in $WIN_OBJDIRS; do
    echo "removing directory $NSPRDIR"
    rm -rf $NSPRDIR
    echo "creating directory $NSPRDIR"
    mkdir $NSPRDIR

    echo "creating directory $NSPRDIR/include"
    mkdir $NSPRDIR/include
    echo "creating directory $NSPRDIR/include/private"
    mkdir $NSPRDIR/include/private
    echo "creating directory $NSPRDIR/include/obsolete"
    mkdir $NSPRDIR/include/obsolete

    # copy headers and adjust unix line-end to Windows line-end
    # Note: Watch out for the "sed" command line.
    # when editing the command, take care to preserve the "^M" as the literal
    # cntl-M character! in vi, use "cntl-v cntl-m" to enter it!
    #
    headers=`ls $FROMTOP/$OBJDIR/include/*.h`
    for header in $headers; do
        sed -e 's/$//g' $header > $NSPRDIR/include/`basename $header`
    done
    headers=`ls $FROMTOP/$OBJDIR/include/obsolete/*.h`
    for header in $headers; do
        sed -e 's/$//g' $header > $NSPRDIR/include/obsolete/`basename $header`
    done
    headers=`ls $FROMTOP/$OBJDIR/include/private/*.h`
    for header in $headers; do
        sed -e 's/$//g' $header > $NSPRDIR/include/private/`basename $header`
    done

    echo "copying $FROMTOP/$OBJDIR/lib"
    cp -r $FROMTOP/$OBJDIR/lib $NSPRDIR

    echo "copying $FROMTOP/$OBJDIR/bin"
    cp -r $FROMTOP/$OBJDIR/bin $NSPRDIR

    echo "creating directory $TOTOP/$OBJDIR"
    mkdir -p $TOTOP/$OBJDIR
    echo "creating $TOTOP/$OBJDIR/$NSPRDIR.zip"
    zip -r $TOTOP/$OBJDIR/$NSPRDIR.zip $NSPRDIR
done

#
# package the source from CVS
#
echo "Packaging source"
echo "removing directory $NSPRDIR"
rm -rf $NSPRDIR
echo "creating directory $NSPRDIR"
mkdir $NSPRDIR
myWD=`pwd`
cd $NSPRDIR
echo "Pulling source from CVS with tag $SOURCETAG"
cvs co -r $SOURCETAG mozilla/nsprpub
cd $myWD
mkdir $TOTOP/src
echo "Creating source tar file: $TOTOP/src/$NSPRDIR.tar"
tar cvf $TOTOP/src/$NSPRDIR.tar $NSPRDIR
echo "gzip $TOTOP/src/$NSPRDIR.tar"
gzip $TOTOP/src/$NSPRDIR.tar

#
# Remove the working directory.
#
echo "removing directory $NSPRDIR"
rm -rf $NSPRDIR
# --- end repackage.sh ---------------------------------------------
