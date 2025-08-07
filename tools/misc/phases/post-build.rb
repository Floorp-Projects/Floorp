require_relative '../../misc/inject-script-loaders/manifest'
require_relative '../../misc/launchDev/writeVersion'
require_relative '../../misc/launchDev/savePrefs'

def log_info(msg) = puts "[INFO] #{msg}"
def log_success(msg) = puts "[SUCCESS] #{msg}"

def write_buildid2(bin_path, buildid)
  File.write(File.join(bin_path, 'buildid2.txt'), buildid)
end

def run_post_build_phase(is_dev = false)
  log_info '🔨 Starting post-build phase...'
  bin_path = File.expand_path('_dist/bin/noraneko', Dir.pwd)
  log_info '📋 Injecting manifest...'
  inject_manifest(bin_path, is_dev ? 'dev' : 'prod')
  log_info '🌐 Injecting XHTML...'
  # Skipped: injectXHTML/injectXHTMLDev (xhtml.ts is excluded)
  log_info '🆔 Writing build ID...'
  write_buildid2(bin_path, 'nora-build-id')
  if is_dev
    log_info '🚀 Setting up development environment...'
    gen_version
    save_prefs_for_profile(File.expand_path('_dist/profile/test', Dir.pwd))
  end
  log_success 'Post-build phase completed'
end

run_post_build_phase(ARGV[0] == 'dev') if $PROGRAM_NAME == __FILE__