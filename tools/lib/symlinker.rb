require 'fileutils'
require_relative './defines'

module FelesBuild
  module Symlinker
    include Defines
    @logger = FelesBuild::Utils::Logger.new('symlinker')

    def self.run
      [
        [Defines::PATHS[:loader_features] + '/link-features-chrome', Defines::PATHS[:features_chrome]],
        [Defines::PATHS[:loader_features] + '/link-i18n-features-chrome', Defines::PATHS[:i18n_features_chrome]],
        [Defines::PATHS[:loader_modules] + '/link-modules', Defines::PATHS[:modules]]
      ].each do |link, target|
        FileUtils.remove_file(link) if File.exist?(link)
        FileUtils.ln_sf(target, link)
      end
      @logger.success "Symlinks created successfully."
    end
  end
end
