#!/usr/bin/env ruby
require_relative './cli'

def log_info(msg)
  puts "[INFO] #{msg}"
end

def build(opts)
  log_info "Building with options: #{opts.inspect}"
  # Actual build logic would go here
end

if $PROGRAM_NAME == __FILE__
  opts = CLI.parse_args(ARGV)
  log_info "Parsed CLI options: #{opts.inspect}"
  build(
    mode: opts[:mode],
    production_mozbuild_phase: opts[:production_mozbuild],
    init_git_for_patch: opts[:init_git_for_patch],
    launch_browser: opts[:launch_browser]
  )
end