package com.nexoriego.iot.core;

import android.util.Log;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.nexoriego.iot.model.TelemetryState;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Repositorio Singleton reactivo que desacopla Eclipse Paho de las Activities
 * mediante LiveData y un ExecutorService dedicado.
 */
public class IoTRepository implements MqttCallbackExtended {

    private static final String TAG = "IoTRepository";
    private static IoTRepository instance;

    private final ExecutorService networkExecutor = Executors.newSingleThreadExecutor();
    private final MutableLiveData<Boolean> connectionLiveData = new MutableLiveData<>(false);
    private final MutableLiveData<TelemetryState> telemetryLiveData = new MutableLiveData<>();

    private MqttClient client;

    private IoTRepository() {}

    public static synchronized IoTRepository getInstance() {
        if (instance == null) {
            instance = new IoTRepository();
        }
        return instance;
    }

    public LiveData<Boolean> getConnectionState() {
        return connectionLiveData;
    }

    public LiveData<TelemetryState> getTelemetry() {
        return telemetryLiveData;
    }

    public void connectIfNeeded() {
        networkExecutor.execute(() -> {
            try {
                if (client != null && client.isConnected()) {
                    connectionLiveData.postValue(true);
                    return;
                }
                MqttConnectOptions options = new MqttConnectOptions();
                options.setCleanSession(true);
                options.setAutomaticReconnect(true);
                options.setConnectionTimeout(10);

                client = new MqttClient(
                        MqttContract.BROKER_URI,
                        MqttContract.generateClientId(),
                        new MemoryPersistence()
                );
                client.setCallback(this);
                client.connect(options);
                client.subscribe(MqttContract.TOPIC_SENSORES, 0);

                connectionLiveData.postValue(true);
                Log.i(TAG, "Conectado a " + MqttContract.BROKER_URI);
            } catch (MqttException e) {
                Log.e(TAG, "Fallo de conexion MQTT", e);
                connectionLiveData.postValue(false);
            }
        });
    }

    public void sendCommand(String commandPayload) {
        networkExecutor.execute(() -> {
            try {
                if (client != null && client.isConnected()) {
                    MqttMessage msg = new MqttMessage(commandPayload.getBytes(StandardCharsets.UTF_8));
                    msg.setQos(0);
                    client.publish(MqttContract.TOPIC_COMANDOS, msg);
                    Log.d(TAG, "Comando enviado -> " + commandPayload);
                }
            } catch (MqttException e) {
                Log.e(TAG, "Error publicando comando", e);
            }
        });
    }

    public void disconnect() {
        networkExecutor.execute(() -> {
            try {
                if (client != null && client.isConnected()) {
                    client.disconnect();
                }
                connectionLiveData.postValue(false);
            } catch (MqttException e) {
                Log.e(TAG, "Error al desconectar", e);
            }
        });
    }

    @Override
    public void connectComplete(boolean reconnect, String serverURI) {
        Log.i(TAG, "connectComplete (reconnect=" + reconnect + ") -> " + serverURI);
        connectionLiveData.postValue(true);
        try {
            if (client != null && client.isConnected()) {
                client.subscribe(MqttContract.TOPIC_SENSORES, 0);
            }
        } catch (MqttException e) {
            Log.e(TAG, "Error resuscribiendo a " + MqttContract.TOPIC_SENSORES, e);
        }
    }

    @Override
    public void connectionLost(Throwable cause) {
        Log.w(TAG, "Conexion perdida con broker", cause);
        connectionLiveData.postValue(false);
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
        String raw = new String(message.getPayload(), StandardCharsets.UTF_8);
        Log.d(TAG, "Telemetria RX -> " + raw);
        try {
            TelemetryState parsed = TelemetryState.fromJson(raw);
            telemetryLiveData.postValue(parsed);
        } catch (Exception e) {
            Log.w(TAG, "Payload JSON invalido: " + raw, e);
        }
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {
    }
}
