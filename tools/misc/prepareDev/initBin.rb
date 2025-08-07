require 'fileutils'
require_relative './decompressBin'

BIN_DIR = '_dist/bin'
BIN_PATH_EXE = '_dist/bin/noraneko'
BIN_ROOT_DIR = '_dist/bin'
BIN_VERSION = '_dist/bin/version.txt'
VERSION = '1.0.0'

def init_bin
  has_version = File.exist?(BIN_VERSION)
  has_bin = File.exist?(BIN_PATH_EXE)
  need_init = false

  if has_bin && has_version
    version = File.read(BIN_VERSION).strip
    if VERSION != version
      puts "[dev] Version mismatch: #{version} !== #{VERSION}. Re-extracting."
      FileUtils.rm_rf(BIN_ROOT_DIR)
      need_init = true
    else
      puts '[dev] Binary version matches. No initialization needed.'
    end
  elsif has_bin && !has_version
    puts "[dev] Binary exists but version file is missing. Writing #{VERSION}."
    FileUtils.mkdir_p(BIN_DIR)
    File.write(BIN_VERSION, VERSION)
    puts '[dev] Initialization complete.'
  elsif !has_bin && has_version
    warn '[dev] Version file exists but binary is missing. Abnormal termination.'
    raise 'Unreachable: !has_bin && has_version'
  else
    puts '[dev] Binary not found. Extracting.'
    need_init = true
  end

  if need_init
    FileUtils.mkdir_p(BIN_DIR)
    decompress_bin
    puts '[dev] Initialization complete.'
  end
end

init_bin if $PROGRAM_NAME == __FILE__