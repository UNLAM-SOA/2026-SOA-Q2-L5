package com.nexoriego.iot.ui;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import com.nexoriego.iot.core.IoTRepository;
import com.nexoriego.iot.core.MqttContract;
import com.nexoriego.iot.databinding.ActivityLightOverrideBinding;

public class LightOverrideActivity extends AppCompatActivity implements SensorEventListener {

    private static final long DEBOUNCE_INTERVAL_MS = 800L;

    private ActivityLightOverrideBinding binding;
    private IoTRepository repository;
    private SensorManager sensorManager;
    private Sensor lightSensor;

    private long lastPublishTimestamp = 0L;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityLightOverrideBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        repository = IoTRepository.getInstance();
        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        if (sensorManager != null) {
            lightSensor = sensorManager.getDefaultSensor(Sensor.TYPE_LIGHT);
        }

        if (lightSensor == null) {
            binding.txtLuxActual.setText("N/D");
            binding.txtEstadoTransmision.setText("Este dispositivo no posee sensor de luz físico.");
        }

        binding.btnRestaurarYVolver.setOnClickListener(v -> finish());
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (sensorManager != null && lightSensor != null) {
            sensorManager.registerListener(this, lightSensor, SensorManager.SENSOR_DELAY_UI);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (sensorManager != null) {
            sensorManager.unregisterListener(this);
        }
        // Restaura inmediatamente la lectura local del LDR en el ESP32
        repository.sendCommand(MqttContract.CMD_LUZ_LOCAL);
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        int lux = Math.round(event.values[0]);
        binding.txtLuxActual.setText(lux + " lx");
        binding.progressLux.setProgress(Math.min(lux, 1500));

        long now = System.currentTimeMillis();
        if (now - lastPublishTimestamp >= DEBOUNCE_INTERVAL_MS) {
            lastPublishTimestamp = now;
            repository.sendCommand(MqttContract.PREFIX_LUZ + lux);
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }
}
