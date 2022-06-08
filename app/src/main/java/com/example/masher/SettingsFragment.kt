package com.example.masher

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat

class SettingsFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.root_preferences, rootKey)
        val autoBackupPath = findPreference<Preference>("auto_backup_path")
        autoBackupPath?.let {
            val preferences = DataModel.preferences
            val path = preferences.getString("auto_backup_path", "")
            it.summary = path
        }
    }
}