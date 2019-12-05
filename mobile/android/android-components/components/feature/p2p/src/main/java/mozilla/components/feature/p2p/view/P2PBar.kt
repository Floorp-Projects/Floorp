/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.p2p.view

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.Button
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.constraintlayout.widget.ConstraintLayout
import kotlinx.android.synthetic.main.mozac_feature_p2p_view.view.*
import mozilla.components.feature.p2p.R

private const val DEFAULT_VALUE = 0

/**
 * A toolbar for peer-to-peer communication between browsers. Setting [listener] causes the
 * buttons to become active.
 */
@Suppress("TooManyFunctions")
class P2PBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr), P2PView {
    override var listener: P2PView.Listener? = null

    init {
        inflate(getContext(), R.layout.mozac_feature_p2p_view, this)
        initializeButtons(true, false)

        p2pAdvertiseBtn.setOnClickListener {
            listener?.onAdvertise() ?: failure("internal error: listener unset")
            showConnectButtons(false)
        }
        p2pDiscoverBtn.setOnClickListener {
            listener?.onDiscover() ?: failure("internal error: listener unset")
            showConnectButtons(false)
        }
        p2pSendUrlBtn.setOnClickListener {
            listener?.onSendUrl()
        }
        p2pSendPageBtn.setOnClickListener {
            listener?.onSendPage()
            showSendButtons(false)
        }
        p2pResetBtn.setOnClickListener {
            listener?.onReset()
            clear()
        }
        p2pCloseBtn.setOnClickListener {
            listener?.onCloseToolbar()
        }
    }

    override fun initializeButtons(connectB: Boolean, sendB: Boolean) {
        showConnectButtons(connectB) // advertise, discover
        showSendButtons(sendB) // send URL, send page
    }

    private fun showButton(btn: Button, b: Boolean) {
        btn.visibility = if (b) View.VISIBLE else View.GONE
        btn.isEnabled = b
    }

    internal fun showConnectButtons(b: Boolean) {
        showButton(p2pAdvertiseBtn, b)
        showButton(p2pDiscoverBtn, b)
        p2pResetBtn.isEnabled = !b
    }

    internal fun showSendButtons(b: Boolean = true) {
        showButton(p2pSendUrlBtn, b)
        showButton(p2pSendPageBtn, b)
    }

    override fun updateStatus(status: String) {
        p2pStatusText.text = status
    }

    override fun updateStatus(id: Int) {
        updateStatus(context.getString(id))
    }

    override fun authenticate(neighborId: String, neighborName: String, token: String) {
        require(listener != null)
        AlertDialog.Builder(context)
            .setTitle(context.getString(R.string.mozac_feature_p2p_accept_connection, neighborName))
            .setMessage(context.getString(R.string.mozac_feature_p2p_accept_token, token))
            .setPositiveButton(android.R.string.yes) { _, _ -> listener?.onAccept(token) }
            .setNegativeButton(android.R.string.no) { _, _ -> listener?.onReject(token) }
            .setIcon(android.R.drawable.ic_dialog_alert)
            .show()
    }

    override fun readyToSend() {
        require(listener != null)
        showSendButtons(true)
        showConnectButtons(false)
    }

    override fun receiveUrl(neighborId: String, neighborName: String?, url: String) {
        AlertDialog.Builder(context)
            .setTitle(
                context.getString(
                    R.string.mozac_feature_p2p_open_url_title, neighborName
                        ?: neighborId
                )
            )
            .setMessage(url)
            .setPositiveButton(context.getString(R.string.mozac_feature_p2p_open_in_current_tab)) { _, _ ->
                listener?.onSetUrl(
                    url,
                    newTab = false
                )
            }
            .setNeutralButton(context.getString(R.string.mozac_feature_p2p_open_in_new_tab)) { _, _ ->
                listener?.onSetUrl(
                    url,
                    newTab = true
                )
            }
            .setNegativeButton(android.R.string.no) { _, _ -> }
            .setIcon(android.R.drawable.ic_dialog_alert)
            .show()
    }

    override fun receivePage(neighborId: String, neighborName: String?, page: String) {
        AlertDialog.Builder(context)
            .setTitle(
                context.getString(
                    R.string.mozac_feature_p2p_open_page_title, neighborName
                        ?: neighborId
                )
            )
            .setMessage(context.getString(R.string.mozac_feature_p2p_open_page_body))
            .setPositiveButton(context.getString(R.string.mozac_feature_p2p_open_in_current_tab)) { _, _ ->
                listener?.onLoadData(
                    context,
                    page,
                    false
                )
            }
            .setNeutralButton(context.getString(R.string.mozac_feature_p2p_open_in_new_tab)) { _, _ ->
                listener?.onLoadData(
                    context,
                    page,
                    true
                )
            }
            .setNegativeButton(android.R.string.no) { _, _ -> }
            .setIcon(android.R.drawable.ic_dialog_alert)
            .show()
    }

    private fun showToast(msg: String) {
        Toast.makeText(context, msg, Toast.LENGTH_LONG).show()
    }

    override fun failure(msg: String) {
        showToast(msg)
    }

    override fun reportSendComplete(resid: Int) {
        showToast(context.getString(resid))
    }

    override fun clear() {
        showConnectButtons(true)
        showSendButtons(false)
    }
}
