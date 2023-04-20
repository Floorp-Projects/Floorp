FROM freedesktopsdk/flatpak:22.08-x86_64
MAINTAINER release@mozilla.com

RUN mkdir /scripts/
WORKDIR /scripts/
# Copy everything in the docker/firefox-flatpak folder but the Dockerfile
#
# XXX The following pattern is neither a regex nor a glob one. It's
# documented at https://golang.org/pkg/path/filepath/#Match. There's no
# way of explicitly filtering out "Dockerfile". If one day, someone needs
# to add a file starting with "D", then we must revisit the pattern below.
COPY [^D]* /scripts/

RUN ["gcc", "-Wall", "-shared", "-o", "/scripts/close_range.so", "/scripts/close_range.c"]

ENV LD_PRELOAD /scripts/close_range.so

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
