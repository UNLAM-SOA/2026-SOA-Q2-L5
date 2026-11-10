package com.nexoriego.app;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public class SensorActivity extends AppCompatActivity implements SensorEventListener {

    private SensorManager sensorManager;
    private Sensor lightSensor;
    private TextView txtValorLuz;
    private MqttHandler mqttHandler;
    private long lastPublishTime = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_sensor);

        txtValorLuz = findViewById(R.id.txtValorLuz);
        Button btnVolver = findViewById(R.id.btnVolver);
        btnVolver.setOnClickListener(v -> finish());

        mqttHandler = MqttHandler.getInstance(this);

        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        if (sensorManager != null) {
            lightSensor = sensorManager.getDefaultSensor(Sensor.TYPE_LIGHT);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (lightSensor != null) {
            sensorManager.registerListener(this, lightSensor, SensorManager.SENSOR_DELAY_NORMAL);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (mqttHandler != null) {
            mqttHandler.publish(ConfigMQTT.TOPIC_COMANDOS, "luz:-1");
        }
        if (sensorManager != null) {
            sensorManager.unregisterListener(this);
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        float lux = event.values[0];
        txtValorLuz.setText("Luz: " + lux + " lx");

        long now = System.currentTimeMillis();
        if (now - lastPublishTime >= 1000) {
            lastPublishTime = now;
            int valorLux = (int) lux;
            if (mqttHandler != null) {
                mqttHandler.publish(ConfigMQTT.TOPIC_COMANDOS, "luz:" + valorLux);
            }
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // No requerido
    }
}
