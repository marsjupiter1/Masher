package com.example.masher

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.graphics.Color
import android.os.Bundle
import android.os.SystemClock.sleep
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import com.example.masher.databinding.ActivityMainBinding
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.components.YAxis
import com.github.mikephil.charting.data.Entry
import com.github.mikephil.charting.data.LineData
import com.github.mikephil.charting.data.LineDataSet
import com.github.mikephil.charting.formatter.IndexAxisValueFormatter
import com.github.mikephil.charting.interfaces.datasets.ILineDataSet
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.json.JSONObject
import org.json.JSONTokener
import java.net.URL
import java.text.SimpleDateFormat
import java.util.*


val time_offset: Long = 1654717191;

class MyCustomFormatter() : IndexAxisValueFormatter() {
    override fun getFormattedValue(value: Float): String {
        val dateInMillis = (value.toLong() + time_offset) * 1000;
        val date = Calendar.getInstance().apply {
            timeInMillis = dateInMillis
        }.time

        return SimpleDateFormat("hh:mm:ss", Locale.getDefault()).format(date)
    }
}

val set: ArrayList<ILineDataSet> = ArrayList()
val low_entries: ArrayList<Entry> = ArrayList()
val high_entries: ArrayList<Entry> = ArrayList()
var low = 0f
var high = 0f
var device = ""
var min = 0f
var max = 0f
var init = false
var newDevice = true
var deviceChanged = false
var cooling_id = ""
var heating_id = ""
var api_key = ""
var tuya_host = ""
var client_id = ""
var status = "unset"

class MainActivity : AppCompatActivity() {
    lateinit var binding: ActivityMainBinding

    // SharedPreferences keeps listeners in a WeakHashMap, so keep this as a member.
    private val sharedPreferenceListener = SharedPreferencesChangeListener()

    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)


        val preferences = PreferenceManager.getDefaultSharedPreferences(applicationContext)
        preferences.registerOnSharedPreferenceChangeListener(sharedPreferenceListener)
        device = preferences.getString("edit_text_preference_ip", "")!!.toString()
        tuya_host = preferences.getString("edit_text_preference_host", "")!!.toString()
        heating_id = preferences.getString("edit_text_preference_heating_id", "")!!.toString()
        cooling_id = preferences.getString("edit_text_preference_cooling_id", "")!!.toString()
        api_key = preferences.getString("edit_text_preference_api_key", "")!!.toString()
        client_id = preferences.getString("edit_text_preference_client_id", "")!!.toString()
        DataModel.init(applicationContext, preferences)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)


        val sharedPref = getPreferences(Context.MODE_PRIVATE)
        min = sharedPref.getString("min", "55")!!.toFloat()
        max = sharedPref.getString("max", "57")!!.toFloat()
        binding.editTextNumberMin.setText(min.toString())
        binding.editTextNumberMax.setText(max.toString())
        binding.run.setOnClickListener {


            min = binding.editTextNumberMin.getText().toString().toFloat()
            max = binding.editTextNumberMax.getText().toString().toFloat()
            binding.debug.setText("onClick");


            val editor: SharedPreferences.Editor? = sharedPref?.edit()
            editor?.putString("min", min.toString())
            editor?.putString("max", max.toString())
            editor?.apply()
            editor?.commit()
            applySettings()


        }

        //GlobalScope.launch(/*Dispatchers.IO*/) {
        GlobalScope.launch(Dispatchers.IO) {
            var count = 0
            val chart = binding.chart
            val xAxis: XAxis = chart.getXAxis()
            val yAxis: YAxis = chart.getAxisLeft()
            yAxis.setTextSize(18f)
            xAxis.position = XAxis.XAxisPosition.BOTTOM

            xAxis.valueFormatter = MyCustomFormatter()
            xAxis.setDrawLabels(true)
            xAxis.setLabelRotationAngle((-45.0).toFloat());
            xAxis.setTextSize(18f);
            chart.getDescription().setEnabled(false);


            chart.legend.isEnabled = false
            chart.axisRight.isEnabled = false

            while (true) {
                count++
                if (deviceChanged) {
                    with(preferences.edit()) {
                        putString("edit_text_preference_heating_id", heating_id)
                        putString("edit_text_preference_cooling_id", cooling_id)
                        putString("edit_text_preference_client_id", client_id)
                        putString("edit_text_preference_api_key", api_key)
                        putString("edit_text_preference_host", tuya_host)

                        apply()
                    }
                    deviceChanged = false;

                }
                try {

                    val c = String.format("%.1f", (low + high) / 2)
                    this@MainActivity.runOnUiThread(java.lang.Runnable {
                        binding.textViewLow.setText(String.format("%.1f", low).plus("c"))
                        binding.textViewC.setText(c.plus("c"))
                        binding.textViewHigh.setText(String.format("%.1f", high).plus("c"))
                        binding.debug.setText(status)
                        if ((count % 10) == 1) {

                            val lineDataSetLow = LineDataSet(low_entries, "")
                            lineDataSetLow.setColor(Color.GREEN)
                            lineDataSetLow.setDrawValues(false);
                            lineDataSetLow.setDrawCircles(false);
                            set.clear()
                            set.add(lineDataSetLow)
                            val lineDataSetHigh = LineDataSet(high_entries, "")
                            lineDataSetHigh.setColor(Color.RED)
                            lineDataSetHigh.setDrawValues(false);
                            lineDataSetHigh.setDrawCircles(false);
                            set.add(lineDataSetHigh)
                            val data = LineData(set)

                            chart.setData(data)

                            chart.invalidate()
                        }
                    })
                } catch (e: Exception) {
                    binding.debug.setText(e.toString())
                }
                sleep(1000)
            }

        }


        //binding = DataBindingUtil.setContentView<ActivityMainBinding>(this@MainActivity, R.layout.activity_main)

    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_settings -> {
                // navigate to settings screen
                startActivity(Intent(this, MySettingsActivity::class.java))
                true
            }

            else -> super.onOptionsItemSelected(item)
        }
    }


}


