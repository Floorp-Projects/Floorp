Please note that no Mozmill restart tests are available, which use local web
pages delivered by httpd. Because of that, a dummy test has to live inside the
restartTests folder, which circumvents another problem on buildbot, where at
least one test has to pass. Otherwise a Tinderbox Fail will be raised.
