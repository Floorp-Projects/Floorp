#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


export CATALINA_HOME=/var/tomcat-4.1.27
export AXIS_HOME=/var/tomcat-4.1.27/axis-1_1
export CLASSPATH=$AXIS_HOME/lib/axis.jar:$AXIS_HOME/lib/commons-discovery.jar:$AXIS_HOME/lib/commons-logging.jar:$AXIS_HOME/lib/jaxrpc.jar:$AXIS_HOME/lib/saaj.jar:$AXIS_HOME/lib/log4j-1.2.4.jar:$AXIS_HOME/lib/wsdl4j.jar:$CATALINA_HOME/webapps/axis/WEB-INF/classes

name="PluginFinderService"
pkg="org.mozilla.pfs"
pkg_dir=$CATALINA_HOME/webapps/axis/WEB-INF/classes/org/mozilla/pfs

echo "Copying Source..."

rm -f $pkg_dir/*.*
cp -f *.java $pkg_dir

echo "Compiling Source..."
javac -g $pkg_dir/*.java

echo "Generating WSDL..."
java org.apache.axis.wsdl.Java2WSDL -o $name.wsdl \
      -l"http://localhost:8080/axis/services/$name" \
      -n "urn:$name" -p"$pkg" "urn:$name" $pkg.$name
      
echo "Generating Stubs from WSDL..."
rm -f org/mozilla/pfs/*.*
java org.apache.axis.wsdl.WSDL2Java -o . -s -S true -Nurn:$name $pkg $name.wsdl

# make our PluginFinderService.java looking the SoapBindingImpl syntax and replace
cp $name.java $name.temp
regexp="s/$name/${name}SoapBindingImpl/g"
sed -e $regexp $name.temp > $name.temp2
rm $name.temp

regexp="s/public class ${name}SoapBindingImpl/public class ${name}SoapBindingImpl implements ${pkg}.${name}/g"
sed -e "$regexp" $name.temp2 > $name.temp
rm $name.temp2
mv $name.temp org/mozilla/pfs/${name}SoapBindingImpl.java

rm -f $pkg_dir/*.java
cp org/mozilla/pfs/* $pkg_dir/
javac $pkg_dir/*.java

echo "Deploying Web Service..."
java org.apache.axis.client.AdminClient -p 8080 $pkg_dir/deploy.wsdd  

echo "All Done. Hoorah!"
