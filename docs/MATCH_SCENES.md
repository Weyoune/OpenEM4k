# Spielszenen

Code: `src/match/`, Programm `em4k_match` (`src/app/match_viewer.cpp`), Test `tests/match_tests.cpp`.

## Was es ist

Die Torszenen von Eishockey Manager als eigene 2D-Eishockey-Darstellung in leicht schräger Ansicht, in beliebiger Auflösung und
beliebigem Seitenverhältnis. Eis, Bande, Tribüne, Zuschauer, Tore, Spieler, Puck und Schrift sind eigene
Vektorzeichnungen, die zur Laufzeit gerastert werden. Aus dem Original kommen nur

- die Szenenskripte (`TORE\*`: wo Puck und zwölf Objekte je Tick stehen und welches Sprite das Original dort zeigte),
- die Maße des Eisfelds (Linien, Kreise, Bande) und
- die Vereinsfarben.

## Aufbau

| Datei | Inhalt |
|---|---|
| `tor_script.{hpp,cpp}` | Leser der Szenenskripte, Zustand nach n Bildern. |
| `raster.hpp` | Software-Vektorrasterer: Formen als vorzeichenbehaftete Abstandsfunktionen in Feldkoordinaten, analytische Kantenglättung, beliebiger Maßstab. |
| `rink.{hpp,cpp}` | Halle: Eis mit Kratzern, Glanzpunkten und gespiegelten Hallenlichtern, Linien, Torräume, Tore, Bande mit Glas und Werbung, Spielerbänke, Strafbänke, Zeitnehmer, Tribüne mit Sitzen, Treppen und Zuschauern. |
| `player.{hpp,cpp}` | Feldspieler, Torhüter, Puck – gezeichnet aus einem Zustand (Ort, Blickrichtung, Schrittphase, Einsatz, Aktion), nicht aus Bildern. |
| `text.{hpp,cpp}` | Eigene Strichschrift auf einem 4×6-Raster, beliebig groß und gedreht. |
| `scene.{hpp,cpp}` | `SceneTrack` bereitet ein Skript vor, `MatchScene::render` zeichnet ein Bild, `scene_viewport` legt den Ausschnitt fest. |
| `png.hpp` | PNG-Ausgabe für Einzelbilder. |

Eingabe ist `MatchSceneInput`. Der obere Teil entspricht dem, was auch die Torszene des Originals bekommt (Skript,
Füllgrad der Halle, Farben, Namen, Spielstand, Infotext, Minute, Bildnummer, angreifende Seite, Tor ja/nein,
Schlussbild); dazu kommen `after` (Sekunden seit Ende des Skripts), `scorer` (Name über dem Torschützen) und `tilt`
(Neigung der Ansicht).

## Vom Skript zur Szene (`SceneTrack`)

- **Ort**: Das Original setzt ein 32×32-Sprite mit der linken oberen Ecke auf (x − 0x50, y − 0x60); die Figur steht
  auf dem Fußpunkt (+16, +26).
- **Blickrichtung**: bei Bewegung die Laufrichtung (über fünf Ticks geglättet); zeigte das Sprite des Originals in die
  Gegenrichtung, läuft die Figur rückwärts. Im Stand schaut sie zum Puck, wenn das zur Sprite-Richtung passt. Die
  Sprite-Richtungen stehen in einer Tabelle (`kFacing`), alles andere wird berechnet.
- **Schritt**: Phase aus der gelaufenen Strecke, Einsatz aus dem Tempo.
- **Aktion**: Sprites mit geschwungenem oder gehobenem Schläger sind ein Schuss, wenn der Puck am Schläger ist, sonst
  Jubel. Der Torhüter geht bei einem schnellen Puck in der Nähe in die Abwehrhaltung. Das sind Faustregeln, keine
  Angaben des Skripts.
- **Puck**: Die Puck-Koordinate des Skripts ist der Ort über dem Eis (das Original zeichnet sie in der Schattenfarbe);
  Objekttyp 0, der graue Punkt, ist der Puck selbst. Bei einem gehobenen Puck liegen Schatten und Scheibe auseinander.
- **Kamera**: gewichtetes Mittel der Skriptkamera über die Ticks davor und danach (die Zukunft ist bekannt, also ohne
  Nachlauf); dazu bis 7 % näher, solange der Puck in Tornähe oder schnell ist. Näher geht es nur, soweit das Fenster
  des Originals sichtbar bleibt.
- **Eisspray**: wo ein Feldspieler hart bremst.
- **Netz**: der erste Tick, in dem der Puck im Netz liegt; von da an beult das Netz an der Stelle des Pucks aus und
  schwingt zurück.
- **Torschütze**: der Angreifer, der zuletzt vor dem Netz-Tick geschossen hat; sonst der am Puck, als der am
  schnellsten wurde.
- **Torhüter** tragen die Farben der verteidigenden Mannschaft.

Zwischen zwei Ticks (54,925 ms) werden Ort, Blickrichtung, Schritt, Kamera und Größe gemischt. `smooth = false` zeigt
Ticks, Kamera und Größe des Skripts unverändert.

