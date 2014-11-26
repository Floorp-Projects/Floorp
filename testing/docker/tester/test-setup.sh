#!/bin/bash -ve

pip install virtualenv;
mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos;
hg clone http://hg.mozilla.org/build/mozharness/
echo 'Xvfb :0 -nolisten tcp -screen 0 1600x1200x24 &> /dev/null &' >> .bashrc
chown -R worker:worker /home/worker/* /home/worker/.*
