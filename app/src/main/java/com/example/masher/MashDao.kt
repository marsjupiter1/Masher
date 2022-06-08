package com.example.masher


import androidx.lifecycle.LiveData
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Update

/**
 * Accesses the database of Mash objects.
 */
@Dao
interface MashDao {
    @Query("SELECT * FROM mash ORDER BY sid ASC")
    suspend fun getAll(): List<Mash>

    @Query("SELECT * FROM mash ORDER BY start_date DESC")
    fun getAllLive(): LiveData<List<Mash>>

    @Query("SELECT * from mash where sid = :id LIMIT 1")
    suspend fun getById(id: Int): Mash

    @Query("SELECT * FROM mash WHERE stop_date > :after ORDER BY start_date DESC")
    fun getAfterLive(after: Long): LiveData<List<Mash>>

    @Insert
    suspend fun insert(mashList: List<Mash>)

    @Insert
    suspend fun insert(mash: Mash)

    @Update
    suspend fun update(mash: Mash)

    @Delete
    suspend fun delete(mash: Mash)

    @Query("delete from mash")
    suspend fun deleteAll()
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */


