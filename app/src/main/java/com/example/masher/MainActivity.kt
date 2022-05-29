package com.example.masher

import android.annotation.SuppressLint
import android.content.Context
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.databinding.DataBindingUtil
import com.example.masher.databinding.ActivityMainBinding
import android.content.SharedPreferences
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.json.JSONObject
import org.json.JSONTokener
import java.net.URL

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)
        //setContentView(R.layout.activity_main)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.debug.text = "hello workd"
        val sharedPref =  getPreferences(Context.MODE_PRIVATE)
        val min =sharedPref.getString("min","55")
        val max =  sharedPref.getString("max","57")
        val device = sharedPref.getString("ip","192.168.68.80")
        binding.editTextNumberMin.setText(min)
        binding.editTextNumberMax.setText( max)
        binding.editTextUri.setText(device)
        binding.debug.setText("setup");

        binding.run.setOnClickListener {
            val sharedPref = getPreferences(Context.MODE_PRIVATE)
            Log.d("masher","run");

            binding.debug.setText("onClick");

            val min = binding?.editTextNumberMin?.getText().toString()
            val max = binding?.editTextNumberMax?.getText().toString()
            val device = binding?.editTextUri?.getText().toString()


            //GlobalScope.launch(/*Dispatchers.IO*/) {
            GlobalScope.launch(Dispatchers.IO) {
                try {
                    var url = "http://".plus( device);
                    binding.textViewResult.setText(url);
                    val result = URL(url).readText().toString()
                    try{
                        val jsonObject = JSONTokener(result).nextValue() as JSONObject
                        val low = String.format("%.1f",jsonObject.getString("low").toFloat())
                        val high = String.format("%.1f",jsonObject.getString("high").toFloat())
                        val c = String.format("%.1f",jsonObject.getString("c").toFloat())
                        this@MainActivity.runOnUiThread(java.lang.Runnable {
                            binding.textViewLow.setText(low.plus("c"))
                            binding.textViewC.setText(c.plus("c"))
                            binding.textViewHigh.setText(high.plus("c"))
                        })

                    }catch(e: Exception){
                        binding.debug.setText(e.toString())
                    }
                   // binding.textViewResult.setText("blah")
                } catch (e: Exception) {
                    binding.debug.setText(e.toString())
                }
            }
            val editor:SharedPreferences.Editor? =  sharedPref?.edit()
            editor?.putString("ip", device)
            editor?.putString("min",min)
            editor?.putString("max",max)
            editor?.apply()
            editor?.commit()


        }




        //binding = DataBindingUtil.setContentView<ActivityMainBinding>(this@MainActivity, R.layout.activity_main)

    }
}