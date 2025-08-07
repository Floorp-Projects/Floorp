require 'fileutils'
require 'securerandom'

def write_version(dir)
  File.write(File.join(dir, 'version.txt'), "version: 1.0.0\n")
end

def write_buildid2(dir, buildid)
  File.write(File.join(dir, 'buildid2.txt'), "buildid2: #{buildid}\n")
end

def gen_version
  root = File.expand_path('../../../', __dir__)
  gecko = File.join(root, 'static/gecko')
  dist = File.join(root, '_dist')
  FileUtils.mkdir_p(dist)
  write_version(gecko)
  write_buildid2(dist, SecureRandom.uuid)
end

gen_version if $PROGRAM_NAME == __FILE__