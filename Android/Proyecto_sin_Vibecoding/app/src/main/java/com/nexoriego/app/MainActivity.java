package com.nexoriego.app;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.json.JSONException;
import org.json.JSONObject;

public class MainActivity extends AppCompatActivity {

    private MqttHandler mqttHandler;

    private TextView temperatura;
    private TextView humedad;
    private TextView luz;
    private TextView estado;
    private ImageView ivPlanta;

    public IntentFilter filterReceive;
    public IntentFilter filterConnectionLost;
    private final ReceptorOperacion receiver = new ReceptorOperacion();
    private final ConnectionLost connectionLost = new ConnectionLost();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
        StrictMode.setThreadPolicy(policy);

        temperatura = findViewById(R.id.tvTemperatura);
        humedad = findViewById(R.id.tvHumedad);
        luz = findViewById(R.id.tvLuz);
        estado = findViewById(R.id.tvEstado);
        ivPlanta = findViewById(R.id.ivPlanta);
        ivPlanta.setImageResource(R.drawable.monitoreo);

        findViewById(R.id.cvLuz).setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, SensorActivity.class);
            startActivity(intent);
        });

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        mqttHandler = MqttHandler.getInstance(this);
        connect();
        configurarBroadcastReceiver();
    }

    private void connect() {
        mqttHandler.connect(ConfigMQTT.MQTT_SERVER, ConfigMQTT.CLIENT_ID);
        mqttHandler.subscribe(ConfigMQTT.TOPIC_SENSORES);
    }

    @SuppressLint("UnspecifiedRegisterReceiverFlag")
    private void configurarBroadcastReceiver() {
        filterReceive = new IntentFilter(MqttHandler.ACTION_DATA_RECEIVE);
        filterConnectionLost = new IntentFilter(MqttHandler.ACTION_CONNECTION_LOST);

        filterReceive.addCategory(Intent.CATEGORY_DEFAULT);
        filterConnectionLost.addCategory(Intent.CATEGORY_DEFAULT);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(receiver, filterReceive, Context.RECEIVER_NOT_EXPORTED);
            registerReceiver(connectionLost, filterConnectionLost, Context.RECEIVER_NOT_EXPORTED);
        } else {
            registerReceiver(receiver, filterReceive);
            registerReceiver(connectionLost, filterConnectionLost);
        }
    }

    @Override
    protected void onDestroy() {
        if (mqttHandler != null) {
            mqttHandler.disconnect();
        }
        super.onDestroy();
        try {
            unregisterReceiver(receiver);
            unregisterReceiver(connectionLost);
        } catch (Exception ignored) {
        }
    }

    public class ConnectionLost extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Toast.makeText(getApplicationContext(), "Conexión Perdida", Toast.LENGTH_SHORT).show();
            connect();
        }
    }

    public class ReceptorOperacion extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String msgJson = intent.getStringExtra("msgJson");
            if (msgJson != null) {
                try {
                    JSONObject json = new JSONObject(msgJson);
                    if (json.has("temperatura")) {
                        temperatura.setText(json.getString("temperatura") + " °C");
                    }
                    if (json.has("humedad")) {
                        int humedadValue = (int) (Double.parseDouble(json.getString("humedad")) / 4096.0 * 100);
                        humedad.setText(humedadValue + " %");
                    }
                    if (json.has("luz")) {
                        luz.setText(json.getString("luz") + " lum");
                    }
                    if (json.has("estado")) {
                        String estadoStr = json.getString("estado");
                        estado.setText(estadoStr);
                        actualizarImagenPlanta(estadoStr);
                    }
                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private void actualizarImagenPlanta(String estadoStr) {
        if (ivPlanta == null || estadoStr == null) return;
        switch (estadoStr) {
            case "Riego":
                ivPlanta.setImageResource(R.drawable.regar);
                break;
            case "Falta de agua":
            case "Falta_agua":
                ivPlanta.setImageResource(R.drawable.falta_agua);
                break;
            case "Absorcion":
                ivPlanta.setImageResource(R.drawable.absorcion);
                break;
            case "Monitoreo":
            default:
                ivPlanta.setImageResource(R.drawable.monitoreo);
                break;
        }
    }
}
