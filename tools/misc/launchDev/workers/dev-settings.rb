def run_vite_server
  Dir.chdir('./src/ui/settings')
  puts '[dev-settings] Would launch Vite server with configFile=./vite.config.ts'
  puts '[dev-settings] Vite server listening (simulated)'
  puts '[dev-settings] URLs: http://localhost:5173 (simulated)'
end

def close_vite_server
  puts '[dev-settings] Vite server closed (simulated)'
end

if $PROGRAM_NAME == __FILE__
  run_vite_server
  Thread.new do
    while (input = STDIN.gets)
      if input.strip == 'close'
        close_vite_server
        exit 0
      end
    end
  end
  sleep
end