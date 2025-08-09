require 'fileutils'
require 'open3'
require_relative './defines'
require_relative './utils'

module FelesBuild
  module Patcher
    include Defines
    @logger = FelesBuild::Utils::Logger.new('patcher')

    # In noraneko, this is 'scripts/git-patches/patches'.
    # We'll need to create this directory and add the patches.
    PATCHES_DIR = 'tools/patches'
    PATCHES_TMP = '_dist/bin/applied_patches'

    def self.bin_dir
      Defines::BIN_DIR
    end

    def self.git_initialized?(dir)
      File.exist?(File.join(dir, '.git'))
    end

    def self.initialize_bin_git
      dir = bin_dir
      if git_initialized?(dir)
        @logger.info 'Git repository is already initialized in _dist/bin.'
        return
      end
      @logger.info 'Initializing Git repository in _dist/bin.'
      FileUtils.mkdir_p(dir)
      File.write(File.join(dir, '.gitignore'), [
        './noraneko-dev/*',
        './browser/chrome/browser/res/activity-stream/data/content/abouthomecache/*'
      ].join("\n"))

      _stdout, _stderr, status = Open3.capture3('git', 'init', chdir: dir)
      raise "Failed to initialize git repo" unless status.success?

      _stdout, _stderr, status = Open3.capture3('git', 'add', '.', chdir: dir)
      raise "Failed to add files to git repo" unless status.success?

      _stdout, _stderr, status = Open3.capture3('git', 'commit', '-m', 'initialize', chdir: dir)
      raise "Failed to commit initial state" unless status.success?

      @logger.success 'Git repository initialization complete.'
    end

    def self.patch_needed?
      return false unless Dir.exist?(PATCHES_DIR)
      return true unless Dir.exist?(PATCHES_TMP)

      patches_dir_files = Dir.children(PATCHES_DIR).sort
      patches_tmp_files = Dir.children(PATCHES_TMP).sort

      return true if patches_dir_files != patches_tmp_files

      patches_dir_files.each do |patch|
        patch_in_dir = File.read(File.join(PATCHES_DIR, patch))
        patch_in_tmp = File.read(File.join(PATCHES_TMP, patch))
        return true if patch_in_dir != patch_in_tmp
      end

      false
    end

    def self.apply_patches
      initialize_bin_git

      unless patch_needed?
        @logger.info 'No patches needed to apply.'
        return
      end

      # Reverse previously applied patches if any
      if Dir.exist?(PATCHES_TMP)
        @logger.info 'Reversing previously applied patches.'
        reverse_is_aborted = false
        Dir.children(PATCHES_TMP).sort.each do |patch|
          patch_path = File.join(PATCHES_TMP, patch)
          _stdout, stderr, status = Open3.capture3('git', 'apply', '-R', '--reject', '--whitespace=fix', '--unsafe-paths', '--directory', bin_dir, patch_path)
          unless status.success?
            @logger.warn "Failed to reverse patch: #{patch_path}"
            @logger.warn stderr
            reverse_is_aborted = true
          end
        end
        raise 'Reverse Patch Failed: aborted' if reverse_is_aborted
        FileUtils.rm_rf(PATCHES_TMP)
      end

      # Apply new patches
      @logger.info 'Applying new patches.'
      FileUtils.mkdir_p(PATCHES_TMP)
      aborted = false
      Dir.children(PATCHES_DIR).sort.each do |patch|
        next unless patch.end_with?('.patch')
        patch_path = File.join(PATCHES_DIR, patch)
        @logger.info "Applying patch: #{patch_path}"
        _stdout, stderr, status = Open3.capture3('git', 'apply', '--reject', '--whitespace=fix', '--unsafe-paths', '--directory', bin_dir, patch_path)
        if status.success?
          FileUtils.cp(patch_path, File.join(PATCHES_TMP, patch))
        else
          @logger.warn "Failed to apply patch: #{patch_path}"
          @logger.warn stderr
          aborted = true
        end
      end
      raise 'Patch failed: aborted' if aborted
      @logger.success 'All patches applied successfully.'
    end

    def self.create_patches
      initialize_bin_git

      dir = bin_dir
      stdout, _stderr, status = Open3.capture3('git', 'diff', '--name-only', chdir: dir)
      raise "Failed to get changed files" unless status.success?

      changed_files = stdout.split("\n").filter { |f| !f.empty? }

      if changed_files.empty?
        @logger.info 'No changes detected.'
        return
      end

      FileUtils.mkdir_p(PATCHES_DIR)

      changed_files.each do |file|
        stdout_diff, stderr_diff, status_diff = Open3.capture3('git', 'diff', file, chdir: dir)
        unless status_diff.success?
          @logger.warn "Failed to create patch for: #{file}"
          @logger.warn stderr_diff
          next
        end

        modified_diff = stdout_diff
          .gsub(/^--- a\//, '--- ./')
          .gsub(/^\+\+\+ b\//, '+++ ./')
          .strip + "\n"

        patch_name = "#{file.gsub('/', '-').gsub(/\.[^/.]+$/, '')}.patch"
        patch_path = File.join(PATCHES_DIR, patch_name)
        File.write(patch_path, modified_diff)
        @logger.info "Created/Updated patch: #{patch_path}"
      end
    end

    def self.run(action = 'apply')
      case action
      when 'apply'
        apply_patches
      when 'create'
        create_patches
      when 'init'
        initialize_bin_git
      else
        @logger.error "Unknown patcher action: #{action}"
      end
    end
  end
end
