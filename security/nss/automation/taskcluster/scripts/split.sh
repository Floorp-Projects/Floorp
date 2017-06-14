copy_top()
{
  srcdir_="$1"
  dstdir_="$2"
  files=`find "$srcdir_" -maxdepth 1 -mindepth 1 -type f`
  for f in $files; do
    cp -p "$f" "$dstdir_"
  done
}

split_util() {
  nssdir="$1"
  dstdir="$2"

  # Prepare a source tree only containing files to build nss-util:
  #
  #   nss/dbm                     full directory
  #   nss/coreconf                full directory
  #   nss                         top files only
  #   nss/lib                     top files only
  #   nss/lib/util                full directory

  # Copy everything.
  cp -R $nssdir $dstdir

  # Skip gtests when building.
  sed '/^DIRS = /s/ cpputil gtests$//' $nssdir/manifest.mn > $dstdir/manifest.mn-t && mv $dstdir/manifest.mn-t $dstdir/manifest.mn

  # Remove subdirectories that we don't want.
  rm -rf $dstdir/cmd
  rm -rf $dstdir/tests
  rm -rf $dstdir/lib
  rm -rf $dstdir/automation
  rm -rf $dstdir/gtests
  rm -rf $dstdir/cpputil
  rm -rf $dstdir/doc

  # Start with an empty cmd lib directories to be filled selectively.
  mkdir $dstdir/cmd
  cp $nssdir/cmd/Makefile $dstdir/cmd
  cp $nssdir/cmd/manifest.mn $dstdir/cmd
  cp $nssdir/cmd/platlibs.mk $dstdir/cmd
  cp $nssdir/cmd/platrules.mk $dstdir/cmd

  # Copy some files at the top and the util subdirectory recursively.
  mkdir $dstdir/lib
  cp $nssdir/lib/Makefile $dstdir/lib
  cp $nssdir/lib/manifest.mn $dstdir/lib
  cp -R $nssdir/lib/util $dstdir/lib/util
}

split_softoken() {
  nssdir="$1"
  dstdir="$2"

  # Prepare a source tree only containing files to build nss-softoken:
  #
  #   nss/dbm                     full directory
  #   nss/coreconf                full directory
  #   nss                         top files only
  #   nss/lib                     top files only
  #   nss/lib/freebl              full directory
  #   nss/lib/softoken            full directory
  #   nss/lib/softoken/dbm        full directory

  # Copy everything.
  cp -R $nssdir $dstdir

  # Skip gtests when building.
  sed '/^DIRS = /s/ cpputil gtests$//' $nssdir/manifest.mn > $dstdir/manifest.mn-t && mv $dstdir/manifest.mn-t $dstdir/manifest.mn

  # Remove subdirectories that we don't want.
  rm -rf $dstdir/cmd
  rm -rf $dstdir/tests
  rm -rf $dstdir/lib
  rm -rf $dstdir/pkg
  rm -rf $dstdir/automation
  rm -rf $dstdir/gtests
  rm -rf $dstdir/cpputil
  rm -rf $dstdir/doc

  # Start with an empty lib directory and copy only what we need.
  mkdir $dstdir/lib
  copy_top $nssdir/lib $dstdir/lib
  cp -R $nssdir/lib/dbm $dstdir/lib/dbm
  cp -R $nssdir/lib/freebl $dstdir/lib/freebl
  cp -R $nssdir/lib/softoken $dstdir/lib/softoken
  cp -R $nssdir/lib/sqlite $dstdir/lib/sqlite

  mkdir $dstdir/cmd
  copy_top $nssdir/cmd $dstdir/cmd
  cp -R $nssdir/cmd/bltest $dstdir/cmd/bltest
  cp -R $nssdir/cmd/ecperf $dstdir/cmd/ecperf
  cp -R $nssdir/cmd/fbectest $dstdir/cmd/fbectest
  cp -R $nssdir/cmd/fipstest $dstdir/cmd/fipstest
  cp -R $nssdir/cmd/lib $dstdir/cmd/lib
  cp -R $nssdir/cmd/lowhashtest $dstdir/cmd/lowhashtest
  cp -R $nssdir/cmd/shlibsign $dstdir/cmd/shlibsign

  mkdir $dstdir/tests
  copy_top $nssdir/tests $dstdir/tests

  cp -R $nssdir/tests/cipher $dstdir/tests/cipher
  cp -R $nssdir/tests/common $dstdir/tests/common
  cp -R $nssdir/tests/ec $dstdir/tests/ec
  cp -R $nssdir/tests/lowhash $dstdir/tests/lowhash

  cp $nssdir/lib/util/verref.h $dstdir/lib/freebl
  cp $nssdir/lib/util/verref.h $dstdir/lib/softoken
  cp $nssdir/lib/util/verref.h $dstdir/lib/softoken/legacydb
}

split_nss() {
  nssdir="$1"
  dstdir="$2"

  # Prepare a source tree only containing files to build nss:
  #
  #   nss/dbm                     full directory
  #   nss/coreconf                full directory
  #   nss                         top files only
  #   nss/lib                     top files only
  #   nss/lib/freebl              full directory
  #   nss/lib/softoken            full directory
  #   nss/lib/softoken/dbm        full directory

  # Copy everything.
  cp -R $nssdir $dstdir

  # Remove subdirectories that we don't want.
  rm -rf $dstdir/lib/freebl
  rm -rf $dstdir/lib/softoken
  rm -rf $dstdir/lib/util
  rm -rf $dstdir/cmd/bltest
  rm -rf $dstdir/cmd/fipstest
  rm -rf $dstdir/cmd/rsaperf_low

  # Copy these headers until the upstream bug is accepted
  # Upstream https://bugzilla.mozilla.org/show_bug.cgi?id=820207
  cp $nssdir/lib/softoken/lowkeyi.h $dstdir/cmd/rsaperf
  cp $nssdir/lib/softoken/lowkeyti.h $dstdir/cmd/rsaperf

  # Copy verref.h which will be needed later during the build phase.
  cp $nssdir/lib/util/verref.h $dstdir/lib/ckfw/builtins/verref.h
  cp $nssdir/lib/util/verref.h $dstdir/lib/nss/verref.h
  cp $nssdir/lib/util/verref.h $dstdir/lib/smime/verref.h
  cp $nssdir/lib/util/verref.h $dstdir/lib/ssl/verref.h
  cp $nssdir/lib/util/templates.c $dstdir/lib/nss/templates.c

  # FIXME: Skip util_gtest because it links with libnssutil.a.  Note
  # that we can't use libnssutil3.so instead, because util_gtest
  # depends on internal symbols not exported from the shared library.
  sed '/	util_gtest \\/d' $dstdir/gtests/manifest.mn > $dstdir/gtests/manifest.mn-t && mv $dstdir/gtests/manifest.mn-t $dstdir/gtests/manifest.mn
}
