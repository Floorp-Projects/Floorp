/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.graphics.Typeface
import android.os.Bundle
import android.text.util.Linkify
import android.view.View
import android.widget.ArrayAdapter
import android.widget.ListView
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAboutLibrariesBinding
import org.mozilla.fenix.ext.showToolbar
import java.nio.charset.Charset
import java.util.Locale

/**
 * Displays the licenses of all the libraries used by Fenix.
 *
 * This is a re-implementation of play-services-oss-licenses library.
 * We can't use the official implementation in the OSS flavor of Fenix
 * because it is proprietary and closed-source.
 *
 * There are popular FLOSS alternatives to Google's plugin and library
 * such as AboutLibraries (https://github.com/mikepenz/AboutLibraries)
 * but we considered the risk of introducing such third-party dependency
 * to Fenix too high. Therefore, we use Google's gradle plugin to
 * extract the dependencies and their licenses, and this fragment
 * to show the extracted licenses to the end-user.
 */
class AboutLibrariesFragment : Fragment(R.layout.fragment_about_libraries) {

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val binding = FragmentAboutLibrariesBinding.bind(view)
        setupLibrariesListView(binding.aboutLibrariesListview)
    }

    override fun onResume() {
        super.onResume()
        val appName = getString(R.string.app_name)
        showToolbar(getString(R.string.open_source_licenses_title, appName))
    }

    private fun setupLibrariesListView(listView: ListView) {
        val libraries = parseLibraries()
        listView.adapter = ArrayAdapter(
            listView.context,
            android.R.layout.simple_list_item_1,
            libraries,
        )
        listView.setOnItemClickListener { _, _, position, _ ->
            showLicenseDialog(libraries[position])
        }
    }

    private fun parseLibraries(): List<LibraryItem> {
        /*
            The gradle plugin "oss-licenses-plugin" creates two "raw" resources:

               - third_party_licenses which is the binary concatenation of all the licenses text for
                 all the libraries. License texts can either be an URL to a license file or just the
                 raw text of the license.

               - third_party_licenses_metadata which contains one dependency per line formatted in
                 the following way: "[start_offset]:[length] [name]"

                 [start_offset]     : first byte in third_party_licenses that contains the license
                                      text for this library.
                 [length]           : length of the license text for this library in
                                      third_party_licenses.
                 [name]             : either the name of the library, or its artifact name.

            See https://github.com/google/play-services-plugins/tree/master/oss-licenses-plugin
         */
        val licensesData = resources
            .openRawResource(R.raw.third_party_licenses)
            .readBytes()
        val licensesMetadataReader = resources
            .openRawResource(R.raw.third_party_license_metadata)
            .bufferedReader()

        return licensesMetadataReader.use { reader -> reader.readLines() }.map { line ->
            val (section, name) = line.split(" ", limit = 2)
            val (startOffset, length) = section.split(":", limit = 2).map(String::toInt)
            val licenseData = licensesData.sliceArray(startOffset until startOffset + length)
            val licenseText = licenseData.toString(Charset.forName("UTF-8"))
            LibraryItem(name, licenseText)
        }.sortedBy { item -> item.name.lowercase(Locale.ROOT) }
    }

    private fun showLicenseDialog(libraryItem: LibraryItem) {
        val dialog = AlertDialog.Builder(requireContext())
            .setTitle(libraryItem.name)
            .setMessage(libraryItem.license)
            .create()
            .withCenterAlignedButtons()
        dialog.show()

        val textView = dialog.findViewById<TextView>(android.R.id.message)!!
        Linkify.addLinks(textView, Linkify.WEB_URLS)
        textView.linksClickable = true
        textView.textSize = LICENSE_TEXT_SIZE
        textView.typeface = Typeface.MONOSPACE
    }

    companion object {
        private const val LICENSE_TEXT_SIZE = 10F
    }
}

private class LibraryItem(val name: String, val license: String) {
    override fun toString(): String {
        return name
    }
}
