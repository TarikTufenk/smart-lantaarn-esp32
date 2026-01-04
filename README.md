# Smart Lantaarn — ESP32

**Onderdelen**
- **Lantaarnpaal**: bestuurt de verlichting en communiceert via UDP‑discovery + WebSocket. Code: [lantaarnpaal/lantaarnpaal.ino](lantaarnpaal/lantaarnpaal.ino)
- **Sensor**: leest een PIR‑bewegingssensor uit en verstuurt activatie‑events via UDP‑discovery + WebSocket. Code: [lantaarnpaal_sensor/lantaarnpaal_sensor.ino](lantaarnpaal_sensor/lantaarnpaal_sensor.ino)
**Onderdelen**
- **Lantaarnpaal**: bestuurt de verlichting/driver en handelt logica/communicatie af. Code: [lantaarnpaal/lantaarnpaal.ino](lantaarnpaal/lantaarnpaal.ino)
## Doel
Een reproduceerbare basis voor een “smart lantaarnpaal” op ESP32, waarbij een sensormodule (PIR) activatie‑events uitstuurt en een lampmodule daarop reageert. Communicatie verloopt via Wi‑Fi, UDP‑discovery en WebSocket met JSON‑berichten (zie “Protocol”).

## Hardware en pinnen
- Lantaarnpaal: één NeoPixel LED op `LED_PIN=5` (`LED_COUNT=1`).
- Sensor: PIR op `PIR_PIN=4`. Status‑LED gebruikt `LED_BUILTIN`.
## Projectstructuur
- [lantaarnpaal/lantaarnpaal.ino](lantaarnpaal/lantaarnpaal.ino)
## Benodigdheden
- ESP32‑ontwikkelbord(en) (bijv. DevKitC of WROOM‑gebaseerd)
- Arduino IDE (2.x aanbevolen) of PlatformIO
- USB‑kabels voor programmeren
- PIR‑sensor (sensor‑module) en een NeoPixel (lamp‑module)
- Externe server die UDP‑discovery en WebSocket afhandelt (zie “Externe server”)
## Benodigdheden
## Afhankelijkheden (Arduino libraries)
- ArduinoJson
- arduinoWebSockets (WebSocketsClient)
- Adafruit NeoPixel (alleen voor lantaarnpaal)
- Preferences (ESP32 NVS, onderdeel van ESP32 core)
- ESP32‑ontwikkelbord(en) (bijv. DevKitC of WROOM‑gebaseerd)
## Standaardconfiguratie (uit de code)
- Wi‑Fi SSID: `lampding`
- Wi‑Fi wachtwoord: `lamp1234`
- UDP discovery poort: `3091`
- WebSocket poorten: Lamp `3090`, Sensor `3092`
- Arduino IDE (2.x aanbevolen) of een alternatief zoals PlatformIO
- USB‑kabels voor programmeren
4. Controleer en pas indien nodig aan:
    - Wi‑Fi SSID/wachtwoord (in beide sketches)
    - Discovery‑/WebSocket‑poorten (lamp: WS `3090`; sensor: WS `3092`)
    - Pinnen: lantaarn `LED_PIN=5`; sensor `PIR_PIN=4`
- Sensor(en) en een geschikte LED‑driver/lamp (afhankelijk van je hardware)
- Externe server/broker (bijv. MQTT of REST) voor communicatie, datagebruik en het aan-/uitzetten van lantaarns
## Externe server
De modules verwachten een eigen “lamp server” die:
- via UDP op poort `3091` regelmatig een JSON‑bericht uitzendt: `{ "type": "lamp_server_announce" }` (broadcast of multicast),
- een WebSocket‑endpoint aanbiedt voor lampen op poort `3090` en voor sensoren op poort `3092` (pad `/`).
   - Lantaarnpaal: [lantaarnpaal/lantaarnpaal.ino](lantaarnpaal/lantaarnpaal.ino)
