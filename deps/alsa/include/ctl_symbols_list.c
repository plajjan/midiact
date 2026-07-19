/*
 * Generated file, normally produced by the alsa-lib build from the enabled
 * BUILD_CTL_PLUGIN_* set and #included by src/control/control_symbols.c into the
 * snd_control_open_objects[] initializer.
 *
 * This MIDI-only build enables no optional control plugins (remap / shm / ext),
 * so there are no extra builtin control modules to force-keep — control_symbols.c
 * retains only its hardcoded hw + empty entries. Keeping this list empty is also
 * what avoids pulling control_ext.c (and thus the pcm subsystem) into the link.
 */
