#!/bin/sh
# package an OS into a multi-OS tree
# usage ospkg.sh targetOS os...

target=$1; shift
pkgdir=./build/package/$target/mstone

errors=0

for obj 
do 
    srcdir=./build/package/$obj/mstone
    if [ ! -d $srcdir ] ; then
	echo "Error! Source directory $srcdir is missing."
	errors=1
	continue;
    fi
    arch=`echo $obj | sed -e 's/_OPT.OBJ//' | sed -e 's/_DBG.OBJ//'`

    echo "===== adding $arch packaging to $pkgdir"

    echo "Installing mailclient"
    [ -d $pkgdir/bin/$arch/bin ] || mkdir -p $pkgdir/bin/$arch/bin
    cp -p $srcdir/bin/mailclient $pkgdir/bin/$arch/bin/

    if [ -d $srcdir/gd ] ; then
	echo "Installing gd"
	[ -d $pkgdir/bin/$arch/gd ] || mkdir -p $pkgdir/bin/$arch/gd
	cp -p $srcdir/gd/* $pkgdir/bin/$arch/gd/
    fi

    if [ -d $srcdir/gnuplot ] ; then
	echo "Installing gnuplot"
	[ -d $pkgdir/bin/$arch/gnuplot ] || mkdir -p $pkgdir/bin/$arch/gnuplot
	cp -p $srcdir/gnuplot/* $pkgdir/bin/$arch/gnuplot/
    fi

    if [ -d $srcdir/perl ] ; then
	perlver=`cd $srcdir/perl/lib; ls -d 5.* | head -1`
	perlarch=`cd $srcdir/perl/lib/$perlver; ls -d */Config.pm | cut -f1 -d/`
	echo "Installing perl $perlver for $perlarch"
	[ -d $pkgdir/perl ] || mkdir -p $pkgdir/perl
	cp -pf $srcdir/perl/Artistic $pkgdir/perl/

	# we dont pull everything in, just the potentially useful parts
	for subdir in man/man1 \
		lib/$perlver lib/$perlver/$perlarch \
		lib/$perlver/Time lib/$perlver/Term lib/$perlver/Class \
		lib/$perlver/Sys lib/$perlver/Data lib/$perlver/Getopt \
		lib/$perlver/Test lib/$perlver/Text \
		lib/$perlver/File lib/$perlver/File/Spec \
		lib/$perlver/CGI lib/$perlver/Net \
		lib/$perlver/$perlarch/auto/DynaLoader \
		lib/$perlver/$perlarch/auto/Socket \
		lib/$perlver/$perlarch/auto/re \
		lib/$perlver/$perlarch/auto/attrs
	do
	    [ -d $srcdir/perl/$subdir ] || continue;
	    [ -d $pkgdir/perl/$subdir ] || mkdir -p $pkgdir/perl/$subdir
	    # HACK: all the files have dots in them, directories dont
	    cp -pf $srcdir/perl/$subdir/*.* $pkgdir/perl/$subdir
	done

	# where we put multi-os perl binaries
	perlbin=$pkgdir/perl/arch/$arch
	[ -d $perlbin ] || mkdir -p $perlbin
	cp -p $srcdir/perl/bin/* $perlbin/
    fi

done

exit $errors
