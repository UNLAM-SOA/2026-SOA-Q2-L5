package com.nexoriego.iot.model;

import org.json.JSONException;
import org.json.JSONObject;

public class TelemetryState {
    public final String estado;
    public final String motivo;
    public final int humedadPct;
    public final double temperaturaC;
    public final int luzActiva;
    public final String fuenteLuz;
    public final int regadas;
    public final int maxRegadas;
    public final String rawJson;

    public TelemetryState(String estado, String motivo, int humedadPct, double temperaturaC,
                          int luzActiva, String fuenteLuz, int regadas, int maxRegadas, String rawJson) {
        this.estado = estado;
        this.motivo = motivo;
        this.humedadPct = humedadPct;
        this.temperaturaC = temperaturaC;
        this.luzActiva = luzActiva;
        this.fuenteLuz = fuenteLuz;
        this.regadas = regadas;
        this.maxRegadas = maxRegadas;
        this.rawJson = rawJson;
    }

    public static TelemetryState fromJson(String payload) throws JSONException {
        JSONObject obj = new JSONObject(payload);
        return new TelemetryState(
                obj.optString("estado", "Desconocido"),
                obj.optString("motivo", "Sin registro"),
                obj.optInt("humedad", 0),
                obj.optDouble("temperatura", 0.0),
                obj.optInt("luz", 0),
                obj.optString("fuente_luz", "ESP32"),
                obj.optInt("regadas", 0),
                obj.optInt("max_regadas", 2),
                payload
        );
    }
}
