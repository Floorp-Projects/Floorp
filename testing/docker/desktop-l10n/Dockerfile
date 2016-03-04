FROM		taskcluster/centos6-build-upd:0.1.3.20160122142300
MAINTAINER	Varun Joshi <varunj.1011@gmail.com>

ADD             bin /home/worker/bin
ADD		.hgrc /home/worker/.hgrc
RUN             chmod +x /home/worker/bin/* 

# Generate machine uuid file (like how desktop-build does)
RUN dbus-uuidgen --ensure=/var/lib/dbus/machine-id

ADD		oauth.txt /home/worker
ADD		buildprops.json /home/worker
ENV		MOZ_AUTOMATION=1

CMD ["/bin/bash", "--login"]
