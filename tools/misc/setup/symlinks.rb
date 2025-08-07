require 'fileutils'

def setup_build_symlinks
  [
    ['src/core/glue/loader-features/link-features-chrome', 'src/features/chrome'],
    ['src/core/glue/loader-features/link-i18n-features-chrome', 'i18n/features-chrome'],
    ['src/core/glue/loader-modules/link-modules', 'src/core/modules']
  ].each do |link, target|
    FileUtils.ln_sf(File.expand_path(target, Dir.pwd), File.expand_path(link, Dir.pwd))
  end
end

setup_build_symlinks if $PROGRAM_NAME == __FILE__