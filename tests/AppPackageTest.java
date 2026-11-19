import pl.psplink.AppPackage;
import java.io.*;
import java.nio.file.*;
import java.util.Arrays;

public final class AppPackageTest {
    private static void check(boolean value) {
        if (!value) throw new AssertionError();
    }
    private static void reject(byte[] bytes, File file) throws Exception {
        boolean failed = false;
        try { AppPackage.copy(new ByteArrayInputStream(bytes), file); }
        catch (IOException expected) { failed = true; }
        check(failed && !file.exists());
    }
    public static void main(String[] args) throws Exception {
        byte[] real = Files.readAllBytes(Paths.get(args[0]));
        File temp = File.createTempFile("pbp-test-", ".pbp");
        try {
            check(AppPackage.validFolder("Snake_2-OK"));
            for (String name : new String[]{"", "../x", "x/y", "x\\y", "PSPLink", "psplink", "a.b", "a b"})
                check(!AppPackage.validFolder(name));
            AppPackage.copy(new ByteArrayInputStream(real), temp);
            check(Arrays.equals(real, Files.readAllBytes(temp.toPath())));
            reject(new byte[39], temp);
            reject(new byte[40], temp);
            byte[] malformed = Arrays.copyOf(real, real.length);
            Arrays.fill(malformed, 8, 12, (byte)0xff);
            reject(malformed, temp);
            // Exercise the size boundary without allocating an oversized array.
            InputStream huge = new InputStream() {
                long remaining = AppPackage.LIMIT + 1;
                public int read() { return remaining-- > 0 ? 0 : -1; }
                public int read(byte[] b, int off, int len) {
                    if (remaining <= 0) return -1;
                    int n = (int)Math.min(remaining, len);
                    Arrays.fill(b, off, off+n, (byte)0);
                    remaining -= n;
                    return n;
                }
            };
            boolean failed = false;
            try { AppPackage.copy(huge, temp); }
            catch (IOException expected) { failed = true; }
            check(failed && !temp.exists());
            System.out.println("PASS: Android PBP staging, byte identity, names, header, size limit and cleanup.");
        } finally { temp.delete(); }
    }
}
