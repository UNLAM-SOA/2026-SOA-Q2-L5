package com.nexoriego.iot.core;

import java.util.UUID;

public final class MqttContract {
    private MqttContract() {}

    public static final String BROKER_URI     = "tcp://broker.emqx.io:1883";
    public static final String TOPIC_SENSORES = "soa/grupo05/nexoriego/sensores";
    public static final String TOPIC_COMANDOS = "soa/grupo05/nexoriego/comandos";

    public static final String CMD_REGAR      = "regar";
    public static final String CMD_RESET      = "reset";
    public static final String PREFIX_LUZ     = "luz:";
    public static final String CMD_LUZ_LOCAL  = "luz:-1";

    public static String generateClientId() {
        return "Android_NexoRiego_Reactive_" + UUID.randomUUID().toString().substring(0, 8);
    }
}
