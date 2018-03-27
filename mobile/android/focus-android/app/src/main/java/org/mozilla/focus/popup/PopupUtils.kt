/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.popup

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.support.v4.content.ContextCompat
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.webkit.URLUtil
import android.widget.ImageView
import android.widget.PopupWindow
import android.widget.TextView
import mozilla.components.ktx.android.graphics.drawable.toBitmap
import mozilla.components.utils.DrawableUtils
import org.mozilla.focus.R
import org.mozilla.focus.session.Session
import java.net.MalformedURLException
import java.net.URL

object PopupUtils {

    fun createSecurityPopup(context: Context, session: Session): PopupWindow? {
        if (session.secure.value != null) {
            val isSecure = session.secure.value
            val url = session.url.value
            val res = context.resources

            val inflater = LayoutInflater.from(context)
            val popUpView = inflater.inflate(R.layout.security_popup, null)
            val origin = session.securityOrigin.value
            val verifierInfo = session.securityVerifier.value
            val hostInfo = popUpView.findViewById<TextView>(R.id.site_identity_title)
            val securityInfoIcon = popUpView.findViewById<ImageView>(R.id.site_identity_icon)
            val verifier = popUpView.findViewById<TextView>(R.id.verifier)

            setOrigin(hostInfo, origin, url)

            val identityState = popUpView.findViewById<TextView>(R.id.site_identity_state)
            identityState.setText(
                if (isSecure)
                    R.string.security_popup_secure_connection
                else
                    R.string.security_popup_insecure_connection)

            if (isSecure) {
                setSecurityInfoSecure(context, identityState, verifierInfo, verifier,
                    securityInfoIcon)
            } else {
                verifier.visibility = View.GONE
                setSecurityInfoInsecure(context, identityState, url, securityInfoIcon)
            }

            identityState.compoundDrawablePadding = res.getDimension(
                R.dimen.doorhanger_drawable_padding).toInt()

            return PopupWindow(popUpView, res.getDimension(R.dimen.doorhanger_width).toInt(),
                ViewGroup.LayoutParams.WRAP_CONTENT, true)
        }
        return null
    }

    private fun setOrigin(hostInfo: TextView, origin: String?, url: String) {
        if (origin != null) {
            hostInfo.text = origin
        } else {
            hostInfo.text = try {
                URL(url).host
            } catch (e: MalformedURLException) {
                url
            }
        }
    }

    private fun setSecurityInfoSecure(context: Context, identityState: TextView,
                                      verifierInfo: String?, verifier: TextView,
                                      securityInfoIcon: ImageView) {
        val photonGreen = ContextCompat.getColor(context, R.color.photonGreen60)
        val checkIcon = DrawableUtils.loadAndTintDrawable(context, R.drawable.ic_check, photonGreen)
        identityState.setCompoundDrawables(
            getScaledDrawable(context, R.dimen.doorhanger_small_icon, checkIcon), null,
            null, null)
        identityState.setTextColor(photonGreen)
        if (verifierInfo != null) {
            verifier.text = context.getString(R.string.security_popup_security_verified,
                verifierInfo)
            verifier.visibility = View.VISIBLE
        } else {
            verifier.visibility = View.GONE
        }
        val securityIcon = DrawableUtils.loadAndTintDrawable(context, R.drawable.ic_lock,
            photonGreen)
        securityInfoIcon.setImageDrawable(securityIcon)
    }

    private fun setSecurityInfoInsecure(context: Context, identityState: TextView, url: String,
                                        securityInfoIcon: ImageView) {
        identityState.setTextColor(Color.WHITE)

        val inactiveColor = ContextCompat.getColor(context, R.color.colorTextInactive)
        val photonYellow = ContextCompat.getColor(context, R.color.photonYellow60)

        val securityIcon: Drawable
        if (URLUtil.isHttpUrl(url)) {
            securityIcon = DrawableUtils.loadAndTintDrawable(context, R.drawable.ic_internet,
                inactiveColor)
        } else {
            securityIcon = DrawableUtils.loadAndTintDrawable(context, R.drawable.ic_warning,
                photonYellow)
            identityState.setCompoundDrawables(
                getScaledDrawable(context, R.dimen.doorhanger_small_icon,
                    securityIcon),
                null,
                null, null)
        }
        securityInfoIcon.setImageDrawable(securityIcon)
    }

    private fun getScaledDrawable(context: Context, size: Int, drawable: Drawable): Drawable {
        val iconSize = context.resources.getDimension(size).toInt()
        val icon = BitmapDrawable(context.resources, drawable.toBitmap())
        icon.setBounds(0, 0, iconSize, iconSize)
        return icon
    }
}
