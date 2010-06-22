# Usage: sh update.sh <upstream_src_directory>
cp $1/include/nestegg/nestegg.h include
cp $1/src/nestegg.c src
cp $1/halloc/halloc.h src
cp $1/halloc/src/align.h src
cp $1/halloc/src/halloc.c src
cp $1/halloc/src/hlist.h src
cp $1/halloc/src/macros.h src
cp $1/LICENSE .
cp $1/README .
cp $1/AUTHORS .
echo 'Remember to update README_MOZILLA with the version details.'
