package cl.paralelos.openpfge;

import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.PorterDuff;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import com.harrysoft.androidbluetoothserial.BluetoothManager;
import com.harrysoft.androidbluetoothserial.BluetoothSerialDevice;
import com.harrysoft.androidbluetoothserial.SimpleBluetoothDeviceInterface;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.schedulers.Schedulers;

public class pfgeConfig extends AppCompatActivity implements ItemPickerDialogFragment.OnItemSelectedListener {

    Switch switchOnOff;
    Switch switchPause;
    Switch switchRamp;
    Switch switchDisplayActive;
    Switch switchDisplayBacklight;
    Switch switchBufferTemperatureAutomaticControl;
    LinearLayout wrapWop;
    LinearLayout wrapRampStart;
    LinearLayout wrapRampEnd;
    LinearLayout wrapRampDuration;
    LinearLayout wrapAutoWop;
    LinearLayout wrapBufferTemperatureUpdateSetpoint;
    LinearLayout wrapDisplayUpdateInterval;
    EditText editTextAngle;
    EditText editTextWop;
    EditText editTextRampStart;
    EditText editTextRampEnd;
    EditText editTextRampDuration;
    EditText editTextDisplayUpdateInterval;
    EditText editTextBufferTemperatureUpdateInterval;
    EditText editTextBufferTemperatureSetpoint;
    EditText editTextBufferTemperatureMaxError;
    EditText editTextServoSpeed;
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
    private Integer minFirmwareVersionSupported;
    private SharedPreferences settings;
    private SharedPreferences.Editor settingsEditor;

    final int REQUEST_ENABLE_BT = 0;
    final String settingsBluetoothAdress = "bta";
    final String settingsBluetoothName = "btn";

    final String methodSync = "y";
    final String methodSet = "s";
    final String methodWho = "w";
    final String methodAutomaticEnd = "a";
    final String methodUnknown = "u";
    final String methodCommunicationError = "c";

    final String paramOpOnOff = "0";
    final String paramOpPause = "1";
    final String paramOpRamp = "2";
    final String paramOpAngle = "3";
    final String paramOpWop = "4";
    final String paramOpAutoWop = "5";
    final String paramOpHasRun= "6";
    final String paramOpAutoEnd = "7";
    final String paramOpRampStart = "8";
    final String paramOpRampEnd = "9";
    final String paramOpRampDuration = "10";
    final String paramOpDisplayActive = "11";
    final String paramOpDisplayUpdateInterval = "12";
    final String paramOpBufferTemperature = "13";
    final String paramOpBufferTemperatureUpdateInterval = "14";
    final String paramOpBufferTemperatureAutomaticControl = "15";
    final String paramOpBufferTemperatureSetpoint = "16";
    final String paramOpBufferTemperatureMaxError = "17";
    final String paramOpDisplayBacklight = "18";
    final String paramOpServoSpeed = "19";

    Map<String, String> methodName = new HashMap<String, String>();

    final String programasStringSetName = "programas";
    final String programasStringSetSep = "¬";
    Toast toast;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        minFirmwareVersionSupported = Integer.parseInt(getResources().getString(R.string.app_firmware_min_firmware_version_supported));

        settings = this.getSharedPreferences("bluetoothDevice", Context.MODE_PRIVATE);
        settingsEditor = settings.edit();

