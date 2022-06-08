package com.example.masher


import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.util.Log
import android.widget.Toast
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.LiveData
import androidx.room.Room
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import java.io.IOException
import java.io.InputStreamReader
import java.io.OutputStream
import java.io.OutputStreamWriter
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import org.apache.commons.csv.CSVFormat
import org.apache.commons.csv.CSVPrinter
import org.apache.commons.csv.CSVRecord

val MIGRATION_1_2 = object : Migration(1, 2) {
    override fun migrate(database: SupportSQLiteDatabase) {
        database.execSQL("ALTER TABLE sleep ADD COLUMN rating INTEGER NOT NULL DEFAULT 0")
    }
}

val MIGRATION_2_3 = object : Migration(2, 3) {
    override fun migrate(database: SupportSQLiteDatabase) {
        database.execSQL("ALTER TABLE sleep ADD COLUMN comment TEXT NOT NULL DEFAULT ''")
    }
}

/**
 * Data model is the singleton shared state between the activity and the
 * service.
 */
object DataModel {

    lateinit var preferences: SharedPreferences

    var preferencesActivity: MySettingsActivity? = null

    var start: Date? = null
        set(start) {
            field = start
            // Save start timestamp in case the foreground service is killed.
            val editor = preferences.edit()
            field?.let {
                editor.putLong("start", it.time)
            }
            editor.apply()
        }

    var stop: Date? = null

    lateinit var database: AppDatabase

    val sleepsLive: LiveData<List<Mash>>
        get() = database.mashDao().getAllLive()

    private var initialized: Boolean = false

    fun init(context: Context, preferences: SharedPreferences) {
        if (initialized) {
            return
        }

        this.preferences = preferences

        val start = preferences.getLong("start", 0)
        if (start > 0) {
            // Restore start timestamp in case the foreground service was
            // killed.
            this.start = Date(start)
        }
        database = Room.databaseBuilder(context, AppDatabase::class.java, "database")
 //           .addMigrations(MIGRATION_1_2)
            .build()
        initialized = true
    }

    suspend fun storeMash() {
        val mash = Mash()
        start?.let {
            mash.start = it.time
        }
        stop?.let {
            mash.stop = it.time
        }
        database.mashDao().insert(mash)

        // Drop start timestamp from preferences, it's in the database now.
        val editor = preferences.edit()
        editor.remove("start")
        editor.apply()
    }

    suspend fun insertMash(mash: Mash) {
        database.mashDao().insert(mash)
    }

    private suspend fun insertMashes(mashList: List<Mash>) {
        database.mashDao().insert(mashList)
    }

    suspend fun updateSleep(sleep: Mash) {
        database.mashDao().update(sleep)
    }

    suspend fun deleteSleep(sleep: Mash) {
        database.mashDao().delete(sleep)
    }

    suspend fun deleteAllMashes() {
        database.mashDao().deleteAll()
    }

    suspend fun getMashById(sid: Int): Mash {
        return database.mashDao().getById(sid)
    }



    suspend fun importData(context: Context, cr: ContentResolver, uri: Uri) {
        val inputStream = cr.openInputStream(uri)
        val records: Iterable<CSVRecord> = CSVFormat.DEFAULT.parse(InputStreamReader(inputStream))
        // We have a speed vs memory usage trade-off here. Pay the cost of keeping all sleeps in
        // memory: the benefit is that inserting all of them once triggers a single notification of
        // observers. This means that importing 100s of sleeps is still ~instant, while it used to
        // take ~forever.
        val importedMashes= mutableListOf<Mash>()
        try {
            var first = true
            for (cells in records) {
                if (first) {
                    // Ignore the header.
                    first = false
                    continue
                }
                val mash = Mash()
                mash.start = cells[1].toLong()
                mash.stop = cells[2].toLong()
                if (cells.isSet(3)) {
                    mash.rating = cells[3].toLong()
                }
                if (cells.isSet(4)) {
                    mash.comment = cells[4]
                }
                importedMashes.add(mash)
            }
            val oldSleeps = database.mashDao().getAll()
            val newSleeps = importedMashes.subtract(oldSleeps.toSet())
            database.mashDao().insert(newSleeps.toList())
        } catch (e: IOException) {
            Log.e(TAG, "importData: readLine() failed")
            return
        } finally {
            if (inputStream != null) {
                try {
                    inputStream.close()
                } catch (e: Exception) {
                }
            }
        }

        val text = context.getString(R.string.import_success)
        val duration = Toast.LENGTH_SHORT
        val toast = Toast.makeText(context, text, duration)
        toast.show()
    }


    suspend fun backupMashes(context: Context, cr: ContentResolver) {
        val autoBackup = preferences.getBoolean("auto_backup", false)
        val autoBackupPath = preferences.getString("auto_backup_path", "")
        if (!autoBackup || autoBackupPath == null || autoBackupPath.isEmpty()) {
            return
        }

        val folder = DocumentFile.fromTreeUri(context, Uri.parse(autoBackupPath))
            ?: return

        // Make sure that we don't create "backup (1).csv", etc.
        val oldBackup = folder.findFile("backup.csv")
        if (oldBackup != null && oldBackup.exists()) {
            oldBackup.delete()
        }

        val backup = folder.createFile("text/csv", "backup.csv") ?: return
        exportDataToFile(
            context, cr, backup.uri, showToast = false
        )
    }

    suspend fun exportDataToFile(
        context: Context,
        cr: ContentResolver,
        uri: Uri,
        showToast: Boolean
    ) {
        val sleeps = database.mashDao().getAll()

        try {
            cr.takePersistableUriPermission(
                uri, Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            )
        } catch (e: SecurityException) {
            Log.e(TAG, "exportData: takePersistableUriPermission() failed for write")
        }

        val os: OutputStream? = cr.openOutputStream(uri)
        if (os == null) {
            Log.e(TAG, "exportData: openOutputStream() failed")
            return
        }
        try {
            val writer = CSVPrinter(OutputStreamWriter(os, "UTF-8"), CSVFormat.DEFAULT)
            writer.printRecord("sid", "start", "stop", "rating", "comment")
            for (sleep in sleeps) {
                writer.printRecord(sleep.sid, sleep.start, sleep.stop, sleep.rating, sleep.comment)
            }
            writer.close()
        } catch (e: IOException) {
            Log.e(TAG, "exportData: write() failed")
            return
        } finally {
            try {
                os.close()
            } catch (e: Exception) {
            }
        }

        if (!showToast) {
            return
        }
        val text = context.getString(R.string.export_success)
        val duration = Toast.LENGTH_SHORT
        val toast = Toast.makeText(context, text, duration)
        toast.show()
    }

    private const val TAG = "DataModel"

    fun getSleepCountStat(sleeps: List<Mash>): String {
        return sleeps.size.toString()
    }

    /**
     * Calculates the avg of sleeps.
     */
    fun getSleepDurationStat(sleeps: List<Mash>): String {
        var sum: Long = 0
        for (sleep in sleeps) {
            var diff = sleep.stop - sleep.start
            diff /= 1000
            sum += diff
        }
        val count = sleeps.size
        return if (count == 0) {
            ""
        } else formatDuration(sum / count)
    }


    fun formatDuration(seconds: Long): String {
        return String.format(
            Locale.getDefault(), "%d:%02d:%02d",
            seconds / 3600, seconds % 3600 / 60,
            seconds % 60
        )
    }

    fun formatTimestamp(date: Date): String {
        val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm:ss XXX", Locale.getDefault())
        return sdf.format(date)
    }

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */