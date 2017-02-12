curl -sSf https://static.rust-lang.org/dist/rust-1.10.0-i686-pc-windows-msvc.exe -o rust.exe
rust.exe /VERYSILENT /NORESTART /DIR="C:\Rust"
set PATH=%PATH%;C:\Rust\bin

curl -sSf http://llvm.org/releases/%LLVM_VERSION%.0/LLVM-%LLVM_VERSION%.0-win32.exe -o LLVM.exe
7z x LLVM.exe -oC:\LLVM
set PATH=%PATH%;C:\LLVM\bin
set LIBCLANG_PATH=C:\LLVM\bin
