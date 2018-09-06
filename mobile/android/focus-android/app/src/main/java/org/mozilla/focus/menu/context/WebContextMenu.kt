/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.menu.context

import android.app.Activity
import android.app.Dialog
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.os.Environment
import android.preference.PreferenceManager
import android.support.design.internal.NavigationMenuView
import android.support.design.widget.NavigationView
import android.support.v7.app.AlertDialog
import android.text.Html
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import org.mozilla.focus.R
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.session.Source
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.telemetry.TelemetryWrapper.BrowserContextMenuValue
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils
import org.mozilla.focus.web.Download
import org.mozilla.focus.web.IWebView

/**
 * The context menu shown when long pressing a URL or an image inside the WebView.
 */
object WebContextMenu {
    private fun createTitleView(context: Context, title: String): View {
        val titleView = LayoutInflater.from(context).inflate(R.layout.context_menu_title, null) as TextView
        titleView.text = title
        return titleView
    }

    @Suppress("ComplexMethod")
    fun show(context: Context, callback: IWebView.Callback, hitTarget: IWebView.HitTarget) {
        if (!(hitTarget.isLink || hitTarget.isImage)) {
            // We don't support any other classes yet:
            throw IllegalStateException("WebContextMenu can only handle long-press on images and/or links.")
        }

        TelemetryWrapper.openWebContextMenuEvent()

        val builder = AlertDialog.Builder(context)

        builder.setCustomTitle(when {
            hitTarget.isLink -> createTitleView(context, hitTarget.linkURL)
            hitTarget.isImage -> createTitleView(context, hitTarget.imageURL)
            else -> throw IllegalStateException("Unhandled long press target type")
        })

        val view = LayoutInflater.from(context).inflate(R.layout.context_menu, null)
        builder.setView(view)

        builder.setOnCancelListener {
            // What type of element was long-pressed
            val value: BrowserContextMenuValue = if (hitTarget.isImage && hitTarget.isLink) {
                BrowserContextMenuValue.ImageWithLink
            } else if (hitTarget.isImage) {
                BrowserContextMenuValue.Image
            } else {
                BrowserContextMenuValue.Link
            }

            // This even is only sent when the back button is pressed, or when a user
            // taps outside of the dialog:
            TelemetryWrapper.cancelWebContextMenuEvent(value)
        }

        val dialog = builder.create()
        dialog.window!!.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

        val menu = view.findViewById<View>(R.id.context_menu) as NavigationView
        menu.elevation = 0f
        val navigationMenuView = menu.getChildAt(0) as? NavigationMenuView
        if (navigationMenuView != null) {
            navigationMenuView.isVerticalScrollBarEnabled = false
        }

        setupMenuForHitTarget(dialog, menu, callback, hitTarget, context)

        val warningView = view.findViewById<View>(R.id.warning) as TextView
        if (hitTarget.isImage) {
            menu.setBackgroundResource(R.drawable.no_corners_context_menu_navigation_view_background)

            @Suppress("DEPRECATION")
            warningView.text = Html.fromHtml(
                context.getString(R.string.contextmenu_image_warning, context.getString(R.string.app_name))
            )
        } else {
            warningView.visibility = View.GONE
        }

        dialog.show()
    }

    /**
     * Set up the correct menu contents. Note: this method can only be called once the Dialog
     * has already been created - we need the dialog in order to be able to dismiss it in the
     * menu callbacks.
     */
    @Suppress("ComplexMethod")
    private fun setupMenuForHitTarget(
        dialog: Dialog,
        navigationView: NavigationView,
        callback: IWebView.Callback,
        hitTarget: IWebView.HitTarget,
        context: Context
    ) = with(navigationView) {
        inflateMenu(R.menu.menu_browser_context)

        menu.findItem(R.id.menu_new_tab).isVisible = hitTarget.isLink
        menu.findItem(R.id.menu_link_share).isVisible = hitTarget.isLink
        menu.findItem(R.id.menu_link_copy).isVisible = hitTarget.isLink
        menu.findItem(R.id.menu_image_share).isVisible = hitTarget.isImage
        menu.findItem(R.id.menu_image_copy).isVisible = hitTarget.isImage

        menu.findItem(R.id.menu_image_save).isVisible = hitTarget.isImage &&
            UrlUtils.isHttpOrHttps(hitTarget.imageURL)

        setNavigationItemSelectedListener { item ->
            dialog.dismiss()

            when (item.itemId) {
                R.id.menu_new_tab -> {
                    SessionManager.getInstance()
                            .createNewTabSession(Source.MENU, hitTarget.linkURL, context).also { session ->
                                if (!Settings.getInstance(context).shouldOpenNewTabs()) {
                                    // Show Snackbar to allow users to switch to tab they just opened
                                    val snackbar = ViewUtils.getBrandedSnackbar(
                                            (context as Activity).findViewById(android.R.id.content),
                                            R.string.new_tab_opened_snackbar)
                                    snackbar.setAction(R.string.open_new_tab_snackbar) {
                                        SessionManager.getInstance().selectSession(session)
                                    }
                                    snackbar.show()
                                }
                            }
                    TelemetryWrapper.openLinkInNewTabEvent()
                    PreferenceManager.getDefaultSharedPreferences(context).edit()
                        .putBoolean(
                            context.getString(R.string.has_opened_new_tab),
                            true
                        ).apply()

                    true
                }
                R.id.menu_link_share -> {
                    TelemetryWrapper.shareLinkEvent()
                    val shareIntent = Intent(Intent.ACTION_SEND)
                    shareIntent.type = "text/plain"
                    shareIntent.putExtra(Intent.EXTRA_TEXT, hitTarget.linkURL)
                    dialog.context.startActivity(
                        Intent.createChooser(
                            shareIntent,
                            dialog.context.getString(R.string.share_dialog_title)
                        )
                    )
                    true
                }
                R.id.menu_image_share -> {
                    TelemetryWrapper.shareImageEvent()
                    val shareIntent = Intent(Intent.ACTION_SEND)
                    shareIntent.type = "text/plain"
                    shareIntent.putExtra(Intent.EXTRA_TEXT, hitTarget.imageURL)
                    dialog.context.startActivity(
                        Intent.createChooser(
                            shareIntent,
                            dialog.context.getString(R.string.share_dialog_title)
                        )
                    )
                    true
                }
                R.id.menu_image_save -> {
                    val download =
                        Download(hitTarget.imageURL, null, null, null, -1, Environment.DIRECTORY_PICTURES, null)
                    callback.onDownloadStart(download)
                    TelemetryWrapper.saveImageEvent()
                    true
                }
                R.id.menu_link_copy, R.id.menu_image_copy -> {
                    val clipboard = dialog.context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                    val uri: Uri = when {
                        item.itemId == R.id.menu_link_copy -> {
                            TelemetryWrapper.copyLinkEvent()
                            Uri.parse(hitTarget.linkURL)
                        }
                        item.itemId == R.id.menu_image_copy -> {
                            TelemetryWrapper.copyImageEvent()
                            Uri.parse(hitTarget.imageURL)
                        }
                        else -> throw IllegalStateException("Unknown hitTarget type - cannot copy to clipboard")
                    }

                    val clip = ClipData.newUri(dialog.context.contentResolver, "URI", uri)
                    clipboard.primaryClip = clip
                    true
                }
                else -> throw IllegalArgumentException("Unhandled menu item id=" + item.itemId)
            }
        }
    }
}
