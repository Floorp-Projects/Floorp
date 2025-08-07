require_relative '../misc/phases/pre-build'
require_relative '../misc/phases/post-build'
require_relative './logger'
require_relative './utils'

module Build
  def self.build(options)
    mode = options[:mode]
    production_mozbuild_phase = options[:production_mozbuild_phase]
    init_git_for_patch = options[:init_git_for_patch] || false

    log.info "🏗️ Starting #{mode} build (#{production_mozbuild_phase || "full"})"

    begin
      Utils.initialize_dist_directory

      # "before" or "full" phase: run pre-build
      if production_mozbuild_phase == 'before' || production_mozbuild_phase.nil?
        run_pre_build_phase(
          init_git_for_patch: init_git_for_patch,
          is_dev: %w[dev test].include?(mode),
          mode: mode
        )

        if production_mozbuild_phase == 'before'
          log.info "🔄 Ready for Mozilla build. Run with --after flag after mozbuild"
          return
        end
      end

      # "after" or "full" phase: run post-build
      if production_mozbuild_phase == 'after' || production_mozbuild_phase.nil?
        run_post_build_phase(%w[dev test].include?(mode))
      end

      log.success "🎉 Build completed successfully!"
    rescue => e
      log.error "💥 Build failed: #{e}"
      raise
    end
  end
end