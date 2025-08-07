# Logger utility (Ruby translation)

class Logger
  COLORS = {
    info: "\e[34m",    # Blue
    warn: "\e[33m",    # Yellow
    error: "\e[31m",   # Red
    success: "\e[32m", # Green
    debug: "\e[90m",   # Gray
    reset: "\e[0m"
  }

  def initialize(prefix = 'build')
    @prefix = prefix
  end

  def format(level, message)
    "[#{@prefix}] #{level.upcase}: #{message}"
  end

  def info(message, *args)
    puts "#{COLORS[:info]}#{format('INFO', message)}#{COLORS[:reset]}", *args
  end

  def warn(message, *args)
    warn "#{COLORS[:warn]}#{format('WARN', message)}#{COLORS[:reset]}", *args
  end

  def error(message, *args)
    warn "#{COLORS[:error]}#{format('ERROR', message)}#{COLORS[:reset]}", *args
  end

  def success(message, *args)
    puts "#{COLORS[:success]}#{format('SUCCESS', message)}#{COLORS[:reset]}", *args
  end

  def debug(message, *args)
    if ENV['DEBUG']
      puts "#{COLORS[:debug]}#{format('DEBUG', message)}#{COLORS[:reset]}", *args
    end
  end
end

def log
  @log ||= Logger.new('build')
end

def create_logger(prefix)
  Logger.new(prefix)
end