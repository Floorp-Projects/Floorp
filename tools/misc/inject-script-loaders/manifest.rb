require 'fileutils'

def inject_manifest(bin_path, mode, dir_name = 'noraneko')
  manifest_path = File.join(bin_path, 'chrome.manifest')
  if mode != 'prod'
    manifest = File.read(manifest_path)
    entry = "manifest #{dir_name}/noraneko.manifest"
    File.write(manifest_path, "#{manifest}\n#{entry}") unless manifest.include?(entry)
  end

  dir_path = File.join(bin_path, dir_name)
  FileUtils.rm_rf(dir_path)
  FileUtils.mkdir_p(dir_path)
  File.write(File.join(dir_path, 'noraneko.manifest'), <<~MANIFEST)
    content noraneko content/ contentaccessible=yes
    content noraneko-startup startup/ contentaccessible=yes
    skin noraneko classic/1.0 skin/
    resource noraneko resource/ contentaccessible=yes
    #{mode != 'dev' ? "\ncontent noraneko-settings settings/ contentaccessible=yes" : ''}
  MANIFEST

  [
    ['content', 'src/core/glue/loader-features/_dist'],
    ['startup', 'src/core/glue/startup/_dist'],
    ['skin', 'src/themes/_dist'],
    ['resource', 'src/core/glue/loader-modules/_dist']
  ].each do |subdir, target|
    FileUtils.ln_sf(File.expand_path(target, Dir.pwd), File.expand_path(File.join(dir_path, subdir), Dir.pwd))
  end
end