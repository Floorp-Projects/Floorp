#!/bin/sh

# Original script by Darin Fisher, modified to also deploy
# by Ben Goodger

pkg="org.mozilla"

CATALINA_HOME=/usr/local/tomcat
AXIS_LIB=$AXIS_HOME/lib
AXISCLASSPATH=$AXIS_LIB/axis.jar:$AXIS_LIB/commons-discovery.jar:$AXIS_LIB/commons-logging.jar:$AXIS_LIB/jaxrpc.jar:$AXIS_LIB/saaj.jar:$AXIS_LIB/log4j-1.2.8.jar:$AXIS_LIB/wsdl4j.jar:$CATALINA_HOME/webapps/axis/WEB-INF/lib/mysql-connector.jar
CLASSPATH=$AXIS_HOME:$CATALINA_HOME/webapps/axis/WEB-INF/classes:$AXISCLASSPATH:$CLASSPATH
JAVA_ENDORSED_DIRS=$CATALINA_HOME/bin
JAVA_HOME=/usr/java/j2sdk1.4.2_04
java=$JAVA_HOME/bin/java
javac=$JAVA_HOME/bin/javac

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

#
# remove old generated files
#
srcdir=$(pwd)
cd "$AXIS_HOME/$pkg_dir"
rm -rf *
cd $srcdir
interface="$pkg_dir/$name.java"

cd "$AXIS_HOME"

#
# create standard interface file, and compile it.
#
#mkdir -p $pkg_dir || exit 1
cp -f "$srcdir"/*.java "$AXIS_HOME/$pkg_dir"
echo "Compiling original source files..."
$javac "$pkg_dir"/*.java || exit 1

#
# create WSDL and supporting files from generated interface file.
#
$java org.apache.axis.wsdl.Java2WSDL -o "$AXIS_HOME/$pkg_dir/$name.wsdl" \
    -l"http://localhost:8080/axis/services/$name" -n  "urn:$name" \
    -p"$pkg" "urn:$name" $pkg.$name || exit 1
  
$java org.apache.axis.wsdl.WSDL2Java -o . \
    -d Session -s -S true  -Nurn:$name $pkg "$AXIS_HOME/$pkg_dir/$name.wsdl" || exit 1

#
# verify results! ;-)
#
if [ ! -f "$AXIS_HOME/$pkg_dir/$name.wsdl" -o ! -f "$pkg_dir/"$name"SoapBindingImpl.java" ]; then
  echo "something went wrong!"
  exit 1
fi

# 
# Now compile the bindings and deploy the web service.
#
cp $srcdir/$name.java $pkg_dir/${name}SoapBindingImpl.java

#
# Replace $name with $nameSoapBindingImpl in the class definition
#
regexp="s/$name/${name}SoapBindingImpl/g"
sed -e $regexp $pkg_dir/${name}SoapBindingImpl.java > $pkg_dir/temp.java
mv $pkg_dir/temp.java $pkg_dir/${name}SoapBindingImpl.java

regexp="s/public class ${name}SoapBindingImpl/public class ${name}SoapBindingImpl implements ${pkg}.${name}/g"
sed -e "$regexp" $pkg_dir/${name}SoapBindingImpl.java > $pkg_dir/temp.java
mv $pkg_dir/temp.java "$pkg_dir/${name}SoapBindingImpl.java"

echo "Compiling generated source files..."
$javac "$pkg_dir/*.java"  # || exit 1
cd "$AXIS_HOME/$pkg_dir"
cp *.class "$CATALINA_HOME/webapps/axis/WEB-INF/classes/$pkg_dir"

$java org.apache.axis.client.AdminClient -p 8080 deploy.wsdd  

cd $srcdir

echo "$name compiled and deployed successfully, you may need to reload Axis before your service is updated."

