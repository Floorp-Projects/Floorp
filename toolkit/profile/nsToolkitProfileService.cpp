Looks like configure has not run yet, running it now...
 0:00.68 Clobber not needed.
 0:00.68 Adding make options from /Users/dave/mozilla/bin/entrymozconfig
    RUN_AUTOCONF_LOCALLY=1
    AUTOCLOBBER=1
    MOZ_OBJDIR=/Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0
    OBJDIR=/Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0
    FOUND_MOZCONFIG=/Users/dave/mozilla/bin/entrymozconfig
    export FOUND_MOZCONFIG
 0:00.70 /usr/bin/make -f client.mk -s configure
 0:00.72 cd /Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0
 0:00.72 /Users/dave/mozilla/source/trunk/configure
 0:00.91 Creating Python environment
 0:02.84 New python executable in /Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0/_virtualenvs/init/bin/python2.7
 0:02.84 Also creating executable in /Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0/_virtualenvs/init/bin/python
 0:02.84 Installing setuptools, pip, wheel...done.
 0:03.15 running build_ext
 0:03.15 copying build/lib.macosx-10.14-x86_64-2.7/psutil/_psutil_osx.so -> psutil
 0:03.15 copying build/lib.macosx-10.14-x86_64-2.7/psutil/_psutil_posix.so -> psutil
 0:03.15 
 0:03.15 Error processing command. Ignoring because optional. (optional:packages.txt:comm/build/virtualenv_packages.txt)
 0:03.15 Reexecuting in the virtualenv
 0:03.32 Adding configure options from /Users/dave/mozilla/bin/entrymozconfig
 0:03.32   --enable-application=browser
 0:03.32   --enable-debug
 0:03.32   --disable-optimize
 0:03.32   --with-ccache=/usr/local/bin/sccache
 0:03.32   --with-macos-sdk=/Users/dave/.mozbuild/MacOSX10.13.sdk
 0:03.32   CONFIG=/Users/dave/mozilla/config
 0:03.32 checking for vcs source checkout... hg
 0:03.37 checking for a shell... /bin/sh
 0:03.43 checking for host system type... x86_64-apple-darwin18.5.0
 0:03.43 checking for target system type... x86_64-apple-darwin18.5.0
 0:03.75 checking whether cross compiling... no
 0:03.90 checking for Python 3... /usr/local/bin/python3 (3.7.2)
 0:03.90 checking for hg... /usr/local/bin/hg
 0:04.08 checking for Mercurial version... 4.9
 0:04.28 checking for sparse checkout... no
 0:04.28 checking for yasm... /usr/local/bin/yasm
 0:04.30 checking yasm version... 1.3.0
 0:04.40 checking for ccache... /usr/local/bin/sccache
 0:04.43 checking for the target C compiler... /usr/bin/clang
 0:05.74 checking whether the target C compiler can be used... yes
 0:05.74 checking the target C compiler version... 10.0.1
 0:05.78 checking the target C compiler works... yes
 0:05.78 checking for the target C++ compiler... /usr/bin/clang++
 0:05.88 checking whether the target C++ compiler can be used... yes
 0:05.88 checking the target C++ compiler version... 10.0.1
 0:05.92 checking the target C++ compiler works... yes
 0:05.92 checking for the host C compiler... /usr/bin/clang
 0:06.00 checking whether the host C compiler can be used... yes
 0:06.00 checking the host C compiler version... 10.0.1
 0:06.04 checking the host C compiler works... yes
 0:06.04 checking for the host C++ compiler... /usr/bin/clang++
 0:06.12 checking whether the host C++ compiler can be used... yes
 0:06.12 checking the host C++ compiler version... 10.0.1
 0:06.16 checking the host C++ compiler works... yes
 0:06.20 checking for 64-bit OS... yes
 0:06.21 checking for llvm_profdata... not found
 0:06.21 checking for nasm... /usr/local/bin/nasm
 0:06.23 checking nasm version... 2.14.02
 0:06.30 checking for linker... ld64
 0:06.30 checking for the assembler... /usr/bin/clang
 0:06.34 checking whether the C compiler supports -fsanitize=fuzzer-no-link... yes
 0:06.34 checking for ar... /usr/bin/ar
 0:06.34 checking for pkg_config... not found
 0:06.41 checking for stdint.h... yes
 0:06.48 checking for inttypes.h... yes
 0:06.51 checking for malloc.h... no
 0:06.56 checking for alloca.h... yes
 0:06.60 checking for sys/byteorder.h... no
 0:06.67 checking for getopt.h... yes
 0:06.72 checking for unistd.h... yes
 0:06.78 checking for nl_types.h... yes
 0:06.82 checking for cpuid.h... yes
 0:06.86 checking for sys/statvfs.h... yes
 0:06.91 checking for sys/statfs.h... no
 0:06.96 checking for sys/vfs.h... no
 0:07.04 checking for sys/mount.h... yes
 0:07.21 checking for sys/quota.h... no
 0:07.25 checking for sys/queue.h... yes
 0:07.31 checking for sys/types.h... yes
 0:07.37 checking for netinet/in.h... yes
 0:07.41 checking for byteswap.h... no
 0:07.46 checking for perf_event_open system call... no
 0:07.50 checking whether the C compiler supports -Wbitfield-enum-conversion... yes
 0:07.55 checking whether the C++ compiler supports -Wbitfield-enum-conversion... yes
 0:07.60 checking whether the C compiler supports -Wshadow-field-in-constructor-modified... yes
 0:07.66 checking whether the C++ compiler supports -Wshadow-field-in-constructor-modified... yes
 0:07.71 checking whether the C compiler supports -Wunreachable-code-return... yes
 0:07.77 checking whether the C++ compiler supports -Wunreachable-code-return... yes
 0:07.81 checking whether the C compiler supports -Wclass-varargs... yes
 0:07.85 checking whether the C++ compiler supports -Wclass-varargs... yes
 0:07.89 checking whether the C compiler supports -Wfloat-overflow-conversion... yes
 0:07.93 checking whether the C++ compiler supports -Wfloat-overflow-conversion... yes
 0:07.97 checking whether the C compiler supports -Wfloat-zero-conversion... yes
 0:08.01 checking whether the C++ compiler supports -Wfloat-zero-conversion... yes
 0:08.04 checking whether the C compiler supports -Wloop-analysis... yes
 0:08.07 checking whether the C++ compiler supports -Wloop-analysis... yes
 0:08.11 checking whether the C++ compiler supports -Wc++1z-compat... yes
 0:08.16 checking whether the C++ compiler supports -Wc++2a-compat... yes
 0:08.20 checking whether the C++ compiler supports -Wcomma... yes
 0:08.24 checking whether the C compiler supports -Wduplicated-cond... no
 0:08.28 checking whether the C++ compiler supports -Wduplicated-cond... no
 0:08.33 checking whether the C++ compiler supports -Wimplicit-fallthrough... yes
 0:08.38 checking whether the C compiler supports -Wstring-conversion... yes
 0:08.42 checking whether the C++ compiler supports -Wstring-conversion... yes
 0:08.46 checking whether the C compiler supports -Wtautological-overlap-compare... yes
 0:08.50 checking whether the C++ compiler supports -Wtautological-overlap-compare... yes
 0:08.55 checking whether the C compiler supports -Wtautological-unsigned-enum-zero-compare... yes
 0:08.58 checking whether the C++ compiler supports -Wtautological-unsigned-enum-zero-compare... yes
 0:08.62 checking whether the C compiler supports -Wtautological-unsigned-zero-compare... yes
 0:08.66 checking whether the C++ compiler supports -Wtautological-unsigned-zero-compare... yes
 0:08.70 checking whether the C++ compiler supports -Wno-inline-new-delete... yes
 0:08.74 checking whether the C compiler supports -Wno-error=maybe-uninitialized... no
 0:08.79 checking whether the C++ compiler supports -Wno-error=maybe-uninitialized... no
 0:08.84 checking whether the C compiler supports -Wno-error=deprecated-declarations... yes
 0:08.89 checking whether the C++ compiler supports -Wno-error=deprecated-declarations... yes
 0:08.94 checking whether the C compiler supports -Wno-error=array-bounds... yes
 0:08.98 checking whether the C++ compiler supports -Wno-error=array-bounds... yes
 0:09.01 checking whether the C compiler supports -Wno-error=coverage-mismatch... no
 0:09.05 checking whether the C++ compiler supports -Wno-error=coverage-mismatch... no
 0:09.09 checking whether the C compiler supports -Wno-error=backend-plugin... yes
 0:09.13 checking whether the C++ compiler supports -Wno-error=backend-plugin... yes
 0:09.16 checking whether the C compiler supports -Wno-error=free-nonheap-object... no
 0:09.20 checking whether the C++ compiler supports -Wno-error=free-nonheap-object... no
 0:09.24 checking whether the C compiler supports -Wno-error=multistatement-macros... no
 0:09.27 checking whether the C++ compiler supports -Wno-error=multistatement-macros... no
 0:09.30 checking whether the C compiler supports -Wno-error=return-std-move... yes
 0:09.34 checking whether the C++ compiler supports -Wno-error=return-std-move... yes
 0:09.38 checking whether the C compiler supports -Wno-error=class-memaccess... no
 0:09.42 checking whether the C++ compiler supports -Wno-error=class-memaccess... no
 0:09.45 checking whether the C compiler supports -Wno-error=atomic-alignment... yes
 0:09.49 checking whether the C++ compiler supports -Wno-error=atomic-alignment... yes
 0:09.53 checking whether the C compiler supports -Wno-error=deprecated-copy... no
 0:09.56 checking whether the C++ compiler supports -Wno-error=deprecated-copy... no
 0:09.60 checking whether the C compiler supports -Wformat... yes
 0:09.63 checking whether the C++ compiler supports -Wformat... yes
 0:09.67 checking whether the C compiler supports -Wformat-security... yes
 0:09.71 checking whether the C++ compiler supports -Wformat-security... yes
 0:09.74 checking whether the C compiler supports -Wformat-overflow=2... no
 0:09.77 checking whether the C++ compiler supports -Wformat-overflow=2... no
 0:09.81 checking whether the C compiler supports -Wno-gnu-zero-variadic-macro-arguments... yes
 0:09.84 checking whether the C++ compiler supports -Wno-gnu-zero-variadic-macro-arguments... yes
 0:09.88 checking whether the C++ compiler supports -fno-sized-deallocation... yes
 0:09.89 checking for rustc... /Users/dave/.cargo/bin/rustc
 0:09.89 checking for cargo... /Users/dave/.cargo/bin/cargo
 0:10.07 checking rustc version... 1.33.0
 0:10.15 checking cargo version... 1.33.0
 0:10.56 checking for rust target triplet... x86_64-apple-darwin
 0:10.64 checking for rust host triplet... x86_64-apple-darwin
 0:10.64 checking for rustdoc... /Users/dave/.cargo/bin/rustdoc
 0:10.64 checking for cbindgen... /Users/dave/.cargo/bin/cbindgen
 0:10.66 checking cbindgen version... 0.8.2
 0:10.66 checking for rustfmt... /Users/dave/.cargo/bin/rustfmt
 0:12.57 checking for llvm-config... /usr/local/opt/llvm/bin/llvm-config
 0:12.78 checking bindgen cflags... -x c++ -fno-sized-deallocation -DTRACING=1 -DIMPL_LIBXUL -DMOZILLA_INTERNAL_API -DRUST_BINDGEN -DOS_POSIX=1 -DOS_MACOSX=1 -stdlib=libc++
 0:12.80 checking for nodejs... /Users/dave/.mozbuild/node/bin/node (8.11.3)
 0:12.81 checking for tar... /usr/local/bin/gtar
 0:12.81 checking for unzip... /usr/bin/unzip
 0:12.81 checking for zip... /usr/bin/zip
 0:12.81 checking for gn... not found
 0:12.81 checking for the Mozilla API key... no
 0:12.81 checking for the Google Location Service API key... no
 0:12.81 checking for the Google Safebrowsing API key... no
 0:12.81 checking for the Bing API key... no
 0:12.81 checking for the Adjust SDK key... no
 0:12.81 checking for the Leanplum SDK key... no
 0:12.81 checking for the Pocket API key... no
 0:12.83 checking for awk... /usr/bin/awk
 0:12.83 checking for perl... /usr/bin/perl
 0:12.86 checking for minimum required perl version >= 5.006... 5.018002
 0:12.89 checking for full perl installation... yes
 0:12.89 checking for gmake... /Applications/Xcode.app/Contents/Developer/usr/bin/make
 0:12.91 checking for watchman... /usr/local/bin/watchman
 0:12.91 checking for watchman version... 4.9.0
 0:12.91 checking for watchman Mercurial integration... yes
 0:12.91 checking for xargs... /usr/bin/xargs
 0:12.91 checking for dsymutil... /usr/bin/dsymutil
 0:12.92 checking for mkfshfs... /sbin/newfs_hfs
 0:12.92 checking for hfs_tool... not found
 0:12.94 checking for llvm-objdump... /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/llvm-objdump
 0:12.94 checking for autoconf... /usr/local/bin/autoconf213
 0:12.94 Refreshing /Users/dave/mozilla/source/trunk/old-configure with /usr/local/bin/autoconf213
 0:15.73 creating cache ./config.cache
 0:15.80 checking host system type... x86_64-apple-darwin18.5.0
 0:15.84 checking target system type... x86_64-apple-darwin18.5.0
 0:15.88 checking build system type... x86_64-apple-darwin18.5.0
 0:15.89 checking for gcc... (cached) /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99
 0:15.89 checking whether the C compiler (/usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99   -Wl,-syslibroot,/Users/dave/.mozbuild/MacOSX10.13.sdk) works... (cached) yes
 0:15.89 checking whether the C compiler (/usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99   -Wl,-syslibroot,/Users/dave/.mozbuild/MacOSX10.13.sdk) is a cross-compiler... no
 0:15.89 checking whether we are using GNU C... (cached) yes
 0:15.89 checking whether /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99 accepts -g... (cached) yes
 0:15.89 checking for c++... (cached) /usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14
 0:15.90 checking whether the C++ compiler (/usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14   -Wl,-syslibroot,/Users/dave/.mozbuild/MacOSX10.13.sdk) works... (cached) yes
 0:15.90 checking whether the C++ compiler (/usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14   -Wl,-syslibroot,/Users/dave/.mozbuild/MacOSX10.13.sdk) is a cross-compiler... no
 0:15.90 checking whether we are using GNU C++... (cached) yes
 0:15.90 checking whether /usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14 accepts -g... (cached) yes
 0:15.90 checking for ranlib... ranlib
 0:15.90 checking for /usr/local/bin/sccache... /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99
 0:15.91 checking for strip... strip
 0:15.91 checking for otool... otool
 0:16.20 checking for X... no
 0:16.34 checking for --noexecstack option to as... yes
 0:16.39 checking for -z noexecstack option to ld... no
 0:16.44 checking for -z text option to ld... no
 0:16.49 checking for -z relro option to ld... no
 0:16.55 checking for -z nocopyreloc option to ld... no
 0:16.60 checking for -Bsymbolic-functions option to ld... no
 0:16.65 checking for --build-id=sha1 option to ld... no
 0:16.69 checking for --ignore-unresolved-symbol option to ld... no
 0:16.73 checking if toolchain supports -mssse3 option... yes
 0:16.77 checking if toolchain supports -msse4.1 option... yes
 0:16.82 checking for x86 AVX2 asm support in compiler... yes
 0:16.86 checking for iOS target... no
 0:16.86 /Users/dave/mozilla/source/trunk/old-configure: line 4548: test: argument expected
 0:16.93 checking for -dead_strip option to ld... yes
 0:16.99 checking for valid debug flags... yes
 0:17.05 checking for working const... yes
 0:17.11 checking for mode_t... yes
 0:17.16 checking for off_t... yes
 0:17.20 checking for pid_t... yes
 0:17.25 checking for size_t... yes
 0:17.43 checking whether 64-bits std::atomic requires -latomic... no
 0:17.49 checking for dirent.h that defines DIR... yes
 0:17.55 checking for opendir in -ldir... no
 0:17.61 checking for sockaddr_in.sin_len... true
 0:17.67 checking for sockaddr_in6.sin6_len... true
 0:17.73 checking for sockaddr.sa_len... true
 0:17.79 checking for gethostbyname_r in -lc_r... no
 0:17.86 checking for dladdr... yes
 0:17.93 checking for memmem... yes
 0:17.99 checking for socket in -lsocket... no
 0:18.05 checking whether /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99 accepts -pthread... yes
 0:18.11 checking for pthread.h... yes
 0:18.18 checking for stat64... yes
 0:18.26 checking for lstat64... yes
 0:18.38 checking for truncate64... no
 0:18.48 checking for statvfs64... no
 0:18.59 checking for statvfs... yes
 0:18.68 checking for statfs64... yes
 0:18.76 checking for statfs... yes
 0:18.82 checking for getpagesize... yes
 0:18.90 checking for gmtime_r... yes
 0:18.98 checking for localtime_r... yes
 0:19.08 checking for arc4random... yes
 0:19.16 checking for arc4random_buf... yes
 0:19.24 checking for mallinfo... no
 0:19.32 checking for gettid... no
 0:19.38 checking for lchown... yes
 0:19.45 checking for setpriority... yes
 0:19.52 checking for strerror... yes
 0:19.59 checking for syscall... yes
 0:19.72 checking for clock_gettime(CLOCK_MONOTONIC)... no
 0:19.79 checking for pthread_cond_timedwait_monotonic_np...
 0:19.89 checking for res_ninit()... no
 0:19.95 checking for an implementation of va_copy()... yes
 0:20.05 checking whether va_list can be copied by value... no
 0:20.15 checking for __thread keyword for TLS variables... yes
 0:20.22 checking for localeconv... yes
 0:20.28 checking for malloc.h... no
 0:20.32 checking for malloc_np.h... no
 0:20.41 checking for malloc/malloc.h... yes
 0:20.48 checking for strndup... yes
 0:20.55 checking for posix_memalign... yes
 0:20.61 checking for memalign... no
 0:20.67 checking for malloc_usable_size... no
 0:20.71 checking for valloc in malloc.h... no
 0:20.77 checking for valloc in unistd.h... yes
 0:20.81 checking for _aligned_malloc in malloc.h... no
 0:20.81 checking NSPR selection... source-tree
 0:20.81 checking if app-specific confvars.sh exists... /Users/dave/mozilla/source/trunk/browser/confvars.sh
 0:21.26 checking for CoreMedia/CoreMedia.h... yes
 0:21.45 checking for VideoToolbox/VideoToolbox.h... yes
 0:21.47 checking for wget... wget
 0:21.64 checking for fdatasync... yes
 0:21.78 checking for __cxa_demangle... yes
 0:21.85 checking for unwind.h... yes
 0:21.97 checking for _Unwind_Backtrace... yes
 0:21.99 checking for -pipe support... yes
 0:23.18 checking what kind of list files are supported by the linker... filelist
 0:23.43 checking for posix_fadvise... no
 0:23.53 checking for posix_fallocate... no
 0:23.55 updating cache ./config.cache
 0:23.56 creating ./config.data
 0:23.58 js/src> configuring
 0:23.59 js/src> running /Users/dave/mozilla/source/trunk/configure.py --enable-project=js --host=x86_64-apple-darwin18.5.0 --target=x86_64-apple-darwin18.5.0 --enable-tests --enable-debug --enable-rust-debug --disable-release --disable-optimize --enable-xcode-checks --with-ccache=/usr/local/bin/sccache --without-toolchain-prefix --enable-debug-symbols --disable-profile-generate --disable-profile-use --without-pgo-profile-path --disable-lto --disable-address-sanitizer --disable-undefined-sanitizer --disable-coverage --disable-linker --disable-clang-plugin --disable-mozsearch-plugin --disable-stdcxx-compat --enable-jemalloc --enable-replace-malloc --without-linux-headers --disable-warnings-as-errors --disable-valgrind --without-libclang-path --without-clang-path --disable-js-shell --disable-shared-js --disable-export-js --enable-ion --disable-simulator --disable-instruments --disable-callgrind --enable-profiling --disable-vtune --disable-gc-trace --enable-gczeal --disable-small-chunk-size --enable-trace-logging --disable-oom-breakpoint --disable-perf --enable-jitspew --enable-masm-verbose --disable-more-deterministic --enable-ctypes --without-system-ffi --disable-fuzzing --disable-pipeline-operator --enable-binast --enable-cranelift --enable-wasm-codegen-debug --enable-typed-objects --enable-wasm-bulk-memory --enable-wasm-reftypes --enable-wasm-gc --enable-wasm-private-reftypes --with-nspr-cflags=-I/Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0/dist/include/nspr --with-nspr-libs=-L/Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0/dist/lib -lnspr4 -lplc4 -lplds4 --prefix=/Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0/dist JS_STANDALONE=
 0:23.60 js/src> checking for vcs source checkout... hg
 0:23.63 js/src> checking for a shell... /bin/sh
 0:23.64 js/src> checking for host system type... x86_64-apple-darwin18.5.0
 0:23.66 js/src> checking for target system type... x86_64-apple-darwin18.5.0
 0:23.94 js/src> checking whether cross compiling... no
 0:23.97 js/src> checking for Python 3... /usr/local/bin/python3 (3.7.2)
 0:23.97 js/src> checking for hg... /usr/local/bin/hg
 0:24.14 js/src> checking for Mercurial version... 4.9
 0:24.34 js/src> checking for sparse checkout... no
 0:24.34 js/src> checking for yasm... /usr/local/bin/yasm
 0:24.34 js/src> checking yasm version... 1.3.0
 0:24.38 js/src> checking for ccache... /usr/local/bin/sccache
 0:24.40 js/src> checking for the target C compiler... /usr/bin/clang
 0:24.44 js/src> checking whether the target C compiler can be used... yes
 0:24.44 js/src> checking the target C compiler version... 10.0.1
 0:24.48 js/src> checking the target C compiler works... yes
 0:24.48 js/src> checking for the target C++ compiler... /usr/bin/clang++
 0:24.52 js/src> checking whether the target C++ compiler can be used... yes
 0:24.52 js/src> checking the target C++ compiler version... 10.0.1
 0:24.56 js/src> checking the target C++ compiler works... yes
 0:24.56 js/src> checking for the host C compiler... /usr/bin/clang
 0:24.59 js/src> checking whether the host C compiler can be used... yes
 0:24.59 js/src> checking the host C compiler version... 10.0.1
 0:24.63 js/src> checking the host C compiler works... yes
 0:24.63 js/src> checking for the host C++ compiler... /usr/bin/clang++
 0:24.67 js/src> checking whether the host C++ compiler can be used... yes
 0:24.67 js/src> checking the host C++ compiler version... 10.0.1
 0:24.71 js/src> checking the host C++ compiler works... yes
 0:24.75 js/src> checking for 64-bit OS... yes
 0:24.75 js/src> checking for llvm_profdata... not found
 0:24.75 js/src> checking for nasm... /usr/local/bin/nasm
 0:24.76 js/src> checking nasm version... 2.14.02
 0:24.80 js/src> checking for linker... ld64
 0:24.80 js/src> checking for the assembler... /usr/bin/clang
 0:24.84 js/src> checking whether the C compiler supports -fsanitize=fuzzer-no-link... yes
 0:24.84 js/src> checking for ar... /usr/bin/ar
 0:24.84 js/src> checking for pkg_config... not found
 0:24.88 js/src> checking for stdint.h... yes
 0:24.94 js/src> checking for inttypes.h... yes
 0:24.98 js/src> checking for malloc.h... no
 0:25.02 js/src> checking for alloca.h... yes
 0:25.06 js/src> checking for sys/byteorder.h... no
 0:25.12 js/src> checking for getopt.h... yes
 0:25.16 js/src> checking for unistd.h... yes
 0:25.20 js/src> checking for nl_types.h... yes
 0:25.24 js/src> checking for cpuid.h... yes
 0:25.28 js/src> checking for sys/statvfs.h... yes
 0:25.32 js/src> checking for sys/statfs.h... no
 0:25.36 js/src> checking for sys/vfs.h... no
 0:25.41 js/src> checking for sys/mount.h... yes
 0:25.47 js/src> checking for sys/quota.h... no
 0:25.51 js/src> checking for sys/queue.h... yes
 0:25.55 js/src> checking for sys/types.h... yes
 0:25.60 js/src> checking for netinet/in.h... yes
 0:25.64 js/src> checking for byteswap.h... no
 0:25.68 js/src> checking for perf_event_open system call... no
 0:25.71 js/src> checking whether the C compiler supports -Wbitfield-enum-conversion... yes
 0:25.75 js/src> checking whether the C++ compiler supports -Wbitfield-enum-conversion... yes
 0:25.79 js/src> checking whether the C compiler supports -Wshadow-field-in-constructor-modified... yes
 0:25.83 js/src> checking whether the C++ compiler supports -Wshadow-field-in-constructor-modified... yes
 0:25.86 js/src> checking whether the C compiler supports -Wunreachable-code-return... yes
 0:25.89 js/src> checking whether the C++ compiler supports -Wunreachable-code-return... yes
 0:25.93 js/src> checking whether the C compiler supports -Wclass-varargs... yes
 0:25.97 js/src> checking whether the C++ compiler supports -Wclass-varargs... yes
 0:26.01 js/src> checking whether the C compiler supports -Wfloat-overflow-conversion... yes
 0:26.05 js/src> checking whether the C++ compiler supports -Wfloat-overflow-conversion... yes
 0:26.07 js/src> checking whether the C compiler supports -Wfloat-zero-conversion... yes
 0:26.11 js/src> checking whether the C++ compiler supports -Wfloat-zero-conversion... yes
 0:26.15 js/src> checking whether the C compiler supports -Wloop-analysis... yes
 0:26.19 js/src> checking whether the C++ compiler supports -Wloop-analysis... yes
 0:26.22 js/src> checking whether the C++ compiler supports -Wc++1z-compat... yes
 0:26.26 js/src> checking whether the C++ compiler supports -Wc++2a-compat... yes
 0:26.30 js/src> checking whether the C++ compiler supports -Wcomma... yes
 0:26.34 js/src> checking whether the C compiler supports -Wduplicated-cond... no
 0:26.37 js/src> checking whether the C++ compiler supports -Wduplicated-cond... no
 0:26.40 js/src> checking whether the C++ compiler supports -Wimplicit-fallthrough... yes
 0:26.44 js/src> checking whether the C compiler supports -Wstring-conversion... yes
 0:26.48 js/src> checking whether the C++ compiler supports -Wstring-conversion... yes
 0:26.52 js/src> checking whether the C compiler supports -Wtautological-overlap-compare... yes
 0:26.56 js/src> checking whether the C++ compiler supports -Wtautological-overlap-compare... yes
 0:26.58 js/src> checking whether the C compiler supports -Wtautological-unsigned-enum-zero-compare... yes
 0:26.62 js/src> checking whether the C++ compiler supports -Wtautological-unsigned-enum-zero-compare... yes
 0:26.66 js/src> checking whether the C compiler supports -Wtautological-unsigned-zero-compare... yes
 0:26.69 js/src> checking whether the C++ compiler supports -Wtautological-unsigned-zero-compare... yes
 0:26.73 js/src> checking whether the C++ compiler supports -Wno-inline-new-delete... yes
 0:26.77 js/src> checking whether the C compiler supports -Wno-error=maybe-uninitialized... no
 0:26.81 js/src> checking whether the C++ compiler supports -Wno-error=maybe-uninitialized... no
 0:26.84 js/src> checking whether the C compiler supports -Wno-error=deprecated-declarations... yes
 0:26.88 js/src> checking whether the C++ compiler supports -Wno-error=deprecated-declarations... yes
 0:26.92 js/src> checking whether the C compiler supports -Wno-error=array-bounds... yes
 0:26.95 js/src> checking whether the C++ compiler supports -Wno-error=array-bounds... yes
 0:26.99 js/src> checking whether the C compiler supports -Wno-error=coverage-mismatch... no
 0:27.03 js/src> checking whether the C++ compiler supports -Wno-error=coverage-mismatch... no
 0:27.06 js/src> checking whether the C compiler supports -Wno-error=backend-plugin... yes
 0:27.10 js/src> checking whether the C++ compiler supports -Wno-error=backend-plugin... yes
 0:27.13 js/src> checking whether the C compiler supports -Wno-error=free-nonheap-object... no
 0:27.17 js/src> checking whether the C++ compiler supports -Wno-error=free-nonheap-object... no
 0:27.21 js/src> checking whether the C compiler supports -Wno-error=multistatement-macros... no
 0:27.25 js/src> checking whether the C++ compiler supports -Wno-error=multistatement-macros... no
 0:27.29 js/src> checking whether the C compiler supports -Wno-error=return-std-move... yes
 0:27.32 js/src> checking whether the C++ compiler supports -Wno-error=return-std-move... yes
 0:27.35 js/src> checking whether the C compiler supports -Wno-error=class-memaccess... no
 0:27.39 js/src> checking whether the C++ compiler supports -Wno-error=class-memaccess... no
 0:27.43 js/src> checking whether the C compiler supports -Wno-error=atomic-alignment... yes
 0:27.46 js/src> checking whether the C++ compiler supports -Wno-error=atomic-alignment... yes
 0:27.50 js/src> checking whether the C compiler supports -Wno-error=deprecated-copy... no
 0:27.54 js/src> checking whether the C++ compiler supports -Wno-error=deprecated-copy... no
 0:27.57 js/src> checking whether the C compiler supports -Wformat... yes
 0:27.61 js/src> checking whether the C++ compiler supports -Wformat... yes
 0:27.64 js/src> checking whether the C compiler supports -Wformat-security... yes
 0:27.67 js/src> checking whether the C++ compiler supports -Wformat-security... yes
 0:27.71 js/src> checking whether the C compiler supports -Wformat-overflow=2... no
 0:27.75 js/src> checking whether the C++ compiler supports -Wformat-overflow=2... no
 0:27.79 js/src> checking whether the C compiler supports -Wno-gnu-zero-variadic-macro-arguments... yes
 0:27.83 js/src> checking whether the C++ compiler supports -Wno-gnu-zero-variadic-macro-arguments... yes
 0:27.87 js/src> checking whether the C++ compiler supports -Wno-noexcept-type... yes
 0:27.89 js/src> checking whether the C++ compiler supports -fno-sized-deallocation... yes
 0:27.90 js/src> checking for rustc... /Users/dave/.cargo/bin/rustc
 0:27.90 js/src> checking for cargo... /Users/dave/.cargo/bin/cargo
 0:27.94 js/src> checking rustc version... 1.33.0
 0:27.98 js/src> checking cargo version... 1.33.0
 0:28.12 js/src> checking for rust target triplet... x86_64-apple-darwin
 0:28.20 js/src> checking for rust host triplet... x86_64-apple-darwin
 0:28.20 js/src> checking for rustdoc... /Users/dave/.cargo/bin/rustdoc
 0:28.20 js/src> checking for rustfmt... /Users/dave/.cargo/bin/rustfmt
 0:29.53 js/src> checking for llvm-config... /usr/local/opt/llvm/bin/llvm-config
 0:29.60 js/src> checking bindgen cflags... -x c++ -fno-sized-deallocation -DTRACING=1 -DIMPL_LIBXUL -DMOZILLA_INTERNAL_API -DRUST_BINDGEN -DOS_POSIX=1 -DOS_MACOSX=1 -stdlib=libc++
 0:29.61 js/src> checking for awk... /usr/bin/awk
 0:29.61 js/src> checking for perl... /usr/bin/perl
 0:29.62 js/src> checking for minimum required perl version >= 5.006... 5.018002
 0:29.65 js/src> checking for full perl installation... yes
 0:29.65 js/src> checking for gmake... /Applications/Xcode.app/Contents/Developer/usr/bin/make
 0:29.68 js/src> checking for watchman... /usr/local/bin/watchman
 0:29.68 js/src> checking for watchman version... 4.9.0
 0:29.68 js/src> checking for watchman Mercurial integration... yes
 0:29.68 js/src> checking for xargs... /usr/bin/xargs
 0:29.68 js/src> checking for dsymutil... /usr/bin/dsymutil
 0:29.68 js/src> checking for mkfshfs... /sbin/newfs_hfs
 0:29.68 js/src> checking for hfs_tool... not found
 0:29.70 js/src> checking for llvm-objdump... /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/llvm-objdump
 0:29.70 js/src> checking for autoconf... /usr/local/bin/autoconf213
 0:29.80 js/src> loading cache /Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0/./config.cache
 0:29.83 js/src> checking host system type... x86_64-apple-darwin18.5.0
 0:29.85 js/src> checking target system type... x86_64-apple-darwin18.5.0
 0:29.87 js/src> checking build system type... x86_64-apple-darwin18.5.0
 0:29.88 js/src> checking for gcc... (cached) /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99
 0:29.88 js/src> checking whether the C compiler (/usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99   ) works... (cached) yes
 0:29.88 js/src> checking whether the C compiler (/usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99   ) is a cross-compiler... no
 0:29.88 js/src> checking whether we are using GNU C... (cached) yes
 0:29.88 js/src> checking whether /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99 accepts -g... (cached) yes
 0:29.88 js/src> checking for c++... (cached) /usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14
 0:29.88 js/src> checking whether the C++ compiler (/usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14   ) works... (cached) yes
 0:29.88 js/src> checking whether the C++ compiler (/usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14   ) is a cross-compiler... no
 0:29.88 js/src> checking whether we are using GNU C++... (cached) yes
 0:29.89 js/src> checking whether /usr/local/bin/sccache /usr/bin/clang++ -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu++14 accepts -g... (cached) yes
 0:29.89 js/src> checking for ranlib... (cached) ranlib
 0:29.89 js/src> checking for /usr/local/bin/sccache... (cached) /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99
 0:29.89 js/src> checking for strip... (cached) strip
 0:29.89 js/src> checking for sb-conf... no
 0:29.89 js/src> checking for ve... no
 0:29.90 js/src> checking for X... (cached) no
 0:30.06 js/src> checking for --noexecstack option to as... yes
 0:30.13 js/src> checking for -z noexecstack option to ld... no
 0:30.19 js/src> checking for -z text option to ld... no
 0:30.25 js/src> checking for -z relro option to ld... no
 0:30.30 js/src> checking for -z nocopyreloc option to ld... no
 0:30.36 js/src> checking for -Bsymbolic-functions option to ld... no
 0:30.42 js/src> checking for --build-id=sha1 option to ld... no
 0:30.50 js/src> checking for -framework ExceptionHandling... yes
 0:30.56 js/src> checking for -dead_strip option to ld... yes
 0:30.62 js/src> checking for valid debug flags... yes
 0:30.62 js/src> checking for working const... (cached) yes
 0:30.62 js/src> checking for mode_t... (cached) yes
 0:30.62 js/src> checking for off_t... (cached) yes
 0:30.62 js/src> checking for pid_t... (cached) yes
 0:30.62 js/src> checking for size_t... (cached) yes
 0:30.68 js/src> checking for ssize_t... yes
 0:30.69 js/src> checking whether 64-bits std::atomic requires -latomic... (cached) no
 0:30.69 js/src> checking for dirent.h that defines DIR... (cached) yes
 0:30.71 js/src> checking for opendir in -ldir... (cached) no
 0:30.71 js/src> checking for gethostbyname_r in -lc_r... (cached) no
 0:30.71 js/src> checking for socket in -lsocket... (cached) no
 0:30.78 js/src> checking whether /usr/local/bin/sccache /usr/bin/clang -isysroot /Users/dave/.mozbuild/MacOSX10.13.sdk -std=gnu99 accepts -pthread... yes
 0:30.85 js/src> checking for getc_unlocked... yes
 0:30.93 js/src> checking for _getc_nolock... no
 0:30.93 js/src> checking for gmtime_r... (cached) yes
 0:30.95 js/src> checking for localtime_r... (cached) yes
 0:31.03 js/src> checking for pthread_getname_np... yes
 0:31.11 js/src> checking for pthread_get_name_np... no
 0:31.11 js/src> checking for clock_gettime(CLOCK_MONOTONIC)... (cached) no
 0:31.17 js/src> checking for sin in -lm... yes
 0:31.27 js/src> checking for sincos in -lm... no
 0:31.34 js/src> checking for __sincos in -lm... yes
 0:31.35 js/src> checking for res_ninit()... (cached) no
 0:31.42 js/src> checking for nl_langinfo and CODESET... yes
 0:31.43 js/src> checking for an implementation of va_copy()... (cached) yes
 0:31.43 js/src> checking whether va_list can be copied by value... (cached) no
 0:31.44 js/src> checking for __thread keyword for TLS variables... (cached) yes
 0:31.45 js/src> checking for localeconv... (cached) yes
 0:31.46 js/src> checking NSPR selection... command-line
 0:31.47 js/src> checking for __cxa_demangle... (cached) yes
 0:31.49 js/src> checking for -pipe support... yes
 0:31.54 js/src> checking for tm_zone tm_gmtoff in struct tm... yes
 0:32.68 js/src> checking what kind of list files are supported by the linker... filelist
 0:32.72 js/src> checking for posix_fadvise... (cached) no
 0:32.72 js/src> checking for posix_fallocate... (cached) no
 0:32.73 js/src> checking for malloc.h... (cached) no
 0:32.74 js/src> checking for malloc_np.h... (cached) no
 0:32.74 js/src> checking for malloc/malloc.h... (cached) yes
 0:32.75 js/src> checking for strndup... (cached) yes
 0:32.77 js/src> checking for posix_memalign... (cached) yes
 0:32.78 js/src> checking for memalign... (cached) no
 0:32.78 js/src> checking for malloc_usable_size... (cached) no
 0:32.83 js/src> checking for valloc in malloc.h... no
 0:32.89 js/src> checking for valloc in unistd.h... yes
 0:32.93 js/src> checking for _aligned_malloc in malloc.h... no
 0:32.93 js/src> checking for localeconv... (cached) yes
 0:32.96 js/src> updating cache /Users/dave/mozilla/source/trunk/obj-x86_64-apple-darwin18.5.0/./config.cache
 0:32.97 js/src> creating ./config.data
 0:32.99 js/src> Creating config.status
 0:33.11 Creating config.status
 0:33.32 Reticulating splines...
 0:35.68  0:02.39 File already read. Skipping: /Users/dave/mozilla/source/trunk/gfx/angle/targets/angle_common/moz.build
 0:47.03 Finished reading 2030 moz.build files in 2.76s
 0:47.03 Read 64 gyp files in parallel contributing 0.00s to total wall time
 0:47.03 Processed into 10568 build config descriptors in 5.50s
 0:47.03 RecursiveMake backend executed in 4.51s
 0:47.03   3745 total backend files; 3745 created; 0 updated; 0 unchanged; 0 deleted; 33 -> 1366 Makefile
 0:47.03 FasterMake backend executed in 0.39s
 0:47.03   14 total backend files; 14 created; 0 updated; 0 unchanged; 0 deleted
 0:47.03 Total wall time: 13.73s; CPU time: 11.71s; Efficiency: 85%; Untracked: 0.56s
Configure complete!
Be sure to run |mach build| to pick up any changes
