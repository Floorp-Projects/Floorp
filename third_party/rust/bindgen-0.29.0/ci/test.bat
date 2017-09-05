@echo off

cd "%~dp0.."

set RUST_BACKTRACE=1

if not defined BINDGEN_FEATURES (
  echo Environment variable BINDGEN_FEATURES must be defined.
  exit /B 1
)

findstr /r /c:"#include *<.*>" tests\headers\* >nul 2>&1 && (
  echo Found a test with an #include directive of a system header file!
  echo.
  echo There is no guarantee that the system running the tests has the header
  echo file, let alone the same version of it that you have. Any test with such an
  echo include directive won't reliably produce the consistent bindings across systems.
  exit /B 1
) || (
  echo Found none. OK!
  set ERRORLEVEL=0
)

@echo on

::Regenerate the test headers' bindings in debug and release modes, and assert
::that we always get the expected generated bindings.

cargo test --features "%BINDGEN_FEATURES%" || exit /b 1
call .\ci\assert-no-diff.bat

cargo test --features "%BINDGEN_FEATURES% testing_only_extra_assertions" || exit /b 1
call .\ci\assert-no-diff.bat

cargo test --release --features "%BINDGEN_FEATURES% testing_only_extra_assertions" || exit /b 1
call .\ci\assert-no-diff.bat

::Now test the expectations' size and alignment tests.

pushd tests\expectations
cargo test || exit /b 1
cargo test --release || exit /b 1
popd

::And finally, test our example bindgen + build.rs integration template project.

cd bindgen-integration
cargo test --features "%BINDGEN_FEATURES%" || exit /b 1
cargo test --release --features "%BINDGEN_FEATURES%" || exit /b 1
