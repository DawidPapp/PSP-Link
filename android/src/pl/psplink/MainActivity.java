package pl.psplink;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.concurrent.*;

public final class MainActivity extends Activity {
    private static final int LIMIT = 1024 * 1024, PICK = 1, SAVE = 2, PICK_APP = 3;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private XmbView xmb;
    private String address = "", pin = "", uploadName = "test.txt", pendingDownload;
    private String appFolder = "MojaAplikacja";
    private boolean busy;
    private volatile boolean destroyed;
    private volatile Socket active;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(android.graphics.Color.rgb(8, 21, 47));
        getWindow().setNavigationBarColor(android.graphics.Color.rgb(12, 41, 67));
        address = getPreferences(0).getString("host", "");
        if (state != null) {
            pendingDownload = state.getString("pendingDownload");
            pin = state.getString("pin", "");
            uploadName = state.getString("uploadName", "test.txt");
            appFolder = state.getString("appFolder", "MojaAplikacja");
        }
        xmb = new XmbView(this, new XmbView.Actions() {
            public void configure() { connectionDialog(); }
            public void refresh() { MainActivity.this.refresh(); }
            public void upload() { uploadDialog(); }
            public void install() { appDialog(); }
            public void download(String name) {
                if (busy) return;
                pendingDownload = name;
                picker(Intent.ACTION_CREATE_DOCUMENT, SAVE, name);
            }
        });
        setContentView(xmb);
        xmb.setHost(address);
        if (state != null) xmb.select(state.getInt("category", 0), false);
    }

    @Override protected void onSaveInstanceState(Bundle state) {
        super.onSaveInstanceState(state);
        state.putString("pendingDownload", pendingDownload);
        state.putString("pin", pin);
        state.putString("uploadName", uploadName);
        state.putString("appFolder", appFolder);
        state.putInt("category", xmb.category());
    }

    @Override protected void onResume() {
        super.onResume();
        if (xmb != null) xmb.animateWaves(true);
    }

    @Override protected void onPause() {
        if (xmb != null) xmb.animateWaves(false);
        super.onPause();
    }

    @Override public void onBackPressed() {
        if (xmb.category() != 0) xmb.select(0, true);
        else super.onBackPressed();
    }

    private LinearLayout fields() {
        LinearLayout fields = new LinearLayout(this);
        fields.setOrientation(LinearLayout.VERTICAL);
        int pad = (int)(24 * getResources().getDisplayMetrics().density);
        fields.setPadding(pad, pad / 2, pad, pad / 2);
        return fields;
    }

    private EditText field(LinearLayout fields, String caption, String value, int type) {
        TextView label = new TextView(this);
        label.setText(caption);
        fields.addView(label);
        EditText edit = new EditText(this);
        edit.setSingleLine(true);
        edit.setInputType(type);
        edit.setText(value);
        fields.addView(edit);
        return edit;
    }

    private void connectionDialog() {
        if (busy) return;
        LinearLayout fields = fields();
        EditText ipField = field(fields, "Adres IP z PSP (bez portu)", address,
                                InputType.TYPE_CLASS_PHONE);
        EditText pinField = field(fields, "Sześciocyfrowy PIN z PSP", pin,
                                 InputType.TYPE_CLASS_NUMBER);
        pinField.setFilters(new android.text.InputFilter[]{new android.text.InputFilter.LengthFilter(6)});
        android.app.AlertDialog dialog = new android.app.AlertDialog.Builder(this)
            .setTitle("Połączenie z PSP").setView(fields)
            .setNegativeButton("Anuluj", null)
            .setPositiveButton("Zapisz", null).create();
        dialog.setOnShowListener(v -> dialog.getButton(android.app.AlertDialog.BUTTON_POSITIVE)
            .setOnClickListener(button -> {
                String host = ipField.getText().toString().trim();
                String code = pinField.getText().toString().trim();
                if (!validHost(host)) {
                    ipField.setError("Wpisz adres IPv4 wyświetlony na PSP.");
                    return;
                }
                if (!code.matches("\\d{6}")) {
                    pinField.setError("PIN powinien mieć sześć cyfr.");
                    return;
                }
                address = host;
                pin = code;
                getPreferences(0).edit().putString("host", address).apply();
                xmb.setHost(address);
                xmb.setStatus("Dane zapisane. Wybierz Połącz i odczytaj pliki.");
                dialog.dismiss();
            }));
        dialog.show();
    }

    private void uploadDialog() {
        if (busy) return;
        if (!validHost(address) || !pin.matches("\\d{6}")) {
            xmb.setStatus("Najpierw wpisz adres i PIN w kategorii Połączenie.");
            connectionDialog();
            return;
        }
        LinearLayout fields = fields();
        EditText name = field(fields, "Nazwa pliku na PSP (.txt)", uploadName,
                              InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        android.app.AlertDialog dialog = new android.app.AlertDialog.Builder(this)
            .setTitle("Wyślij na Memory Stick").setView(fields)
            .setNegativeButton("Anuluj", null)
            .setPositiveButton("Wybierz plik", null).create();
        dialog.setOnShowListener(v -> dialog.getButton(android.app.AlertDialog.BUTTON_POSITIVE)
            .setOnClickListener(button -> {
                String value = name.getText().toString().trim();
                if (!validName(value)) {
                    name.setError("Nazwa do 40 znaków, bez spacji, z końcówką .txt.");
                    return;
                }
                uploadName = value;
                dialog.dismiss();
                picker(Intent.ACTION_OPEN_DOCUMENT, PICK, null);
            }));
        dialog.show();
    }

    private void appDialog() {
        if (busy) return;
        if (!validHost(address) || !pin.matches("\\d{6}")) {
            connectionDialog();
            return;
        }
        LinearLayout fields = fields();
        TextView info = new TextView(this);
        info.setText("Wybierz pojedynczy EBOOT.PBP (do 32 MiB). Zapis w PSP/GAME/Nazwa/EBOOT.PBP. " +
            "Aplikacja musi być zgodna z systemem PSP. Dodatkowe pliki i ZIP nie są jeszcze obsługiwane.");
        fields.addView(info);
        EditText name = field(fields, "Nazwa nowego folderu w PSP/GAME", appFolder,
            InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        android.app.AlertDialog dialog = new android.app.AlertDialog.Builder(this)
            .setTitle("Wgraj aplikację na PSP").setView(fields)
            .setNegativeButton("Anuluj", null).setPositiveButton("Wybierz EBOOT.PBP", null).create();
        dialog.setOnShowListener(v -> dialog.getButton(android.app.AlertDialog.BUTTON_POSITIVE)
            .setOnClickListener(button -> {
                String folder = name.getText().toString().trim();
                if (!AppPackage.validFolder(folder)) {
                    name.setError("1–40 znaków: litery ASCII, cyfry, _ lub -. PSPLink jest zarezerwowane.");
                    return;
                }
                appFolder = folder;
                dialog.dismiss();
                picker(Intent.ACTION_OPEN_DOCUMENT, PICK_APP, null);
            }));
        dialog.show();
    }

    private void uploadApp(Uri uri) {
        if (busy) return;
        final String folder = appFolder;
        if (!AppPackage.validFolder(folder)) return;
        setBusy(true);
        xmb.setStatus("Sprawdzanie EBOOT.PBP…");
        worker.execute(() -> {
            File temp = null;
            try {
                temp = File.createTempFile("psplink-", ".pbp", getCacheDir());
                try (InputStream source = getContentResolver().openInputStream(uri)) {
                    if (source == null) throw new IOException("Brak dostępu do pliku.");
                    AppPackage.copy(source, temp);
                }
                final File prepared = temp;
                runOnUiThread(() -> {
                    if (destroyed) { prepared.delete(); return; }
                    setBusy(false);
                    connect((in, out, code) -> {
                        long size = prepared.length();
                        send(out, code + " APP " + folder + " " + size + "\n");
                        expect(in, "GO");
                        try (InputStream source = new FileInputStream(prepared)) {
                            byte[] buffer = new byte[16384];
                            long count = 0, last = 0;
                            for (int n; (n = source.read(buffer)) != -1;) {
                                if (destroyed || Thread.currentThread().isInterrupted())
                                    throw new IOException("Transfer przerwany.");
                                out.write(buffer, 0, n);
                                count += n;
                                long now = android.os.SystemClock.uptimeMillis();
                                if (now - last > 500 || count == size) {
                                    message("Wgrywanie " + folder + ": " + (count * 100 / size) + "%");
                                    last = now;
                                }
                            }
                            out.flush();
                        }
                        expect(in, "OK");
                        transfer("Wgrano " + folder, "Zapisano PSP/GAME/" + folder +
                            "/EBOOT.PBP. Zamknij PSP Link i otwórz Gra → Memory Stick.");
                    }, () -> prepared.delete());
                });
            } catch (Exception error) {
                if (temp != null) temp.delete();
                runOnUiThread(() -> {
                    if (!destroyed) {
                        setBusy(false);
                        xmb.setStatus("Błąd: " + error.getMessage());
                    }
                });
            }
        });
    }

    private static boolean validHost(String host) {
        if (!host.matches("(\\d{1,3}\\.){3}\\d{1,3}")) return false;
        for (String part : host.split("\\.")) {
            if (Integer.parseInt(part) > 255) return false;
        }
        return true;
    }

    private void picker(String action, int code, String filename) {
        Intent intent = new Intent(action).addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(code == SAVE ? "text/plain" : "*/*");
        if (filename != null) intent.putExtra(Intent.EXTRA_TITLE, filename);
        try {
            startActivityForResult(intent, code);
        } catch (android.content.ActivityNotFoundException error) {
            xmb.setStatus("Brak aplikacji obsługującej wybór pliku.");
        }
    }

    private static boolean validName(String name) {
        return name != null && name.length() <= 40 &&
               name.matches("[A-Za-z0-9_-][A-Za-z0-9_.-]*\\.txt");
    }

    @FunctionalInterface private interface Exchange {
        void run(DataInputStream in, OutputStream out, String code) throws Exception;
    }

    private void setBusy(boolean value) {
        busy = value;
        xmb.setBusy(value);
    }

    private void connect(Exchange action) { connect(action, null); }

    private void connect(Exchange action, Runnable cleanup) {
        if (busy || destroyed) { if (cleanup != null) cleanup.run(); return; }
        String host = address;
        String code = pin;
        if (!host.matches("(\\d{1,3}\\.){3}\\d{1,3}") || !code.matches("\\d{6}")) {
            if (cleanup != null) cleanup.run();
            xmb.setStatus("Wpisz adres IP i sześciocyfrowy PIN z PSP.");
            return;
        }
        for (String part : host.split("\\.")) {
            if (Integer.parseInt(part) > 255) {
                if (cleanup != null) cleanup.run();
                xmb.setStatus("Niepoprawny adres IP.");
                return;
            }
        }
        getPreferences(0).edit().putString("host", host).apply();
        setBusy(true);
        xmb.setStatus("Łączenie…");
        worker.execute(() -> {
            try (Socket socket = new Socket()) {
                active = socket;
                if (destroyed) return;
                socket.connect(new InetSocketAddress(host, 4646), 5000);
                socket.setSoTimeout(12000);
                action.run(new DataInputStream(new BufferedInputStream(socket.getInputStream())),
                           socket.getOutputStream(), code);
            } catch (Exception error) {
                message("Błąd: " + error.getMessage());
            } finally {
                if (cleanup != null) cleanup.run();
                active = null;
                runOnUiThread(() -> { if (!destroyed) setBusy(false); });
            }
        });
    }

    private void message(String text) {
        runOnUiThread(() -> { if (!destroyed) xmb.setStatus(text); });
    }

    private void transfer(String entry, String message) {
        runOnUiThread(() -> {
            if (!destroyed) {
                xmb.addTransfer(entry);
                xmb.setStatus(message);
            }
        });
    }

    private static void send(OutputStream out, String text) throws IOException {
        out.write(text.getBytes(StandardCharsets.US_ASCII));
        out.flush();
    }

    private static String line(DataInputStream in) throws IOException {
        StringBuilder text = new StringBuilder();
        while (text.length() < 128) {
            int c = in.readUnsignedByte();
            if (c == '\n') return text.toString();
            if (c < 32 || c > 126) throw new IOException("Niepoprawna odpowiedź PSP.");
            text.append((char)c);
        }
        throw new IOException("Odpowiedź PSP jest za długa.");
    }

    private static void expect(DataInputStream in, String expected) throws IOException {
        String actual = line(in);
        if (!actual.equals(expected)) throw new IOException(actual);
    }

    private void refresh() {
        connect((in, out, code) -> {
            send(out, code + " LIST - 0\n");
            expect(in, "OK");
            ArrayList<String> names = new ArrayList<>();
            for (;;) {
                String name = line(in);
                if (name.equals(".")) break;
                if (!validName(name) || names.size() >= 10000)
                    throw new IOException("Niepoprawna lub zbyt długa lista plików.");
                names.add(name);
            }
            java.util.Collections.sort(names);
            runOnUiThread(() -> {
                if (destroyed) return;
                xmb.setFiles(names);
                xmb.select(1, true);
                xmb.setStatus("Pliki w folderze Transfer: " + names.size());
            });
        });
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        if (request == PICK_APP) {
            uploadApp(uri);
        } else if (request == PICK) {
            String name = uploadName;
            if (!validName(name)) {
                xmb.setStatus("Nazwa do 40 znaków: litery, cyfry, _ lub - i końcówka .txt.");
                return;
            }
            // Read the selected document before opening the PSP connection.
            setBusy(true);
            xmb.setStatus("Odczyt pliku…");
            worker.execute(() -> {
                try {
                    byte[] bytes = readUpload(uri);
                    runOnUiThread(() -> {
                        if (destroyed) return;
                        setBusy(false);
                        connect((in, out, code) -> {
                            send(out, code + " PUT " + name + " " + bytes.length + "\n");
                            expect(in, "GO");
                            out.write(bytes);
                            out.flush();
                            expect(in, "OK");
                            transfer("Wysłano " + name, "Wysłano " + name + ". Wybierz Odśwież.");
                        });
                    });
                } catch (Exception error) {
                    runOnUiThread(() -> {
                        if (!destroyed) {
                            setBusy(false);
                            xmb.setStatus("Błąd odczytu: " + error.getMessage());
                        }
                    });
                }
            });
        } else if (request == SAVE && validName(pendingDownload)) {
            String name = pendingDownload;
            pendingDownload = null;
            connect((in, out, code) -> {
                send(out, code + " GET " + name + " 0\n");
                String header = line(in);
                if (!header.matches("OK \\d{1,7}")) throw new IOException(header);
                int size = Integer.parseInt(header.substring(3));
                if (size > LIMIT) throw new IOException("Plik jest za duży.");
                byte[] bytes = new byte[size];
                in.readFully(bytes);
                try (OutputStream target = getContentResolver().openOutputStream(uri, "wt")) {
                    if (target == null) throw new IOException("Brak dostępu do pliku.");
                    target.write(bytes);
                }
                transfer("Pobrano " + name, "Pobrano " + name + " (" + size + " B).");
            });
        }
    }

    private byte[] readUpload(Uri uri) throws IOException {
        try (InputStream source = getContentResolver().openInputStream(uri)) {
            if (source == null) throw new IOException("Brak dostępu do pliku.");
            ByteArrayOutputStream result = new ByteArrayOutputStream();
            byte[] buffer = new byte[4096];
            for (int count; (count = source.read(buffer)) != -1;) {
                if (result.size() + count > LIMIT) throw new IOException("Limit to 1 MiB.");
                result.write(buffer, 0, count);
            }
            return result.toByteArray();
        }
    }

    @Override protected void onDestroy() {
        destroyed = true;
        Socket socket = active;
        if (socket != null) {
            try { socket.close(); } catch (IOException ignored) { }
        }
        worker.shutdownNow();
        super.onDestroy();
    }
}
