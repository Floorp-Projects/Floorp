echo "Comparing Resource Headers..."
echo off
diff -wbu setuprsc.good setuprsc.h > checkrsc.diff
notepad checkrsc.diff
del checkrsc.diff