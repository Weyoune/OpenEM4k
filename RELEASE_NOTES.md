Vorschau der Spielszenen: die Torszenen von Eishockey Manager als eigene, hochauflösende 2D-Darstellung.

Neu in v0.2.0: leicht schräge Ansicht wie im Original – Spieler mit Höhe, Helme und Gesichter von der Seite, stehende
Tore. Mit `T` / `G` lässt sich die Neigung verstellen (`--tilt 0..1`, Vorgabe 0,5).

**Spieldaten sind nicht enthalten.** Das Programm braucht den Ordner `TORE` aus einer vorhandenen Installation von
Eishockey Manager – neben das Programm kopieren oder mit `--data <Ordner>` angeben.

- **macOS** (Apple Silicon und Intel, ab macOS 11): `OpenEM4k-…-macos-universal.zip`. Das Programm ist nicht signiert;
  nach dem Entpacken im Terminal `xattr -d com.apple.quarantine em4k_match` und dann `./em4k_match`.
- **Windows**: `OpenEM4k-…-windows-x64.zip`
- **Linux** (x86-64): `OpenEM4k-…-linux-x64.zip`

Tasten und Optionen stehen in `LIESMICH.txt` im Paket und in `docs/MATCH_SCENES.md`.
