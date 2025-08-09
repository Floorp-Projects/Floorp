require 'fileutils'
require 'open3'

module FelesBuild
  module Utils
    PROJECT_ROOT = File.expand_path('../../..', __dir__)

    def self.resolve_from_root(path)
      File.expand_path(path, PROJECT_ROOT)
    end

    def self.platform
      case RUBY_PLATFORM
      when /mswin|mingw|cygwin/
        :windows
      when /darwin/
        :darwin
      else
        :linux
      end
    end

    def self.arch
      RUBY_PLATFORM.include?('aarch64') ? :aarch64 : :x86_64
    end

    def self.safe_remove(path)
      FileUtils.rm_rf(path)
    rescue => e
      warn "Failed to remove #{path}: #{e.message}"
    end

    def self.create_symlink(link, target)
      safe_remove(link)
      FileUtils.ln_sf(target, link)
    rescue => e
      warn "Failed to create symlink #{link} -> #{target}: #{e.message}"
    end

    def self.exists?(path)
      File.exist?(path) || Dir.exist?(path)
    end

    def self.git_initialized?(path)
      File.exist?(File.join(path, '.git'))
    end

    def self.run_command_checked(command, args = [], cwd: nil)
      stdout, stderr, status = Open3.capture3(command, *args, chdir: cwd)
      { stdout: stdout, stderr: stderr, success: status.success? }
    rescue => e
      { stdout: '', stderr: e.message, success: false }
    end

    def self.run_command(command, args = [], cwd: nil)
      result = run_command_checked(command, args, cwd: cwd)
      raise "Command failed: #{command} #{args.join(' ')}\n#{result[:stderr]}" unless result[:success]
      result
    end

    def self.initialize_dist_directory
      dist_dir = resolve_from_root('_dist')
      bin_dir = File.join(dist_dir, 'bin')
      profile_dir = File.join(dist_dir, 'profile')
      [dist_dir, bin_dir, profile_dir].each { |d| FileUtils.mkdir_p(d) }
    end

    class Logger
      COLORS = {
        info: "\e[34m",    # Blue
        warn: "\e[33m",    # Yellow
        error: "\e[31m",   # Red
        success: "\e[32m", # Green
        debug: "\e[90m",   # Gray
        reset: "\e[0m"
      }

      def initialize(prefix = 'build')
        @prefix = prefix
      end

      def format(level, message)
        "[#{@prefix}] #{level.upcase}: #{message}"
      end

      def info(message, *args)
        puts "#{COLORS[:info]}#{format('INFO', message)}#{COLORS[:reset]}", *args
      end

      def warn(message, *args)
        warn "#{COLORS[:warn]}#{format('WARN', message)}#{COLORS[:reset]}", *args
      end

      def error(message, *args)
        warn "#{COLORS[:error]}#{format('ERROR', message)}#{COLORS[:reset]}", *args
      end

      def success(message, *args)
        puts "#{COLORS[:success]}#{format('SUCCESS', message)}#{COLORS[:reset]}", *args
      end

      def debug(message, *args)
        if ENV['DEBUG']
          puts "#{COLORS[:debug]}#{format('DEBUG', message)}#{COLORS[:reset]}", *args
        end
      end
    end

    module ProcessUtils
      require 'open3'

      # Run a command and yield stdout and stderr to blocks
      def self.run_command_with_logging(cmd, &block)
        Open3.popen3(*cmd) do |_stdin, stdout, stderr, wait_thr|
          threads = []
          threads << Thread.new { stdout.each_line { |line| block.call(:stdout, line) } }
          threads << Thread.new { stderr.each_line { |line| block.call(:stderr, line) } }
          threads.each(&:join)
          wait_thr.value
        end
      end
    end
  end
end
