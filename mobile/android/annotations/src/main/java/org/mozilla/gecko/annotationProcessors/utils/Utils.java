/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;
import org.mozilla.gecko.annotationProcessors.AnnotationInfo;

/** A collection of utility methods used by CodeGenerator. Largely used for translating types. */
public class Utils {

  // A collection of lookup tables to simplify the functions to follow...
  private static final HashMap<String, String> NATIVE_TYPES = new HashMap<String, String>();

  static {
    NATIVE_TYPES.put("void", "void");
    NATIVE_TYPES.put("boolean", "bool");
    NATIVE_TYPES.put("byte", "int8_t");
    NATIVE_TYPES.put("char", "char16_t");
    NATIVE_TYPES.put("short", "int16_t");
    NATIVE_TYPES.put("int", "int32_t");
    NATIVE_TYPES.put("long", "int64_t");
    NATIVE_TYPES.put("float", "float");
    NATIVE_TYPES.put("double", "double");
  }

  private static final HashMap<String, String> NATIVE_ARRAY_TYPES = new HashMap<String, String>();

  static {
    NATIVE_ARRAY_TYPES.put("boolean", "mozilla::jni::BooleanArray");
    NATIVE_ARRAY_TYPES.put("byte", "mozilla::jni::ByteArray");
    NATIVE_ARRAY_TYPES.put("char", "mozilla::jni::CharArray");
    NATIVE_ARRAY_TYPES.put("short", "mozilla::jni::ShortArray");
    NATIVE_ARRAY_TYPES.put("int", "mozilla::jni::IntArray");
    NATIVE_ARRAY_TYPES.put("long", "mozilla::jni::LongArray");
    NATIVE_ARRAY_TYPES.put("float", "mozilla::jni::FloatArray");
    NATIVE_ARRAY_TYPES.put("double", "mozilla::jni::DoubleArray");
  }

  private static final HashMap<String, String> CLASS_DESCRIPTORS = new HashMap<String, String>();

  static {
    CLASS_DESCRIPTORS.put("void", "V");
    CLASS_DESCRIPTORS.put("boolean", "Z");
    CLASS_DESCRIPTORS.put("byte", "B");
    CLASS_DESCRIPTORS.put("char", "C");
    CLASS_DESCRIPTORS.put("short", "S");
    CLASS_DESCRIPTORS.put("int", "I");
    CLASS_DESCRIPTORS.put("long", "J");
    CLASS_DESCRIPTORS.put("float", "F");
    CLASS_DESCRIPTORS.put("double", "D");
  }

  private static boolean isMozClass(final Class<?> type) {
    return type.getName().startsWith("org.mozilla.");
  }

  private static boolean useObjectForType(final Class<?> type, final boolean isHint) {
    // Essentially we want to know whether we can use generated wrappers or not:
    // If |type| is not ours, then it most likely doesn't have generated C++ wrappers.
    // Furthermore, we skip interfaces as we generally do not wrap those.
    return !isHint || type.equals(Object.class) || !isMozClass(type) || type.isInterface();
  }

  /**
   * Returns the simplified name of a class that includes any outer classes but excludes
   * package/namespace qualifiers.
   *
   * @param genScope The current scope of the class containing the current declaration. @Param type
   *     The class whose simplified name is to be generated. @Param connector String to be used for
   *     concatenating scopes.
   * @return String containing the result
   */
  private static String getSimplifiedClassName(
      final Class<?> genScope, final Class<?> type, final String connector) {
    final ArrayList<String> names = new ArrayList<>();

    // Starting with |type|, walk up our enclosing classes until we either reach genScope or we
    // have reached the outermost scope. We save them to a list because we need to reverse them
    // during output.
    Class<?> c = type;
    do {
      names.add(c.getSimpleName());
      c = c.getEnclosingClass();
    } while (c != null && (genScope == null || !genScope.equals(c)));

    // Walk through names in reverse order, joining them using |connector|
    final StringBuilder builder = new StringBuilder();
    for (int i = names.size() - 1; i >= 0; --i) {
      builder.append(names.get(i));
      if (i > 0) {
        builder.append(connector);
      }
    }

    return builder.toString();
  }

  /**
   * Returns the simplified name of a Java class that includes any outer classes but excludes
   * package qualifiers. Used for Java signature hints.
   *
   * @param genScope The current scope of the class containing the current declaration. @Param type
   *     The class whose simplified name is to be generated.
   * @return String containing the result
   */
  public static String getSimplifiedJavaClassName(final Class<?> genScope, final Class<?> type) {
    return getSimplifiedClassName(genScope, type, ".");
  }

  /** Returns the fully-qualified name of the native class wrapper for the given type. */
  public static String getWrappedNativeClassName(final Class<?> type) {
    return "mozilla::java::" + getSimplifiedClassName(null, type, "::");
  }

