#!/bin/sh
# APT version 2.1.15 and newer changed how they handle quoting in redirections
# in a way that breaks the setup for APT repos in taskcluster artifacts
# (unfortunately, there's also no setup on the taskcluster end that would work
# with both old and newer versions of APT, short of removing redirections
# entirely).
/usr/lib/apt/methods/https | sed -u '/^New-URI:/s/+/%2b/g'
