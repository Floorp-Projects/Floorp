FROM          taskcluster/ubuntu1204-test:0.1.0
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

### update to latest from upstream repositories
# if this becomes a long list of packages, consider bumping the
# ubunt1204-test version
RUN apt-get update && apt-get upgrade -y --force-yes

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
