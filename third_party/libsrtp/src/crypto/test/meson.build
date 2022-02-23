# crypto test suite

test_apps = [
  'cipher_driver',
  'datatypes_driver',
  'kernel_driver',
  'env',
]

if not use_openssl and not use_nss
  test_apps += ['sha1_driver']
endif

foreach test_name : test_apps
  test_exe = executable(test_name,
    '@0@.c'.format(test_name), '../../test/getopt_s.c', '../../test/util.c',
    include_directories: [config_incs, crypto_incs, srtp2_incs, test_incs],
    dependencies: [srtp2_deps, syslibs],
    link_with: libsrtp2_for_tests)
  test(test_name, test_exe, args: ['-v'])
endforeach

if not use_openssl and not use_nss
  test_exe = executable('aes_calc',
    'aes_calc.c', '../../test/getopt_s.c', '../../test/util.c',
    include_directories: [config_incs, crypto_incs, srtp2_incs, test_incs],
    dependencies: [srtp2_deps, syslibs],
    link_with: libsrtp2_for_tests)

  # data values used to test the aes_calc application for AES-128
  k128 = '000102030405060708090a0b0c0d0e0f'
  p128 = '00112233445566778899aabbccddeeff'
  c128 = '69c4e0d86a7b0430d8cdb78070b4c55a'
  test('aes_calc_128', test_exe, args: [k128, p128, c128])

  # data values used to test the aes_calc application for AES-256
  k256 = '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f'
  p256 = '00112233445566778899aabbccddeeff'
  c256 = '8ea2b7ca516745bfeafc49904b496089'
  test('aes_calc_256', test_exe, args: [k256, p256, c256])
endif
