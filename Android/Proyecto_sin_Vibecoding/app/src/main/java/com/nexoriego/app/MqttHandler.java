package com.nexoriego.app;

import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

public class MqttHandler implements MqttCallback {

    public static final String ACTION_DATA_RECEIVE = "com.nexoriego.app.intent.action.DATA_RECEIVE";
    public static final String ACTION_CONNECTION_LOST = "com.nexoriego.app.intent.action.CONNECTION_LOST";

    private MqttClient client;
    private final Context mContext;

    public MqttHandler(Context mContext) {
        this.mContext = mContext;
    }

    public void connect(String brokerUrl, String clientId, String username, String password) {
        try {
            MqttConnectOptions options = new MqttConnectOptions();
            options.setCleanSession(true);
            if (username != null && !username.isEmpty()) {
                options.setUserName(username);
            }
            if (password != null && !password.isEmpty()) {
                options.setPassword(password.toCharArray());
            }

            MemoryPersistence persistence = new MemoryPersistence();
            client = new MqttClient(brokerUrl, clientId, persistence);
            client.connect(options);
            client.setCallback(this);
        } catch (MqttException e) {
            Log.d("Aplicacion", e.getMessage() + "  " + e.getCause());
        }
    }

    public void disconnect() {
        try {
            if (client != null && client.isConnected()) {
                client.disconnect();
            }
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    public void publish(String topic, String message) {
        try {
            if (client != null && client.isConnected()) {
                MqttMessage mqttMessage = new MqttMessage(message.getBytes());
                mqttMessage.setQos(1);
                client.publish(topic, mqttMessage);
            }
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    public void subscribe(String topic) {
        try {
            if (client != null && client.isConnected()) {
                client.subscribe(topic);
            }
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void connectionLost(Throwable cause) {
        Log.d("MAIN ACTIVITY", "conexion perdida " + (cause != null ? cause.getMessage() : ""));
        Intent i = new Intent(ACTION_CONNECTION_LOST);
        i.setPackage(mContext.getPackageName());
        mContext.sendBroadcast(i);
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
        String msgJson = message.toString();

        Intent i = new Intent(ACTION_DATA_RECEIVE);
        i.putExtra("msgJson", msgJson);
        i.putExtra("topic", topic);
        i.setPackage(mContext.getPackageName());
        mContext.sendBroadcast(i);
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {

    }
}
