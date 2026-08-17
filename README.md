# Witness Bridge

Ein eigenständiges, Teensy-4.1-basiertes Gerät für forensische Begleitprotokollierung und USB-Geräte-Triage — unabhängig, manipulationssicher und ohne Abhängigkeit von einem Heim- oder Firmennetz.

> **Status:** Konzeptphase. Noch keine Hardware aufgebaut, noch keine Firmware geschrieben. Dieses Dokument beschreibt das Design als Ausgangspunkt für die Umsetzung.

## Inhalt

- [Zielsetzung](#zielsetzung)
- [Technische Grenzen](#technische-grenzen)
- [Architektur — Zeugenprotokoll](#architektur--zeugenprotokoll)
- [Ausbaustufen](#ausbaustufen)
- [USB-Hub-Triage & Anomalieerkennung](#usb-hub-triage--anomalieerkennung)
- [Bedienung ohne Netzwerk-Infrastruktur](#bedienung-ohne-netzwerk-infrastruktur)
- [Hardware](#hardware)
- [Log-Format](#log-format)
- [Kryptografie & Zeit](#kryptografie--zeit)
- [Datenschutz & Aufbewahrung](#datenschutz--aufbewahrung)
- [Governance-Checkliste vor Produktiveinsatz](#governance-checkliste-vor-produktiveinsatz)
- [Roadmap](#roadmap)

## Zielsetzung

Witness Bridge begleitet eine forensische Untersuchung als unabhängige zweite Instanz. Während die eigentliche Extraktion mit einem etablierten, validierten Werkzeug auf der Prüf-Workstation läuft, führt Witness Bridge parallel ein eigenständiges, kryptografisch versiegeltes Sitzungsprotokoll: wer, wann, welcher Fall, welche Hashes, welche Ereignisse.

Die Unabhängigkeit von der Workstation ist der eigentliche Wert. Ein Protokoll, das dieselbe Maschine schreibt, die auch extrahiert, ist als Nachweis schwächer als ein zweites, getrenntes System mit eigener Zeitquelle und eigenem Schlüssel.

**Kernprinzip:** Trennung von Durchführung und Bezeugung. Die Workstation extrahiert. Witness Bridge bezeugt, signiert und versiegelt — unabhängig, offline-fähig, auditierbar.

## Technische Grenzen

Aktuelle iPhones (A12 Bionic und neuer, ab iPhone XS) sind von keinem öffentlich bekannten BootROM-Exploit betroffen. Der DFU-Modus ist ein reines Firmware-Flash-Protokoll; ohne einen ungepatchten SecureROM-Exploit lässt sich darüber weder Verschlüsselung umgehen noch Gerätezugriff erzwingen.

**Kein Bypass-Werkzeug.** Witness Bridge extrahiert, entsperrt oder analysiert keine Geräteinhalte. Es dokumentiert ausschließlich den Prozess und die Ergebnis-Hashes eines bereits autorisierten, mit anderen Mitteln durchgeführten Zugriffs.

## Architektur — Zeugenprotokoll

Das Zielgerät kommuniziert ausschließlich mit der Extraktions-Workstation. Witness Bridge sitzt daneben, nicht dazwischen: Prüfer:in eröffnet die Sitzung, scannt Fallnummer und die von der Workstation erzeugten Ergebnis-Hashes ein, und Witness Bridge verkettet jeden Eintrag kryptografisch mit dem vorherigen — zeitlich verankert über eine GPS-disziplinierte, manipulationsresistente Uhrzeit.

```mermaid
flowchart LR
  A["Zielgerät<br/>iPhone - DFU / Recovery / Normal"]
  B["Extraktions-Workstation<br/>validiertes Tool<br/>(checkra1n, idevicerestore, kommerziell)"]

  subgraph WB["Witness Bridge - Teensy 4.1"]
    direction TB
    C["Sitzungssteuerung<br/>Fallnummer, Pruefer-ID"]
    D["Hash-Eingabe<br/>Barcode / Keypad"]
    E["GPS-Trusted-Time<br/>PPS-diszipliniert"]
    F["Secure Element<br/>ECDSA P-256 Signatur"]
    G["Append-Only Log<br/>Hash-Chain auf SD"]
  end

  H[("Sync-Fenster<br/>TSA-Gegensignatur")]

  A -- USB --> B
  B -- "Ergebnis-Hash" --> D
  C --> G
  D --> G
  E --> G
  F --> G
  G -. "nur ausserhalb der Sitzung" .-> H
```

Ein echter, transparenter USB-Inline-Tap zwischen Workstation und Gerät erfordert dedizierte Analyzer-Hardware und würde, falsch implementiert, selbst zum Risiko für die Integrität der Extraktion werden. Die getrennte Zeugen-Rolle liefert den forensischen Mehrwert ohne dieses Risiko.

## Ausbaustufen

| Stufe | Umsetzung | Beschreibung |
|---|---|---|
| **A — Zeugenprotokoll** | Jetzt umsetzbar | Sitzungs-, Hash- und Zeit-Protokollierung. Nutzt ausschließlich native Teensy-4.1-Fähigkeiten (SD, RTC, USB-Host für Scanner). Geringes Risiko, hoher Nachweiswert. |
| **B — Gespiegelter USB-Mitschnitt** | Forschung / später | Echter Protokoll-Mitschnitt über einen Hub mit Monitor-Port, ausgewertet über den Teensy-USB-Host-Controller. Nur nach validierter Stufe A sinnvoll. |

## USB-Hub-Triage & Anomalieerkennung

Der Host-Controller des Teensy 4.1 sieht ohnehin jedes Gerät hinter jedem Port eines angeschlossenen Hubs — das ist eine Grundeigenschaft von USB, keine Zusatzfunktion. Damit lässt sich Witness Bridge um ein zweites Modul erweitern: eine Geräte-Triage mit Abgleich gegen ein Known-Device-Register und Heuristiken gegen bekannte HID-Injection-Hardware.

### Checkpoint-Modus — sofort umsetzbar

Der Hub hängt ausschließlich am Teensy, nicht an einer produktiven Workstation — etwa als Prüfstation für gefundene oder eingeschickte USB-Sticks, bevor sie überhaupt in die Nähe eines echten Rechners kommen. Der Teensy enumeriert jedes angeschlossene Gerät, liest Deskriptoren aus, wendet Heuristiken an und meldet pro Gerät einen Status. Da nie ein produktives System am selben Bus hängt, ist das Risiko minimal.

### Inline-HID-Firewall — Stufe C, deutlich komplexer

Für Echtzeit-Schutz einer aktiv genutzten Workstation müsste der Teensy tatsächlich dazwischen sitzen: Host-Port zum untersuchten Gerät/Hub, nativer USB-Port zur Workstation, dort als HID-Passthrough (Tastatur/Maus) emuliert — jeder Report wird live bewertet, bevor er weitergereicht wird. Massenspeicher-Geräte sollten hier bewusst **nicht** transparent durchgereicht, sondern standardmäßig blockiert oder nur nach manueller Freigabe im Nur-Lese-Modus erlaubt werden.

```mermaid
flowchart LR
  subgraph Hub["USB-Hub"]
    D1["Geraet 1"]
    D2["Geraet 2"]
    D3["Geraet n"]
  end

  subgraph TB["Teensy 4.1 - Checkpoint"]
    direction TB
    P["Enumeration & Deskriptor-Analyse"]
    Q["Heuristik-Engine<br/>Composite-Check, Timing"]
    R[("Known-Device-Register<br/>lokal auf SD")]
    WS["Eingebetteter Web-Server"]
  end

  L["Laptop der pruefenden Person"]

  D1 & D2 & D3 -- USB --> P
  P --> Q
  Q -- Abgleich --> R
  Q --> WS
  WS -- "USB-Netzwerk-Gadget (RNDIS/ECM)<br/>Punkt-zu-Punkt, kein Netz noetig" --> L
```

### Erkennungsheuristiken

| Muster | Erkennungsmethode | Zielt auf |
|---|---|---|
| Composite HID + Storage auf einem Gerät | Interface-Deskriptor-Prüfung bei Enumeration | Rubber Ducky, Bash Bunny, O.MG Cable |
| VID/PID nicht im Known-Device-Register | Abgleich gegen gepflegte Positivliste | generische Nachbauten |
| Verstümmelte/fehlende String-Deskriptoren | String-Deskriptor-Validierung | Billig-Klone |
| Tastatureingabe ohne menschliche Variabilität | Statistische Analyse der Inter-Keystroke-Zeit (Keystroke-Dynamics) | skriptgesteuerte HID-Injection |
| Mausbewegung streng periodisch, kein Klick/Scroll | Bewegungsmuster-Analyse der HID-Reports | Mouse-Jiggler |
| Wiederholtes schnelles Neu-Enumerieren | Frequenzanalyse der Enumeration-Events | instabile/manipulierte Firmware |

Für rekonstruierte Tastatureingaben lässt sich zusätzlich eine einfache Signaturliste führen (z. B. `powershell`, `IEX(New-Object`, `certutil -urlcache`) — das ist eine heuristische Zusatzschicht, keine Garantie. Der harte technische Boden ist die Deskriptor-Analyse; alles Verhaltensbasierte liefert Wahrscheinlichkeiten, keine Beweise, und sollte im Interface auch so dargestellt werden.

> **Mouse-Jiggler-Erkennung ist Mitarbeiterüberwachung.** Sie erfasst faktisch Aktivitäts-/Anwesenheitsverhalten von Personen und ist rechtlich eine andere Kategorie als Malware-Erkennung — in Deutschland vermutlich ein Mitbestimmungstatbestand nach § 87 BetrVG mit eigener Rechtsgrundlage. Als eigenes, standardmäßig deaktiviertes Modul führen, nicht stillschweigend in die Security-Funktion einbetten (Zweckbindung, Art. 5 Abs. 1 lit. b DSGVO).

## Bedienung ohne Netzwerk-Infrastruktur

Witness Bridge ist als portables Gerät gedacht und darf nicht von einem bestimmten Heim- oder Firmennetz abhängen. Deshalb kein Cloud- oder Broker-Anschluss — die Auswertung läuft direkt auf dem Gerät:

1. **Primär — USB-Netzwerk-Gadget:** Der native USB-Port des Teensy meldet sich beim Laptop der prüfenden Person als eigenes Netzwerkinterface (RNDIS/CDC-ECM) an. Punkt-zu-Punkt-Verbindung über Link-Local-Adressierung, keine Infrastruktur nötig, funktioniert an jedem Ort.
2. **Optional — Ethernet:** Der native Ethernet-Port des Teensy 4.1 kann zusätzlich in ein vor Ort vorhandenes Netz eingebunden werden, falls gewünscht.
3. **Fallback — Offline-Export:** Für Archiv-/Aktenzwecke schreibt das Gerät einen in sich geschlossenen statischen HTML-Bericht auf die SD-Karte, der in jedem Browser ohne laufende Verbindung zum Gerät geöffnet werden kann.

Das Known-Device-Register liegt lokal auf der SD-Karte als einfache Datei und wird über das Web-Interface oder einen manuellen Import (USB-Stick) aktualisiert — ohne Abhängigkeit von einer externen Datenbank.

## Hardware

| Komponente | Zweck | Beispiel |
|---|---|---|
| Teensy 4.1 | Kernsteuerung, USB-Host, SD-Slot, gepufferte RTC | PJRC |
| GPS-Modul mit PPS | Unabhängige, manipulationsresistente Zeitquelle | u-blox NEO-M8N/M9N |
| Secure Element | ECDSA-P256-Signatur, privater Schlüssel hardware-gebunden | Microchip ATECC608A |
| Industrietaugliche microSD | Append-Only-Log-Speicher, AES-256 auf Feldebene | pSLC-Karte |
| USB-Barcode-/QR-Scanner | Manipulationsarme Hash-Eingabe statt Tippen | HID-Scanner am USB-Host-Port |
| Tamper-evident-Gehäuse | Physischer Schutz, verplombbar | verplombtes Gehäuse + Sabotageschalter |
| Sabotageschalter | Log-Eintrag bei Gehäuseöffnung | Mikroschalter an GPIO, Dauerstrom-Pufferung |
| Status-Display | Sitzungsstatus ohne Laptop einsehbar | kleines OLED (SSD1306) |

*Aufbaufotos folgen, sobald die Hardware real existiert.*

## Log-Format

Jeder Eintrag ist mit dem vorherigen verkettet (Hash-Chain) und einzeln signiert. Ein nachträglich verändertes oder entferntes Ereignis bricht die Kette sichtbar.

```json
{
  "seq": 42,
  "ts_gps_utc": "2026-08-17T14:32:01.184Z",
  "case_id": "IR-XXXX-XXXX",
  "examiner_id": "EX-XXXX",
  "event": "hash_ingest",
  "detail": {
    "source_tool": "checkra1n",
    "artifact": "full_extraction.tar",
    "sha256": "..."
  },
  "prev_hash": "...",
  "entry_hash": "SHA256(prev_hash|seq|ts|case_id|examiner_id|event|detail)",
  "sig_ecdsa_p256": "..."
}
```

Ereignistypen umfassen mindestens: `session_open`, `session_close`, `hash_ingest`, `tamper_detected`, `examiner_note`. Das Format ist bewusst als JSON Lines gehalten — zeilenweise anhängbar, ohne die Datei neu zu schreiben.

## Kryptografie & Zeit

- **Zeitquelle:** GPS-PPS liefert UTC unabhängig von Netzwerk und Systemuhr der Workstation — nicht durch die untersuchende Person manipulierbar.
- **Signatur:** ECDSA P-256 im Secure Element, privater Schlüssel verlässt das Bauteil nie.
- **Verkettung:** jeder Eintrag referenziert den Hash des Vorgängers — nachträgliche Änderung bricht die Kette überprüfbar.
- **Externe Verankerung:** außerhalb der eigentlichen Sitzung, in einem definierten Sync-Fenster, wird der aktuelle Kettenstand bei einer Time-Stamping-Authority (RFC 3161) gegengezeichnet — das Gerät bleibt während der Untersuchung selbst offline/air-gapped.

## Datenschutz & Aufbewahrung

Witness Bridge speichert bewusst keine Geräteinhalte, sondern nur Metadaten und Hashes. Nach Fallabschluss und regulärer Übergabe des Logs in die Fallakte wird der Gerätespeicher sicher gelöscht (kryptografische Löschung durch Schlüsselvernichtung), um eine Vermischung zwischen Fällen auszuschließen.

## Governance-Checkliste vor Produktiveinsatz

Die folgenden Punkte sind organisatorische Voraussetzungen, keine technischen — sie ersetzen keine Rechts- oder Compliance-Beratung und müssen im jeweiligen Einsatzkontext verantwortet werden.

- [ ] Rechtsgrundlage für die Untersuchung des konkreten Geräts dokumentiert (z. B. Betriebsvereinbarung, Einzelfallfreigabe bei Mitarbeitendengeräten)
- [ ] Datenschutz-Folgenabschätzung mit der/dem Datenschutzbeauftragten abgestimmt
- [ ] Betriebsrat einbezogen, sofern Mitarbeitendengeräte betroffen sind
- [ ] Tool-Validierung mit dokumentierten Referenzfällen abgeschlossen
- [ ] Bei Einsatz im regulierten/Finanzsektor: Aufnahme in das ICT-Risikomanagement-Framework gemäß DORA Art. 5–16 (Asset-Inventar, Risikobewertung, Verantwortung der Geschäftsleitung)
- [ ] Einbindung in den Incident-Management-Prozess gemäß DORA Art. 17–23 (Klassifizierung, Nachweisführung, Meldefristen)
- [ ] Regelmäßige Funktions-/Resilienztests eingeplant, DORA Art. 24–27
- [ ] Lieferketten-/Open-Source-Review der verwendeten Bibliotheken, DORA Art. 28 ff.
- [ ] Freigabe durch IT-Security-Leitung, ggf. interne Revision
- [ ] Verfahrensschulung der Prüfer:innen, dokumentiert
- [ ] Malware-Erkennung und Jiggler-/Aktivitätserkennung strikt getrennt freigegeben, letztere nur mit eigener Betriebsvereinbarung

## Roadmap

1. Firmware-Grundgerüst: Sitzungssteuerung, Hash-Chain, SD-Schreibpfad
2. GPS/PPS-Integration und Zeitquelle validieren
3. Secure-Element-Anbindung und Signaturpfad testen
4. USB-Netzwerk-Gadget (RNDIS/ECM) und eingebettetes Web-Interface
5. Gehäuse mit Sabotageschalter, Siegel-Konzept
6. Checkpoint-Modus: Enumeration, Heuristik-Engine, Known-Device-Register
7. Testreihe mit bekannten Referenzfällen zur Validierung
8. Governance-Freigabe einholen, dann erst Produktiveinsatz

---

*Entwurf, kein Ersatz für Rechts- oder Compliance-Beratung.*
