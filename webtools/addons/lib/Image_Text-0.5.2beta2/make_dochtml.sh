#
# This is part of the PEAR::Image_Text package.
#
# Generates the HTML version of Image_Text's API documentation into the subdirectory dochtml/.
#


rm -rf ./dochtml/
phpdoc -d ./ -i docbook/ dochtml/ generate_package_xml.php make_* package.xml CVS/ -t ./dochtml/ -o HTML:frames:phpedit -s -p -dc images -dn Image_Text

