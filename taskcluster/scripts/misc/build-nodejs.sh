#!/bin/bash
set -x -e -v

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
project=${artifact%.tar.*}
workspace=$HOME/workspace

cd $MOZ_FETCHES_DIR/$project

for compiler in clang clang++; do
	echo "#!/bin/sh" > $compiler
	echo "$MOZ_FETCHES_DIR/clang/bin/$compiler --sysroot=$MOZ_FETCHES_DIR/sysroot \"\$@\"" >> $compiler
	chmod +x $compiler
done

CC=$PWD/clang CXX=$PWD/clang++ ./configure --verbose --prefix=/
make -j$(nproc) install DESTDIR=$workspace/$project

tar -C $workspace -acvf $artifact $project
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR
