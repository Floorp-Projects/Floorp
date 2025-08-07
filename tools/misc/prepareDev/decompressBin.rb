require 'fileutils'

BIN_ARCHIVE = 'noraneko-bin.tar.xz'
BIN_ROOT_DIR = '_dist/bin'
BIN_VERSION = '_dist/bin/version.txt'
VERSION = '1.0.0'

def decompress_bin
  puts "[dev] Binary extraction started: #{BIN_ARCHIVE}"
  unless File.exist?(BIN_ARCHIVE)
    puts "[dev] #{BIN_ARCHIVE} not found. Downloading from GitHub release."
    puts "[stub] Downloading binary archive (not implemented)"
  end

  case RUBY_PLATFORM
  when /mswin|mingw|cygwin/
    puts '[stub] Windows extraction (Expand-Archive)'
  when /darwin/
    puts '[stub] macOS extraction (hdiutil, xattr, etc.)'
  when /linux/
    system('tar', '-xf', BIN_ARCHIVE, '-C', BIN_ROOT_DIR)
    system('chmod', '-R', '755', './_dist/bin')
  end

  File.write(BIN_VERSION, VERSION)
  puts '[dev] Extraction complete!'
rescue => e
  warn "[dev] Error during extraction: #{e.message}"
  exit 1
end

decompress_bin if $PROGRAM_NAME == __FILE__