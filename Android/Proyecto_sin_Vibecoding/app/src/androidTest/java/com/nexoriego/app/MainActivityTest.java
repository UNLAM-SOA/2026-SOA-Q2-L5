import android.content.Context;
import android.content.Intent;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import com.nexoriego.app.MainActivity;
import com.nexoriego.app.MqttHandler;
import com.nexoriego.app.R;

@RunWith(AndroidJUnit4.class)
public class MainActivityTest {

    // 1. Le decimos que levante la MainActivity para probarla
    @Rule
    public ActivityScenarioRule<MainActivity> activityRule = new ActivityScenarioRule<>(MainActivity.class);

    @Test
    public void alRecibirEstadoRiego_actualizaTextoVisual() throws InterruptedException {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // 1. OJO ACÁ: Tiene que ser exactamente el mismo String que usa el IntentFilter de tu compañero
        Intent intent = new Intent(MqttHandler.ACTION_DATA_RECEIVE);

        // BORRAMOS o comentamos la categoría para que no haya conflicto de filtros
        // intent.addCategory(Intent.CATEGORY_DEFAULT);

        intent.setPackage(context.getPackageName());
        intent.putExtra("msgJson", "{\"temperatura\":\"25.0\", \"humedad\":\"2048\", \"luz\":\"5000\", \"estado\":\"Riego\"}");

        context.sendBroadcast(intent);

        Thread.sleep(500); // Pausa para que la UI reaccione

        onView(withId(R.id.tvEstado)).check(matches(withText("Riego")));
    }
}