  /**
   * Get the C++ parameter type corresponding to the provided type parameter.
   *
   * @param type Class to determine the corresponding JNI type for.
   * @return C++ type as a String
   */
  public static String getNativeParameterType(Class<?> type, AnnotationInfo info) {
    return getNativeParameterType(type, info, false);
  }

  /**
   * Get the C++ hint type corresponding to the provided type parameter. The returned type may be
   * more specific than the type returned by getNativeParameterType, as this method is used for
   * generating comments instead of machine-readable code.
   *
   * @param type Class to determine the corresponding JNI type for.
   * @return C++ type as a String
   */
  public static String getNativeParameterTypeHint(Class<?> type, AnnotationInfo info) {
    return getNativeParameterType(type, info, true);
  }

  private static String getNativeParameterType(
      final Class<?> type, final AnnotationInfo info, final boolean isHint) {
    final String name = type.getName().replace('.', '/');

    String value = NATIVE_TYPES.get(name);
    if (value != null) {
      return value;
    }

    if (type.isArray()) {
      final String compName = type.getComponentType().getName();
      value = NATIVE_ARRAY_TYPES.get(compName);
      if (value != null) {
        return value + "::Param";
      }
      return "mozilla::jni::ObjectArray::Param";
    }

    if (type.equals(String.class) || type.equals(CharSequence.class)) {
      return "mozilla::jni::String::Param";
    }

    if (type.equals(Class.class)) {
      // You're doing reflection on Java objects from inside C, returning Class objects
      // to C, generating the corresponding code using this Java program. Really?!
      return "mozilla::jni::Class::Param";
    }

    if (type.equals(Throwable.class)) {
      return "mozilla::jni::Throwable::Param";
    }

    if (type.equals(ByteBuffer.class)) {
      return "mozilla::jni::ByteBuffer::Param";
    }

    if (useObjectForType(type, isHint)) {
      return "mozilla::jni::Object::Param";
    }

    return getWrappedNativeClassName(type) + "::Param";
  }

  /**
   * Get the C++ return type corresponding to the provided type parameter.
   *
   * @param type Class to determine the corresponding JNI type for.
   * @return C++ type as a String
   */
  public static String getNativeReturnType(Class<?> type, AnnotationInfo info) {
    return getNativeReturnType(type, info, false);
  }

  /**
   * Get the C++ hint return type corresponding to the provided type parameter. The returned type
   * may be more specific than the type returned by getNativeReturnType, as this method is used for
   * generating comments instead of machine-readable code.
   *
   * @param type Class to determine the corresponding JNI type for.
   * @return C++ type as a String
   */
  public static String getNativeReturnTypeHint(Class<?> type, AnnotationInfo info) {
    return getNativeReturnType(type, info, true);
  }

  private static String getNativeReturnType(
      final Class<?> type, final AnnotationInfo info, final boolean isHint) {
    final String name = type.getName().replace('.', '/');

    String value = NATIVE_TYPES.get(name);
    if (value != null) {
      return value;
    }

    if (type.isArray()) {
      final String compName = type.getComponentType().getName();
      value = NATIVE_ARRAY_TYPES.get(compName);
      if (value != null) {
        return value + "::LocalRef";
      }
      return "mozilla::jni::ObjectArray::LocalRef";
    }

    if (type.equals(String.class)) {
      return "mozilla::jni::String::LocalRef";
    }

    if (type.equals(Class.class)) {
      // You're doing reflection on Java objects from inside C, returning Class objects
      // to C, generating the corresponding code using this Java program. Really?!
      return "mozilla::jni::Class::LocalRef";
    }

    if (type.equals(Throwable.class)) {
      return "mozilla::jni::Throwable::LocalRef";
    }

    if (type.equals(ByteBuffer.class)) {
      return "mozilla::jni::ByteBuffer::LocalRef";
    }

    if (useObjectForType(type, isHint)) {
      return "mozilla::jni::Object::LocalRef";
    }

    return getWrappedNativeClassName(type) + "::LocalRef";
  }

  /**
   * Get the JNI class descriptor corresponding to the provided type parameter.
   *
   * @param type Class to determine the corresponding JNI descriptor for.
   * @return Class descripor as a String
   */
  public static String getClassDescriptor(Class<?> type) {
    final String name = type.getName().replace('.', '/');

    final String classDescriptor = CLASS_DESCRIPTORS.get(name);
    if (classDescriptor != null) {
      return classDescriptor;
    }

    if (type.isArray()) {
      // Array names are already in class descriptor form.
      return name;
    }

    return "L" + name + ';';
  }

  /**
   * Get the JNI signaure for a member.
   *
   * @param member Member to get the signature for.
   * @return JNI signature as a string
   */
  public static String getSignature(Member member) {
    return member instanceof Field
        ? getSignature((Field) member)
        : member instanceof Method
            ? getSignature((Method) member)
            : getSignature((Constructor<?>) member);
  }

  /**
   * Get the JNI signaure for a field.
   *
   * @param member Field to get the signature for.
   * @return JNI signature as a string
   */
  public static String getSignature(Field member) {
    return getClassDescriptor(member.getType());
  }

