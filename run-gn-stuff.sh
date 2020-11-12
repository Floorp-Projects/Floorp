hg update -C -r .
hg purge
./mach build-backend -b GnConfigGen
cp obj-x86_64-pc-linux-gnu/third_party/libwebrtc/gn-output/x64_True_x64_linux.json dom/media/webrtc/third_party_build/gn-configs/
./mach build-backend -b GnMozbuildWriter
