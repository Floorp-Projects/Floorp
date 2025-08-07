require 'json'

def generate_update_xml(meta_path, output_path)
  meta = JSON.parse(File.read(meta_path))
  patch_url = 'http://github.com/nyanrus/noraneko/releases/download/alpha/noraneko-win-amd64-full.mar'
  xml = <<~XML
    <?xml version="1.0" encoding="UTF-8"?>
    <updates>
      <update type="minor" displayVersion="#{meta['version_display']}" appVersion="#{meta['version']}" platformVersion="#{meta['version']}" buildID="#{meta['buildid']}" appVersion2="#{meta['noraneko_version']}">
        <patch type="complete" URL="#{patch_url}" size="#{meta['mar_size']}" hashFunction="sha512" hashValue="#{meta['mar_shasum']}"/>
      </update>
    </updates>
  XML
  File.write(output_path, xml)
end

generate_update_xml(ARGV[0], ARGV[1]) if $PROGRAM_NAME == __FILE__