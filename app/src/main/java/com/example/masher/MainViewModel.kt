package com.example.masher


import android.app.Application
import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.Transformations
import androidx.lifecycle.viewModelScope
import androidx.preference.PreferenceManager
import java.util.Calendar
import java.util.Date
import kotlinx.coroutines.launch

/**
 * This is the view model of MainActivity, providing coroutine scopes.
 */
class MainViewModel(application: Application) : AndroidViewModel(application) {

    private val preferences = PreferenceManager.getDefaultSharedPreferences(application)



    fun stopMash(context: Context, cr: ContentResolver) {
        viewModelScope.launch {
            DataModel.storeMash()
            DataModel.backupMashes(context, cr)
        }
    }

    fun exportDataToFile(context: Context, cr: ContentResolver, uri: Uri, showToast: Boolean) {
        viewModelScope.launch {
            DataModel.exportDataToFile(context, cr, uri, showToast)
        }
    }



    fun importData(context: Context, cr: ContentResolver, uri: Uri) {
        viewModelScope.launch {
            DataModel.importData(context, cr, uri)
        }
    }

    fun insertSleep(sleep: Mash) {
        viewModelScope.launch {
            DataModel.insertMash(sleep)
        }
    }

    fun deleteSleep(sleep: Mash, context: Context, cr: ContentResolver) {
        viewModelScope.launch {
            DataModel.deleteSleep(sleep)
            DataModel.backupMashes(context, cr)
        }
    }

    fun deleteAllMashes() {
        viewModelScope.launch {
            DataModel.deleteAllMashes()
        }
    }
}
