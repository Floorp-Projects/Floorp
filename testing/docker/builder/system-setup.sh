#!/bin/bash -ve

############################### system-setup.sh ###############################

home="/home/worker"
tools_dir="/tools/tools"

mkdir -p $home/bin
mkdir -p $home/tools

chown -R worker:worker $home/* $home/.*

# Install android repo tool
curl https://storage.googleapis.com/git-repo-downloads/repo > $home/bin/repo
chmod a+x $home/bin/repo

# Install build tools
hg clone http://hg.mozilla.org/build/tools/ $tools_dir
cd $tools_dir
python setup.py install

# Initialize git (makes repo happy)
git config --global user.email "docker@docker.com"
git config --global user.name "docker"

cd $home

# Remove the setup.sh setup, we don't really need this script anymore, deleting
# it keeps the image as clean as possible.
rm $0; echo "Deleted $0";

