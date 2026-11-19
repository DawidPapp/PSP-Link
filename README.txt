PSP LINK 0.5.0 — ODTWORZONY INTERFEJS + AKTUALIZACJE HTTPS

Instalacja teraz, bez repozytorium
1. Pobierz sam plik EBOOT.PBP na telefon.
2. W działającym PSP Link 0.3.2/0.4 wybierz na Androidzie Wgraj aplikację.
3. Podaj NOWY folder PSPLink050 i wyślij EBOOT.PBP.
4. Zamknij dotychczasową wersję i uruchom PSP Link 0.5.0 w Gra > Memory Stick.
5. W nowej zakładce Aktualizacje wybierz Test HTTPS z GitHub.
   PSP samo wybierze sieć przez systemowe okno, jeśli nie jest połączone.
   Sukces ma komunikat: GitHub HTTPS działa. Certyfikat sprawdzony.
Android 0.3 pozostaje zgodny; nie potrzebuje aktualizacji.

Wygląd
Odtworzono układ z wersji 0.4 na podstawie wcześniejszej implementacji:
niebieski gradient, trzy warstwy animowanej fali, liniowe ikony,
wygładzone napisy z polskimi znakami, podświetlenie i przejścia.
Dodano czwartą kategorię Aktualizacje. Rozmiary dostosowano do 480x272.
Krzyżak wybiera kategorię i opcję; X potwierdza; O wychodzi/anuluje pobieranie.
Wszystkie grafiki, czcionka i certyfikaty są zawarte w EBOOT.PBP.

Skąd aktualizacje
Repozytorium DawidPapp/PSP-Link jest wbudowane w EBOOT.PBP.
Nie trzeba wysyłać pliku repozytorium.txt. Starszy plik jest ignorowany.
Wybierz Aktualizacje > Sprawdź aktualizacje. Jeśli dostępna jest wyższa
wersja, naciśnij X ponownie, aby pobrać ją do nowego folderu.
Numer wcześniejszego wydania testowego 0.5.1 zmieniono na 0.5.0.
Jeśli masz już 0.5.1, wgraj tę poprawioną 0.5.0 przez Androida/USB.
Aktualizator nie instaluje niższych numerów wersji.

Przygotowanie wydań GitHub
Kod źródłowy znajduje się w folderze Zrodla w archiwum projektu.
Przy następnym wydaniu zmień CURRENT_VERSION w psp/update_format.h
na wyższą wersję (np. 0.5.1), skompiluj EBOOT.PBP, a następnie uruchom:
  python3 tools/make_release.py
Skrypt tworzy release/EBOOT.PBP i release/psplink-update.txt.
Utwórz GitHub Release z tagiem vNUMER, np. v0.5.1, i dołącz OBA pliki.
Opublikuj jako zwykłe wydanie (nie draft ani prerelease) i ustaw jako Latest.
Numer w tagu, opisie aktualizacji i kompilacji musi się zgadzać.
Wydanie 0.5.0 nie jest aktualizacją dla aplikacji 0.5.0: potrzebny wyższy numer.
Wydania publikuje workflow GitHub Actions z tagiem odpowiadającym CURRENT_VERSION.

Kontrola pobierania
Połączenia i przekierowania są ograniczone do HTTPS. Sprawdzany jest certyfikat
serwera i nazwa hosta; wymagany co najmniej TLS 1.2. Ustaw poprawną datę PSP.
Wbudowano zestaw urzędów certyfikacji z https://curl.se/ca/cacert.pem.
Biblioteka curl PSPDEV używa mbedTLS. Nie korzystamy ze starego systemowego SSL.
Opis wydania pochodzi z wybranego repozytorium przez zweryfikowany HTTPS.
SHA-256 wykrywa niezgodność EBOOT z tym opisem. To NIE jest osobny podpis autora;
ufasz właścicielowi skonfigurowanego repozytorium i poprawności HTTPS.
Limit EBOOT to 32 MiB. Są sprawdzane rozmiar, nagłówek i SHA-256 przed oraz po
zmianie nazwy LINK.TMP na EBOOT.PBP wewnątrz folderu nowej wersji.
Istniejące foldery nie są nadpisywane. Nieudane pobranie jest sprzątane.
Przerwanie zasilania może pozostawić folder i LINK.TMP — usuń je przez USB.

Diagnostyka
Test HTTPS nie pobiera EBOOT i nie wymaga repozytorium.
Przy błędzie prześlij pełny komunikat „HTTPS: błąd N, HTTP M”.
HTTP 404 zwykle oznacza brak repozytorium, Latest lub właściwego załącznika.
Kod curl 60: sprawdź datę PSP; możliwy problem z certyfikatem.
Kod 35: błąd negocjacji TLS; kod 6: DNS; kod 7: połączenie; kod 28: limit czasu.
Testy na komputerze NIE dowodzą działania TLS na PSP-3004 OFW 6.20.
Potrzebny jest test na Twojej konsoli. Nie omijaj sprawdzania certyfikatów.

Nadal dostępne
Transfer TXT do 1 MiB i pojedynczych PBP do 32 MiB z Androida,
ze sprawdzaniem końcowego miejsca zapisu. ZIP-y i aplikacje wymagające
plików obok EBOOT nie są automatycznie instalowane. PBP musi być zgodny z OFW.

Budowanie
PSPDEV v20261101 (pakiet debian), biblioteki curl 7.64.1 i mbedTLS 2.28.10.
Ustaw PSPDEV, dodaj $PSPDEV/bin do PATH, następnie make -C psp.
Nagłówki font.h i ca.h są już wygenerowane; Python/Pillow nie są wymagane
przy zwykłej kompilacji. Ich odtworzenie: python3 psp/ui/build_assets.py
(skrypt używa Open Sans Light z /usr/share/fonts/truetype/open-sans).
Wszystkie zależności i źródła są odtwarzalne; nie potrzebujesz prywatnego klucza PSP.

Testy
python3 tests/test_protocol.py
python3 tests/test_wifi.py
cc -Wall -Wextra -Werror tests/test_update_format.c -o test-format && ./test-format
python3 tests/test_install.py  (wymaga nagłówków OpenSSL/libcrypto)
Testy instalacji symulują HTTPS i API PSP, sprawdzając zapis, przerwania,
niezgodny hash, pozorną zmianę nazwy i brak nadpisania istniejącej instalacji.
Renderowanie interfejsu sprawdzono na komputerze z ASan/UBSan.
Pełnego cyklu GitHub -> fizyczne PSP jeszcze nie sprawdzono.