fun applySettings() {
    val thread = Thread {
        try {
            try {
                var url = "http://".plus(device).plus("/setting?");

                url = url.plus("min=").plus(min.toString())
                url = url.plus("&max=").plus(max.toString())
                url = url.plus("&tuya_host=").plus(tuya_host)
                url = url.plus("&tuya_api_key=").plus(api_key)
                url = url.plus("&tuya_api_client=").plus(client_id)
                url = url.plus("&tuya_deviceh=").plus(heating_id)
                url = url.plus("&tuya_devicec=").plus(cooling_id)
                URL(url).readText().toString()
                status = "applied settings ".plus(url)
            } catch (e: Exception) {
                status = e.toString()
            }
        } catch (e: java.lang.Exception) {
            status = e.toString()
            e.printStackTrace()
        }
    }

    thread.start()

}

// A global coroutine to log statistics every second, must be always active
@OptIn(DelicateCoroutinesApi::class)
val globalScopeReporter = GlobalScope.launch {

    var count = 0

    while (true) {
        count++

        try {
            var url = "http://".plus(device);

            val result = URL(url).readText().toString()

            val jsonObject = JSONTokener(result).nextValue() as JSONObject
            low = jsonObject.getString("low").toFloat()
            high = jsonObject.getString("high").toFloat()
            val c = String.format("%.1f", jsonObject.getString("c").toFloat())

            if ((count % 10) == 1) {

                low_entries.add(
                    Entry(
                        (System.currentTimeMillis() / 1000 - time_offset).toFloat(),
                        low
                    )
                )
                val lineDataSetLow = LineDataSet(low_entries, "")
                lineDataSetLow.setColor(Color.GREEN)
                lineDataSetLow.setDrawValues(false);
                lineDataSetLow.setDrawCircles(false);
                set.clear()
                set.add(lineDataSetLow)

                high_entries.add(
                    Entry(
                        (System.currentTimeMillis() / 1000 - time_offset).toFloat(),
                        high
                    )
                )
                val lineDataSetHigh = LineDataSet(high_entries, "")
                lineDataSetHigh.setColor(Color.RED)
                lineDataSetHigh.setDrawValues(false);
                lineDataSetHigh.setDrawCircles(false);
                set.add(lineDataSetHigh)

            }
            status= count.toString()

            // binding.textViewResult.setText("blah")
        } catch (e: Exception) {
            status = e.toString()

        }
        sleep(1000)
    }
}

@OptIn(DelicateCoroutinesApi::class)
val globalScopeInit = GlobalScope.launch {

    var count = 0

    while (true) {
        if (newDevice) {
            try {

                var change = false
                var url = "http://".plus(device).plus("/get");

                val result = URL(url).readText().toString()

                val jsonObject = JSONTokener(result).nextValue() as JSONObject
                val tuya_deviceh = jsonObject.getString("tuya_deviceh")
                if (tuya_deviceh != heating_id) {
                    heating_id = tuya_deviceh;
                    change = true;
                }
                val tuya_devicec = jsonObject.getString("tuya_devicec")
                if (tuya_devicec != cooling_id) {
                    cooling_id = tuya_devicec;
                    change = true;
                }
                val new_tuya_host = jsonObject.getString("tuya_host")
                if (new_tuya_host != tuya_host) {
                    tuya_host = new_tuya_host;
                    change = true;
                }

                val tuya_api_client = jsonObject.getString("tuya_api_client")
                if (tuya_api_client != client_id) {
                    client_id = tuya_api_client;
                    change = true;
                }
                val tuya_api_key = jsonObject.getString("tuya_api_key")
                if (tuya_api_key != api_key) {
                    api_key = tuya_api_key;
                    change = true;
                }
                if (change) {
                    deviceChanged = true
                }
                newDevice = false;
            } catch (e: Exception) {
                status = e.toString()
            }
        }
        sleep(1000)
    }
}