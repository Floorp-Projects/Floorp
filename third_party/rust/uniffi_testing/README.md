This crate contains helper code for testing bindings.  Our general system is to
generate bindings for the libraries from the examples and fixtures
directories, then execute a script that tests the bindings.

Each bindings crate can do this in a different way, but the typical system is:

  - Construct a `UniFFITestHelper` struct to assist the process
  - Call `UniFFITestHelper.create_out_dir()` to create a temp directory to
    store testing files
  - Call `UniFFITestHelper.copy_cdylibs_to_out_dir()` to copy the dylib
    artifacts for the example/fixture library to the `out_dir`.  This is needed
    because the bindings code dynamically links to or loads from this library.
  - Call `UniFFITestHelper.get_compile_sources()` to iterate over (`udl_path`,
    `uniffi_config_path`) pairs and generate the bindings from them. This step
    is specific to the bindings language, it may mean creating a .jar file,
    compiling a binary, or just copying script files over.
  - Execute the test script and check if it succeeds.  This step is also
    specific to the bindings language.
