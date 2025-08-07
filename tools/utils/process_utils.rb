# Process and Thread Utilities for Ruby scripts

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

  # Example: print log lines with a prefix
  def self.print_log(line, prefix = '')
    puts "#{prefix}#{line}"
  end
end