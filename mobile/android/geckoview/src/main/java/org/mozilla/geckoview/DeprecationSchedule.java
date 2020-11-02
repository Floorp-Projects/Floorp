package org.mozilla.geckoview;

import java.lang.annotation.Documented;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

import static java.lang.annotation.ElementType.CONSTRUCTOR;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.LOCAL_VARIABLE;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PACKAGE;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.ElementType.TYPE;

/**
 * Additional metadata about a deprecation notice.
 */
@Documented
@Retention(RetentionPolicy.RUNTIME)
@Target(value = {CONSTRUCTOR, FIELD, LOCAL_VARIABLE, METHOD, PACKAGE, PARAMETER, TYPE})
public @interface DeprecationSchedule {
    /**
     * @return Major version when we expect to remove the
     * deprecated member attached to this annotation.
     */
    int version();

    /**
     * @return Identifier for a deprecation notice. All notices with the
     * same identifier will be removed at the same time.
     */
    String id();
}
