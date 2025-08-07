#!/usr/bin/env ruby
require_relative './utils/cli-args'

module CLI
  HELP = <<~TXT
    🏗️  feles-build : Noraneko Build System

    Development:
      ruby tools/feles-build.rb --mode=dev
      ruby tools/feles-build.rb --mode=stage
      ruby tools/feles-build.rb --mode=test

    Production (CI recommended):
      ruby tools/feles-build.rb --production-mozbuild=before
      [Run: ./mach build]
      ruby tools/feles-build.rb --production-mozbuild=after

    Patch Development:
      ruby tools/feles-build.rb --init-git-for-patch
  TXT

  def self.show_help
    puts HELP
  end

  def self.parse_args(argv)
    options = CLIArgs.parse(argv)
    if options[:help]
      show_help
      exit 0
    end
    if options.values_at(:mode, :production_mozbuild, :init_git_for_patch).all?(&:nil?) || options.values_at(:mode, :production_mozbuild, :init_git_for_patch).all? { |v| v == false }
      warn 'You must specify --mode, --production-mozbuild, or --init-git-for-patch.'
      show_help
      exit 1
    end
    options
  end
end