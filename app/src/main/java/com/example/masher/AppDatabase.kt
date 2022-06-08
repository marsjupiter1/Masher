package com.example.masher


import androidx.room.Database
import androidx.room.RoomDatabase

/**
 * Contains the database holder and serves as the main access point for the
 * stored data.
 */
@Database(entities = [Mash::class], version = 3, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun mashDao(): MashDao
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
