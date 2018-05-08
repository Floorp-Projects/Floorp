/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.classloader;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Collections;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

/**
 * A classloader which can be initialised with a list of jar files and which can provide an iterator
 * over the top level classes in the jar files it was initialised with.
 * classNames is kept sorted to ensure iteration order is consistent across program invocations.
 * Otherwise, we'd forever be reporting the outdatedness of the generated code as we permute its
 * contents.
 */
public class IterableJarLoadingURLClassLoader extends URLClassLoader {
    LinkedList<String> classNames = new LinkedList<String>();

    /**
     * Create an instance and return its iterator. Provides an iterator over the classes in the jar
     * files provided as arguments.
     * Inner classes are not supported.
     *
     * @param args A list of jar file names an iterator over the classes of which is desired.
     * @return An iterator over the top level classes in the jar files provided, in arbitrary order.
     */
    public static Iterator<ClassWithOptions> getIteratorOverJars(String[] args) {
        URL[] urlArray = new URL[args.length];
        LinkedList<String> aClassNames = new LinkedList<String>();

        for (int i = 0; i < args.length; i++) {
            try {
                urlArray[i] = (new File(args[i])).toURI().toURL();

                Enumeration<JarEntry> entries = new JarFile(args[i]).entries();
                while (entries.hasMoreElements()) {
                    JarEntry e = entries.nextElement();
                    String fName = e.getName();
                    if (!fName.endsWith(".class")) {
                        continue;
                    }
                    final String className = fName.substring(0, fName.length() - 6).replace('/', '.');

                    aClassNames.add(className);
                }
            } catch (IOException e) {
                System.err.println("Error loading jar file \"" + args[i] + '"');
                e.printStackTrace(System.err);
            }
        }
        Collections.sort(aClassNames);
        return new JarClassIterator(new IterableJarLoadingURLClassLoader(urlArray, aClassNames));
    }

    /**
     * Constructs a classloader capable of loading all classes given as URLs in urls. Used by static
     * method above.
     *
     * @param urls        URLs for all classes the new instance shall be capable of loading.
     * @param aClassNames A list of names of the classes this instance shall be capable of loading.
     */
    protected IterableJarLoadingURLClassLoader(URL[] urls, LinkedList<String> aClassNames) {// Array to populate with URLs for each class in the given jars.
        super(urls);
        classNames = aClassNames;
    }
}
