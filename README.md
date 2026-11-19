# PSP Link

Przesyłanie plików między PSP i Androidem przez Wi-Fi oraz eksperymentalne pobieranie aktualizacji bezpośrednio z GitHub Releases.

**Sprzęt docelowy:** PSP-3004, OFW 6.20. Kompilacja i testy na komputerze nie zastępują testu HTTPS na konsoli.

## Instalacja i test aktualizacji

1. Pobierz `EBOOT.PBP` z [Releases](https://github.com/DawidPapp/PSP-Link/releases/latest).
2. Prześlij go działającym PSP Link do **nowego** folderu, np. `PSPLink050`, albo skopiuj przez USB do `PSP/GAME/PSPLink050/EBOOT.PBP`.
3. Uruchom nową wersję w **Gra → Memory Stick**.
4. Wybierz **Aktualizacje → Test HTTPS z GitHub**.

## Repozytorium wbudowane

Wersja **0.5.0** ma fabrycznie ustawione **DawidPapp/PSP-Link**. Wybierz **Aktualizacje → Sprawdź aktualizacje**; nie trzeba przesyłać `repozytorium.txt`. Starszy plik konfiguracyjny jest ignorowany.

Numer wcześniejszego testowego wydania 0.5.1 został skorygowany na 0.5.0 na prośbę autora repozytorium. Zmieniony EBOOT trzeba wgrać ręcznie przez Androida lub USB — aktualizator nie instaluje niższych wersji.

Gdy pojawi się wydanie nowsze od 0.5.0, po wykryciu naciśnij X, aby je pobrać. Nowy EBOOT jest zapisywany obok dotychczasowej instalacji, bez zmiany firmware PSP.

## Funkcje

- Menu inspirowane wersją Androidową, dostosowane do ekranu 480×272.
- Systemowe okno wyboru zapisanej sieci Wi-Fi.
- Przesyłanie plików TXT do 1 MiB i pojedynczych aplikacji PBP do 32 MiB.
- GitHub HTTPS z kontrolą certyfikatu i nazwy hosta; sprawdzanie rozmiaru, PBP i SHA-256 aktualizacji.
- Brak nadpisywania istniejących folderów aplikacji.

Suma SHA-256 pochodzi z opisu wydania pobranego przez HTTPS. Nie jest to osobny podpis autora. Użytkownik ufa właścicielowi skonfigurowanego repozytorium.

## Budowanie

PSPDEV **v20261101**, biblioteki curl i mbedTLS dostarczone z SDK. Na Debianie zainstaluj Python 3, Pillow oraz `fonts-open-sans`.

```sh
python3 psp/ui/build_assets.py
export PSPDEV=/pelna/sciezka/do/pspdev
export PATH="$PSPDEV/bin:$PATH"
make -C psp
python3 tools/make_release.py
```

`psp/ui/cacert.pem` zawiera publiczne certyfikaty CA z curl.se; nie zawiera kluczy prywatnych. Generator osadza je i czcionkę w EBOOT, aby nie wymagał dodatkowych plików.

Workflow buduje PSP, uruchamia testy i publikuje `EBOOT.PBP` wraz z `psplink-update.txt`. Przed kolejnym wydaniem zwiększ `CURRENT_VERSION` w `psp/update_format.h`. Opublikowane wersje nie są nadpisywane.

## Testy

```sh
python3 tests/test_protocol.py
python3 tests/test_wifi.py
cc -Wall -Wextra -Werror tests/test_update_format.c -o test-format
./test-format
python3 tests/test_install.py
```

Testy instalacji używają symulowanego pobierania i API PSP. Testy na fizycznym PSP pozostają konieczne.

Pełna instrukcja: [README.txt](README.txt). Licencje zależności: [licenses](licenses/).
