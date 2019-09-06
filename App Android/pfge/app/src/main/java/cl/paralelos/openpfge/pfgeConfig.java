package cl.paralelos.openpfge;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.harrysoft.androidbluetoothserial.BluetoothManager;
import com.harrysoft.androidbluetoothserial.BluetoothSerialDevice;
import com.harrysoft.androidbluetoothserial.SimpleBluetoothDeviceInterface;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.schedulers.Schedulers;

// https://github.com/harry1453/android-bluetooth-serial

public class pfgeConfig extends AppCompatActivity implements ItemPickerDialogFragment.OnItemSelectedListener {

    CheckBox checkboxAutoSend;
    Switch switchOnOff;
    Switch switchPause;
    Switch switchRamp;
    Switch switchLcdActive;
    Switch switchLcdBacklight;
    LinearLayout wrapWop;
    LinearLayout wrapRampStart;
    LinearLayout wrapRampEnd;
    LinearLayout wrapRampDuration;
    LinearLayout wrapAutoWop;
    EditText editTextAngle;
    EditText editTextWop;
    EditText editTextRampStart;
    EditText editTextRampEnd;
    EditText editTextRampDuration;
    EditText editTextLcdUpdateInterval;
    EditText editTextBufferTemperatureUpdateInterval;
    TextView textViewAutoWop;
    TextView textViewBufferTemperature;
    TextView textViewHasRun;
    TextView textViewDeviceName;

    private String bluetoothName = null;
    private String bluetoothAdress = null;
    private SimpleBluetoothDeviceInterface deviceInterface;
    BluetoothManager bluetoothManager;
    BluetoothAdapter mBluetoothAdapter;
    private int firmwareVersion = 0;
    private int firmwareSubversion = 0;
    private Boolean preventAutoSend = false;
    private SharedPreferences settings;
    private SharedPreferences.Editor settingsEditor;

    final int REQUEST_ENABLE_BT = 0;
    final String settingsBluetoothAdress = "bta";
    final String settingsBluetoothName = "btn";
    final String methodGet = "g";
    final String methodSet = "s";
    final String methodWho = "w";
    final String methodAutomaticEnd = "a";
    final Integer minFirmwareVersionSupported = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getSupportActionBar().hide();

        settings = this.getSharedPreferences("bluetoothDevice", Context.MODE_PRIVATE);
        settingsEditor=settings.edit();

        firmwareVersion = 0;

