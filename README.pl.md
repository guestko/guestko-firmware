# Sterownik domofonu działający offline

[English](README.md) | [Polski](README.pl.md)

## Cel projektu

Projekt ma na celu sprawdzenie niedrogiego sposobu automatycznego otwierania drzwi z wykorzystaniem istniejącego analogowego domofonu w mieszkaniu. Po wywołaniu mieszkania przez gościa sterownik ma automatycznie zewrzeć styki przycisku otwierania drzwi — bez aplikacji w telefonie, Wi-Fi ani usług w chmurze.

Prototyp wykorzystuje **ESP32-C3 SuperMini**, układ wykrywania sygnału wywołania oraz przekaźnik podłączony do styków otwierania drzwi w unifonie. Docelowym środowiskiem jest **analogowa instalacja 4+n z unifonem typu Urmet CS1132 / 1131**. Połączenia i parametry sygnału należy sprawdzić w konkretnej instalacji; repozytorium nie potwierdza zgodności ze wszystkimi unifonami z tych rodzin.

Z myślą o klientach i osobach oceniających projekt repozytorium zawiera oprogramowanie odbiornika oraz wcześniejszy symulator sygnału do testów na stole. Dokumentuje kolejne iteracje prototypu sprzętowego, a nie produkt zweryfikowany we wszystkich instalacjach domofonowych. Obecne oprogramowanie otwiera drzwi po każdym sygnale wywołania spełniającym kryterium czasu trwania; nie rozpoznaje ani nie uwierzytelnia gościa.

## Historia prac i materiały projektowe

Początkowo używałem płytki ESP mini do symulowania sygnału wywołania domofonu podczas testów na stole. Okazało się, że nie jest to dobre zastępstwo rzeczywistej instalacji: rozwiązanie, które działało na stole, nie działało w warunkach rzeczywistych. Symulowany sygnał nie wystarczał do wiarygodnego potwierdzenia działania z prawdziwym domofonem.

W kolejnych iteracjach do testów używałem zakupionego generatora sygnałowego. Jest on widoczny na części zdjęć projektu. Pierwotny szkic symulatora pozostaje w repozytorium jako element historii prac.

