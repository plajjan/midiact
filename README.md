# Cross-platform MIDI bindings for Acton

Acton bindings and helpers for reading MIDI input from a Pioneer DDJ-1000, a
Novation Launchkey, or other MIDI controllers. Portable across macOS and Linux
via [RtMidi](https://github.com/thestk/rtmidi) — CoreMIDI on macOS, ALSA on
Linux, WinMM on Windows.

The package is split into two layers:

- `midi`: a small RtMidi binding with endpoint discovery, an input actor that
  emits decoded MIDI messages, and an output actor for sending short
  channel-voice messages (e.g. note-ons to drive a controller's LEDs).
- `ddj1000`: a controller-facing wrapper that currently forwards MIDI messages
  and is intended to grow a named DDJ-1000 control map.

Both RtMidi and its Linux ALSA backend (alsa-lib) are vendored as Zig
dependencies (see `Build.act` and `deps/`) and linked statically. The upstream
sources are fetched pristine and hash-pinned — nothing is forked or patched, the
build files live in `deps/rtmidi` and `deps/alsa`. As a result **no MIDI library
needs to be installed to build**, on macOS (CoreMIDI framework) or Linux (no
`libasound2-dev` required).

At runtime on Linux, alsa-lib still reads its configuration from
`/usr/share/alsa` (e.g. `alsa.conf`) to resolve the `default` sequencer, so that
data — shipped by the `alsa-ucm-conf` / `libasound2-data` package — needs to be
present on the target machine even though the library itself is linked in
statically.

## Build

```sh
acton build
```

## Monitor

List endpoints, open a MIDI input source, and print incoming MIDI messages:

```sh
out/bin/monitor
```

By default the monitor opens the first available source. Pass the number of
seconds to run as the first argument, and optionally a name substring to select
a specific source:

```sh
out/bin/monitor 5                 # first source, listen 5 seconds
out/bin/monitor 5 "MIDI Port"     # a source whose name contains "MIDI Port"
```

Without a seconds argument it keeps running until interrupted.

## VU Meter TUI

Open the DDJ-1000 input source and show one MIDI input as a terminal meter:

```sh
out/bin/vu
```

By default, `vu` starts in learn mode. Move one supported DDJ-1000 control and
the meter locks onto that MIDI input. The display uses colored braille cells
for a higher-resolution bar and a peak marker. Press `q` to quit.

For a bounded smoke test:

```sh
out/bin/vu 10
```

You can also select an input explicitly:

```sh
out/bin/vu cc:1:11
out/bin/vu note:1:36
out/bin/vu pitch:1
```

## VU Meter Demo

Run the same terminal meter against a synthetic signal without a MIDI
controller or source:

```sh
out/bin/vu_demo
```

The demo exits after 12 seconds by default. Pass a duration in seconds, or `0`
to run until `q` is pressed:

```sh
out/bin/vu_demo 3
out/bin/vu_demo 0
```

## API Sketch

```acton
import midi
import ddj1000

actor main(env):
    cap = midi.MidiCap(env.cap)

    def on_event(controller: ddj1000.Controller, event: midi.Event):
        if event.is_error():
            print("MIDI error:", event.err)
        else:
            message = event.unwrap()
            print(message.kind, message.channel, message.data1, message.data2)

    controller = ddj1000.Controller(cap, on_event)
```

`midi.Message.raw` keeps the original MIDI bytes, so device-specific mapping
can be built without losing data that is not decoded yet.
```
