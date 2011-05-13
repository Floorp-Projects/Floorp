function run_test() {
  manifest = do_get_file('bug656331.manifest');
  Components.manager.autoRegister(manifest);

  do_check_false("{f18fb09b-28b4-4435-bc5b-8027f18df743}" in Components.classesByID);
}
