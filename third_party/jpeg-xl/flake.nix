{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let
          pkgs = import nixpkgs {
            inherit system;
          };
        in
        with pkgs;
        {
          devShells.default = mkShell {
            buildInputs = [
              clang
              cmake
              pkg-config
              gtest
              gmock
              doxygen
              graphviz
              python3
              libclang.python
              libpng
              giflib
              lcms2
              brotli
            ];
            shellHook = ''
              export CC=clang
              export CXX=clang++
            '';
          };
        }
      );
}
