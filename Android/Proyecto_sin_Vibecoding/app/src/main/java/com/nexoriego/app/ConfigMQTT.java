package com.nexoriego.app;

import java.util.UUID;

public class ConfigMQTT {
    public static final String MQTT_SERVER = "tcp://broker.emqx.io:1883";
    public static final String CLIENT_ID = "Android_NexoRiego_G05_" + UUID.randomUUID().toString().substring(0, 5);
    public static final String TOPIC_BASE = "soa/grupo05/nexoriego/";
    public static final String TOPIC_SENSORES = TOPIC_BASE + "sensores";
    public static final String TOPIC_BOMBA = TOPIC_BASE + "bomba";
    public static final String TOPIC_COMANDOS = TOPIC_BASE + "comandos";
}
