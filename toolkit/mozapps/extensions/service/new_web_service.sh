#!/bin/sh

clear

pkg="org.mozilla"

name=$1
if [ "x$name" = "x" ]; then
  echo "usage: $(basename $0) <webservice-name> [pkg-name]"
  exit 1
fi

if [ ! -f $name.java ]; then
  echo "$name.java not found"
  exit 1
fi

if [ "x$2" != "x" ]; then
  pkg=$2
fi

pkg_dir=$(echo $pkg | sed 's/\./\//g')
echo "pkg_dir = $AXIS_HOME/$pkg_dir"
rm -rf "$AXIS_HOME/$pkg_dir/*"
interface="$pkg_dir/$name.java"

srcdir=$(pwd)
cd "$AXIS_HOME"

#
# create standard interface file, and compile it.
#
#mkdir -p $pkg_dir || exit 1
cp -f $srcdir/$name.java $interface
javac $interface || exit 1

#
# create WSDL and supporting files from generated interface file.
#
java org.apache.axis.wsdl.Java2WSDL -o $pkg_dir/$name.wsdl \
    -l"http://localhost:8080/axis/services/$name" -n  "urn:$name" \
    -p"$pkg" "urn:$name" $pkg.$name || exit 1
  
java org.apache.axis.wsdl.WSDL2Java -o . \
    -d Session -s -S true  -Nurn:$name $pkg $name.wsdl || exit 1

#
# verify results! ;-)
#
if [ ! -f "$name.wsdl" -o ! -f "$pkg_dir/"$name"SoapBindingImpl.java" ]; then
  echo "something went wrong!"
  exit 1
fi
echo "ok, now add your implementation code to $pkg_dir/$nameSoapBindingImpl.java"

# 
# Now compile the bindings and deploy the web service.
#
cp $srcdir/$name.java $pkg_dir/${name}SoapBindingImpl.java

#
# Replace $name with $nameSoapBindingImpl in the class definition
#
regexp="s/$name/${name}SoapBindingImpl/g"
echo "regexp = $regexp"
sed -e $regexp $pkg_dir/${name}SoapBindingImpl.java > $pkg_dir/temp.java
mv $pkg_dir/temp.java $pkg_dir/${name}SoapBindingImpl.java

echo "sed -e 's/public class ${name}SoapBindingImpl/public class ${name}SoapBindingImpl implements ${pkg}.${name}/g' $pkg_dir/${name}SoapBindingImpl.java > $pkg_dir/temp.java"
sed -e 's/public class ${name}SoapBindingImpl/public class ${name}SoapBindingImpl implements ${pkg}.${name}/g' $pkg_dir/${name}SoapBindingImpl.java > $pkg_dir/temp.java
mv $pkg_dir/temp.java $pkg_dir/${name}SoapBindingImpl.java

javac $pkg_dir/*.java || exit 1
cp $name.class ${name}SoapBindingImpl.class ${name}SoapBindingSkeleton.class \
  "$CATALINA_HOME/webapps/axis/WEB-INF/classes/$pkg_dir"

java org.apache.axis.client.AdminClient -p 8080 $pkg_dir/deploy.wsdd  

cd $srcdir