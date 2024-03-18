package org.mozilla.fenix.debugsettings

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.preferencesDataStore
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.flow.take
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.debugsettings.data.DefaultDebugSettingsRepository

private val Context.testDataStore: DataStore<Preferences> by preferencesDataStore(name = "DefaultDebugSettingsRepositoryTest")

@RunWith(AndroidJUnit4::class)
class DefaultDebugSettingsRepositoryTest {

    @Test
    fun `GIVEN the debug drawer is disabled WHEN the flag is enabled THEN the store should emit true`() = runTest {
        val dataStore = testContext.testDataStore
        val defaultDebugSettingsRepository = DefaultDebugSettingsRepository(
            context = testContext,
            dataStore = dataStore,
            writeScope = this,
        )
        val expected = listOf(false, false, true) // First emit is from initialization
        val expectedEmitCount = expected.size

        defaultDebugSettingsRepository.setDebugDrawerEnabled(false)

        defaultDebugSettingsRepository.setDebugDrawerEnabled(true)

        assertEquals(expected, defaultDebugSettingsRepository.debugDrawerEnabled.take(expectedEmitCount).toList())

        dataStore.edit { it.clear() }
    }

    @Test
    fun `GIVEN the debug drawer is enabled WHEN the flag is disabled THEN the store should emit false`() = runTest {
        val dataStore = testContext.testDataStore
        val defaultDebugSettingsRepository = DefaultDebugSettingsRepository(
            context = testContext,
            dataStore = dataStore,
            writeScope = this,
        )
        val expected = listOf(false, true, false) // First emit is from initialization
        val expectedEmitCount = expected.size

        defaultDebugSettingsRepository.setDebugDrawerEnabled(true)

        defaultDebugSettingsRepository.setDebugDrawerEnabled(false)

        assertEquals(expected, defaultDebugSettingsRepository.debugDrawerEnabled.take(expectedEmitCount).toList())

        dataStore.edit { it.clear() }
    }
}
