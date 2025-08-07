# Core build system types (Ruby translation)

module Types
  # Platform: :windows, :linux, :darwin
  # Architecture: :x86_64, :aarch64
  # BuildMode: :dev, :test, :stage, :production
  # ProductionMozbuildPhase: :before, :after

  Branding = Struct.new(:base_name, :display_name, :dev_suffix, keyword_init: true)
  BinaryArchive = Struct.new(:filename, :format, :platform, :architecture, keyword_init: true)
  BuildOptions = Struct.new(:mode, :production_mozbuild_phase, :clean, :init_git_for_patch, :launch_browser, keyword_init: true)
  PreBuildOptions = Struct.new(:init_git_for_patch, :is_dev, :mode, keyword_init: true)
  Paths = Struct.new(
    :root, :bin_root, :buildid2, :profile_test, :loader_features, :features_chrome,
    :i18n_features_chrome, :loader_modules, :modules, :actual_modules, :mozbuild_output,
    keyword_init: true
  )
end