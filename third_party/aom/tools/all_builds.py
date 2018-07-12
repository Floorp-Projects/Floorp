#!/usr/bin/python

import getopt
import subprocess
import sys

LONG_OPTIONS = ["shard=", "shards="]
BASE_COMMAND = "./configure --enable-internal-stats --enable-experimental"

def RunCommand(command):
  run = subprocess.Popen(command, shell=True)
  output = run.communicate()
  if run.returncode:
    print "Non-zero return code: " + str(run.returncode) + " => exiting!"
    sys.exit(1)

def list_of_experiments():
  experiments = []
  configure_file = open("configure")
  list_start = False
  for line in configure_file.read().split("\n"):
    if line == 'EXPERIMENT_LIST="':
      list_start = True
    elif line == '"':
      list_start = False
    elif list_start:
      currently_broken = ["csm"]
      experiment = line[4:]
      if experiment not in currently_broken:
        experiments.append(experiment)
  return experiments

def main(argv):
  # Parse arguments
  options = {"--shard": 0, "--shards": 1}
  if "--" in argv:
    opt_end_index = argv.index("--")
  else:
    opt_end_index = len(argv)
  try:
    o, _ = getopt.getopt(argv[1:opt_end_index], None, LONG_OPTIONS)
  except getopt.GetoptError, err:
    print str(err)
    print "Usage: %s [--shard=<n> --shards=<n>] -- [configure flag ...]"%argv[0]
    sys.exit(2)

  options.update(o)
  extra_args = argv[opt_end_index + 1:]

  # Shard experiment list
  shard = int(options["--shard"])
  shards = int(options["--shards"])
  experiments = list_of_experiments()
  base_command = " ".join([BASE_COMMAND] + extra_args)
  configs = [base_command]
  configs += ["%s --enable-%s" % (base_command, e) for e in experiments]
  my_configs = zip(configs, range(len(configs)))
  my_configs = filter(lambda x: x[1] % shards == shard, my_configs)
  my_configs = [e[0] for e in my_configs]

  # Run configs for this shard
  for config in my_configs:
    test_build(config)

def test_build(configure_command):
  print "\033[34m\033[47mTesting %s\033[0m" % (configure_command)
  RunCommand(configure_command)
  RunCommand("make clean")
  RunCommand("make")

if __name__ == "__main__":
  main(sys.argv)
