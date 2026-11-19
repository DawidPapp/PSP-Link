Testy na komputerze, uruchamiane z katalogu Zrodla:

python3 tests/test_protocol.py

Wymaga kompilatora C (cc), Pythona 3 i pliku psp/EBOOT.PBP.
Test kompiluje rzeczywiste funkcje protokołu z psp/main.c, podmieniając
wyłącznie API systemu PSP na funkcje POSIX. Korzysta z katalogu tymczasowego.

Test kodu przygotowania plików na Androidzie (JDK):
mkdir -p test-build
javac -d test-build android/src/pl/psplink/AppPackage.java tests/AppPackageTest.java
java -cp test-build AppPackageTest psp/EBOOT.PBP

Te testy nie zastępują uruchomienia APK ani testu Wi-Fi na prawdziwym PSP.

Test logiki systemowego dialogu z symulowanym API PSP:
python3 tests/test_wifi.py
Nie testuje grafiki ani rzeczywistego uzyskiwania adresu IP.
