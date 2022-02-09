# test suite

# XXX: Makefile only runs test_srtp and srtp_driver with valgrind
add_test_setup('valgrind',
  exe_wrapper: ['valgrind', '--leak-check=full'],
  timeout_multiplier: 10)

test_apps = [
  ['srtp_driver', {'extra_sources': 'util.c', 'run_args': '-v'}],
  ['replay_driver', {'extra_sources': 'ut_sim.c', 'run_args': '-v'}],
  ['roc_driver', {'extra_sources': 'ut_sim.c', 'run_args': '-v'}],
  ['rdbx_driver', {'extra_sources': 'ut_sim.c', 'run_args': '-v'}],
  ['test_srtp', {'run_args': '-v'}],
  ['rtpw', {'extra_sources': ['rtp.c', 'util.c', '../crypto/math/datatypes.c'], 'define_test': false}],
]

foreach t : test_apps
  test_name = t.get(0)
  test_dict = t.get(1, {})
  test_extra_sources = test_dict.get('extra_sources', [])
  test_run_args = test_dict.get('run_args', [])

  test_exe = executable(test_name,
    '@0@.c'.format(test_name), 'getopt_s.c', test_extra_sources,
    include_directories: [config_incs, crypto_incs, srtp2_incs, test_incs],
    dependencies: [srtp2_deps, syslibs],
    link_with: libsrtp2_for_tests)

  if test_dict.get('define_test', true)
    test(test_name, test_exe, args: test_run_args)
  else
    set_variable(test_name + '_exe', test_exe)
  endif
endforeach

# rtpw test needs to be run using shell scripts
can_run_rtpw = find_program('sh', 'bash', required: false).found()

# Meson only passes the exe_wrapper to shell scripts starting 0.55
if meson.is_cross_build() and meson.version().version_compare('<0.55')
  can_run_rtpw = false
endif

if can_run_rtpw
  words_txt = files('words.txt')

  rtpw_test_sh = find_program('rtpw_test.sh', required: false)
  if rtpw_test_sh.found()
    test('rtpw_test', rtpw_test_sh,
         args: ['-w', words_txt],
         depends: rtpw_exe,
         is_parallel: false,
         workdir: meson.current_build_dir())
  endif

  rtpw_test_gcm_sh = find_program('rtpw_test_gcm.sh', required: false)
  if (use_openssl or use_nss) and rtpw_test_gcm_sh.found()
    test('rtpw_test_gcm', rtpw_test_gcm_sh,
         args: ['-w', words_txt],
         depends: rtpw_exe,
         is_parallel: false,
         workdir: meson.current_build_dir())
  endif
endif

# rtp_decoder
pcap_dep = dependency('libpcap', required: get_option('pcap-tests'))

if pcap_dep.found()
  executable('rtp_decoder',
    'rtp_decoder.c', 'getopt_s.c', 'rtp.c', 'util.c', 'getopt_s.c',
    '../crypto/math/datatypes.c',
    include_directories: [config_incs, crypto_incs, srtp2_incs, test_incs],
    dependencies: [srtp2_deps, pcap_dep, syslibs],
    link_with: libsrtp2,
    install: false)
endif
