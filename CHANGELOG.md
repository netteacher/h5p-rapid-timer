# Changelog

## 2.0 (Breaking Changes)
- **Item-Serien**: 1–10 Aufgaben mit je eigenem Zeitfenster statt Ein-Item-Diagnose; Einstufung über Schwellenwerte (Anteil richtig UND in der Zeit).
- **3 Lernpfade**: Novize / Fortgeschritten / Experte mit konfigurierbaren Score-Bändern; Mittelpfad optional (leere URL = 2-Pfad-Verhalten).
- **Echte 3-Stufen-Konfidenz**: Sicher / Teils-teils / Unsicher als eigene Werte mit 6 Feedback-Varianten (vorher wurde "Teils-teils" auf "Unsicher" gemappt).
- **Elaboriertes Feedback**: Optionale Erklärung pro Item in der Endauswertung; bei "sicher + falsch" prominent aufgeklappt (Hypercorrection-Effekt).
- **Kalibrierungsübersicht**: Aggregiertes metakognitives Feedback am Ende (Über-/Unterschätzung).
- **Intro-Seite + Übungs-Item**: Erwartungsmanagement vor dem Test, optionales unbewertetes Probe-Item.
- **Partial Credit**: Konfigurierbarer Score-Schwellenwert pro Item statt striktem "volle Punktzahl".
- **Timer-Härtung**: Timestamp-basierte Messung (Tab-Wechsel verlängert die Zeit nicht), Timer-Start erst nach Rendering, `immediate`-Start erst bei Sichtbarkeit im Viewport, kein Zeit-Cheat per Reload (unterbrochenes Item = Timeout).
- **Zustandspersistenz**: Ergebnis und Einstufung überleben Reloads; nach Abschluss optional gesperrt (`lockAfterCompletion`, ersetzt `retryLabel`).
- **Barrierefreiheit**: Optionaler Zeitfaktor (Nachteilsausgleich), echter Tastatur-Lock (`inert`), pulsierende Warnung + Symbol statt nur Farbe, gedrosselte Screenreader-Ansagen, `prefers-reduced-motion`.
- **xAPI erweitert**: Pro Item `answered` mit Konfidenz/Zeit-Extensions, Abschluss-`completed` mit Score x/n und Klassifikation.
- **Migration**: `upgrades.js` migriert 1.x-Inhalte automatisch (1 Item, 2 Pfade, 100 %-Schwelle – funktional identisch). Falls die Plattform das Upgrade-Skript nicht ausführt, Inhalte manuell neu konfigurieren.

## 1.4
- Lösungs-Abdeckung: bei Selbsteinschätzung "nach der Antwort" wird das Feedback des Kind-Inhalts verdeckt, bis die Einschätzung abgegeben ist (verhindert kontaminierte Kalibrierung).

## 1.3
- Zeitpunkt der Selbsteinschätzung konfigurierbar: "vor der Aufgabe" (Thema) oder "nach der Antwort" (Lösung, empfohlen).

## 1.2
- Kalibrierung: optionale Selbsteinschätzung mit vier konfigurierbaren Feedbacktexten (sicher/unsicher × richtig/falsch).

## 1.1
- Konfigurierbare Ziel-Links für Experten- und Novizenpfad (klickbar oder automatische Weiterleitung).

## 1.0
- Erste Version: Zeitlimit-Wrapper für beliebige H5P-Inhalte, Sperr-/Hinweis-Verhalten bei Zeitablauf, xAPI-Reporting inkl. Bearbeitungsdauer.
