curl -sSf https://static.rust-lang.org/dist/rust-1.36.0-i686-pc-windows-msvc.exe -o rust.exe
rust.exe /VERYSILENT /NORESTART /DIR="C:\Rust"
set PATH=%PATH%;C:\Rust\bin

curl -sSf http://releases.llvm.org/%LLVM_VERSION%/LLVM-%LLVM_VERSION%-win32.exe -o LLVM.exe
7z x LLVM.exe -oC:\LLVM
set PATH=%PATH%;C:\LLVM\bin
set LIBCLANG_PATH=C:\LLVM\bin
