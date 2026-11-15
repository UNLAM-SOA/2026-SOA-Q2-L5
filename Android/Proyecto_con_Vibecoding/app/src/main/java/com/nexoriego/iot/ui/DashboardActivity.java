package com.nexoriego.iot.ui;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import com.nexoriego.iot.R;
import com.nexoriego.iot.core.IoTRepository;
import com.nexoriego.iot.core.MqttContract;
import com.nexoriego.iot.databinding.ActivityDashboardBinding;
import com.nexoriego.iot.model.TelemetryState;

import java.util.Locale;

public class DashboardActivity extends AppCompatActivity {

    private ActivityDashboardBinding binding;
    private IoTRepository repository;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityDashboardBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        repository = IoTRepository.getInstance();
        setupObservers();
        setupListeners();
    }

    @Override
    protected void onResume() {
        super.onResume();
        repository.connectIfNeeded();
    }

    private void setupObservers() {
        repository.getConnectionState().observe(this, connected -> {
            if (Boolean.TRUE.equals(connected)) {
                binding.badgeConexion.setText("MQTT Online");
                binding.badgeConexion.setBackgroundColor(ContextCompat.getColor(this, R.color.state_absorcion_bg));
                binding.badgeConexion.setTextColor(ContextCompat.getColor(this, R.color.state_absorcion_fg));
            } else {
                binding.badgeConexion.setText("Desconectado");
                binding.badgeConexion.setBackgroundColor(ContextCompat.getColor(this, R.color.state_alerta_bg));
                binding.badgeConexion.setTextColor(ContextCompat.getColor(this, R.color.state_alerta_fg));
            }
        });

        repository.getTelemetry().observe(this, this::renderTelemetry);
    }

    private void renderTelemetry(TelemetryState state) {
        binding.txtEstadoFsm.setText(state.estado);
        binding.txtMotivoTrazabilidad.setText("Último evento: " + state.motivo);
        binding.txtCiclosTanque.setText(String.format(Locale.US,
                "Ciclos de riego consumidos: %d / %d", state.regadas, state.maxRegadas));

        binding.txtHumedad.setText(state.humedadPct + " %");
        binding.txtTemperatura.setText(String.format(Locale.US, "%.1f °C", state.temperaturaC));
        binding.txtLuzActiva.setText(state.luzActiva + " lx");
        binding.badgeFuenteLuz.setText("Fuente: " + state.fuenteLuz);
        binding.txtRawJson.setText(state.rawJson);

        applyStateCardStyle(state.estado);
    }

    private void applyStateCardStyle(String estado) {
        int bgColor;
        int fgColor;
        switch (estado) {
            case "Riego":
                bgColor = R.color.state_riego_bg;
                fgColor = R.color.state_riego_fg;
                break;
            case "Absorcion":
                bgColor = R.color.state_absorcion_bg;
                fgColor = R.color.state_absorcion_fg;
                break;
            case "Falta de agua":
                bgColor = R.color.state_alerta_bg;
                fgColor = R.color.state_alerta_fg;
                break;
            default:
                bgColor = R.color.state_monitoreo_bg;
                fgColor = R.color.state_monitoreo_fg;
                break;
        }
        binding.cardEstadoFsm.setCardBackgroundColor(ContextCompat.getColor(this, bgColor));
        binding.txtEstadoFsm.setTextColor(ContextCompat.getColor(this, fgColor));
    }

    private void setupListeners() {
        binding.btnRegar.setOnClickListener(v -> {
            repository.sendCommand(MqttContract.CMD_REGAR);
            Toast.makeText(this, "Orden de riego manual enviada", Toast.LENGTH_SHORT).show();
        });

        binding.btnReset.setOnClickListener(v -> {
            repository.sendCommand(MqttContract.CMD_RESET);
            Toast.makeText(this, "Orden de restablecimiento enviada", Toast.LENGTH_SHORT).show();
        });

        binding.btnEnviarComando.setOnClickListener(v -> {
            String customCmd = binding.inputComando.getText() != null
                    ? binding.inputComando.getText().toString().trim()
                    : "";
            if (!customCmd.isEmpty()) {
                repository.sendCommand(customCmd);
                binding.inputComando.setText("");
                Toast.makeText(this, "Publicado: " + customCmd, Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "Escribí un comando primero", Toast.LENGTH_SHORT).show();
            }
        });

        binding.btnAbrirSensorLuz.setOnClickListener(v -> {
            Intent intent = new Intent(DashboardActivity.this, LightOverrideActivity.class);
            startActivity(intent);
        });
    }

    @Override
    protected void onDestroy() {
        if (isFinishing()) {
            repository.disconnect();
        }
        super.onDestroy();
    }
}
