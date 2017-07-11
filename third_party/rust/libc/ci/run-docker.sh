# Small script to run tests for a target (or all targets) inside all the
# respective docker images.

set -ex

run() {
    echo $1
    # use -f so we can use ci/ as build context
    docker build -t libc -f ci/docker/$1/Dockerfile ci/
    mkdir -p target
    if [ -w /dev/kvm ]; then
      kvm="--volume /dev/kvm:/dev/kvm"
    fi
    docker run \
      --user `id -u`:`id -g` \
      --rm \
      --volume $HOME/.cargo:/cargo \
      $kvm \
      --env CARGO_HOME=/cargo \
      --volume `rustc --print sysroot`:/rust:ro \
      --volume `pwd`:/checkout:ro \
      --volume `pwd`/target:/checkout/target \
      --env CARGO_TARGET_DIR=/checkout/target \
      --workdir /checkout \
      --privileged \
      libc \
      ci/run.sh $1
}

if [ -z "$1" ]; then
  for d in `ls ci/docker/`; do
    run $d
  done
else
  run $1
fi
