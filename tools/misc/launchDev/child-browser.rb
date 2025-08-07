require_relative '../../../tools/utils/process_utils'

BRANDING_BASE_NAME = 'noraneko'
BRANDING_NAME = 'Noraneko'

def print_firefox_log(line)
  case line
  when /MOZ_CRASH|JavaScript error:|console\.error|\] Errors|\[fluent\] Couldn't find a message:|\[fluent\] Missing|EGL Error:/
    puts "\e[31m#{line}\e[0m"
  when /console\.warn|WARNING:|\[WARN|JavaScript warning:/
    puts "\e[33m#{line}\e[0m"
  when /console\.debug/
    puts "\e[36m#{line}\e[0m"
  else
    puts line
  end
end

def browser_command(port)
  case RUBY_PLATFORM
  when /mswin|mingw|cygwin/
    ["./_dist/bin/#{BRANDING_BASE_NAME}/#{BRANDING_BASE_NAME}.exe", "--profile", "./_dist/profile/test", "--remote-debugging-port", port.to_s, "--wait-for-browser", "--jsdebugger"]
  when /linux/
    ["./_dist/bin/#{BRANDING_BASE_NAME}/#{BRANDING_BASE_NAME}-bin", "--profile", "./_dist/profile/test", "--remote-debugging-port", port.to_s, "--wait-for-browser", "--jsdebugger"]
  when /darwin/
    ["./_dist/bin/#{BRANDING_BASE_NAME}/#{BRANDING_NAME}.app/Contents/MacOS/#{BRANDING_BASE_NAME}", "--profile", "./_dist/profile/test", "--remote-debugging-port", port.to_s, "--wait-for-browser", "--jsdebugger"]
  else
    raise "Unsupported platform: #{RUBY_PLATFORM}"
  end
end

def run_browser(port = 5180)
  cmd = browser_command(port)
  intended_shutdown = false

  # Use ProcessUtils for process/thread management
  ProcessUtils.run_command_with_logging(cmd) do |stream, line|
    if stream == :stderr && line =~ /^WebDriver BiDi listening on (ws:\/\/.*)/
      puts "nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-webdriver"
    end
    print_firefox_log(line.chomp)
  end

  Thread.new do
    while (input = STDIN.gets)
      if input.start_with?('q')
        puts '[child-browser] Shutdown Browser'
        intended_shutdown = true
        # No direct access to wait_thr.pid, so just exit
        exit 0
      end
    end
  end

  puts '[child-browser] Browser Closed' unless intended_shutdown
  exit 0 unless intended_shutdown
end

run_browser if $PROGRAM_NAME == __FILE__