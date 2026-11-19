package pl.psplink;

import java.io.*;

/** Bounded disk staging; no full application is loaded into phone RAM. */
public final class AppPackage {
    public static final long LIMIT = 32L * 1024 * 1024;

    public static boolean validFolder(String name) {
        return name != null && name.matches("[A-Za-z0-9_-]{1,40}") &&
               !name.equalsIgnoreCase("PSPLink");
    }

    public static void validate(byte[] header, long size) throws IOException {
        if (size < 40 || size > LIMIT || header.length < 40 ||
            header[0] != 0 || header[1] != 'P' || header[2] != 'B' || header[3] != 'P')
            throw new IOException("Wybierz plik PBP o rozmiarze od 40 B do 32 MiB.");
        long previous = 40;
        for (int i = 8; i < 40; i += 4) {
            long offset = (header[i] & 255L) | ((header[i+1] & 255L) << 8) |
                          ((header[i+2] & 255L) << 16) | ((header[i+3] & 255L) << 24);
            if (offset < previous || offset > size)
                throw new IOException("Niepoprawny nagłówek PBP.");
            previous = offset;
        }
    }

    public static void copy(InputStream source, File target) throws IOException {
        try {
            long size = 0;
            try (OutputStream out = new FileOutputStream(target)) {
                byte[] buffer = new byte[16384];
                for (int n; (n = source.read(buffer)) != -1;) {
                    if (Thread.currentThread().isInterrupted()) throw new IOException("Anulowano.");
                    size += n;
                    if (size > LIMIT) throw new IOException("Limit aplikacji to 32 MiB.");
                    out.write(buffer, 0, n);
                }
            }
            byte[] header = new byte[40];
            try (DataInputStream in = new DataInputStream(new FileInputStream(target))) {
                in.readFully(header);
            }
            validate(header, size);
        } catch (IOException error) {
            target.delete();
            throw error;
        }
    }
}
