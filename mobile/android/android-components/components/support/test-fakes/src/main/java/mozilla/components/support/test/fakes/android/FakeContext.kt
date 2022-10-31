/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.fakes.android

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.IntentSender
import android.content.ServiceConnection
import android.content.SharedPreferences
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.content.res.AssetManager
import android.content.res.Configuration
import android.content.res.Resources
import android.database.DatabaseErrorHandler
import android.database.sqlite.SQLiteDatabase
import android.graphics.Bitmap
import android.graphics.drawable.Drawable
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.UserHandle
import android.view.Display
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream

/**
 * A [Context] implementation for when you just need a Context instance in a test.
 *
 * Using the fake is faster than launching all Robolectric bells and whistles.
 *
 * The current implementation just throws for most access.
 */
@Suppress("LargeClass", "TooManyFunctions")
open class FakeContext(
    private val sharedPreferences: SharedPreferences = FakeSharedPreferences(),
) : Context() {
    override fun getAssets(): AssetManager = throw NotImplementedError()
    override fun getResources(): Resources = throw NotImplementedError()
    override fun getPackageManager(): PackageManager = throw NotImplementedError()
    override fun getContentResolver(): ContentResolver = throw NotImplementedError()
    override fun getMainLooper(): Looper = throw NotImplementedError()
    override fun getApplicationContext(): Context = throw NotImplementedError()
    override fun setTheme(resid: Int) = throw NotImplementedError()
    override fun getTheme(): Resources.Theme = throw NotImplementedError()
    override fun getClassLoader(): ClassLoader = throw NotImplementedError()
    override fun getPackageName(): String = throw NotImplementedError()
    override fun getApplicationInfo(): ApplicationInfo = throw NotImplementedError()
    override fun getPackageResourcePath(): String = throw NotImplementedError()
    override fun getPackageCodePath(): String = throw NotImplementedError()
    override fun getSharedPreferences(name: String?, mode: Int): SharedPreferences {
        return sharedPreferences
    }
    override fun moveSharedPreferencesFrom(sourceContext: Context?, name: String?): Boolean =
        throw NotImplementedError()
    override fun deleteSharedPreferences(name: String?): Boolean = throw NotImplementedError()
    override fun openFileInput(name: String?): FileInputStream = throw NotImplementedError()
    override fun openFileOutput(name: String?, mode: Int): FileOutputStream = throw NotImplementedError()
    override fun deleteFile(name: String?): Boolean = throw NotImplementedError()
    override fun getFileStreamPath(name: String?): File = throw NotImplementedError()
    override fun getDataDir(): File = throw NotImplementedError()
    override fun getFilesDir(): File = throw NotImplementedError()
    override fun getNoBackupFilesDir(): File = throw NotImplementedError()
    override fun getExternalFilesDir(type: String?): File? = throw NotImplementedError()
    override fun getExternalFilesDirs(type: String?): Array<File> = throw NotImplementedError()
    override fun getObbDir(): File = throw NotImplementedError()
    override fun getObbDirs(): Array<File> = throw NotImplementedError()
    override fun getCacheDir(): File = throw NotImplementedError()
    override fun getCodeCacheDir(): File = throw NotImplementedError()
    override fun getExternalCacheDir(): File? = throw NotImplementedError()
    override fun getExternalCacheDirs(): Array<File> = throw NotImplementedError()
    override fun getExternalMediaDirs(): Array<File> = throw NotImplementedError()
    override fun fileList(): Array<String> = throw NotImplementedError()
    override fun getDir(name: String?, mode: Int): File = throw NotImplementedError()
    override fun openOrCreateDatabase(
        name: String?,
        mode: Int,
        factory: SQLiteDatabase.CursorFactory?,
    ): SQLiteDatabase = throw NotImplementedError()
    override fun openOrCreateDatabase(
        name: String?,
        mode: Int,
        factory: SQLiteDatabase.CursorFactory?,
        errorHandler: DatabaseErrorHandler?,
    ): SQLiteDatabase = throw NotImplementedError()
    override fun moveDatabaseFrom(sourceContext: Context?, name: String?): Boolean =
        throw NotImplementedError()
    override fun deleteDatabase(name: String?): Boolean = throw NotImplementedError()
    override fun getDatabasePath(name: String?): File = throw NotImplementedError()
    override fun databaseList(): Array<String> = throw NotImplementedError()
    override fun getWallpaper(): Drawable = throw NotImplementedError()
    override fun peekWallpaper(): Drawable = throw NotImplementedError()
    override fun getWallpaperDesiredMinimumWidth(): Int = throw NotImplementedError()
    override fun getWallpaperDesiredMinimumHeight(): Int = throw NotImplementedError()
    override fun setWallpaper(bitmap: Bitmap?) = throw NotImplementedError()
    override fun setWallpaper(data: InputStream?) = throw NotImplementedError()
    override fun clearWallpaper() = throw NotImplementedError()
    override fun startActivity(intent: Intent?) = throw NotImplementedError()
    override fun startActivity(intent: Intent?, options: Bundle?) = throw NotImplementedError()
    override fun startActivities(intents: Array<out Intent>?) = throw NotImplementedError()
    override fun startActivities(intents: Array<out Intent>?, options: Bundle?) =
        throw NotImplementedError()
    override fun startIntentSender(
        intent: IntentSender?,
        fillInIntent: Intent?,
        flagsMask: Int,
        flagsValues: Int,
        extraFlags: Int,
    ) = throw NotImplementedError()
    override fun startIntentSender(
        intent: IntentSender?,
        fillInIntent: Intent?,
        flagsMask: Int,
        flagsValues: Int,
        extraFlags: Int,
        options: Bundle?,
    ) = throw NotImplementedError()
    override fun sendBroadcast(intent: Intent?) = throw NotImplementedError()
    override fun sendBroadcast(intent: Intent?, receiverPermission: String?) = throw NotImplementedError()
    override fun sendOrderedBroadcast(intent: Intent?, receiverPermission: String?) =
        throw NotImplementedError()
    override fun sendOrderedBroadcast(
        intent: Intent,
        receiverPermission: String?,
        resultReceiver: BroadcastReceiver?,
        scheduler: Handler?,
        initialCode: Int,
        initialData: String?,
        initialExtras: Bundle?,
    ) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun sendBroadcastAsUser(intent: Intent?, user: UserHandle?) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun sendBroadcastAsUser(
        intent: Intent?,
        user: UserHandle?,
        receiverPermission: String?,
    ) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun sendOrderedBroadcastAsUser(
        intent: Intent?,
        user: UserHandle?,
        receiverPermission: String?,
        resultReceiver: BroadcastReceiver?,
        scheduler: Handler?,
        initialCode: Int,
        initialData: String?,
        initialExtras: Bundle?,
    ) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun sendStickyBroadcast(intent: Intent?) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun sendStickyOrderedBroadcast(
        intent: Intent?,
        resultReceiver: BroadcastReceiver?,
        scheduler: Handler?,
        initialCode: Int,
        initialData: String?,
        initialExtras: Bundle?,
    ) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun removeStickyBroadcast(intent: Intent?) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun sendStickyBroadcastAsUser(intent: Intent?, user: UserHandle?) =
        throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun sendStickyOrderedBroadcastAsUser(
        intent: Intent?,
        user: UserHandle?,
        resultReceiver: BroadcastReceiver?,
        scheduler: Handler?,
        initialCode: Int,
        initialData: String?,
        initialExtras: Bundle?,
    ) = throw NotImplementedError()

    @SuppressLint("MissingPermission")
    override fun removeStickyBroadcastAsUser(intent: Intent?, user: UserHandle?) =
        throw NotImplementedError()
    override fun registerReceiver(receiver: BroadcastReceiver?, filter: IntentFilter?): Intent? =
        throw NotImplementedError()
    override fun registerReceiver(
        receiver: BroadcastReceiver?,
        filter: IntentFilter?,
        flags: Int,
    ): Intent? = throw NotImplementedError()
    override fun registerReceiver(
        receiver: BroadcastReceiver?,
        filter: IntentFilter?,
        broadcastPermission: String?,
        scheduler: Handler?,
    ): Intent? = throw NotImplementedError()
    override fun registerReceiver(
        receiver: BroadcastReceiver?,
        filter: IntentFilter?,
        broadcastPermission: String?,
        scheduler: Handler?,
        flags: Int,
    ): Intent? = throw NotImplementedError()
    override fun unregisterReceiver(receiver: BroadcastReceiver?) = throw NotImplementedError()
    override fun startService(service: Intent?): ComponentName? = throw NotImplementedError()
    override fun startForegroundService(service: Intent?): ComponentName? = throw NotImplementedError()
    override fun stopService(service: Intent?): Boolean = throw NotImplementedError()
    override fun bindService(service: Intent?, conn: ServiceConnection, flags: Int): Boolean =
        throw NotImplementedError()
    override fun unbindService(conn: ServiceConnection) = throw NotImplementedError()
    override fun startInstrumentation(
        className: ComponentName,
        profileFile: String?,
        arguments: Bundle?,
    ): Boolean = throw NotImplementedError()
    override fun getSystemService(name: String): Any? = throw NotImplementedError()
    override fun getSystemServiceName(serviceClass: Class<*>): String? = throw NotImplementedError()
    override fun checkPermission(permission: String, pid: Int, uid: Int): Int = throw NotImplementedError()
    override fun checkCallingPermission(permission: String): Int = throw NotImplementedError()
    override fun checkCallingOrSelfPermission(permission: String): Int = throw NotImplementedError()
    override fun checkSelfPermission(permission: String): Int = throw NotImplementedError()
    override fun enforcePermission(permission: String, pid: Int, uid: Int, message: String?) =
        throw NotImplementedError()
    override fun enforceCallingPermission(permission: String, message: String?) = throw NotImplementedError()
    override fun enforceCallingOrSelfPermission(permission: String, message: String?) = throw NotImplementedError()
    override fun grantUriPermission(toPackage: String?, uri: Uri?, modeFlags: Int) = throw NotImplementedError()
    override fun revokeUriPermission(uri: Uri?, modeFlags: Int) = throw NotImplementedError()
    override fun revokeUriPermission(toPackage: String?, uri: Uri?, modeFlags: Int) = throw NotImplementedError()
    override fun checkUriPermission(uri: Uri?, pid: Int, uid: Int, modeFlags: Int): Int = throw NotImplementedError()
    override fun checkUriPermission(
        uri: Uri?,
        readPermission: String?,
        writePermission: String?,
        pid: Int,
        uid: Int,
        modeFlags: Int,
    ): Int = throw NotImplementedError()
    override fun checkCallingUriPermission(uri: Uri?, modeFlags: Int): Int = throw NotImplementedError()
    override fun checkCallingOrSelfUriPermission(uri: Uri?, modeFlags: Int): Int = throw NotImplementedError()
    override fun enforceUriPermission(
        uri: Uri?,
        pid: Int,
        uid: Int,
        modeFlags: Int,
        message: String?,
    ) = throw NotImplementedError()
    override fun enforceUriPermission(
        uri: Uri?,
        readPermission: String?,
        writePermission: String?,
        pid: Int,
        uid: Int,
        modeFlags: Int,
        message: String?,
    ) = throw NotImplementedError()
    override fun enforceCallingUriPermission(uri: Uri?, modeFlags: Int, message: String?) = throw NotImplementedError()
    override fun enforceCallingOrSelfUriPermission(uri: Uri?, modeFlags: Int, message: String?) =
        throw NotImplementedError()
    override fun createPackageContext(packageName: String?, flags: Int): Context = throw NotImplementedError()
    override fun createContextForSplit(splitName: String?): Context = throw NotImplementedError()
    override fun createConfigurationContext(overrideConfiguration: Configuration): Context = throw NotImplementedError()
    override fun createDisplayContext(display: Display): Context = throw NotImplementedError()
    override fun createDeviceProtectedStorageContext(): Context = throw NotImplementedError()
    override fun isDeviceProtectedStorage(): Boolean = throw NotImplementedError()
}
