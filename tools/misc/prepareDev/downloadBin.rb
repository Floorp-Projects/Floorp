require 'open-uri'

BIN_ARCHIVE = 'noraneko-bin.tar.xz'

def git_origin_url
  `git remote get-url origin`.strip
end

def download_file(url, path)
  puts "[dev] Downloading: #{url}"
  URI.open(url) { |remote| File.write(path, remote.read, mode: 'wb') }
end

def download_bin_archive
  file = BIN_ARCHIVE
  origin = git_origin_url.chomp('/')
  origin_url = "#{origin}-runtime/releases/latest/download/#{file}"
  puts "[dev] Downloading from origin: #{origin_url}"
  begin
    download_file(origin_url, file)
    puts '[dev] Download from origin complete!'
  rescue => e
    puts "[dev] Download from origin failed. Falling back to upstream: #{e.message}"
    upstream = "https://github.com/nyanrus/noraneko-runtime/releases/latest/download/#{file}"
    puts "[dev] Downloading from upstream: #{upstream}"
    begin
      download_file(upstream, file)
      puts '[dev] Download from upstream complete!'
    rescue => e2
      puts "[dev] Download from upstream failed: #{e2.message}"
      raise
    end
  end
end

download_bin_archive if $PROGRAM_NAME == __FILE__