        registerReceiver(mReceiver, new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));

        bluetoothAdress = settings.getString(settingsBluetoothAdress, null);
        bluetoothName = settings.getString(settingsBluetoothName, null);
        bluetoothManager = BluetoothManager.getInstance();
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        if (bluetoothManager == null) {
            // Bluetooth unavailable on this device :( tell the user
            Toast.makeText(getApplicationContext(), "Bluetooth not available.", Toast.LENGTH_LONG).show();
            finish();
        }

        if (!mBluetoothAdapter.isEnabled()) {
            requireTurnBtOn();
        } else {
            connectDevice();
        }
    }

    // BT COM

    private void requestMethodSet() {
        Map<String, String> params = new HashMap<String, String>();
        params.put("o", switchOnOff.isChecked() ? "t" : "f");
        params.put("p", switchPause.isChecked() ? "t" : "f");
        params.put("r", switchRamp.isChecked() ? "t" : "f");
        params.put("a", editTextAngle.getText().toString());
        params.put("w", editTextWop.getText().toString());
        params.put("rs", editTextRampStart.getText().toString());
        params.put("re", editTextRampEnd.getText().toString());
        params.put("rd", editTextRampDuration.getText().toString());
        // deep config
        params.put("la", switchLcdActive.isChecked() ? "t" : "f");
        params.put("lb", switchLcdBacklight.isChecked() ? "t" : "f");
        params.put("lui", editTextLcdUpdateInterval.getText().toString());
        params.put("bui", editTextBufferTemperatureUpdateInterval.getText().toString());
        requestMethodWithParams(methodSet, params);
    }

    private void requestMethod(String method) {
        requestMethodWithParams(method, null);
    }

    private void requestMethodWithParams(String method, Map<String, String> params) {
        String finalRequest = "m=" + method;
        if (params != null) {
            for (Map.Entry<String, String> entry : params.entrySet()) {
                finalRequest += "@" + entry.getKey() + "=" + entry.getValue();
            }
        }
        Log.d("Request", finalRequest);
        deviceInterface.sendMessage(finalRequest);
    }

    private void connectDevice() {
        if (bluetoothAdress == null) {
            selectBluetoothDevice();
        } else {
            setContentView(R.layout.activity_connecting_bt_device);
            TextView textViewBtName = (TextView) findViewById(R.id.textViewBtName);
            textViewBtName.setText(bluetoothName);
            bluetoothManager.openSerialDevice(bluetoothAdress)
                    .subscribeOn(Schedulers.io())
                    .observeOn(AndroidSchedulers.mainThread())
                    .subscribe(this::onConnected, this::onError);
        }
    }

    private void disconnectDevice() {
        setContentView(R.layout.activity_disconnected_from_bt_device);
        TextView textViewBtName = (TextView) findViewById(R.id.textViewBtName);
        textViewBtName.setText(bluetoothName);
        LinearLayout linearLayoutTapToReconnect = (LinearLayout) findViewById(R.id.linearLayoutTapToReconnect);
        linearLayoutTapToReconnect.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                connectDevice();
            }
        });
        linearLayoutTapToReconnect.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                selectBluetoothDevice();
                return false;
            }
        });
        bluetoothManager.closeDevice(bluetoothAdress);
        bluetoothManager.close();
    }

    private void onConnected(BluetoothSerialDevice connectedDevice) {
        // You are now connected to this device!
        // Here you may want to retain an instance to your device:
        deviceInterface = connectedDevice.toSimpleDeviceInterface();

        // Listen to bluetooth events
        deviceInterface.setListeners(this::onMessageReceived, this::onMessageSent, this::onError);
        requestMethod(methodWho);
    }

    private void onMessageSent(String message) {
        // We sent a message! Handle it here.
        //Toast.makeText(getApplicationContext(), "Sent a message! Message was: " + message, Toast.LENGTH_LONG).show();
    }

    private void onMessageReceived(String message) {
        // We received a message! Handle it here.
        Log.d("Response ", message);
        Map<String, String> response = new HashMap<String, String>();
        for (String value : message.split("@")) {
            String param1 = value.split("=")[0];
            String param2 = value.split("=")[1];
            response.put(param1, param2);
        }
        if (response.get("m") == null) {
            Toast.makeText(getApplicationContext(), "Bad response from device", Toast.LENGTH_LONG).show();
            return;
        }
        if (response.get("m").compareTo(methodWho) == 0) {
            if (response.get("fv") == null || response.get("fs") == null) {
                Toast.makeText(getApplicationContext(), "Device not recognized", Toast.LENGTH_LONG).show();
                return;
            }
            firmwareVersion = Integer.parseInt(response.get("fv"));
            firmwareSubversion = Integer.parseInt(response.get("fv"));
            if (firmwareVersion < minFirmwareVersionSupported) {
                Toast.makeText(getApplicationContext(), "Firmware version not supported\nPlease choose another device", Toast.LENGTH_LONG).show();
                selectBluetoothDevice();
                return;
            }
            requestMethod(methodGet);
            setMainView();
        }
        if (firmwareVersion == 0) {
            Toast.makeText(getApplicationContext(), "Firmware version is not set\nPlease start again", Toast.LENGTH_LONG).show();
            return;
        }
        if (firmwareVersion < minFirmwareVersionSupported) {
            Toast.makeText(getApplicationContext(), "Firmware version not supported\nPlease choose another device", Toast.LENGTH_LONG).show();
            selectBluetoothDevice();
            return;
        }
        if (response.get("m").compareTo(methodSet) == 0 && firmwareVersion != 0) {
            if (response.get("res").compareTo("ok") != 0) {
                Toast.makeText(getApplicationContext(), "SET597029366810 | Bad response", Toast.LENGTH_LONG).show();
                return;
            }
            processResponse(response);
            Toast.makeText(getApplicationContext(), "SET & SYNC done at " + getCurrentDate(null), Toast.LENGTH_LONG).show();
        }
        if (response.get("m").compareTo(methodGet) == 0 && firmwareVersion != 0) {
            processResponse(response);
            Toast.makeText(getApplicationContext(), "SYNC done at " + getCurrentDate(null), Toast.LENGTH_LONG).show();
        }
        if (response.get("m").compareTo(methodAutomaticEnd) == 0 && firmwareVersion != 0) {
            if (response.get("res").compareTo("ok") != 0) {
                Toast.makeText(getApplicationContext(), "Automatic end | Bad response", Toast.LENGTH_LONG).show();
                return;
            } else{
                setMainView();
            }
        }
    }

    private void processResponse(Map<String, String> response) {
        preventAutoSend = true;
        for (Map.Entry<String, String> entry : response.entrySet()) {
            switch (entry.getKey()) {
                case "o":
                    switchOnOff.setChecked("t".equals(entry.getValue()));
                    break;
                case "p":
                    switchPause.setChecked("t".equals(entry.getValue()));
                    break;
                case "r":
                    switchRamp.setChecked("t".equals(entry.getValue()));
                    break;
                case "a":
                    editTextAngle.setText(entry.getValue());
                    break;
                case "w":
                    editTextWop.setText(entry.getValue());
                    break;
                case "rs":
                    editTextRampStart.setText(entry.getValue());
                    break;
                case "re":
                    editTextRampEnd.setText(entry.getValue());
                    break;
                case "rd":
                    editTextRampDuration.setText(entry.getValue());
                    break;
                case "aw":
                    textViewAutoWop.setText(entry.getValue());
                    break;
                case "bt":
                    textViewBufferTemperature.setText(entry.getValue());
                    break;
                case "hr":
                    long seconds = Integer.parseInt(entry.getValue());
                    String hms = String.format("%02d:%02d:%02d", TimeUnit.SECONDS.toHours(seconds),
                            TimeUnit.SECONDS.toMinutes(seconds) - TimeUnit.HOURS.toMinutes(TimeUnit.SECONDS.toHours(seconds)),
                            TimeUnit.SECONDS.toSeconds(seconds) - TimeUnit.MINUTES.toSeconds(TimeUnit.SECONDS.toMinutes(seconds)));
                    textViewHasRun.setText(hms);
                    break;
                case "ae":
                    if("t".equals(entry.getValue())) {
                        setContentView(R.layout.activity_automatic_end);
                        LinearLayout linearLayoutTapToContinue = (LinearLayout) findViewById(R.id.linearLayoutTapToContinue);
                        linearLayoutTapToContinue.setOnClickListener(new View.OnClickListener() {
                            public void onClick(View v) {
                                requestMethod(methodAutomaticEnd); // reset automatic end
                            }
                        });
                    }
                    break;
                // deep config
                case "la":
                    switchLcdActive.setChecked("t".equals(entry.getValue()));
                    break;
                case "lb":
                    switchLcdBacklight.setChecked("t".equals(entry.getValue()));
                    break;
                case "lui":
                    editTextLcdUpdateInterval.setText(entry.getValue());
                    break;
                case "bui":
                    editTextBufferTemperatureUpdateInterval.setText(entry.getValue());
                    break;
                default:
                    break;
            }
        }
        preventAutoSend = false;
    }

    private void onError(Throwable error) {
        // Handle the error
        disconnectDevice();
    }

    // UI

    private void showHideUI() {
        if (switchRamp.isChecked()) {
            wrapWop.setVisibility(View.GONE);
            wrapRampStart.setVisibility(View.VISIBLE);
            wrapRampEnd.setVisibility(View.VISIBLE);
            wrapRampDuration.setVisibility(View.VISIBLE);
            wrapAutoWop.setVisibility(View.VISIBLE);
        } else {
            wrapWop.setVisibility(View.VISIBLE);
            wrapRampStart.setVisibility(View.GONE);
            wrapRampEnd.setVisibility(View.GONE);
            wrapRampDuration.setVisibility(View.GONE);
            wrapAutoWop.setVisibility(View.GONE);
        }
    }

    private void selectBluetoothDevice() {
        setContentView(R.layout.activity_select_bt_device);
        List<BluetoothDevice> pairedDevices = bluetoothManager.getPairedDevicesList();

        // If there are paired devices
        if (pairedDevices.size() > 0) {
            // Loop through paired devices
            ArrayList<ItemPickerDialogFragment.Item> pickerItems = new ArrayList<>();
            for (BluetoothDevice device : pairedDevices) {
                // Add the name and address to an array adapter to show in a ListView
                pickerItems.add(new ItemPickerDialogFragment.Item(device.getName(), device.getAddress()));
            }
            ItemPickerDialogFragment dialog = ItemPickerDialogFragment.newInstance(
                    "Select BT Device",
                    pickerItems,
                    -1
            );
            dialog.show(getFragmentManager(), "ItemPicker");
        } else {
            Toast.makeText(getApplicationContext(), "No paired devices. Add one and retry", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onItemSelected(ItemPickerDialogFragment fragment, ItemPickerDialogFragment.Item item, int index) {
        bluetoothName = item.getTitle();
        bluetoothAdress = item.getStringValue();
        // save to memory
        settingsEditor.putString(settingsBluetoothName, bluetoothName);
        settingsEditor.putString(settingsBluetoothAdress, bluetoothAdress);
        settingsEditor.commit();

        connectDevice();
    }

    private String getCurrentDate(String pattern) {
        DateFormat dateFormat = new SimpleDateFormat("HH:mm:ss dd/MM/YY");
        if (pattern != null) {
            dateFormat = new SimpleDateFormat(pattern);
        }
        Date date = new Date();
        return dateFormat.format(date);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mReceiver);
        disconnectDevice();
    }

    private void requireTurnBtOn() {
        setContentView(R.layout.activity_turn_bt_on);
        LinearLayout linearLayoutTapToReconnect = (LinearLayout) findViewById(R.id.linearLayoutTapToRetry);
        linearLayoutTapToReconnect.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
                startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
            }
        });
        Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
    }

    private void setMainView() {
        setContentView(R.layout.activity_pfge_config);

        checkboxAutoSend = (CheckBox) findViewById(R.id.checkboxAutoSend);
        switchOnOff = (Switch) findViewById(R.id.switchRunning);
        switchPause = (Switch) findViewById(R.id.switchPause);
        switchRamp = (Switch) findViewById(R.id.switchRamp);
        switchLcdActive = (Switch) findViewById(R.id.switchLcdActive);
        switchLcdBacklight = (Switch) findViewById(R.id.switchLcdBacklight);

        wrapWop = (LinearLayout) findViewById(R.id.wrapWop);
        wrapRampStart = (LinearLayout) findViewById(R.id.wrapRampStart);
        wrapRampEnd = (LinearLayout) findViewById(R.id.wrapRampEnd);
        wrapRampDuration = (LinearLayout) findViewById(R.id.wrapRampDuration);
        wrapAutoWop = (LinearLayout) findViewById(R.id.wrapAutoWop);

        editTextAngle = findViewById(R.id.editTextAngle);
        editTextWop = findViewById(R.id.editTextWop);
        editTextRampStart = findViewById(R.id.editTextRampStart);
        editTextRampEnd = findViewById(R.id.editTextRampEnd);
        editTextRampDuration = findViewById(R.id.editTextRampDuration);
        editTextLcdUpdateInterval = findViewById(R.id.editTextLcdUpdateInterval);
        editTextBufferTemperatureUpdateInterval = findViewById(R.id.editTextBufferTemperatureUpdateInterval);

        final Button buttonGetFromPfge = findViewById(R.id.buttonGetFromPfge);
        final Button buttonSetFromPfge = findViewById(R.id.buttonSetFromPfge);
        final Button buttonChangeDevice = findViewById(R.id.buttonChangeDevice);
        final Button buttonDisconnectDevice = findViewById(R.id.buttonDisconnectDevice);

        textViewAutoWop = findViewById(R.id.textViewAutoWop);
        textViewBufferTemperature = findViewById(R.id.textViewBufferTemperature);
        textViewHasRun = findViewById(R.id.textViewHasRun);
        textViewDeviceName = findViewById(R.id.textViewDeviceName);

        textViewDeviceName.setText("Device: " + bluetoothName);

        checkboxAutoSend.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    buttonSetFromPfge.setEnabled(false);
                } else {
                    buttonSetFromPfge.setEnabled(true);
                }
            }
        });

        switchRamp.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                showHideUI();
            }
        });

        buttonGetFromPfge.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                requestMethod(methodGet);
            }
        });

        buttonSetFromPfge.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                requestMethodSet();
            }
        });

        buttonChangeDevice.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                selectBluetoothDevice();
            }
        });

        buttonDisconnectDevice.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                disconnectDevice();
            }
        });

        CompoundButton.OnCheckedChangeListener occl = new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (checkboxAutoSend.isChecked() && !preventAutoSend) {
                    requestMethodSet();
                }
                showHideUI();
            }
        };

        switchOnOff.setOnCheckedChangeListener(occl);
        switchPause.setOnCheckedChangeListener(occl);
        switchRamp.setOnCheckedChangeListener(occl);
        switchLcdActive.setOnCheckedChangeListener(occl);
        switchLcdBacklight.setOnCheckedChangeListener(occl);

        View.OnKeyListener okl = new View.OnKeyListener() {
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if ((event.getAction() == KeyEvent.ACTION_DOWN) && (keyCode == KeyEvent.KEYCODE_ENTER)) {
                    if (checkboxAutoSend.isChecked() && !preventAutoSend) {
                        requestMethodSet();
                    }
                    return true;
                }
                return false;
            }
        };

        editTextAngle.setOnKeyListener(okl);
        editTextWop.setOnKeyListener(okl);
        editTextRampStart.setOnKeyListener(okl);
        editTextRampEnd.setOnKeyListener(okl);
        editTextRampDuration.setOnKeyListener(okl);
        editTextLcdUpdateInterval.setOnKeyListener(okl);
        editTextBufferTemperatureUpdateInterval.setOnKeyListener(okl);

        showHideUI();
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();

            if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                final int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE,
                        BluetoothAdapter.ERROR);
                switch (state) {
                    case BluetoothAdapter.STATE_OFF:
                        requireTurnBtOn();
                        break;
                    case BluetoothAdapter.STATE_ON:
                        connectDevice();
                        break;
                }
            }
        }
    };
}
