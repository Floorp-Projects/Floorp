set -ex

mkdir -p target

docker build --rm -t libz-sys-ci ci
docker run \
  --rm \
  --init \
  --user $(id -u):$(id -g) \
  --volume `rustc --print sysroot`:/usr/local:ro \
  --volume `pwd`:/src:ro \
  --volume `pwd`/target:/src/target \
  --workdir /src \
  --env CARGO_HOME=/cargo \
  --volume $HOME/.cargo:/cargo \
  -it \
  libz-sys-ci \
  cargo run --manifest-path systest/Cargo.toml -vv