        registerReceiver(mReceiver, new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));

        bluetoothAdress = settings.getString(settingsBluetoothAdress, null);
        bluetoothName = settings.getString(settingsBluetoothName, null);
        bluetoothManager = BluetoothManager.getInstance();
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();


        methodName.put(methodSync, "Sync");
        methodName.put(methodSet, "Set");
        methodName.put(methodWho, "Who");
        methodName.put(methodAutomaticEnd, "Auto End");
        methodName.put(methodUnknown, "Unkown");
        methodName.put(methodCommunicationError, "Comm. Error");


        toast = Toast.makeText(getApplicationContext(), "", Toast.LENGTH_LONG);
        View view = toast.getView();
        view.getBackground().setColorFilter(ContextCompat.getColor(getApplicationContext(), R.color.colorToastBackground), PorterDuff.Mode.SRC_IN);
        TextView textView = (TextView) toast.getView().findViewById(android.R.id.message);
        textView.setTextColor(ContextCompat.getColor(getApplicationContext(), R.color.colorToastText));

        if (bluetoothManager == null) {
            // Bluetooth unavailable on this device :( tell the user
            showFlashMessage("Bluetooth not available.");
            finish();
        }

        if (!mBluetoothAdapter.isEnabled()) {
            requireTurnBtOn();
        } else {
            connectDevice();
        }
    }

    private Map<String, String> encodeParams() {
        Map<String, String> params = new HashMap<String, String>();
        params.put(paramOpOnOff, switchOnOff.isChecked() ? "t" : "f");
        params.put(paramOpPause, switchPause.isChecked() ? "t" : "f");
        params.put(paramOpRamp, switchRamp.isChecked() ? "t" : "f");
        params.put(paramOpAngle, editTextAngle.getText().toString());
        params.put(paramOpWop, editTextWop.getText().toString());
        params.put(paramOpRampStart, editTextRampStart.getText().toString());
        params.put(paramOpRampEnd, editTextRampEnd.getText().toString());
        params.put(paramOpRampDuration, editTextRampDuration.getText().toString());
        params.put(paramOpDisplayActive, switchDisplayActive.isChecked() ? "t" : "f");
        params.put(paramOpDisplayUpdateInterval, editTextDisplayUpdateInterval.getText().toString());
        params.put(paramOpDisplayBacklight, switchDisplayBacklight.isChecked() ? "t" : "f");
        params.put(paramOpBufferTemperatureUpdateInterval, editTextBufferTemperatureUpdateInterval.getText().toString());
        params.put(paramOpBufferTemperatureAutomaticControl, switchBufferTemperatureAutomaticControl.isChecked() ? "t" : "f");
        params.put(paramOpBufferTemperatureSetpoint, editTextBufferTemperatureSetpoint.getText().toString());
        params.put(paramOpBufferTemperatureMaxError, editTextBufferTemperatureMaxError.getText().toString());
        params.put(paramOpServoSpeed, editTextServoSpeed.getText().toString());
        return params;
    }

    private String encondeParamsToString() {
        String encondedParams = "";
        Map<String, String> params = encodeParams();
        for (Map.Entry<String, String> param : params.entrySet()) {
            encondedParams += param.getKey() + "=" + param.getValue() + "@";
        }
        encondedParams.substring(encondedParams.length() - 1);
        return encondedParams;
    }


    private void requestMethodSet() {
        requestMethodWithParams(methodSet, encodeParams());
    }

    private void requestMethodSync() {
        requestMethod(methodSync);
    }

    private void requestMethodCommunicationError() {
        requestMethod(methodCommunicationError);
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
        deviceInterface.sendMessage(finalRequest);
        showFlashMessage("Requesting " + methodName.get(method) + " method");
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
        menuItemsHide();
        firmwareVersion = 0;
    }

    private void onConnected(BluetoothSerialDevice connectedDevice) {
        // You are now connected to this device!
        // Here you may want to retain an instance to your device:
        deviceInterface = connectedDevice.toSimpleDeviceInterface();

        // Listen to bluetooth events
        deviceInterface.setListeners(this::onMessageReceived, this::onMessageSent, this::onError);
        requestMethod(methodWho);
        menuItemsShow();
    }

    private void onMessageSent(String message) {
        // We sent a message! Handle it here.
    }

    private void onMessageReceived(String message) {
        // We received a message! Handle it here.
        Map<String, String> response = new HashMap<String, String>();
        for (String value : message.split("@")) {
            String param1 = value.split("=")[0];
            String param2 = value.split("=")[1];
            response.put(param1, param2);
        }
        if (response.get("m") == null) {
            showFlashMessage("Bad response from device");
            return;
        }
        if (response.get("m").compareTo(methodWho) == 0) {
            if (response.get("fv") == null) {
                showFlashMessage("Device not recognized");
                return;
            }
            firmwareVersion = Integer.parseInt(response.get("fv"));
            if (firmwareVersion < minFirmwareVersionSupported) {
                showFlashMessage("Firmware version not supported\nPlease choose another device");
                selectBluetoothDevice();
                return;
            }
            requestMethod(methodSync);
            setMainView();
        }
        if (firmwareVersion == 0) {
            showFlashMessage("Firmware version is not set\nPlease start again");
            return;
        }
        if (firmwareVersion < minFirmwareVersionSupported) {
            showFlashMessage("Firmware version not supported\nPlease choose another device");
            selectBluetoothDevice();
            return;
        }
        if (response.get("m").compareTo(methodSet) == 0) {
            setMainView();
            processResponse(response);
            showFlashMessage("SET & SYNC done at " + getCurrentDate(null));
            return;
        }
        if (response.get("m").compareTo(methodSync) == 0) {
            setMainView();
            processResponse(response);
            showFlashMessage("SYNC done at " + getCurrentDate(null));
            return;
        }
        if (response.get("m").compareTo(methodAutomaticEnd) == 0) {
            processResponse(response);
            return;
        }
        if (response.get("m").compareTo(methodUnknown) == 0) {
            showFlashMessage("Unkown method requested");
            return;
        }
        if (response.get("m").compareTo(methodCommunicationError) == 0) {
            showFlashMessage("Communication error");
            return;
        }
    }

    private void processResponse(Map<String, String> response) {
        for (Map.Entry<String, String> entry : response.entrySet()) {
            switch (entry.getKey()) {
                case paramOpOnOff:
                    switchOnOff.setChecked("t".equals(entry.getValue()));
                    break;
                case paramOpPause:
                    switchPause.setChecked("t".equals(entry.getValue()));
                    break;
                case paramOpRamp:
                    switchRamp.setChecked("t".equals(entry.getValue()));
                    break;
                case paramOpAngle:
                    editTextAngle.setText(entry.getValue());
                    break;
                case paramOpWop:
                    editTextWop.setText(entry.getValue());
                    break;
                case paramOpRampStart:
                    editTextRampStart.setText(entry.getValue());
                    break;
                case paramOpRampEnd:
                    editTextRampEnd.setText(entry.getValue());
                    break;
                case paramOpRampDuration:
                    editTextRampDuration.setText(entry.getValue());
                    break;
                case paramOpAutoWop:
                    textViewAutoWop.setText(entry.getValue());
                    break;
                case paramOpBufferTemperature:
                    textViewBufferTemperature.setText(entry.getValue());
                    break;
                case paramOpHasRun:
                    long seconds = Integer.parseInt(entry.getValue());
                    String hms = String.format("%02d:%02d:%02d", TimeUnit.SECONDS.toHours(seconds),
                            TimeUnit.SECONDS.toMinutes(seconds) - TimeUnit.HOURS.toMinutes(TimeUnit.SECONDS.toHours(seconds)),
                            TimeUnit.SECONDS.toSeconds(seconds) - TimeUnit.MINUTES.toSeconds(TimeUnit.SECONDS.toMinutes(seconds)));
                    textViewHasRun.setText(hms);
                    break;
                case paramOpAutoEnd:
                    if ("t".equals(entry.getValue())) {
                        setContentView(R.layout.activity_automatic_end);
                        LinearLayout linearLayoutTapToContinue = (LinearLayout) findViewById(R.id.linearLayoutTapToContinue);
                        linearLayoutTapToContinue.setOnClickListener(new View.OnClickListener() {
                            public void onClick(View v) {
                                requestMethod(methodAutomaticEnd); // reset automatic end
                            }
                        });
                    }
                    break;
                case paramOpDisplayActive:
                    switchDisplayActive.setChecked("t".equals(entry.getValue()));
                    break;
                case paramOpDisplayBacklight:
                    switchDisplayBacklight.setChecked("t".equals(entry.getValue()));
                    break;
                case paramOpDisplayUpdateInterval:
                    editTextDisplayUpdateInterval.setText(entry.getValue());
                    break;
                case paramOpBufferTemperatureUpdateInterval:
                    editTextBufferTemperatureUpdateInterval.setText(entry.getValue());
                    break;
                case paramOpBufferTemperatureAutomaticControl:
                    switchBufferTemperatureAutomaticControl.setChecked("t".equals(entry.getValue()));
                    break;
                case paramOpBufferTemperatureSetpoint:
                    editTextBufferTemperatureSetpoint.setText(entry.getValue());
                    break;
                case paramOpBufferTemperatureMaxError:
                    editTextBufferTemperatureMaxError.setText(entry.getValue());
                    break;
                case paramOpServoSpeed:
                    editTextServoSpeed.setText(entry.getValue());
                    break;
                default:
                    break;
            }
        }
    }

    private void onError(Throwable error) {
        // Handle the error
        disconnectDevice();
    }

    // UI

    private void showHideUI() {
        // switch ramp
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
        // Buffer temperature
        if (switchBufferTemperatureAutomaticControl.isChecked()) {
            wrapBufferTemperatureUpdateSetpoint.setVisibility(View.VISIBLE);
        } else {
            wrapBufferTemperatureUpdateSetpoint.setVisibility(View.GONE);
        }
        // Display
        if (switchDisplayActive.isChecked()) {
            wrapDisplayUpdateInterval.setVisibility(View.VISIBLE);
            switchDisplayBacklight.setVisibility(View.VISIBLE);
        } else {
            wrapDisplayUpdateInterval.setVisibility(View.GONE);
            switchDisplayBacklight.setVisibility(View.GONE);
        }
    }

    private void selectBluetoothDevice() {
        setContentView(R.layout.activity_select_bt_device);
        LinearLayout linearLayoutSelectBluetoothTapToRetry = (LinearLayout) findViewById(R.id.linearLayoutSelectBluetoothTapToRetry);
        linearLayoutSelectBluetoothTapToRetry.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                showBtSelector();
            }
        });
        showBtSelector();
    }


    public void showBtSelector() {
        List<BluetoothDevice> pairedDevices = bluetoothManager.getPairedDevicesList();

        // If there are paired devices
        if (pairedDevices.size() > 0) {
            // Loop through paired devices
            ArrayList<ItemPickerDialogFragment.Item> pickerItems = new ArrayList<>();
            for (BluetoothDevice device : pairedDevices) {
                // Add the name and address to an array adapter to show in a ListView
                pickerItems.add(new ItemPickerDialogFragment.Item(device.getName(), device.getAddress()));
            }
            ItemPickerDialogFragment itemPickerDialogFragmentDialog = ItemPickerDialogFragment.newInstance(
                    "Select BT Device",
                    pickerItems,
                    -1
            );
            itemPickerDialogFragmentDialog.show(getFragmentManager(), "ItemPickerBtSelector");
        } else {
            showFlashMessage("No paired devices. Add one and retry");
        }
    }

    @Override
    public void onItemSelected(ItemPickerDialogFragment fragment, ItemPickerDialogFragment.Item item, int index) {
        if (fragment.getTag() == "ItemPickerBtSelector") {
            bluetoothName = item.getTitle();
            bluetoothAdress = item.getStringValue();
            // save to memory
            settingsEditor.putString(settingsBluetoothName, bluetoothName);
            settingsEditor.putString(settingsBluetoothAdress, bluetoothAdress);
            settingsEditor.commit();

            connectDevice();
        } else if (fragment.getTag() == "ItemPickerProgram") {
            loadProgram(item.getTitle());
        } else if (fragment.getTag() == "ItemPickerProgramOverwrite") {
            overwriteProgram(item.getTitle());
        } else if (fragment.getTag() == "ItemPickerProgramDelete") {
            deleteProgram(item.getTitle());
        }
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
        LinearLayout linearLayoutTurnBtOnTapToRetry = (LinearLayout) findViewById(R.id.linearLayoutTurnBtOnTapToRetry);
        linearLayoutTurnBtOnTapToRetry.setOnClickListener(new View.OnClickListener() {
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

        switchOnOff = (Switch) findViewById(R.id.switchRunning);
        switchPause = (Switch) findViewById(R.id.switchPause);
        switchRamp = (Switch) findViewById(R.id.switchRamp);
        switchDisplayActive = (Switch) findViewById(R.id.switchDisplayActive);
        switchDisplayBacklight = (Switch) findViewById(R.id.switchDisplayBacklight);
        switchBufferTemperatureAutomaticControl = (Switch) findViewById(R.id.switchBufferTemperatureAutomaticControl);

        wrapWop = (LinearLayout) findViewById(R.id.wrapWop);
        wrapRampStart = (LinearLayout) findViewById(R.id.wrapRampStart);
        wrapRampEnd = (LinearLayout) findViewById(R.id.wrapRampEnd);
        wrapRampDuration = (LinearLayout) findViewById(R.id.wrapRampDuration);
        wrapAutoWop = (LinearLayout) findViewById(R.id.wrapAutoWop);
        wrapBufferTemperatureUpdateSetpoint = (LinearLayout) findViewById(R.id.wrapBufferTemperatureUpdateSetpoint);
        wrapDisplayUpdateInterval = (LinearLayout) findViewById(R.id.wrapDisplayUpdateInterval);

        editTextAngle = findViewById(R.id.editTextAngle);
        editTextWop = findViewById(R.id.editTextWop);
        editTextRampStart = findViewById(R.id.editTextRampStart);
        editTextRampEnd = findViewById(R.id.editTextRampEnd);
        editTextRampDuration = findViewById(R.id.editTextRampDuration);
        editTextDisplayUpdateInterval = findViewById(R.id.editTextDisplayUpdateInterval);
        editTextBufferTemperatureUpdateInterval = findViewById(R.id.editTextBufferTemperatureUpdateInterval);
        editTextBufferTemperatureSetpoint = findViewById(R.id.editTextBufferTemperatureUpdateSetpoint);
        editTextBufferTemperatureMaxError = findViewById(R.id.editTextBufferTemperatureMaxError);
        editTextServoSpeed = findViewById(R.id.editTextServoSpeed);

        final Button buttonChangeDevice = findViewById(R.id.buttonChangeDevice);
        final Button buttonDisconnectDevice = findViewById(R.id.buttonDisconnectDevice);

        textViewAutoWop = findViewById(R.id.textViewAutoWop);
        textViewBufferTemperature = findViewById(R.id.textViewBufferTemperature);
        textViewHasRun = findViewById(R.id.textViewHasRun);
        textViewDeviceName = findViewById(R.id.textViewDeviceName);

        textViewDeviceName.setText("Device name: " + bluetoothName);

        CompoundButton.OnCheckedChangeListener coccl = new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                showHideUI();
            }
        };

        switchRamp.setOnCheckedChangeListener(coccl);
        switchDisplayActive.setOnCheckedChangeListener(coccl);
        switchBufferTemperatureAutomaticControl.setOnCheckedChangeListener(coccl);

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

        showHideUI();
    }

    private MenuItem menuItemSync;
    private MenuItem menuItemSet;
    private MenuItem menuItemSaveProgram;
    private MenuItem menuItemLoadProgram;
    private MenuItem menuItemDeleteProgram;

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        menuItemSync = menu.getItem(0);
        menuItemSet = menu.getItem(1);
        menuItemSaveProgram = menu.getItem(2);
        menuItemLoadProgram = menu.getItem(3);
        menuItemDeleteProgram = menu.getItem(4);
        menuItemsHide();
        return super.onCreateOptionsMenu(menu);
    }

    private void menuItemsHide() {
        menuItemSync.setVisible(false);
        menuItemSet.setVisible(false);
        menuItemSaveProgram.setVisible(false);
        menuItemLoadProgram.setVisible(false);
        menuItemDeleteProgram.setVisible(false);
    }

    private void menuItemsShow() {
        menuItemSync.setVisible(true);
        menuItemSet.setVisible(true);
        menuItemSaveProgram.setVisible(true);
        menuItemLoadProgram.setVisible(true);
        menuItemDeleteProgram.setVisible(true);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.sync:
                requestMethodSync();
                return true;

            case R.id.set:
                requestMethodSet();
                return true;

            case R.id.loadProgram:
                initLoadProgram();
                return true;

            case R.id.saveProgram:
                initSaveProgram();
                return true;

            case R.id.deleteProgram:
                deleteProgram();
                return true;

            case R.id.about:
                showAbout();
                return true;

            default:
                // If we got here, the user's action was not recognized.
                // Invoke the superclass to handle it.
                return super.onOptionsItemSelected(item);

        }
    }

    private boolean deleteProgram() {
        if (hasCustomPrograms()) {
            listPrograms("Select program to delete", "ItemPickerProgramDelete");
        } else {
            showFlashMessage("You have no custom programs");
        }
        return false;
    }

    private void deleteProgram(String programName) {
        Set<String> stringSetProgramas = settings.getStringSet(programasStringSetName, new HashSet<>());
        if (!stringSetProgramas.isEmpty()) {
            String toRemove = "";
            for (String programa : stringSetProgramas) {
                String[] programaPars = programa.split(programasStringSetSep);
                if (programaPars[0].compareTo(programName) == 0) {
                    toRemove = programa;
                    break;
                }
            }
            if (!toRemove.isEmpty()) {
                stringSetProgramas.remove(toRemove);
                if (stringSetProgramas.isEmpty()) {
                    settingsEditor.remove(programasStringSetName);
                    settingsEditor.commit();
                } else {
                    settingsEditor.putStringSet(programasStringSetName, stringSetProgramas);
                    settingsEditor.commit();
                }
                showFlashMessage("Deleted");
                return;
            }
        }
        showFlashMessage("Error while deleting");
    }


    List<Program> programs = new ArrayList<Program>();

    private boolean loadPrograms() {
        programs.clear();
        // defaults
        programs.add(new Program(
                "MidRange PFG Marker",
                "o=f@p=f@r=t@a=120@w=3@rs=1@re=25@rd=24@da=t@dui=3@bui=10@btac=t@bts=14@btm=3",
                "NEB",
                "N0342S",
                "15–291 kb",
                "0.5X TBE | 6 volts/cm",
                "https://international.neb.com/products/n0342-midrange-pfg-marker",
                true
        ));
        programs.add(new Program(
                "CHEF DNA Size Marker",
                "o=f@p=f@r=t@a=120@w=3@rs=26@re=228@rd=26@da=t@dui=3@bui=10@btac=t@bts=14@btm=3",
                "BIO-RAD",
                "1703605",
                "0.225–2.2 Mb",
                "0.5X TBE | 6 volts/cm",
                "https://www.researchgate.net/post/How_to_figure_out_a_PFGE_protocol",
                true
        ));
        // custom
        Set<String> stringSetProgramas = settings.getStringSet(programasStringSetName, new HashSet<>());
        if (!stringSetProgramas.isEmpty()) {
            for (String programa : stringSetProgramas) {
                String[] programaPars = programa.split(programasStringSetSep);
                programs.add(new Program(
                        programaPars[0],
                        programaPars[1],
                        programaPars[2]
                ));
            }
        }
        return false;
    }

    private void initLoadProgram() {
        listPrograms("Select program", "ItemPickerProgram");
    }

    private void listPrograms(String title, String tag) {
        Boolean justCustom = tag == "ItemPickerProgramDelete" ? true : false;
        loadPrograms();
        ArrayList<ItemPickerDialogFragment.Item> pickerItems = new ArrayList<>();
        for (Program program : programs) {
            // Add the name and address to an array adapter to show in a ListView
            if (justCustom && program.defaultProgram) continue;
            pickerItems.add(new ItemPickerDialogFragment.Item(program.name, program.name));
        }
        ItemPickerDialogFragment itemPickerDialogFragmentDialog = ItemPickerDialogFragment.newInstance(
                title,
                pickerItems,
                -1
        );
        itemPickerDialogFragmentDialog.show(getFragmentManager(), tag);
    }

    private boolean hasCustomPrograms() {
        loadPrograms();
        for (Program program : programs) {
            if (!program.defaultProgram) {
                return true;
            }
        }
        return false;
    }

    private void loadProgram(String programName) {
        Map<String, String> programConfig = new HashMap<String, String>();
        String programDetails = "";
        for (Program program : programs) {
            if (program.name == programName) {
                for (String value : program.programConfig.split("@")) {
                    String param1 = value.split("=")[0];
                    String param2 = value.split("=")[1];
                    programConfig.put(param1, param2);
                }
                programDetails = program.getProgramDetail();
                break;
            }
        }
        new AlertDialog.Builder(pfgeConfig.this)
                .setTitle("Load program")
                .setMessage(programDetails)
                .setPositiveButton("OK. LOAD", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int whichButton) {
                        processResponse(programConfig);
                        showFlashMessage("Program loaded");
                    }
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private boolean initSaveProgram() {

        // prepare input text view
        final EditText inputProgramName = new EditText(pfgeConfig.this);
        inputProgramName.setSingleLine();
        inputProgramName.setHint("Program name");
        final EditText inputProgramMessage = new EditText(pfgeConfig.this);
        inputProgramMessage.setSingleLine();
        inputProgramMessage.setHint("Program message");
        final LinearLayout container = new LinearLayout(pfgeConfig.this);
        container.setOrientation(LinearLayout.VERTICAL);
        final LinearLayout.LayoutParams params1 = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params1.setMargins(40, 0, 40, 0);
        inputProgramName.setLayoutParams(params1);
        final LinearLayout.LayoutParams params2 = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params2.setMargins(40, 40, 40, 0);
        inputProgramMessage.setLayoutParams(params2);
        container.addView(inputProgramName);
        container.addView(inputProgramMessage);

        // show dialog
        new AlertDialog.Builder(pfgeConfig.this)
                .setTitle("Save program")
                .setMessage("Do you want to create a new program or replace an existing one?")
                .setPositiveButton("NEW", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        new AlertDialog.Builder(pfgeConfig.this)
                                .setTitle("Save program")
                                .setMessage("Give a name to your program and set some custom message")
                                .setView(container)
                                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int whichButton) {
                                        String saveResultText = "Saved";
                                        Set<String> stringSetProgramas = settings.getStringSet(programasStringSetName, new HashSet<>());
                                        if (inputProgramName.getText().toString().isEmpty()) {
                                            saveResultText = "Error. Please provide a name for the program. Try again.";
                                        } else {
                                            String toAdd = inputProgramName.getText().toString() + programasStringSetSep + encondeParamsToString() + programasStringSetSep + inputProgramMessage.getText().toString();
                                            if (stringSetProgramas.contains(toAdd))
                                                stringSetProgramas.remove(toAdd);
                                            stringSetProgramas.add(toAdd);
                                            settingsEditor.putStringSet(programasStringSetName, stringSetProgramas);
                                            settingsEditor.commit();
                                        }
                                        showFlashMessage(saveResultText);
                                    }
                                })
                                .setNegativeButton(android.R.string.cancel, null)
                                .show();
                    }
                })
                .setNeutralButton("REPLACE", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        if (hasCustomPrograms()) {
                            listPrograms("Select program to overwrite", "ItemPickerProgramOverwrite");
                        } else {
                            showFlashMessage("No custom programs to replace");
                        }
                    }
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
        return false;
    }

    private void overwriteProgram(String programName) {
        Set<String> stringSetProgramas = settings.getStringSet(programasStringSetName, new HashSet<>());
        if (!stringSetProgramas.isEmpty()) {
            String toRemove = "";
            for (String programa : stringSetProgramas) {
                String[] programaPars = programa.split(programasStringSetSep);
                if (programaPars[0].compareTo(programName) == 0) {
                    toRemove = programa;
                    break;
                }
            }
            if (!toRemove.isEmpty()) {
                String[] newProgram = toRemove.split(programasStringSetSep);
                stringSetProgramas.remove(toRemove);
                String toAdd = newProgram[0] + programasStringSetSep + encondeParamsToString() + programasStringSetSep + newProgram[2];
                if (stringSetProgramas.contains(toAdd)) {
                    stringSetProgramas.remove(toAdd);
                }
                stringSetProgramas.add(toAdd);
                settingsEditor.putStringSet(programasStringSetName, stringSetProgramas);
                settingsEditor.commit();
                showFlashMessage("Saved");
                return;
            }
        }
        showFlashMessage("Error while overwriting");
    }

    private void showAbout() {
        new AlertDialog.Builder(pfgeConfig.this)
                .setTitle("About")
                .setMessage("App version: " + firmwareVersion +"Min firmware supported version: " + minFirmwareVersionSupported  + "\nSupport: https://gitlab.com/diegusleik/openpfge")
                .setNegativeButton(android.R.string.ok, null)
                .show();
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

    private void showFlashMessage(String flashMessage) {
        toast.setText(flashMessage);
        toast.show();
    }
}
