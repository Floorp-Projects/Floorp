config = {
  'build_command': "build.b2g",
  'expect_file': "expect.b2g.json",

  'default_actions': [
      'clobber',
      'checkout-tools',
      'checkout-sources',
      'get-blobs',
      'clobber-shell',
      'configure-shell',
      'build-shell',
      'clobber-analysis',
      'setup-analysis',
      'run-analysis',
      'collect-analysis-output',
      'upload-analysis',
      'check-expectations',
   ],

    'sixgill_manifest': "build/sixgill-b2g.manifest",
    'b2g_target_compiler_prefix': "target_compiler/gcc/linux-x86/arm/arm-linux-androideabi-4.7/bin/arm-linux-androideabi-",
}
