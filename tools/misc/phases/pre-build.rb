require_relative '../../misc/prepareDev/initBin'
require_relative '../../misc/git-patches/git-patches-manager'
require_relative '../../misc/setup/symlinks'

def log_info(msg) = puts "[INFO] #{msg}"
def log_success(msg) = puts "[SUCCESS] #{msg}"

def run_child_build(is_dev)
  puts "[stub] Running child-build process (is_dev=#{is_dev})"
end

def run_child_dev(mode)
  puts "[stub] Running child-dev process (mode=#{mode})"
end

def run_pre_build_phase(init_git_for_patch: false, is_dev: false, mode: 'dev')
  log_info '🔧 Starting pre-build phase...'
  init_bin
  if init_git_for_patch
    log_info '🔀 Initializing Git for patches...'
    initialize_bin_git
  end
  log_info '🔗 Setting up build symlinks...'
  setup_build_symlinks
  log_info '🔨 Running child-build process...'
  run_child_build(is_dev)
  if is_dev
    log_info '🚀 Starting development server...'
    run_child_dev(mode == 'stage' ? 'production' : mode)
  end
  log_success 'Pre-build phase completed'
end

if $PROGRAM_NAME == __FILE__
  opts = ARGV.map { |a| a.split('=') }.to_h.transform_keys(&:to_sym)
  run_pre_build_phase(
    init_git_for_patch: opts[:init_git_for_patch] == 'true',
    is_dev: opts[:is_dev] == 'true',
    mode: opts[:mode] || 'dev'
  )
end