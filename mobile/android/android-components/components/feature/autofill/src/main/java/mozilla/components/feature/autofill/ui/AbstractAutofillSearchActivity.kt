/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.ui

import android.app.assist.AssistStructure
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.autofill.AutofillManager
import android.widget.EditText
import android.widget.inline.InlinePresentationSpec
import androidx.annotation.RequiresApi
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.Login
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.facts.emitAutofillSearchDisplayedFact
import mozilla.components.feature.autofill.facts.emitAutofillSearchSelectedFact
import mozilla.components.feature.autofill.response.dataset.LoginDatasetBuilder
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.feature.autofill.structure.parseStructure
import mozilla.components.feature.autofill.structure.toRawStructure
import mozilla.components.feature.autofill.ui.search.LoginsAdapter
import mozilla.components.support.ktx.android.view.showKeyboard

/**
 * Activity responsible for letting the user manually search and pick credentials for auto-filling a
 * third-party app.
 */
@RequiresApi(Build.VERSION_CODES.O)
abstract class AbstractAutofillSearchActivity : FragmentActivity() {
    abstract val configuration: AutofillConfiguration

    private lateinit var parsedStructure: ParsedStructure
    private lateinit var loginsDeferred: Deferred<List<Login>>
    private val scope = CoroutineScope(Dispatchers.IO)
    private val adapter = LoginsAdapter(::onLoginSelected)
    private var imeSpec: InlinePresentationSpec? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        if (savedInstanceState == null) {
            emitAutofillSearchDisplayedFact()
        }

        val structure: AssistStructure? =
            intent.getParcelableExtra(AutofillManager.EXTRA_ASSIST_STRUCTURE)
        if (structure == null) {
            finish()
            return
        }
        imeSpec = intent.getImeSpec()

        val parsedStructure = parseStructure(this, structure.toRawStructure())
        if (parsedStructure == null) {
            finish()
            return
        }

        this.parsedStructure = parsedStructure
        this.loginsDeferred = loadAsync()

        setContentView(R.layout.mozac_feature_autofill_search)

        val recyclerView = findViewById<RecyclerView>(R.id.mozac_feature_autofill_list)
        recyclerView.layoutManager = LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false)
        recyclerView.adapter = adapter
        recyclerView.addItemDecoration(DividerItemDecoration(this, DividerItemDecoration.HORIZONTAL))

        val searchView = findViewById<EditText>(R.id.mozac_feature_autofill_search)
        searchView.doOnTextChanged { text, _, _, _ ->
            if (text != null && text.isNotEmpty()) {
                performSearch(text)
            } else {
                clearResults()
            }
        }

        searchView.showKeyboard()
    }

    private fun onLoginSelected(login: Login) {
        val builder = LoginDatasetBuilder(parsedStructure, login, needsConfirmation = false)
        val dataset = builder.build(this, configuration, imeSpec)

        val replyIntent = Intent()
        replyIntent.putExtra(AutofillManager.EXTRA_AUTHENTICATION_RESULT, dataset)

        emitAutofillSearchSelectedFact()

        setResult(RESULT_OK, replyIntent)
        finish()
    }

    override fun onDestroy() {
        super.onDestroy()
        scope.cancel()
    }

    private fun performSearch(text: CharSequence) = scope.launch {
        val logins = loginsDeferred.await()

        val filteredLogins = logins.filter { login ->
            login.username.contains(text) ||
                login.origin.contains(text)
        }

        withContext(Dispatchers.Main) {
            adapter.update(filteredLogins)
        }
    }

    private fun clearResults() {
        adapter.clear()
    }

    private fun loadAsync(): Deferred<List<Login>> {
        return scope.async {
            configuration.storage.list()
        }
    }
}
