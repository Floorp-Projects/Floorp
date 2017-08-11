# Small script to run tests for a target (or all targets) inside all the
# respective docker images.

set -ex

run() {
    echo $1
    docker build -t libc ci/docker/$1
    mkdir -p target
    docker run \
      --user `id -u`:`id -g` \
      --rm \
      --volume $HOME/.cargo:/cargo \
      --env CARGO_HOME=/cargo \
      --volume `rustc --print sysroot`:/rust:ro \
      --volume `pwd`:/checkout:ro \
      --volume `pwd`/target:/checkout/target \
      --env CARGO_TARGET_DIR=/checkout/target \
      --workdir /checkout \
      --privileged \
      --interactive \
      --tty \
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
