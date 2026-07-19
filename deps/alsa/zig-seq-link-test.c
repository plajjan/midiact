/*
 * Link-closure check for the zig-build static libasound (not part of the
 * library; only built with `zig build -Dtest`). It exercises the same ALSA
 * entry points RtMidi's backend uses — snd_seq_open() and the snd_midi_event_*
 * codec — so the linker must resolve the entire builtin seq path against the
 * archive. It is never run here (the host is macOS); a clean link is the
 * signal that the vendored archive is self-contained.
 */
#include <alsa/asoundlib.h>

int main(void)
{
    snd_seq_t *seq = NULL;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0)
        return 1;
    snd_seq_set_client_name(seq, "zig-link-test");

    snd_midi_event_t *codec = NULL;
    if (snd_midi_event_new(32, &codec) == 0 && codec != NULL)
        snd_midi_event_free(codec);

    snd_seq_close(seq);
    return 0;
}
