require 'json'

def abs(path)
  File.expand_path(File.join('..', '..', '..', path), __dir__)
end

def package_version
  JSON.parse(File.read(abs('package.json')))['version']
end

def launch_dev(mode, buildid2)
  if mode == 'dev'
    puts "[child-dev] Would launch Vite dev server with mode=#{mode}, buildid2=#{buildid2}, version=#{package_version}"
  elsif mode == 'test'
    puts "[child-dev] Would launch Vitest with buildid2=#{buildid2}, version=#{package_version}"
  end
  puts 'nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev'
end

def shutdown_dev
  puts '[child-dev] Completed Shutdown ViteDevServer✅'
end

if $PROGRAM_NAME == __FILE__
  mode = ARGV[0] || 'dev'
  buildid2 = ARGV[1] || 'default-build-id'
  require_relative '../../../tools/utils/process_utils'

  Thread.new do
    while (input = STDIN.gets)
      if input.start_with?('q')
        puts '[child-dev] Shutdown ViteDevServer'
        shutdown_dev
        exit 0
      end
    end
  end
  launch_dev(mode, buildid2)
end