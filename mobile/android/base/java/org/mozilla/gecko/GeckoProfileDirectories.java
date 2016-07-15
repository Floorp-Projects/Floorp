/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;

import android.content.Context;

/**
 * <code>GeckoProfileDirectories</code> manages access to mappings from profile
 * names to salted profile directory paths, as well as the default profile name.
 *
 * This class will eventually come to encapsulate the remaining logic embedded
 * in profiles.ini; for now it's a read-only wrapper.
 */
public class GeckoProfileDirectories {
    @SuppressWarnings("serial")
    public static class NoMozillaDirectoryException extends Exception {
        public NoMozillaDirectoryException(Throwable cause) {
            super(cause);
        }

        public NoMozillaDirectoryException(String reason) {
            super(reason);
        }

        public NoMozillaDirectoryException(String reason, Throwable cause) {
            super(reason, cause);
        }
    }

    @SuppressWarnings("serial")
    public static class NoSuchProfileException extends Exception {
        public NoSuchProfileException(String detailMessage, Throwable cause) {
            super(detailMessage, cause);
        }

        public NoSuchProfileException(String detailMessage) {
            super(detailMessage);
        }
    }

    private interface INISectionPredicate {
        public boolean matches(INISection section);
    }

    private static final String MOZILLA_DIR_NAME = "mozilla";

    /**
     * Returns true if the supplied profile entry represents the default profile.
     */
    private static final INISectionPredicate sectionIsDefault = new INISectionPredicate() {
        @Override
        public boolean matches(INISection section) {
            return section.getIntProperty("Default") == 1;
        }
    };

    /**
     * Returns true if the supplied profile entry has a 'Name' field.
     */
    private static final INISectionPredicate sectionHasName = new INISectionPredicate() {
        @Override
        public boolean matches(INISection section) {
            final String name = section.getStringProperty("Name");
            return name != null;
        }
    };

    @RobocopTarget
    public static INIParser getProfilesINI(File mozillaDir) {
        return new INIParser(new File(mozillaDir, "profiles.ini"));
    }

    /**
     * Utility method to compute a salted profile name: eight random alphanumeric
     * characters, followed by a period, followed by the profile name.
     */
    public static String saltProfileName(final String name) {
        if (name == null) {
            throw new IllegalArgumentException("Cannot salt null profile name.");
        }

        final String allowedChars = "abcdefghijklmnopqrstuvwxyz0123456789";
        final int scale = allowedChars.length();
        final int saltSize = 8;

        final StringBuilder saltBuilder = new StringBuilder(saltSize + 1 + name.length());
        for (int i = 0; i < saltSize; i++) {
            saltBuilder.append(allowedChars.charAt((int)(Math.random() * scale)));
        }
        saltBuilder.append('.');
        saltBuilder.append(name);
        return saltBuilder.toString();
    }

    /**
     * Return the Mozilla directory within the files directory of the provided
     * context. This should always be the same within a running application.
     *
     * This method is package-scoped so that new {@link GeckoProfile} instances can
     * contextualize themselves.
     *
     * @return a new File object for the Mozilla directory.
     * @throws NoMozillaDirectoryException
     *             if the directory did not exist and could not be created.
     */
    @RobocopTarget
    public static File getMozillaDirectory(Context context) throws NoMozillaDirectoryException {
        final File mozillaDir = new File(context.getFilesDir(), MOZILLA_DIR_NAME);
        if (mozillaDir.mkdirs() || mozillaDir.isDirectory()) {
            return mozillaDir;
        }

        // Although this leaks a path to the system log, the path is
        // predictable (unlike a profile directory), so this is fine.
        throw new NoMozillaDirectoryException("Unable to create mozilla directory at " + mozillaDir.getAbsolutePath());
    }

    /**
     * Discover the default profile name by examining profiles.ini.
     *
     * Package-scoped because {@link GeckoProfile} needs access to it.
     *
     * @return null if there is no "Default" entry in profiles.ini, or the profile
     *         name if there is.
     * @throws NoMozillaDirectoryException
     *             if the Mozilla directory did not exist and could not be created.
     */
    static String findDefaultProfileName(final Context context) throws NoMozillaDirectoryException {
      final INIParser parser = GeckoProfileDirectories.getProfilesINI(getMozillaDirectory(context));
      if (parser.getSections() != null) {
          for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements(); ) {
              final INISection section = e.nextElement();
              if (section.getIntProperty("Default") == 1) {
                  return section.getStringProperty("Name");
              }
          }
      }
      return null;
    }

    static Map<String, String> getDefaultProfile(final File mozillaDir) {
        return getMatchingProfiles(mozillaDir, sectionIsDefault, true);
    }

    static Map<String, String> getProfilesNamed(final File mozillaDir, final String name) {
        final INISectionPredicate predicate = new INISectionPredicate() {
            @Override
            public boolean matches(final INISection section) {
                return name.equals(section.getStringProperty("Name"));
            }
        };
        return getMatchingProfiles(mozillaDir, predicate, true);
    }

    /**
     * Calls {@link GeckoProfileDirectories#getMatchingProfiles(File, INISectionPredicate, boolean)}
     * with a filter to ensure that all profiles are named.
     */
    static Map<String, String> getAllProfiles(final File mozillaDir) {
        return getMatchingProfiles(mozillaDir, sectionHasName, false);
    }

    /**
     * Return a mapping from the names of all matching profiles (that is,
     * profiles appearing in profiles.ini that match the supplied predicate) to
     * their absolute paths on disk.
     *
     * @param mozillaDir
     *            a directory containing profiles.ini.
     * @param predicate
     *            a predicate to use when evaluating whether to include a
     *            particular INI section.
     * @param stopOnSuccess
     *            if true, this method will return with the first result that
     *            matches the predicate; if false, all matching results are
     *            included.
     * @return a {@link Map} from name to path.
     */
    public static Map<String, String> getMatchingProfiles(final File mozillaDir, INISectionPredicate predicate, boolean stopOnSuccess) {
        final HashMap<String, String> result = new HashMap<String, String>();
        final INIParser parser = GeckoProfileDirectories.getProfilesINI(mozillaDir);

        if (parser.getSections() != null) {
            for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements(); ) {
                final INISection section = e.nextElement();
                if (predicate == null || predicate.matches(section)) {
                    final String name = section.getStringProperty("Name");
                    final String pathString = section.getStringProperty("Path");
                    final boolean isRelative = section.getIntProperty("IsRelative") == 1;
                    final File path = isRelative ? new File(mozillaDir, pathString) : new File(pathString);
                    result.put(name, path.getAbsolutePath());

                    if (stopOnSuccess) {
                        return result;
                    }
                }
            }
        }
        return result;
    }

    public static File findProfileDir(final File mozillaDir, final String profileName) throws NoSuchProfileException {
        // Open profiles.ini to find the correct path.
        final INIParser parser = GeckoProfileDirectories.getProfilesINI(mozillaDir);
        if (parser.getSections() != null) {
            for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements(); ) {
                final INISection section = e.nextElement();
                final String name = section.getStringProperty("Name");
                if (name != null && name.equals(profileName)) {
                    if (section.getIntProperty("IsRelative") == 1) {
                        return new File(mozillaDir, section.getStringProperty("Path"));
                    }
                    return new File(section.getStringProperty("Path"));
                }
            }
        }
        throw new NoSuchProfileException("No profile " + profileName);
    }
}
