require 'json'

def abs(path)
  File.expand_path(File.join('..', '..', '..', path), __dir__)
end

def package_version
  JSON.parse(File.read(abs('package.json')))['version']
end

def run(cmd, dir)
  system(*cmd, chdir: dir)
end

def launch_build(mode, buildid2)
  return unless mode.start_with?('dev')
  run(['deno', 'task', 'build', "--env.MODE=#{mode}"], abs('src/core/glue/startup'))
  run(['deno', 'task', 'build', "--env.__BUILDID2__=#{buildid2}", "--env.__VERSION2__=#{package_version}"], abs('src/core/glue/loader-modules'))
  run(['deno', 'task', 'build', "--env.MODE=#{mode}"], abs('src/core/glue/startup'))
  run(['deno', 'task', 'build', "--env.__BUILDID2__=#{buildid2}", "--env.__VERSION2__=#{package_version}"], abs('src/core/glue/loader-modules'))
end

if $PROGRAM_NAME == __FILE__
  launch_build(ARGV[0] || 'dev', ARGV[1] || 'default-build-id')
end