  private static String getSignature(Class<?>[] args, Class<?> ret) {
    final StringBuilder sig = new StringBuilder("(");
    for (int i = 0; i < args.length; i++) {
      sig.append(getClassDescriptor(args[i]));
    }
    return sig.append(')').append(getClassDescriptor(ret)).toString();
  }

  /**
   * Get the JNI signaure for a method.
   *
   * @param member Method to get the signature for.
   * @return JNI signature as a string
   */
  public static String getSignature(Method member) {
    return getSignature(member.getParameterTypes(), member.getReturnType());
  }

  /**
   * Get the JNI signaure for a constructor.
   *
   * @param member Constructor to get the signature for.
   * @return JNI signature as a string
   */
  public static String getSignature(Constructor<?> member) {
    return getSignature(member.getParameterTypes(), void.class);
  }

  /**
   * Get the C++ name for a member.
   *
   * @param member Member to get the name for.
   * @return JNI name as a string
   */
  public static String getNativeName(Member member) {
    final String name = getMemberName(member);
    return name.substring(0, 1).toUpperCase(Locale.ROOT) + name.substring(1);
  }

  /**
   * Get the C++ name for a member.
   *
   * @param member Member to get the name for.
   * @return JNI name as a string
   */
  public static String getNativeName(Class<?> clz) {
    final String name = clz.getName();
    return name.substring(0, 1).toUpperCase(Locale.ROOT) + name.substring(1);
  }

  /**
   * Get the C++ name for a member.
   *
   * @param member Member to get the name for.
   * @return JNI name as a string
   */
  public static String getNativeName(AnnotatedElement element) {
    if (element instanceof Class<?>) {
      return getNativeName((Class<?>) element);
    } else if (element instanceof Member) {
      return getNativeName((Member) element);
    } else {
      return null;
    }
  }

  /**
   * Get the JNI name for a member.
   *
   * @param member Member to get the name for.
   * @return JNI name as a string
   */
  public static String getMemberName(Member member) {
    if (member instanceof Constructor) {
      return "<init>";
    }
    return member.getName();
  }

  public static String getUnqualifiedName(String name) {
    return name.substring(name.lastIndexOf(':') + 1);
  }

  /**
   * Determine if a member is declared static.
   *
   * @param member The Member to check.
   * @return true if the member is declared static, false otherwise.
   */
  public static boolean isStatic(final Member member) {
    return Modifier.isStatic(member.getModifiers());
  }

  /**
   * Determine if a member is declared final.
   *
   * @param member The Member to check.
   * @return true if the member is declared final, false otherwise.
   */
  public static boolean isFinal(final Member member) {
    return Modifier.isFinal(member.getModifiers());
  }

  /**
   * Determine if a member is declared public.
   *
   * @param member The Member to check.
   * @return true if the member is declared public, false otherwise.
   */
  public static boolean isPublic(final Member member) {
    return Modifier.isPublic(member.getModifiers());
  }

  /**
   * Return an enum value with the given name.
   *
   * @param type Enum class type.
   * @param name Enum value name.
   * @return Enum value with the given name.
   */
  public static <T extends Enum<T>> T getEnumValue(Class<T> type, String name) {
    try {
      return Enum.valueOf(type, name.toUpperCase(Locale.ROOT));

    } catch (IllegalArgumentException e) {
      final Object[] values;
      try {
        values = (Object[]) type.getDeclaredMethod("values").invoke(null);
      } catch (final NoSuchMethodException
          | IllegalAccessException
          | InvocationTargetException exception) {
        throw new RuntimeException("Cannot access enum: " + type, exception);
      }

      StringBuilder names = new StringBuilder();

      for (int i = 0; i < values.length; i++) {
        if (i != 0) {
          names.append(", ");
        }
        names.append(values[i].toString().toLowerCase(Locale.ROOT));
      }

      System.err.println("***");
      System.err.println("*** Invalid value \"" + name + "\" for " + type.getSimpleName());
      System.err.println("*** Specify one of " + names.toString());
      System.err.println("***");
      e.printStackTrace(System.err);
      System.exit(1);
      return null;
    }
  }

  public static String getIfdefHeader(String ifdef) {
    if (ifdef.isEmpty()) {
      return "";
    } else if (ifdef.startsWith("!")) {
      return "#ifndef " + ifdef.substring(1) + "\n";
    }
    return "#ifdef " + ifdef + "\n";
  }

  public static String getIfdefFooter(String ifdef) {
    if (ifdef.isEmpty()) {
      return "";
    }
    return "#endif // " + ifdef + "\n";
  }

  public static boolean isJNIObject(Class<?> cls) {
    for (; cls != null; cls = cls.getSuperclass()) {
      if (cls.getName().equals("org.mozilla.gecko.mozglue.JNIObject")) {
        return true;
      }
    }
    return false;
  }
}
