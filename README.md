# Zalejvatko - ESPHome external component

**Verze: v6**

## Changelog

- **v6**: pridana nova platforma `text_sensor` - "posledni zaliti" per kanal
  (citelny format `YYYY-MM-DD HH:MM:SS`, "nikdy" pokud se jeste nezalevalo).
  `example.yaml` rozsiren na kanaly 0-7 se vsemi peti entitami.
- **v5**: prehlednejsi logovani - hlaseni pri nastaveni rozvrhu (kazdy cas
  zvlast), povoleni/zakazani kanalu, zmene davky, zarazeni do fronty
  (naplanovane i rucni) a start/konec zalevani.
- **v4**: opraveno chybejici povinne pole `mode` u `text.zalejvatko` platformy
  (`'mode' is a required option`) - pridan vychozi `mode: text` primo do
  schema, takze uz to neni potreba psat rucne v YAML.
- **v3**: oprava relativnich importu v `switch.py`/`number.py`/`text.py`/`button.py`
  (`from .. import` -> `from . import`) - po zplosteni struktury ve v2 zustal
  spatny pocet tecek, coz zpusobovalo `ImportError: cannot import name
  'zalejvatko_ns' from 'esphome.components'`.
- **v2**: opravena struktura platform (`switch.py`, `number.py`, `text.py`,
  `button.py` prime v `components/zalejvatko/`, ne v podadresarich s
  `__init__.py` - odpovida standardni ESPHome konvenci).
- **v1**: prvni funkcni navrh (hub + 4 platformy + rozvrh podle NTP casu).

Prepis puvodniho Arduino/ESP32 firmware (`zalejvatko.ino` + `deska.h`, `eeprom.h`,
`webserver.h`, `wifi.h`, `serial.h`) do ESPHome external component.

## Co dela hub (`zalejvatko.h/.cpp`)

- Ovlada 16-kanalovy multiplexer pro ventily (adresace 4 piny + spolecny signalni pin)
  presne stejnym zpusobem jako puvodni `Mux` trida v `deska.h`.
- Ovlada cerpadlo (jeden GPIO pin).
- Pro kazdy kanal drzi konfiguraci (povoleno, davka v ml, rozvrh, cas posledniho
  zalevani) a uklada ji do flash (NVS) pres ESPHome preferences API - **nezavisle
  na Home Assistantovi**.
- Kazdou sekundu (`update()`) kontroluje rozvrh vsech kanalu podle skutecneho casu
  (NTP pres `time:` komponentu) a naplanovane zalevani rovnou zaradi do fronty.
- Fronta (`queue_`) zajistuje, ze se v jednu chvili zaliva vzdy jen jeden kanal
  (stejne jako v originale) - manualni pozadavky z tlacitka jdou do stejne fronty.

## Rozvrh - zmena oproti originalu

Puvodni firmware pocital periodu jako "kazdych N minut od posledniho zalevani".
Novy pristup: kazdy kanal ma textovy retezec s konkretnimi casy, napr.:

```
08:00,14:00,18:00
```

Kdyz aktualni cas prekroci nektery z techto casu a kanal se od te doby jeste
nezalel, spusti se zalevani. Format se parsuje v `parse_schedule_()` v C++,
takze validace probiha primo na zarizeni.

## Entity per kanal (ESPHome plaformy)

| Platforma | Ucel |
|---|---|
| `switch` | povolit/zakazat kanal |
| `number` | davka v ml (objem na jedno zalevani) |
| `text`   | rozvrh (CSV casu HH:MM) |
| `button` | okamzite zalit (mimo rozvrh) |
| `text_sensor` | posledni zaliti (jen ke cteni, format `YYYY-MM-DD HH:MM:SS`) |

Vsechny ctyri se registruji do hubu pres `zalejvatko_id: hub` + `channel: N`
(viz `example.yaml`).

## Co jsem VYNECHAL oproti originalu (a proc)

- **Vlastni EEPROM kod (`eeprom.h`)** - nahrazeno ESPHome preferences API
  (`global_preferences->make_preference<T>()`), ktere dela totez (uklada do
  flash/NVS), jen bez rucni serializace.
- **Vlastni WiFi/AP logika (`wifi.h`)** - ESPHome uz ma `wifi:` + `captive_portal:`
  + fallback AP presne pro tenhle pripad, viz `example.yaml`.
- **Vlastni AsyncWebServer HTML (`webserver.h`)** - nahrazeno nativni ESPHome
  `web_server:` komponentou. Bezi primo na ESP32, funguje i bez HA. Pokud ti
  bude prehlednost pro 16 kanalu vadit, muzeme pozdeji pridat vlastni lambda
  stranku.
- **Merici multiplexer** - v originale byl zadratovany, ale nikde se fakticky
  necetl (zadna funkce ho nepouzivala). Do noveho hubu jsem ho nedaval, ale
  pribyt to neni problem, kdyz mi reknes, jaky senzor za nim je (vlhkost pudy?).

## Co je potreba udelat/otestovat

1. **Zkontrolovat cislovani pinu** - prevzal jsem hodnoty z `deska.h`
   (`enable=17`, `signal=5` pro ventily, `pump=19`, adresni piny `4,0,2,15`).
   Zkontroluj, jestli sedi s tvym zapojenim.
2. **Kalibrace `ml_per_sec`** - v originale nevidim, ze by se pocitalo z
   prutoku, spis to bylo asi natvrdo/empiricky. Zmer skutecny prutok cerpadla
   a uprav v YAML.
3. **Prvni kompilace** - Python API ESPHome (`switch.switch_schema`,
   `number.number_schema` apod.) jsem psal podle aktualnich konvenci ESPHome
   external components, ale presne nazvy parametru se mezi verzemi ESPHome
   obcas meni. Doporucuju zkusit `esphome compile example.yaml` a poslat mi
   chybove hlasky, pokud nejaka platforma neprojde - opravime to na miste.
4. **Doplnit zbylych 15 kanalu** do YAML podle vzoru kanalu 0 (zakomentovany
   blok v `example.yaml`).
5. Zvazit, jestli chces `restore_value`/vychozi rozvrh nahrat rovnou pri
   prvnim behu (dnes je vychozi rozvrh prazdny -> kanal se nezaliva, dokud
   nekdo nenastavi cas přes HA/web UI).

## Soubory

```
components/zalejvatko/
  __init__.py           - hlavni hub, schema + codegen
  zalejvatko.h/.cpp     - C++ hub: HW driver, rozvrh, perzistence
  switch.py             - platforma "povoleno"
  number.py             - platforma "davka"
  text.py               - platforma "rozvrh"
  button.py             - platforma "zalit hned"
  text_sensor.py        - platforma "posledni zaliti"
example.yaml            - ukazkova konfigurace (kanaly 0-7, vzor pro dalsich 8)
```
