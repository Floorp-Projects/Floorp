FROM          taskcluster/centos6-build:0.1.5
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

### update to latest from upstream repositories
# if this becomes a long list of packages, consider bumping the
# centos6-build version
RUN yum update -y

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
