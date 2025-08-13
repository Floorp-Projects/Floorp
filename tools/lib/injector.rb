require 'fileutils'
require_relative './defines'

module FelesBuild
  module Injector
    include Defines
    @logger = FelesBuild::Utils::Logger.new('injector')

    def self.inject_xhtml_from_ts(is_dev)
        script_path = File.expand_path('../scripts/xhtml.ts', __dir__)
        bin_path = Defines::BIN_DIR

        cmd = ['deno', 'run', '--allow-read', '--allow-write', script_path, bin_path]
        cmd << '--dev' if is_dev

        _stdout, stderr, status = Open3.capture3(*cmd)

        unless status.success?
            raise "Failed to inject XHTML: #{stderr}"
        end
        @logger.success "XHTML injection complete."
    end

    def self.run(mode, dir_name = 'noraneko')
      manifest_path = File.join(Defines::BIN_DIR, 'chrome.manifest')
      if mode != 'prod'
        manifest = File.read(manifest_path)
        entry = "manifest #{dir_name}/noraneko.manifest"
        File.write(manifest_path, "#{manifest}\n#{entry}") unless manifest.include?(entry)
      end

      dir_path = File.join(Defines::BIN_DIR, dir_name)
      FileUtils.rm_rf(dir_path)
      FileUtils.mkdir_p(dir_path)
      File.write(File.join(dir_path, 'noraneko.manifest'), <<~MANIFEST)
        content noraneko content/ contentaccessible=yes
        content noraneko-startup startup/ contentaccessible=yes
        skin noraneko classic/1.0 skin/
        resource noraneko resource/ contentaccessible=yes
        #{mode != 'dev' ? "\ncontent noraneko-settings settings/ contentaccessible=yes" : ''}
      MANIFEST

      [
        ['content', 'bridge/loader-features/_dist'],
        ['startup', 'bridge/startup/_dist'],
        ['skin', 'browser-features/skin'],
        ['resource', 'bridge/loader-modules/_dist']
      ].each do |subdir, target|
        FileUtils.remove_file(File.expand_path(File.join(dir_path, subdir), Dir.pwd)) if File.exist?(File.expand_path(File.join(dir_path, subdir), Dir.pwd))
        FileUtils.ln_sf(File.expand_path(target, Dir.pwd), File.expand_path(File.join(dir_path, subdir), Dir.pwd))
      end
      @logger.success "Manifest injected successfully."
    end

  end
end
