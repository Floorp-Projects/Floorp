require_relative './utils'
require_relative './defines'

module FelesBuild
  module BrowserLauncher
    include Defines

    def self.print_firefox_log(line)
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

    def self.browser_command(port)
      [Defines::BIN_PATH_EXE, "--profile", Defines::PATHS[:profile_test], "--remote-debugging-port", port.to_s, "--wait-for-browser", "--jsdebugger"]
    end

    def self.run(port = 5180)
      cmd = browser_command(port)

      puts "[launcher] Launching browser with command: #{cmd.join(' ')}"

      FelesBuild::Utils::ProcessUtils.run_command_with_logging(cmd) do |stream, line|
        if stream == :stderr && line =~ /^WebDriver BiDi listening on (ws:\/\/.*)/
          puts "nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-webdriver"
        end
        print_firefox_log(line.chomp)
      end

      puts '[launcher] Browser Closed'
    end
  end
end
