require 'json'
require 'open3'
require_relative './defines'
require_relative './utils'

module FelesBuild
  module Builder
    include Defines
    @logger = FelesBuild::Utils::Logger.new('builder')

    def self.package_version
      JSON.parse(File.read(File.join(Defines::PROJECT_ROOT, 'package.json')))['version']
    end

    def self.run_in_parallel(commands)
      processes = commands.map do |cmd, dir|
        @logger.info "Running `#{cmd.join(' ')}` in `#{dir}`"
        stdout_r, stdout_w = IO.pipe
        stderr_r, stderr_w = IO.pipe
        pid = spawn(*cmd, chdir: dir, out: stdout_w, err: stderr_w)
        # Close write ends in parent
        stdout_w.close
        stderr_w.close
        { pid: pid, stdout: stdout_r, stderr: stderr_r, cmd: cmd, dir: dir }
      end

      processes.each do |proc|
        pid = proc[:pid]
        out = proc[:stdout].read
        err = proc[:stderr].read
        proc[:stdout].close
        proc[:stderr].close
        Process.wait(pid)
        unless $?.success?
          raise "Build command `#{proc[:cmd].join(' ')}` in `#{proc[:dir]}` failed with status #{$?.exitstatus}\nSTDOUT:\n#{out}\nSTDERR:\n#{err}"
        end
      end
    end

    def self.run(mode: 'dev', buildid2: 'default-build-id')
      @logger.info "Building features with mode=#{mode}"

      version = package_version

      dev_commands = [
        [['deno', 'task', 'build', "--env.MODE=#{mode}}"], File.join(PROJECT_ROOT, 'bridge/startup')],
        [['deno', 'task', 'build',"--env.__BUILDID2__=#{buildid2}","--env.__VERSION2__=#{version}"], File.join(PROJECT_ROOT, 'bridge/loader-modules')],
        [['deno', 'run', '-A', 'vite', 'build', '--base', 'chrome://noraneko/content'], File.join(PROJECT_ROOT, 'bridge/loader-features')],
      ]

      prod_commands = [
        [['deno', 'task', 'build'], File.join(PROJECT_ROOT, 'bridge/startup')],
        [['deno', 'run', '-A', 'vite', 'build', '--base', 'chrome://noraneko/content'], File.join(PROJECT_ROOT, 'bridge/loader-features')],
        [['deno', 'run', '-A', 'vite', 'build', '--base', 'resource://noraneko'], File.join(PROJECT_ROOT, 'bridge/loader-modules')],
        [['deno', 'run', '-A', 'vite', 'build', '--base', 'chrome://noraneko-settings/content'], File.join(PROJECT_ROOT, 'src/ui/settings')]
      ]

      if mode.start_with?('dev')
        run_in_parallel(dev_commands)
      else
        run_in_parallel(prod_commands)
      end

      @logger.success "Build complete."
    end
  end
end
