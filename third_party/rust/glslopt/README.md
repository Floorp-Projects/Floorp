# glslopt-rs

Rust bindings to [glsl-optimizer](https://github.com/jamienicol/glsl-optimizer).

## Updating glsl-optimizer

To update the version of glsl-optimizer, update the git submodule:

```sh
git submodule update --remote glsl-optimizer
```

Then, if required, regenerate the bindings:

```sh
cargo install bindgen
bindgen wrapper.hpp -o src/bindings.rs
```

Then commit the changes.
