# H5P.RapidTimer

Rapid-Assessment-**Serie** nach Kalyuga: mehrere kurze Aufgaben mit je eigenem, hartem Zeitfenster. Einstufung in **Novize / Fortgeschritten / Experte** anhand von Richtigkeit *und* Antwortzeit über konfigurierbare Schwellenwerte. Optional: metakognitive Kalibrierung per 3-Stufen-Selbsteinschätzung (Certainty-Based Marking) und elaboriertes Feedback mit Hypercorrection-Priorisierung.

## Features

- **Item-Serie** (1–10 Aufgaben) mit je eigenem Zeitlimit, Score-Schwellenwert (Partial Credit) und optionaler Erklärung
- Beliebiger Kind-Inhaltstyp einbettbar (MultiChoice, TrueFalse, Blanks, SingleChoiceSet, MarkTheWords, DragText, Summary)
- **3 Lernpfade** mit Ziel-Links (Novize/Fortgeschritten/Experte), klickbar oder automatisch; Mittelpfad optional
- **3-Stufen-Konfidenz** (Sicher / Teils-teils / Unsicher) vor der Aufgabe oder nach der Antwort, 6 Feedback-Varianten; Lösung wird bis zur Einschätzung verdeckt
- **Endauswertung** mit Einstufung, Kalibrierungsübersicht und Item-Rückblick – „sicher + falsch"-Fälle zuerst, Erklärung aufgeklappt (Hypercorrection)
- **Intro-Seite** und optionales unbewertetes Übungs-Item (Erwartungsmanagement, Verfahrensgewöhnung)
- **Timer-Härtung**: Timestamp-basiert (Tab-Wechsel hilft nicht), Start erst nach Rendering/Sichtbarkeit, Reload mitten im Item = Timeout
- **Persistenz**: Ergebnis überlebt Reloads; nach Abschluss optional gesperrt
- **Barrierefreiheit**: optionaler Zeitfaktor (Nachteilsausgleich), Tastatur-Lock (`inert`), Warnung mit Puls + Symbol, `prefers-reduced-motion`

## Installation

1. Repo als `.zip` herunterladen oder klonen.
2. Ordner in `H5P.RapidTimer-<majorVersion>.<minorVersion>` umbenennen (siehe `library.json`), z. B. `H5P.RapidTimer-2.0`.
3. In Lumi Desktop oder ILIAS-H5P-Administration als Bibliothek hochladen (dort erwartet H5P i. d. R. ein gepacktes `.h5p`, siehe `example/`).

Für einen sofort testbaren Inhalt: Ordner `library.json`, `semantics.json`, `upgrades.js`, `rapid-timer.js`, `rapid-timer.css` zusammen mit `example/h5p.json` und `example/content/` in ein `.h5p`-Zip packen (Struktur wie im Ordner `H5P.RapidTimer-2.0/` erwartet – siehe oben).

**Update von 1.x**: `upgrades.js` migriert bestehende Inhalte automatisch (1 Item, 2 Pfade, 100 %-Schwelle – funktional identisch). Danach im Editor Items ergänzen und Schwellen anpassen.

## Konfiguration

Alle Optionen sind über den H5P-Editor auf Deutsch beschriftet und beschrieben (`semantics.json`). Kurzüberblick:

| Bereich | Optionen |
|---|---|
| Einführung | Intro-Text vor dem Start (Erwartungsmanagement) |
| Aufgaben-Serie | 1–10 Items: Inhalt, Zeitlimit, Score-Schwelle, Erklärung |
| Übungs-Item | Optionales unbewertetes Probe-Item |
| Zeit & Ablauf | Startmodus, Übergangsmodus, Warnschwelle, Balken, Timeout-Verhalten, Zeitfaktor |
| Kalibrierung | Ein/Aus, Zeitpunkt (vor/nach), Frage, Abdeckungstext, 6 Feedbacktexte |
| Einstufung | Schwellen für Experte/Fortgeschritten (%) |
| Lernpfade | URL + Label je Stufe, Weiterleitungsmodus |
| Verhalten | Sperre nach Abschluss, Beschriftungen, Einstufungstexte |

## Entwicklung

Reines Vanilla-JS (H5P-Core-API, `H5P.EventDispatcher`, `H5P.newRunnable`), keine Build-Schritte, keine Abhängigkeiten. Datei direkt bearbeiten und Bibliothek in Lumi neu laden.

## Wissenschaftlicher Hintergrund

- Kalyuga (2006) – Rapid Cognitive Assessment (Item-Serien, enges Zeitfenster)
- Kalyuga et al. (2003) – Expertise Reversal Effect (adaptive Pfade)
- Butterfield & Metcalfe (2001) – Hypercorrection-Effekt (Erklärung nach „sicher + falsch")
- Gardner-Medwin (UCL) – Certainty-Based Marking (3-Stufen-Konfidenz)
- Hattie & Timperley (2007) – The Power of Feedback (Task-Level-Erklärungen)
- CAST – Universal Design for Learning (Zeitfaktor als Nachteilsausgleich)

## Lizenz

MIT, siehe `LICENSE`.
