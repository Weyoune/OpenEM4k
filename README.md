# OpenEM4k

Die hochauflösende Oberfläche für OpenEM, den Nachbau von *Eishockey Manager* (Software 2000, 1993) – ein Remaster,
das Schritt für Schritt entsteht. Alle Grafik ist eigene Vektorgrafik, die zur Laufzeit in der Auflösung des Bildschirms
gezeichnet wird; aus dem Originalspiel kommen nur Daten, keine Bilder.

**Stand:** Schritt 1, die Spielszenen (Torszenen). Siehe [docs/MATCH_SCENES.md](docs/MATCH_SCENES.md).

## Bauen

Voraussetzung: CMake ≥ 3.24 und ein C++20-Compiler. SDL2 wird beim Konfigurieren geholt
(`-DEM4K_USE_SYSTEM_SDL2=ON` nimmt ein installiertes).

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Spieldaten

Die Dateien des Originalspiels gehören nicht zu diesem Repository. Die Spielszenen brauchen den Ordner `TORE` einer
vorhandenen Installation von Eishockey Manager:

```
em4k_match --data <Ordner mit TORE>
```

Ohne `--data` wird `OPENEM_DATA`, dann `EM`, `data`, der aktuelle Ordner und `../OpenEM/work/EM` durchsucht.
