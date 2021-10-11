/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.classloader;

import org.mozilla.gecko.annotationProcessors.utils.GeneratableElementIterator;

public class ClassWithOptions {
  public final Class<?> wrappedClass;
  public final String generatedName;
  public final String ifdef;

  public ClassWithOptions(Class<?> someClass, String name, String ifdef) {
    wrappedClass = someClass;
    generatedName = name;
    this.ifdef = ifdef;
  }

  public boolean hasGenerated() {
    final GeneratableElementIterator methodIterator = new GeneratableElementIterator(this);

    if (methodIterator.hasNext()) {
      return true;
    }

    final ClassWithOptions[] innerClasses = methodIterator.getInnerClasses();
    for (ClassWithOptions innerClass : innerClasses) {
      if (innerClass.hasGenerated()) {
        return true;
      }
    }

    return false;
  }
}
