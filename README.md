# Apple CoreMIDI bindings for Acton

Acton bindings and helpers for reading MIDI input from a Pioneer DDJ-1000 or
other MIDI controllers on macOS.

The package is split into two layers:

- `coremidi`: a small CoreMIDI binding with endpoint discovery and an input
  actor that emits decoded MIDI messages.
- `ddj1000`: a controller-facing wrapper that currently forwards CoreMIDI
  messages and is intended to grow a named DDJ-1000 control map.

## Build

```sh
source ~/dt/acton/actrc
acton build
```

## Monitor

List endpoints, open the DDJ-1000 input source, and print incoming MIDI
messages:

```sh
out/bin/monitor
```

For a bounded smoke test:

```sh
out/bin/monitor 1
```

The monitor accepts the number of seconds to run as its first argument. Without
an argument it keeps running.

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

Run the same terminal meter against a synthetic signal without a DDJ-1000 or
CoreMIDI source:

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
import coremidi
import ddj1000

actor main(env):
    cap = coremidi.CoreMIDICap(env.cap)

    def on_event(controller: ddj1000.Controller, event: coremidi.Event):
        if event.is_error():
            print("MIDI error:", event.err)
        else:
            message = event.unwrap()
            print(message.kind, message.channel, message.data1, message.data2)

    controller = ddj1000.Controller(cap, on_event)
```

`coremidi.Message.raw` keeps the original MIDI bytes, so device-specific
mapping can be built without losing data that is not decoded yet.