### WebSocket berichten (lamp)
Verbinden naar server → lamp stuurt:
- Zonder ID: `{ type: "request_id" }`
- Met opgeslagen ID: `{ type: "authorize", id: "<lampId>" }`
   - Sensor: [lantaarnpaal_sensor/lantaarnpaal_sensor.ino](lantaarnpaal_sensor/lantaarnpaal_sensor.ino)
Server reacties die de lamp ondersteunt:
- ID toekennen: `{ type: "assigned_id", id: "<lampId>" }` → lamp slaat ID op en autoriseert.
- Autorisatie voltooid: `{ type: "authorized" }`
- Status/aansturing: `{ type: "state" | "activated", state: { on: <bool>, brightness: <0..255>, color: "#RRGGBB" } }`
- Identificeer lamp: `{ type: "identify", id: "<lampId>", durationMs?: <uint> }` → lamp knippert wit, daarna herstelt vorige toestand.
3. Selecteer het juiste bord en seriële poort: Tools → Board: “ESP32 …”, Tools → Port.
### WebSocket berichten (sensor)
Verbinden naar server → sensor stuurt:
- Zonder ID: `{ type: "request_sensor_id" }`
- Met opgeslagen ID: `{ type: "authorize_sensor", id: "<sensorId>" }`
4. Controleer eventuele configuratie‑variabelen in de sketch (bijv. Wi‑Fi SSID/wachtwoord, server/broker host/poort, MQTT‑gegevens of REST‑endpoint, pin‑definities) en pas deze aan jouw setup aan.
Server reacties die de sensor ondersteunt:
- ID toekennen: `{ type: "assigned_sensor_id", id: "<sensorId>" }`
- Autorisatie voltooid: `{ type: "authorized_sensor" }`
5. Upload elke sketch naar het juiste ESP32‑bord: Sensor‑sketch naar het bord met sensoren, Lantaarnpaal‑sketch naar het bord dat de lamp aanstuurt.
PIR‑activatie door sensor:
- Bij opgaande flank en vervolgens elke ~1s zolang beweging gedetecteerd wordt:
   `{ type: "sensor_activate", id: "<sensorId>" }`

De server hoort deze activaties te vertalen naar lamp‑commando’s (bijv. `activated/state`) en beleidslogica (aan/uit, dimmen, kleur).
## Externe server
Deze sketches verwachten een externe server of broker die:
- De lantaarnpaal reageert op `sensor_activate`‑events en op server‑berichten `state/activated` (helderheid, kleur, aan/uit).
- Gebruik servercommando’s (WebSocket berichten zoals hierboven) om lantaarns centraal te enablen/disablen, te dimmen of te identificeren.

## Gebruik
- Geen data/communicatie: controleer SSID/wachtwoord, UDP‑announce (poort 3091), WebSocket‑services (lamp: 3090, sensor: 3092) en pin‑aansluitingen; gebruik de Seriële Monitor voor foutmeldingen.

## Troubleshooting
   - Let op dat deze implementatie WebSocket gebruikt (geen MQTT/REST); de server moet de hierboven beschreven berichten ondersteunen.
- Bord/poort niet zichtbaar: installeer juiste USB‑drivers en gebruik een datakabel (niet alleen voeding).
## Status‑indicaties in de lamp
- Wi‑Fi verbinden: knipperend blauw
- Discovery actief: knipperend groen
- WebSocket (verbinden/autoriseren): knipperend paars
- Identify: snel wit knipperen gedurende opgegeven tijd
- Uploadfouten: verlaag de upload‑snelheid of druk op “BOOT/EN” volgens instructies van jouw ESP32‑bord.
- Geen data/communicatie: controleer SSID/wachtwoord, MQTT‑broker, IP‑adressen en pin‑aansluitingen; gebruik de Seriële Monitor voor foutmeldingen.
- Hardware: verifieer voedingsspanning, GND‑referenties en datalijnen; begin met één sensor en breid stap voor stap uit.
- Server niet bereikbaar: controleer of de broker/service draait, firewall/poorten, certificaten/credentials en netwerkverbinding.
