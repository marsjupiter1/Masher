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
import com.github.mikephil.charting.components.AxisBase
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.components.YAxis
import com.github.mikephil.charting.data.*
import com.github.mikephil.charting.formatter.IndexAxisValueFormatter
import com.github.mikephil.charting.interfaces.datasets.ILineDataSet
import kotlinx.coroutines.*
import org.json.JSONObject
import org.json.JSONTokener
import java.net.URL
import java.text.DateFormat
import java.text.SimpleDateFormat
import java.util.*


val  time_offset:Long = 1654717191;

class MyCustomFormatter() : IndexAxisValueFormatter()
{
    override fun getFormattedValue(value: Float): String
    {
        val dateInMillis = (value.toLong()+time_offset)*1000;
        val date = Calendar.getInstance().apply {
            timeInMillis = dateInMillis
        }.time

        return SimpleDateFormat("hh:mm:ss", Locale.getDefault()).format(date)
    }
}

val set: ArrayList<ILineDataSet> = ArrayList()
val low_entries:ArrayList<Entry> = ArrayList()
val high_entries:ArrayList<Entry> = ArrayList()
var low=0f
var high=0f
var device=""

class MainActivity : AppCompatActivity() {
    lateinit var binding: ActivityMainBinding

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
                val preferences = PreferenceManager.getDefaultSharedPreferences(applicationContext)
                while (true) {
                    count++

                    device = preferences.getString("edit_text_preference_ip", "192.168.68.90").toString()
                    try {
                        var url = "http://".plus(device);


                        val c = String.format("%.1f", (low+high)/2)
                        this@MainActivity.runOnUiThread(java.lang.Runnable {
                            binding.textViewLow.setText(String.format("%.1f", low).plus("c"))
                            binding.textViewC.setText(c.plus("c"))
                            binding.textViewHigh.setText(String.format("%.1f", high).plus("c"))

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
                        }   )
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

    fun GetBinding() :ActivityMainBinding{
        return this.binding
    }
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

                        if ((count % 10)==1) {

                            low_entries.add(
                                Entry(
                                    (System.currentTimeMillis()/1000 -time_offset).toFloat() ,
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
                                    (System.currentTimeMillis()/1000 -time_offset).toFloat(),
                                    high
                                )
                            )
                            val lineDataSetHigh = LineDataSet(high_entries, "")
                            lineDataSetHigh.setColor(Color.RED)
                            lineDataSetHigh.setDrawValues(false);
                            lineDataSetHigh.setDrawCircles(false);
                            set.add(lineDataSetHigh)

                        }



                // binding.textViewResult.setText("blah")
            } catch (e: Exception) {

            sleep(1000)
        }
    }
}
