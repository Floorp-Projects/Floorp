/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.prompts.provider

/**
 * A file provider to provide functionality for the feature prompts component.
 *
 * We need this class to create a fully qualified class name that doesn't clash with other
 * file providers in other components see https://stackoverflow.com/a/43444164/5533820.
 *
 * Be aware, when creating new file resources avoid using common names like "@xml/file_paths",
 * as other file providers could be using the same names and this could case unexpected behaviors.
 * As a convention try to use unique names like using the name of the component as a prefix of the
 * name of the file, like component_xxx_file_paths.xml.
 */
/** @suppress */
class FileProvider : androidx.core.content.FileProvider()
