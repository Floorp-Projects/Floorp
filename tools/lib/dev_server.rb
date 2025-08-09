require 'childprocess'
require_relative './defines'
require_relative './utils'

module FelesBuild
  module DevServer
    include Defines
    @logger = FelesBuild::Utils::Logger.new('dev-server')
    @vite_processes = []

    def self.run(writer)
      @logger.info "Starting Vite dev servers..."

      servers = [
        { name: 'main', path: File.join(PROJECT_ROOT, 'bridge/loader-features') },
        { name: 'designs', path: File.join(PROJECT_ROOT, 'browser-features/skin') },
        { name: 'settings', path: File.join(PROJECT_ROOT, 'src/ui/settings') }
      ]

      # Ensure logs directory exists
      logs_dir = File.join(PROJECT_ROOT, 'logs')
      Dir.mkdir(logs_dir) unless Dir.exist?(logs_dir)

      servers.each do |server|
        process = ChildProcess.build('npx', 'vite', '--port', get_port_for(server[:name]).to_s)
        process.cwd = server[:path]
        log_file_path = File.join(logs_dir, "vite-#{server[:name]}.log")
        log_file = File.open(log_file_path, 'w')
        process.io.stdout = process.io.stderr = log_file # Redirect output to log file
        process.start
        @vite_processes << process
        @logger.info "Started Vite dev server for #{server[:name]} with PID: #{process.pid}, logging to #{log_file_path}"
      end

      @logger.info "All Vite dev servers started."
      # A more robust solution would be to check the output of each server.
      # For now, we assume they are ready after a short delay.
      sleep 5
      writer.puts Defines::DEV_SERVER[:ready_string]
      writer.close
    end

    def self.shutdown
      @logger.info "Shutting down Vite dev servers..."
      @vite_processes.each do |process|
        begin
          process.stop if process.alive?
        rescue => e
          @logger.warn "Failed to stop process #{process.pid}: #{e.message}"
        end
      end
      @logger.success "Vite dev servers shut down."
    end

    def self.get_port_for(server_name)
      case server_name
      when 'main'
        5173
      when 'designs'
        5174
      when 'settings'
        5175
      else
        Defines::DEV_SERVER[:default_port]
      end
    end
  end
end
