FROM snapcore/snapcraft:stable

RUN apt-get update && apt-get install -qy python3 bzip2 curl git && apt-get clean

RUN mkdir /scripts/
WORKDIR /scripts/
# Copy everything in the docker/firefox-snap folder but the Dockerfile
#
# XXX The following pattern is neither a regex nor a glob one. It's
# documented at https://golang.org/pkg/path/filepath/#Match. There's no
# way of explicitly filtering out "Dockerfile". If one day, someone needs
# to add a file starting with "D", then we must revisit the pattern below.
COPY [^D]* /scripts/

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