[Zdjęcia i materiały związane z projektem (album udostępniony w iCloud)](https://www.icloud.com/sharedalbum/#D1gv3LkAJSC1a3V6JjvIoOCJMTwCAEQARogahy9vvzygcSkKMiSHnLvQaCZdfpO5kJrTrVS_D_g5ss)

## Zawartość repozytorium

| Plik | Przeznaczenie |
| --- | --- |
| [`intercom-esp32-receiver.ino`](intercom-esp32-receiver.ino) | Samodzielny odbiornik: mierzy czas trwania sygnału wywołania i steruje przekaźnikiem. |
| [`intercom-signal-simulator.ino`](intercom-signal-simulator.ino) | Wczesny symulator na ESP do testów na stole: generuje PWM o częstotliwości 800 Hz i wypełnieniu 50%. GPIO 5 generuje sygnał, gdy przycisk podłączony do GPIO 4 zwiera wejście do masy; GPIO 6 generuje sygnał stale. |
| [`1132_INSTRUKCJA.pdf`](1132_INSTRUKCJA.pdf), [`ds.pdf`](ds.pdf) | Dołączone materiały referencyjne dotyczące sprzętu w formacie PDF. |

## Sprzęt i połączenia odbiornika

Wymagane elementy:

- ESP32-C3 SuperMini.
- Moduł przekaźnika z wejściem sterującym zgodnym z logiką 3,3 V, aktywowanym stanem wysokim. Moduł należy zasilać napięciem zgodnym z jego specyfikacją.
- Detektor szczytowy: dioda 1N4007, kondensator 4,7 µF i rezystor rozładowujący 10 kΩ.
- Izolowany zasilacz sieciowy USB lub powerbank.

**Przed podłączeniem ESP32 do komputera przez USB odłącz układ od domofonu.** W tym prototypie masa ESP32 jest połączona z linią słuchawki unifonu (zacisk 1), a nie z domyślnie przyjętą wspólną masą instalacji. Przed podłączeniem ESP32 sprawdź połączenia instalacji i napięcie wyjściowe detektora; detektor nie stabilizuje ani nie ogranicza napięcia wejściowego.

### Wykrywanie wywołania

Detektor szczytowy prostuje i wygładza przebieg wywołania dla wejścia analogowego. W prototypie zakładano sygnał o częstotliwości około 800 Hz i napięciu ±3,5 V względem wybranego punktu odniesienia; parametry rzeczywistych instalacji mogą być inne.

- Domofon **CA (linia wywołania)** → anoda diody.
- Katoda diody → **GPIO 3**.
- Między węzłem GPIO 3 a GND ESP32 podłącz równolegle **kondensator 4,7 µF** i **rezystor 10 kΩ**. W przypadku kondensatora elektrolitycznego podłącz jego biegun dodatni do GPIO 3.
- Domofon **zacisk 1 (linia słuchawki)** → GND ESP32.

### Otwieranie drzwi

- ESP32 **GPIO 10** → IN przekaźnika.
- VCC przekaźnika → zasilanie zgodne ze specyfikacją modułu.
- GND przekaźnika → GND ESP32.
- Styki otwierania drzwi w unifonie, **zaciski 9 i 10 w opisanym połączeniu prototypu**, → COM i NO (styk normalnie otwarty) przekaźnika. Przed podłączeniem sprawdź te styki w konkretnym unifonie.

## Działanie odbiornika

Implementacja w `intercom-esp32-receiver.ino` używa następujących ustawień:

| Ustawienie | Wartość |
| --- | --- |
| Analogowe wejście sygnału | GPIO 3 |
| Wyjście przekaźnika aktywne stanem wysokim | GPIO 10 |
| Próg wykrycia sygnału | Odczyt ADC ściśle większy niż 1000 |
| Akceptowany czas wykrytego sygnału | 500–5000 ms, włącznie z wartościami granicznymi |
| Nominalny czas załączenia przekaźnika | 500 ms |
| Opóźnienie pętli | 10 ms |
| Prędkość portu szeregowego | 115200 |

1. Po uruchomieniu oprogramowanie ustawia wyjście przekaźnika w stan LOW. Nie inicjalizuje Wi-Fi ani połączenia z chmurą.
2. Odczyt ADC powyżej 1000 rozpoczyna pomiar czasu sygnału. Próg jest surową wartością ADC, a nie skalibrowanym pomiarem napięcia.
3. Gdy odczyt spadnie do 1000 lub poniżej, oprogramowanie sprawdza czas trwania sygnału. Załącza przekaźnik tylko wtedy, gdy czas ten wynosi od 500 do 5000 ms włącznie. **Otwarcie drzwi następuje po zakończeniu wykrytego sygnału wywołania.** Krótsze i dłuższe sygnały są ignorowane; sygnał stale aktywny nie powoduje otwarcia.
4. Wyjście przekaźnika wraca do stanu LOW, gdy pętla główna stwierdzi upływ ponad 500 ms. Czas załączenia wynosi więc około 500 ms i zależy od opóźnienia pętli oraz czasu przetwarzania.

Kontrola czasu dotyczy każdego nieprzerwanego odcinka sygnału powyżej progu. Nie ma osobnej programowej eliminacji drgań styków, tolerancji przerw w sygnale ani okresu blokady między zaakceptowanymi wywołaniami. Detektor szczytowy ma wygładzać przebieg, a krótki impuls przekaźnika ma ograniczać zakłócenia; działanie w warunkach rzeczywistych zależy od sprzętu i instalacji.

## Wgrywanie oprogramowania i uruchomienie

1. Przed podłączeniem ESP32 do komputera odłącz je od domofonu.
2. W Arduino IDE z zainstalowaną obsługą płytek ESP32 wybierz **ESP32C3 Dev Module** dla płytki odbiornika.
3. Umieść `intercom-esp32-receiver.ino` w osobnym folderze szkicu o nazwie `intercom-esp32-receiver`, a następnie otwórz i wgraj szkic. Symulator przechowuj w osobnym folderze szkicu: oba pliki definiują `setup()` i `loop()`, więc nie mogą być kompilowane razem.
4. Odłącz komputer, podłącz odbiornik do sprawdzonych zacisków domofonu i zasil go z izolowanego zasilacza lub powerbanku.
5. Sprawdź wykrywanie wywołania i otwieranie drzwi w rzeczywistej instalacji, w tym odrzucanie sygnałów zbyt krótkich i zbyt długich.

Symulator jest osobnym szkicem testowym i nie stanowi części odbiornika używanego w instalacji. Przypisanie jego GPIO i konfigurację PWM zdefiniowano w `intercom-signal-simulator.ino`; wyniki testów na stole nie potwierdzają zgodności z rzeczywistym domofonem.

## Licencja

Repozytorium nie zawiera obecnie pliku licencji.
