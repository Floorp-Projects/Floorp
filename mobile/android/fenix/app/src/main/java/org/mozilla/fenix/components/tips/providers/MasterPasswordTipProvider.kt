/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.tips.providers

import android.content.Context
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import io.sentry.Sentry
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.logins.IdCollisionException
import mozilla.appservices.logins.InvalidRecordException
import mozilla.appservices.logins.LoginsStorageException
import mozilla.appservices.logins.ServerPassword
import mozilla.components.concept.storage.Login
import mozilla.components.support.migration.FennecLoginsMPImporter
import mozilla.components.support.migration.FennecProfile
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.components.tips.TipProvider
import org.mozilla.fenix.components.tips.TipType
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.settings

/**
 * Tip explaining to master password users how to migrate their logins.
 */
class MasterPasswordTipProvider(
    private val context: Context,
    private val navigateToLogins: () -> Unit,
    private val dismissTip: (Tip) -> Unit
) : TipProvider {

    private val fennecLoginsMPImporter: FennecLoginsMPImporter? by lazy {
        FennecProfile.findDefault(
            context,
            context.components.analytics.crashReporter
        )?.let {
            FennecLoginsMPImporter(
                it
            )
        }
    }

    override val tip: Tip? by lazy { masterPasswordMigrationTip() }

    override val shouldDisplay: Boolean by lazy {
        context.settings().shouldDisplayMasterPasswordMigrationTip &&
                fennecLoginsMPImporter?.hasMasterPassword() == true
    }

    private fun masterPasswordMigrationTip(): Tip =
        Tip(
            type = TipType.Button(
                text = context.getString(R.string.mp_homescreen_button),
                action = ::showMasterPasswordMigration
            ),
            identifier = context.getString(R.string.pref_key_master_password_tip),
            title = context.getString(R.string.mp_homescreen_tip_title),
            description = context.getString(R.string.mp_homescreen_tip_message),
            learnMoreURL = null,
            titleDrawable = ContextCompat.getDrawable(context, R.drawable.ic_login)
        )

    private fun showMasterPasswordMigration() {
        val dialogView = LayoutInflater.from(context).inflate(R.layout.mp_migration_dialog, null)

        val dialogBuilder = AlertDialog.Builder(context).apply {
            setTitle(context.getString(R.string.mp_dialog_title_recovery_transfer_saved_logins))
            setMessage(context.getString(R.string.mp_dialog_message_recovery_transfer_saved_logins))
            setView(dialogView)
            create()
        }

        val dialog = dialogBuilder.show()

        context.metrics.track(Event.MasterPasswordMigrationDisplayed)

        val passwordErrorText = context.getString(R.string.mp_dialog_error_transfer_saved_logins)
        val migrationContinueButton =
            dialogView.findViewById<MaterialButton>(R.id.migration_continue)
        val passwordView = dialogView.findViewById<TextInputEditText>(R.id.password_field)
        val passwordLayout =
            dialogView.findViewById<TextInputLayout>(R.id.password_text_input_layout)
        passwordView.addTextChangedListener(
            object : TextWatcher {
                var isValid = false
                override fun afterTextChanged(p: Editable?) {
                    when {
                        p.toString().isEmpty() -> {
                            isValid = false
                            passwordLayout.error = passwordErrorText
                        }
                        else -> {
                            val possiblePassword = passwordView.text.toString()
                            isValid =
                                fennecLoginsMPImporter?.checkPassword(possiblePassword) == true
                            passwordLayout.error = if (isValid) null else passwordErrorText
                        }
                    }
                    migrationContinueButton.alpha = if (isValid) 1F else HALF_OPACITY
                    migrationContinueButton.isEnabled = isValid
                }

                override fun beforeTextChanged(
                    p: CharSequence?,
                    start: Int,
                    count: Int,
                    after: Int
                ) {
                    // NOOP
                }

                override fun onTextChanged(p: CharSequence?, start: Int, before: Int, count: Int) {
                    // NOOP
                }
            })

        migrationContinueButton.apply {
            setOnClickListener {
                // Step 1: Verify the password again before trying to use it
                val possiblePassword = passwordView.text.toString()
                val isValid = fennecLoginsMPImporter?.checkPassword(possiblePassword) == true

                // Step 2: With valid MP, get logins and complete the migration
                if (isValid) {
                    val logins = fennecLoginsMPImporter?.getLoginRecords(
                        possiblePassword,
                        context.components.analytics.crashReporter
                    )

                    if (logins.isNullOrEmpty()) {
                        showFailureDialog()
                        dialog.dismiss()
                    } else {
                        saveLogins(logins, dialog)
                    }
                } else {
                    passwordView.error =
                        context?.getString(R.string.mp_dialog_error_transfer_saved_logins)
                }
            }
        }

        dialogView.findViewById<MaterialButton>(R.id.migration_cancel).apply {
            setOnClickListener {
                dialog.dismiss()
            }
        }
    }

    private fun showFailureDialog() {
        val dialogView =
            LayoutInflater.from(context).inflate(R.layout.mp_migration_done_dialog, null)

        val dialogBuilder = AlertDialog.Builder(context).apply {
            setTitle(context.getString(R.string.mp_dialog_title_transfer_failure))
            setMessage(context.getString(R.string.mp_dialog_message_transfer_failure))
            setView(dialogView)
            create()
        }

        val dialog = dialogBuilder.show()

        dialogView.findViewById<MaterialButton>(R.id.positive_button).apply {
            text = context.getString(R.string.mp_dialog_close_transfer)
            setOnClickListener {
                tip?.let { dismissTip(it) }
                dialog.dismiss()
            }
        }
        dialogView.findViewById<MaterialButton>(R.id.negative_button).apply {
            isVisible = false
        }
    }

    private fun saveLogins(logins: List<ServerPassword>, dialog: AlertDialog) {
        CoroutineScope(IO).launch {
            logins.map { it.toLogin() }.forEach {
                try {
                    context.components.core.passwordsStorage.add(it)
                } catch (e: InvalidRecordException) {
                    // This record was invalid and we couldn't save this login
                    Sentry.capture("Master Password migration add login error $e for reason ${e.reason}")
                } catch (e: IdCollisionException) {
                    // Nonempty ID was provided
                    Sentry.capture("Master Password migration add login error $e")
                } catch (e: LoginsStorageException) {
                    // Some other error occurred
                    Sentry.capture("Master Password migration add login error $e")
                }
            }
            withContext(Dispatchers.Main) {
                // Step 3: Dismiss this dialog and show the success dialog
                showSuccessDialog()
                dialog.dismiss()
            }
        }
    }

    private fun showSuccessDialog() {
        tip?.let { dismissTip(it) }

        context.metrics.track(Event.MasterPasswordMigrationSuccess)

        val dialogView =
            LayoutInflater.from(context).inflate(R.layout.mp_migration_done_dialog, null)

        val dialogBuilder = AlertDialog.Builder(context).apply {
            setTitle(context.getString(R.string.mp_dialog_title_transfer_success))
            setMessage(context.getString(R.string.mp_dialog_message_transfer_success))
            setView(dialogView)
            create()
        }

        val dialog = dialogBuilder.show()

        dialogView.findViewById<MaterialButton>(R.id.positive_button).apply {
            setOnClickListener {
                navigateToLogins()
                dialog.dismiss()
            }
        }
        dialogView.findViewById<MaterialButton>(R.id.negative_button).apply {
            setOnClickListener {
                dialog.dismiss()
            }
        }
    }

    /**
     * Converts an Application Services [ServerPassword] to an Android Components [Login]
     */
    fun ServerPassword.toLogin() = Login(
        origin = hostname,
        formActionOrigin = formSubmitURL,
        httpRealm = httpRealm,
        username = username,
        password = password,
        timesUsed = timesUsed,
        timeCreated = timeCreated,
        timeLastUsed = timeLastUsed,
        timePasswordChanged = timePasswordChanged,
        usernameField = usernameField,
        passwordField = passwordField
    )

    companion object {
        private const val HALF_OPACITY = .5F
    }
}
