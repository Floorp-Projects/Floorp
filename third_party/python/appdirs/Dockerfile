FROM activestate/activepython:2.7

# For Python3 compact
RUN apt-get -y update && apt-get -y install python3-setuptools && \
	apt-get -y clean

WORKDIR /app
ADD . /app
RUN python setup.py install && python setup.py test
RUN python3 setup.py install && python3 setup.py test

RUN python -m appdirs
RUN python3 -m appdirs
