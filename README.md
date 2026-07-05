# H5P.RapidTimer

Konfigurierbarer Zeitrahmen-Wrapper für beliebige H5P-Inhalte. Umsetzung des **Rapid-Assessment-Verfahrens** nach Kalyuga: eine Aufgabe, enges Zeitfenster, Verzweigung Novize/Experte anhand von Richtigkeit *und* Antwortzeit. Optional: Kalibrierung per Selbsteinschätzung (Certainty-Based Marking).

## Features

- Zeitlimit konfigurierbar (Start sofort oder per Klick), Warnschwelle, Fortschrittsbalken
- Beliebiger Kind-Inhaltstyp einbettbar (MultiChoice, TrueFalse, Blanks, SingleChoiceSet, MarkTheWords, DragText, Summary)
- Getrennte Ziel-Links für Experten- und Novizenpfad, klickbar oder automatisch nach Wartezeit
- Optionale Selbsteinschätzung ("Wie sicher bist du?") vor der Aufgabe oder nach der Antwort, mit vier frei betextbaren Feedback-Varianten (sicher/unsicher × richtig/falsch)
- Bei Abfrage *nach* der Antwort: Lösung des Kind-Inhalts wird bis zur Einschätzung verdeckt (verhindert kontaminierte Selbsteinschätzung)
- Kein externer Speicher, keine Serveranbindung – Zustand lebt ausschließlich im Element

## Installation

1. Repo als `.zip` herunterladen oder klonen.
2. Ordner in `H5P.RapidTimer-<majorVersion>.<minorVersion>` umbenennen (siehe `library.json`), z. B. `H5P.RapidTimer-1.4`.
3. In Lumi Desktop oder ILIAS-H5P-Administration als Bibliothek hochladen (dort erwartet H5P i. d. R. ein gepacktes `.h5p`, siehe `example/`).

Für einen sofort testbaren Inhalt: Ordner `library.json`, `semantics.json`, `rapid-timer.js`, `rapid-timer.css` zusammen mit `example/h5p.json` und `example/content/` in ein `.h5p`-Zip packen (Struktur wie im Ordner `H5P.RapidTimer-1.4/` erwartet – siehe oben).

## Konfiguration

Alle Optionen sind über den H5P-Editor auf Deutsch beschriftet und beschrieben (`semantics.json`). Kurzüberblick:

| Bereich | Optionen |
|---|---|
| Inhalt | Kind-Inhaltstyp wählen |
| Zeit | Zeitlimit, Startmodus, Warnschwelle, Fortschrittsbalken, Verhalten bei Ablauf |
| Kalibrierung | Ein/Aus, Zeitpunkt (vor/nach), Frage, Abdeckungstext, 4 Feedbacktexte |
| Pfade | Ziel-URL + Label für Experten- und Novizenpfad, Weiterleitungsmodus |

## Entwicklung

Reines Vanilla-JS (H5P-Core-API, `H5P.EventDispatcher`, `H5P.newRunnable`), keine Build-Schritte, keine Abhängigkeiten. Datei direkt bearbeiten und Bibliothek in Lumi neu laden.

## Wissenschaftlicher Hintergrund

- Kalyuga (2006) – Rapid Cognitive Assessment
- Kalyuga et al. (2003) – Expertise Reversal Effect
- Butterfield & Metcalfe (2001) – Hypercorrection-Effekt
- Gardner-Medwin (UCL) – Certainty-Based Marking

## Lizenz

MIT, siehe `LICENSE`.
