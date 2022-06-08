package com.example.masher

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.databinding.DataBindingUtil
import com.example.masher.databinding.ActivityMainBinding
import android.content.SharedPreferences
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.SystemClock.sleep
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.json.JSONObject
import org.json.JSONTokener
import java.net.URL
import android.view.Menu
import android.view.MenuItem
import com.github.mikephil.charting.charts.BarChart;
import com.github.mikephil.charting.utils.ColorTemplate;
import android.widget.Toolbar;
import androidx.preference.Preference

import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceManager
import com.github.mikephil.charting.charts.LineChart
import com.github.mikephil.charting.components.AxisBase
import com.github.mikephil.charting.data.*
import com.github.mikephil.charting.formatter.IAxisValueFormatter
import com.github.mikephil.charting.formatter.IndexAxisValueFormatter
import com.github.mikephil.charting.utils.ViewPortHandler
import java.text.DateFormat
import java.text.SimpleDateFormat
import java.time.Instant
import java.util.*
import kotlin.collections.ArrayList

class MyCustomFormatter() : IndexAxisValueFormatter()
{
    override fun getFormattedValue(value: Float, axis: AxisBase?): String
    {
        val dateInMillis = value.toLong()
        val date = Calendar.getInstance().apply {
            timeInMillis = dateInMillis
        }.time

        return SimpleDateFormat("hh:mm", Locale.getDefault()).format(date)
    }
}

class MyCustomFormatter1 : IndexAxisValueFormatter() {

    override fun getFormattedValue(value: Float): String? {
        // Convert float value to date string
        // Convert from seconds back to milliseconds to format time  to show to the user
        val emissionsMilliSince1970Time = value.toLong() * 1000

        // Show time in local version
        val timeMilliseconds = Date(emissionsMilliSince1970Time)
        val dateTimeFormat = DateFormat.getDateInstance(DateFormat.MEDIUM, Locale.getDefault())
        return dateTimeFormat.format(timeMilliseconds)
    }
}

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding

    // SharedPreferences keeps listeners in a WeakHashMap, so keep this as a member.
    private val sharedPreferenceListener = SharedPreferencesChangeListener()

    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)

        val preferences = PreferenceManager.getDefaultSharedPreferences(applicationContext)
        preferences.registerOnSharedPreferenceChangeListener(sharedPreferenceListener)
        DataModel.init(applicationContext, preferences)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)



        val sharedPref = getPreferences(Context.MODE_PRIVATE)
        val min = sharedPref.getString("min", "55")
        val max = sharedPref.getString("max", "57")
         binding.editTextNumberMin.setText(min)
        binding.editTextNumberMax.setText(max)
        binding.run.setOnClickListener {
            val sharedPref = getPreferences(Context.MODE_PRIVATE)
            Log.d("masher", "run");

            binding.debug.setText("onClick");

            val min = binding?.editTextNumberMin?.getText().toString()
            val max = binding?.editTextNumberMax?.getText().toString()

            //GlobalScope.launch(/*Dispatchers.IO*/) {
            GlobalScope.launch(Dispatchers.IO){
                var count = 0
                val  chart = binding.chart
                chart?.xAxis?.valueFormatter = MyCustomFormatter()
                chart.getDescription().setEnabled(false);
                val preferences = PreferenceManager.getDefaultSharedPreferences(applicationContext)
                val sharedPref = getPreferences(Context.MODE_PRIVATE)
                val entries: ArrayList<Entry> = ArrayList()
                while (true) {
                    count++
                    val min = sharedPref.getString("min", "55")
                    val max = sharedPref.getString("max", "57")
                    val device = preferences.getString("edit_text_preference_ip", "192.168.68.90")
                    try {
                        var url = "http://".plus(device);

                        val result = URL(url).readText().toString()
                        try {
                            val jsonObject = JSONTokener(result).nextValue() as JSONObject
                            val low = String.format("%.1f", jsonObject.getString("low").toFloat())
                            val high = String.format("%.1f", jsonObject.getString("high").toFloat())
                            val c = String.format("%.1f", jsonObject.getString("c").toFloat())
                            this@MainActivity.runOnUiThread(java.lang.Runnable {
                                binding.textViewLow.setText(low.plus("c"))
                                binding.textViewC.setText(c.plus("c"))
                                binding.textViewHigh.setText(high.plus("c"))

                                val f1: Float =      jsonObject.getString("low").toFloat()
                                val f2: Float =      jsonObject.getString("high").toFloat()
                                //nstant.now()
                                if ((count % 10)==0) {
                                    entries.add(
                                        Entry(
                                            System.currentTimeMillis().toFloat() / 1000,
                                            f2
                                        )
                                    )
                                    val lineDataSet = LineDataSet(entries, "")

                                    val data = LineData(lineDataSet)
                                    chart.data = data

                                    chart.invalidate()
                                }
                            })

                        } catch (e: Exception) {
                            binding.debug.setText(e.toString())
                        }
                        // binding.textViewResult.setText("blah")
                    } catch (e: Exception) {
                        binding.debug.setText(e.toString())
                    }
                    sleep(1000)
                }
                binding.debug.setText("ooooooooooo");
            }
            val editor: SharedPreferences.Editor? = sharedPref?.edit()
            editor?.putString("min", min)
            editor?.putString("max", max)
            editor?.apply()
            editor?.commit()


        }


        //binding = DataBindingUtil.setContentView<ActivityMainBinding>(this@MainActivity, R.layout.activity_main)

    }
    override fun onCreateOptionsMenu( menu: Menu): Boolean{
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_settings -> {
                // navigate to settings screen
                startActivity( Intent(this,MySettingsActivity::class.java ))
                true
            }

            else -> super.onOptionsItemSelected(item)
        }
    }
}

