#!/bin/bash -ve

############################### system-setup.sh ###############################

home="/home/worker"
tools_dir="/tmp/tools"

mkdir -p $home/bin
mkdir -p $home/tools

chown -R worker:worker $home/* $home/.*

# Install android repo tool
curl https://storage.googleapis.com/git-repo-downloads/repo > $home/bin/repo
chmod a+x $home/bin/repo

# Install build tools
cd /tmp
hg clone http://hg.mozilla.org/build/tools/
cd $tools_dir
python setup.py install

# Put gittool and hgtool in the PATH
cp $tools_dir/buildfarm/utils/gittool.py $home/bin
cp $tools_dir/buildfarm/utils/hgtool.py $home/bin
chmod +x $home/bin/gittool.py
chmod +x $home/bin/hgtool.py

# Initialize git (makes repo happy)
git config --global user.email "docker@docker.com"
git config --global user.name "docker"

cd $home

# cleanup
rm -rf $tools_dir

# Remove the setup.sh setup, we don't really need this script anymore, deleting
# it keeps the image as clean as possible.
rm $0; echo "Deleted $0";

