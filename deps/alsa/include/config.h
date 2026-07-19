/*
 *  config.h
 *
 *  Hand-written configuration header for the vendored alsa-lib build, replacing
 *  the autotools-generated include/config.h. This library is only ever compiled
 *  for glibc Linux (the sole platform where RtMidi's ALSA backend — and thus
 *  this dependency — is built), so the feature macros describe a modern glibc
 *  userland as provided by the Zig-bundled cross toolchain.
 *
 *  It pairs with the compile flags in build.zig (-D_GNU_SOURCE for
 *  eaccess/uselocale/wordexp, etc.). ALSA_LIBRARY_BUILD is NOT passed on the
 *  command line — include/local.h defines it itself, which is what makes the
 *  public headers expose their declarations instead of redirecting to
 *  <alsa/asoundlib.h>.
 *
 *  The pcm/hwdep/mixer/ucm/topology subsystems are left out of the build by not
 *  defining their BUILD_* macros (equivalent to `configure --disable-pcm ...`).
 *  Those code paths are already guarded by #ifdef in the sources, so no source
 *  patches are required — only the seq + rawmidi MIDI subsystems are compiled.
 *
 *  PIC and the symbol-versioning macros (USE_VERSIONED_SYMBOLS, HAVE_ELF,
 *  HAVE_GNU_LD, HAVE_*_SYMS, HAVE_ATTRIBUTE_SYMVER, ...) are intentionally left
 *  undefined: this is a static, non-PIC archive linked into a non-PIE
 *  executable, so builtin modules register through the constructor-based
 *  snd_dlsym_start list (see SND_DLSYM_BUILD_VERSION in include/global.h) rather
 *  than versioned dynamic symbols.
 */

#ifndef ALSA_LIB_CONFIG_H_ZIGBUILD
#define ALSA_LIB_CONFIG_H_ZIGBUILD

#define PACKAGE "alsa-lib"
#define VERSION "1.2.14"

/* Max number of sound cards (configure default). */
#define SND_MAX_CARDS 32

/* Toolchain symbol prefix: empty on ELF/Linux. */
#define __SYMBOL_PREFIX ""

/* Standard FHS locations used at runtime for config data and devices. */
#define ALSA_CONFIG_DIR "/usr/share/alsa"
#define ALSA_PLUGIN_DIR "/usr/lib/alsa-lib"
#define ALSA_DEVICE_DIRECTORY "/dev/snd/"
#define TMPDIR "/tmp"

/* libc / toolchain features available on glibc. */
#define HAVE___THREAD 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_EACCESS 1
#define HAVE_ENDIAN_H 1
#define HAVE_LFS 1
#define HAVE_LIBDL 1
#define HAVE_LIBPTHREAD 1
#define HAVE_MALLOC_H 1
#define HAVE_PTHREAD_MUTEX_RECURSIVE 1
#define HAVE_SYS_SHM_H 1
#define HAVE_USELOCALE 1
#define HAVE_WORDEXP 1

#endif /* ALSA_LIB_CONFIG_H_ZIGBUILD */
