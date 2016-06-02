#!/usr/bin/env bash
# This allows ubuntu-desktop to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive

set -ve

test `whoami` == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('mercurial')
apt_packages+=('python')
apt_packages+=('xz-utils')

apt-get update
apt-get install -y ${apt_packages[@]}

tooltool_fetch() {
    cat >manifest.tt
    /build/tooltool.py fetch
    rm manifest.tt
}


###
# ESLint Setup
###

# install node

# For future reference things like this don't need to be uploaded to tooltool, as long
# as we verify the hash, we can download it from the external net.
tooltool_fetch <<'EOF'
[
{
    "size": 8310316,
    "digest": "95f4fa3d9b215348393dfac4a1c5eff72e9ef85dca38eb69cc8e6c1fe5aada0136c3b182dc04ed5c19fb69f0ac7df85d9c4045b9eb382fcb545b0ccacfece25b",
    "algorithm": "sha512",
    "filename": "node-v4.4.5-linux-x64.tar.xz"
}
]
EOF
tar -C /usr/local --strip-components 1 -xJ < node-*.tar.xz
node -v  # verify
npm -v

# install taskcluster-vcs@2.3.12
tooltool_fetch <<'EOF'
[
{
"size": 6282161,
"visibility": "public",
"digest": "a781a96e596f6403eca6ec2300adb9c1a396659393e16993c66f98a658050e557bc681d521f70b50c1162aa4b435274e0098ffcbd37cbe969c0e4f69be19a1e0",
"algorithm": "sha512",
"filename": "taskcluster-vcs-v2.3.12.tar.gz"
}
]
EOF
npm install -g taskcluster-vcs-v2.3.12.tar.gz

cd /
rm -rf /setup
