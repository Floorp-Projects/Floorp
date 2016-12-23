FROM        ubuntu:14.04
MAINTAINER  Anthony Miyaguchi <amiyaguchi@mozilla.com>

WORKDIR     /tmp

# Add the upload script
ADD         bin /tmp/bin
RUN         chmod +x /tmp/bin/*

# Define the environmental variables for the scripts
COPY        socorro_token   /tmp/
ENV         SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE /tmp/socorro_token
ENV         SCRIPT_PATH toolkit/crashreporter/tools/upload_symbols.py 

# Install dependencies for the script
RUN         apt-get update
RUN         apt-get install -y python python-pip wget
RUN         pip install redo requests

# Default command
CMD         ["/bin/bash", "--login"]
