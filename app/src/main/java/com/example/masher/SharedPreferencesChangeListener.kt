package com.example.masher

import android.content.SharedPreferences
import android.util.Log
import androidx.appcompat.app.AppCompatDelegate

class SharedPreferencesChangeListener : SharedPreferences.OnSharedPreferenceChangeListener {
    companion object {
        private const val TAG = "SPChangeListener"
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        if (key == "auto_backup") {
            val autoBackup = sharedPreferences.getBoolean("auto_backup", false)
            val autoBackupPath = sharedPreferences.getString("auto_backup_path", "")
            if (autoBackup) {
                if (autoBackupPath == null || autoBackupPath.isEmpty()) {
                    val preferencesActivity = DataModel.preferencesActivity
                    if (preferencesActivity != null) {
                        Log.i(TAG, "onSharedPreferenceChanged: setting new backup path")
                        preferencesActivity.openFolderChooser()
                    }
                }
            } else {
                // Forget old path, so it's possible to set a different one later.
                Log.i(TAG, "onSharedPreferenceChanged: clearing old backup path")
                val editor = DataModel.preferences.edit()
                editor.remove("auto_backup_path")
                editor.apply()
            }
            return
        }
        device = sharedPreferences.getString("edit_text_preference_ip", "").toString()
        heating_id = sharedPreferences.getString("edit_text_preference_heating_id", "").toString()
        cooling_id = sharedPreferences.getString("edit_text_preference_cooling_id", "").toString()
        api_key = sharedPreferences.getString("edit_text_preference_api_key", "").toString()
        client_id = sharedPreferences.getString("edit_text_preference_client_id", "").toString()
        tuya_host = sharedPreferences.getString("edit_text_preference_host", "").toString()
        applySettings()

        applyTheme(sharedPreferences)
    }

    fun applyTheme(sharedPreferences: SharedPreferences) {
        val themeFollowSystem = sharedPreferences.getBoolean("follow_system_theme", true)
        val darkTheme = sharedPreferences.getBoolean("dark_mode", false)
        when {
            themeFollowSystem -> {
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
            }
            darkTheme -> {
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES)
            }
            else -> {
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO)
            }
        }
    }
}

