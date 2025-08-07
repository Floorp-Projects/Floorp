require 'optparse'

module CLIArgs
  VALID_MODES = %w[dev test stage production]

  def self.parse(argv)
    options = { mode: nil, init_git_for_patch: false, production_mozbuild: nil, help: false }
    OptionParser.new do |opts|
      opts.on('--mode=MODE', String) { |m| options[:mode] = m }
      opts.on('--production-mozbuild=PHASE', String) { |p| options[:production_mozbuild] = p }
      opts.on('--init-git-for-patch') { options[:init_git_for_patch] = true }
      opts.on('-h', '--help') { options[:help] = true }
    end.parse!(argv)
    options
  end

  def self.validate(options)
    return if options[:help]
    if options[:production_mozbuild] && options[:mode] && options[:mode] != 'production'
      raise '`--production-mozbuild` is not compatible with `--mode=`.'
    end
  end

  def self.map_to_build_options(options)
    mode = VALID_MODES.include?(options[:mode]) ? options[:mode] : 'dev'
    phase = case options[:production_mozbuild]
            when 'before' then 'before'
            when 'after' then 'after'
            end
    mode = 'production' if phase
    {
      mode: mode,
      init_git_for_patch: options[:init_git_for_patch],
      production_mozbuild_phase: phase,
      launch_browser: %w[dev test stage].include?(mode)
    }
  end
end