## Schrägblick (`tilt`)

Wie das Original zeichnet die Szene das Eis unverzerrt (Kreise bleiben rund) und stellt nur auf, was auf dem Eis
steht: Ein Punkt in der Höhe h über dem Eis erscheint h × Neigung weiter oben im Bild. Die Bodenebene – Linien,
Kamera, Skriptkoordinaten, Schatten, Spuren – hängt nicht von der Neigung ab; der Fußpunkt einer Figur liegt immer
auf der Skriptposition.

- Neigung 0 ist die reine Draufsicht, 1 etwa die Schräge der Originalsprites. Vorgabe 0,5.
- **Figuren** sind ein kleines Skelett mit Höhen (Schlittschuhe 0, Knie, Hüfte, Hände, Schultern, Kopf). Der Rumpf
  hat eine Seitenwand, der Helm ist eine Kugel: Visier und Gesicht zeigen sich umso mehr, je mehr die Figur zum
  Betrachter schaut; von hinten sieht man die Rückennummer auf dem Rücken, sobald die Wand hoch genug ist. Schaut
  die Figur weg, liegen Arme und Schläger hinter dem Rumpf, sonst davor.
- **Tore** stehen: Pfosten, Latte, Netzdach, Rück- und Seitennetz.
- **Zuschauer und Bankspieler**: Kopf über den Schultern, Gesicht je nach Blickrichtung.
- Was auf dem Eis steht, wird von hinten nach vorn gezeichnet (Figuren nach ihrem Ort, das ferne Tor hinter, das
  nahe Tor vor denen, die vor seiner Linie stehen).

## Nach dem Skript (`final`, `after`)

Bei einem Tor: Die Zuschauer, die sich freuen, springen (Heimfans beim Heimtor, die meisten Neutralen mit ihnen; beim
Gästetor nur wenige), die Torlampe hinter dem Tor pulsiert, der neue Spielstand steht in Gold, über dem Torschützen
erscheint sein Name. Solange niemand springt, liegen die Zuschauer mit in den Hintergrundkacheln; für den Jubel wird
ein zweiter Kachelsatz ohne Zuschauer benutzt und sie werden je Bild gezeichnet.

## Bildausschnitt

Das 320×200-Fenster des Originals plus 18 Feldeinheiten oben und unten (Platz für die Tore hinter der Torlinie) bleibt über der
Anzeigeleiste immer sichtbar. Maßstab = min(Breite / 320, (Höhe − Leiste) / 236). Breitere Bilder zeigen links und
rechts mehr Halle (16:9 etwa 480 statt 320 Feldeinheiten), höhere mehr oben und unten. Der Ausschnitt verlässt die
Halle (672×1152) nicht; ist das Bild breiter als die Halle, wird sie mittig gezeigt. Die Anzeigeleiste (12,5 % der
Höhe) liegt halb durchsichtig über dem Bild.

## Technik

- Der Hintergrund wird in 256-Pixel-Kacheln gerastert und zwischengespeichert, fehlende Kacheln parallel – in zwei
  Maßstäben: dem des ungezoomten Bilds und dem des ruhenden Zooms. Dort wird kopiert; nur während der Zoom gleitet,
  wird aus den ungezoomten Kacheln bilinear abgetastet (höchstens 7 %
  Vergrößerung). Figuren, Tore, Puck, Effekte und Leiste werden je Bild in waagerechten Bändern gezeichnet, ein Thread
  je Band.
- Gemessen ohne Fensterausgabe (`--bench`) auf einem Rechner mit vier Kernen, Neigung 0,5: rund 15 ms je Bild bei
  1920×1080, rund 58 ms bei 3840×2160. Für 4K in 60 Bildern/s reicht das Software-Rastern so nicht.
- Abhängigkeiten: C++20 und Threads; SDL2 nur im Fensterprogramm.

## Benutzen

```
em4k_match [--data <EM-Ordner>] [--script 5.at] [--arena 0..4] [--home RGB] [--away RGB]
           [--names <Heim> <Gast>] [--scorer <Name>] [--tilt 0..1] [--size BxH] [--fullscreen] [--ticks]
           [--shot bild.png [--frame n[.f]] [--after Sekunden]] [--bench]
```

Tasten: ←/→ Skript, Leertaste Pause, `,` / `.` Tick zurück / vor, `I` Glättung und ruhige Kamera, `A` Füllgrad der
Halle, `T` / `G` Ansicht steiler / flacher, `+` / `−` Tempo, `F` oder F11 Vollbild, Esc Ende.

## Offen

- Ton (die Skripte tragen je Bild eine Tonmarke).
- Vereinswappen im Mittelkreis statt „EM“.
- Die Bänke sind nur in sehr breiten Bildern an der Mittellinie zu sehen (Penaltyszenen in 21:9).
- Am nahen Tor liegt die Torlampe unter der Anzeigeleiste.
- Figuren können sich überdecken, wo die Sprites des Originals hintereinander standen.
- Anbindung an das Spiel: `MatchSceneInput` wird bisher nur vom Betrachter gefüllt.
