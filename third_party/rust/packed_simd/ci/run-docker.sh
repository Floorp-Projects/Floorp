# Small script to run tests for a target (or all targets) inside all the
# respective docker images.

set -ex

run() {
    echo "Building docker container for TARGET=${TARGET} RUSTFLAGS=${RUSTFLAGS}"
    docker build -t packed_simd -f ci/docker/${TARGET}/Dockerfile ci/
    mkdir -p target
    target=$(echo "${TARGET}" | sed 's/-emulated//')
    echo "Running docker"
    docker run \
      --user `id -u`:`id -g` \
      --rm \
      --init \
      --volume $HOME/.cargo:/cargo \
      --env CARGO_HOME=/cargo \
      --volume `rustc --print sysroot`:/rust:ro \
      --env TARGET=$target \
      --env NORUN \
      --env NOVERIFY \
      --env RUSTFLAGS \
      --volume `pwd`:/checkout:ro \
      --volume `pwd`/target:/checkout/target \
      --workdir /checkout \
      --privileged \
      packed_simd \
      bash \
      -c 'PATH=$PATH:/rust/bin exec ci/run.sh'
}

if [ -z "${TARGET}" ]; then
  for d in `ls ci/docker/`; do
    run $d
  done
else
  run ${TARGET}
fi
