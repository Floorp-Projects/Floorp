require 'fileutils'
require 'open3'

BRANDING_BASE_NAME = 'noraneko'
BRANDING_NAME = 'Noraneko'

def bin_dir
  if /darwin/ =~ RUBY_PLATFORM
    "_dist/bin/#{BRANDING_BASE_NAME}/#{BRANDING_NAME}.app/Contents/Resources"
  else
    "_dist/bin/#{BRANDING_BASE_NAME}"
  end
end

PATCHES_DIR = 'tools/build/tasks/git-patches/patches'
PATCHES_TMP = '_dist/bin/applied_patches'

def git_initialized?(dir)
  File.exist?(File.join(dir, '.git'))
end

def initialize_bin_git
  dir = bin_dir
  if git_initialized?(dir)
    puts '[git-patches] Git repository is already initialized in _dist/bin.'
    return
  end
  puts '[git-patches] Initializing Git repository in _dist/bin.'
  FileUtils.mkdir_p(dir)
  File.write(File.join(dir, '.gitignore'), [
    './noraneko-dev/*',
    './browser/chrome/browser/res/activity-stream/data/content/abouthomecache/*'
  ].join("\n"))
  %w[init add . commit -m initialize].each_slice(2) do |cmd, arg|
    Open3.capture3('git', cmd, arg, chdir: dir)
  end
  puts '[git-patches] Git repository initialization complete.'
end

def patch_needed?
  Dir.exist?(PATCHES_DIR) && !Dir.exist?(PATCHES_TMP)
end