require 'fileutils'

module FelesBuild
  module Defines
    BRANDING = {
      base_name: 'noraneko',
      display_name: 'Noraneko',
      dev_suffix: 'noraneko-dev'
    }

    BRANDING_BASE_NAME = BRANDING[:base_name]
    BRANDING_NAME = BRANDING[:display_name]

    PLATFORM =
      case RUBY_PLATFORM
      when /mswin|mingw|cygwin/
        'windows'
      when /darwin/
        'darwin'
      else
        'linux'
      end

    VERSION = %w[windows linux].include?(PLATFORM) ? '001' : '000'

    PROJECT_ROOT = File.expand_path('../..', __dir__)

    PATHS = {
      root: PROJECT_ROOT,
      bin_root: File.join(PROJECT_ROOT, '_dist', 'bin'),
      buildid2: File.join(PROJECT_ROOT, '_dist', 'buildid2'),
      profile_test: File.join(PROJECT_ROOT, '_dist', 'profile', 'test'),
      loader_features: File.join(PROJECT_ROOT, 'bridge/loader-features'),
      features_chrome: File.join(PROJECT_ROOT, 'browser-features/chrome'),
      i18n_features_chrome: File.join(PROJECT_ROOT, 'i18n/features-chrome'),
      loader_modules: File.join(PROJECT_ROOT, 'bridge/loader-modules'),
      modules: File.join(PROJECT_ROOT, 'browser-features/modules'),
      mozbuild_output: File.join(PROJECT_ROOT, 'obj-artifact-build-output/dist')
    }

    BIN_DIR =
      if PLATFORM != 'darwin'
        File.join(PATHS[:bin_root], BRANDING[:base_name])
      else
        File.join(PATHS[:bin_root], BRANDING[:base_name], "#{BRANDING[:display_name]}.app", 'Contents', 'Resources')
      end

    BIN_ROOT_DIR = PATHS[:bin_root]
    BIN_PATH = File.join(BIN_DIR, BRANDING[:base_name])

    BIN_PATH_EXE =
      if PLATFORM != 'darwin'
        BIN_PATH + (PLATFORM == 'windows' ? '.exe' : '-bin')
      else
        File.join(PATHS[:bin_root], BRANDING[:base_name], "#{BRANDING[:display_name]}.app", 'Contents', 'MacOS', BRANDING[:base_name])
      end

    BIN_VERSION = File.join(BIN_DIR, 'nora.version.txt')

    DEV_SERVER = {
      ready_string: 'nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev',
      default_port: 8080
    }

    def self.get_bin_archive
      case PLATFORM
      when 'windows'
        {
          filename: "#{BRANDING[:base_name]}-win-amd64-moz-artifact.zip",
          format: 'zip',
          platform: PLATFORM,
          architecture: 'x86_64'
        }
      when 'linux'
        linux_arch = RUBY_PLATFORM.include?('x86_64') ? 'x86_64' : 'arm64'
        {
          filename: "#{BRANDING[:base_name]}-linux-#{linux_arch}-moz-artifact.tar.xz",
          format: 'tar.xz',
          platform: PLATFORM,
          architecture: linux_arch
        }
      when 'darwin'
        {
          filename: "#{BRANDING[:base_name]}-macOS-universal-moz-artifact.dmg",
          format: 'dmg',
          platform: PLATFORM,
          architecture: 'universal'
        }
      else
        raise "Unsupported platform: #{PLATFORM}. Supported: windows (x86_64), linux (x86_64, aarch64), darwin (universal)"
      end
    end

    def self.platform_supported?
      !!get_bin_archive rescue false
    end
  end
end
