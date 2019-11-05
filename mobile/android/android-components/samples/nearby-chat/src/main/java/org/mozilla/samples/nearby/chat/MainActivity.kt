/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.nearby.chat

import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import kotlinx.android.synthetic.main.activity_main.*
import mozilla.components.lib.nearby.NearbyConnection
import mozilla.components.lib.nearby.NearbyConnection.ConnectionState
import mozilla.components.lib.nearby.NearbyConnectionObserver
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.log.sink.AndroidLogSink

class MainActivity : AppCompatActivity() {
    // initialized in init() after permissions granted
    private lateinit var connection: NearbyConnection

    // updated in listener
    private lateinit var state: ConnectionState

    // Lifecycle methods

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        Log.addSink(AndroidLogSink())

        // This button must not be enabled until permissions have been granted.
        resetButton.setOnClickListener {
            connection.disconnect()
            init()
        }

        // These two buttons must not be enabled until connection is initialized in init().
        advertiseButton.setOnClickListener {
            disableConnectionButtons()
            connection.startAdvertising()
        }
        discoverButton.setOnClickListener {
            disableConnectionButtons()
            connection.startDiscovering()
        }

        // This button must not be enabled unless the current state is READY_TO_SEND.
        sendMessageButton.setOnClickListener {
            if (state !is ConnectionState.ReadyToSend) {
                Logger.error("Attempt to send message when in state ${state.name}")
            } else {
                connection.sendMessage(outgoingMessageText.text.toString())
                outgoingMessageText.text?.clear()
            }
        }
    }

    override fun onStart() {
        super.onStart()
        requestPermissions()
    }

    override fun onStop() {
        super.onStop()
        // Unless permissions were granted and init() ran to completion, connection may be uninitialized.
        if (this::connection.isInitialized) {
            connection.disconnect()
        }
    }

    // Permissions

    private fun requestPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val neededPermissions = NearbyConnection.PERMISSIONS.filter {
                ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
            }

            if (neededPermissions.isNotEmpty()) {
                ActivityCompat.requestPermissions(this, neededPermissions.toTypedArray(), 0)
            } else {
                init()
            }
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (grantResults.any { it == PackageManager.PERMISSION_DENIED }) {
            statusText.text = getString(R.string.mozac_samples_nearby_chat_permissions_denied)
            init()
        }
    }

    // Called after permissions first granted or reset button pressed
    private fun init() {
        // Can't do in onCreate because context not ready.
        connection = NearbyConnection(this)
        connection.register(ObserverForChat(), this)
        enableConnectionButtons()
        resetButton.isEnabled = true
    }

    private fun disableConnectionButtons() {
        advertiseButton.isEnabled = false
        discoverButton.isEnabled = false
    }

    private fun enableConnectionButtons() {
        advertiseButton.isEnabled = true
        discoverButton.isEnabled = true
    }

    private inner class ObserverForChat : NearbyConnectionObserver {
        override fun onStateUpdated(connectionState: ConnectionState) {
            state = connectionState
            statusText.text =
                if (statusText.text.isEmpty()) state.name else "${statusText.text} -> ${state.name}"
            textInputLayout.visibility =
                if (state is ConnectionState.ReadyToSend) View.VISIBLE else View.INVISIBLE
            if (state is ConnectionState.Authenticating) {
                val auth = state as ConnectionState.Authenticating
                AlertDialog.Builder(this@MainActivity)
                    .setTitle(
                        getString(R.string.mozac_samples_nearby_chat_accept_connection,
                            auth.neighborName))
                    .setMessage(getString(R.string.mozac_samples_nearby_chat_confirm_token,
                        auth.token))
                    .setPositiveButton(android.R.string.yes) { _, _ -> auth.accept() }
                    .setNegativeButton(android.R.string.no) { _, _ -> auth.reject() }
                    .setIcon(android.R.drawable.ic_dialog_alert)
                    .show()
            }
        }

        override fun onMessageReceived(neighborId: String, neighborName: String?, message: String) {
            incomingMessageText.text = getString(
                R.string.mozac_samples_nearby_chat_message_received,
                neighborName ?: neighborId,
                message)
        }

        override fun onMessageDelivered(payloadId: Long) {
            Toast.makeText(
                this@MainActivity,
                getString(R.string.mozac_samples_nearby_chat_message_delivered),
                Toast.LENGTH_LONG
            ).show()
        }
    }
}
