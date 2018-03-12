FROM snapcore/snapcraft:stable

RUN apt-get update && apt-get install -qy python3 bzip2 curl git && apt-get clean

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
