function run_test()
{
  var dirEntries = do_get_cwd().directoryEntries;

  while (dirEntries.hasMoreElements())
    dirEntries.getNext();

  // We ensure there is no crash
  dirEntries.hasMoreElements();
}
