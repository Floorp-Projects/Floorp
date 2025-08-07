require 'json'
require 'fileutils'

def write_version(gecko_dir)
  version = JSON.parse(File.read(File.expand_path('../../../../package.json', __dir__)))['version']
  config_dir = File.join(gecko_dir, 'config')
  FileUtils.mkdir_p(config_dir)
  %w[version.txt version_display.txt].each do |file|
    File.write(File.join(config_dir, file), version)
  end
end

if $PROGRAM_NAME == __FILE__
  write_version(ARGV[0] || './static/gecko')
end