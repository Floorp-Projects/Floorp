#
# This is part of the PEAR::Image_Text package.
#
# Generates the peardoc2 (DocBook XML) version of Image_Text's API documentation into the subdirectory docbook/.
#


rm -rf ./docbook/
phpdoc -d ./ -i example.php,generate_package_xml.php,*.xml -it todo -t ./docbook/ -o XML:DocBook/peardoc2 -s -p -dc images -dn Image_Text

