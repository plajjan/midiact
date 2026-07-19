const std = @import("std");

// Vendored build of alsa-lib as a static library. It is a MIDI-focused
// configuration: only the sequencer (snd_seq_*) and
// rawmidi subsystems plus the core config machinery they need are compiled, the
// equivalent of `configure --disable-pcm --disable-mixer --disable-hwdep
// --disable-ucm --disable-topology`. Those subsystems are already guarded by
// #ifdef BUILD_* in the sources, so nothing needs patching; we simply do not
// define their BUILD_* macros.
//
// The archive is built non-PIC / non-versioned. In that mode alsa-lib registers
// its builtin modules (seq_hw, ...) through constructor functions that populate
// the internal snd_dlsym_start list (see SND_DLSYM_BUILD_VERSION in
// include/global.h) and force-keeps them via the `#ifndef PIC` object lists in
// *_symbols.c. That is exactly how distributions build a static libasound.a and
// needs no --export-dynamic at the final link: symbol lookup walks the
// in-process list rather than calling dlsym(RTLD_DEFAULT, ...). Acton links its
// Linux executables non-PIE, so the non-PIC archive drops straight in.

const cflags = [_][]const u8{
    "-D_GNU_SOURCE", // eaccess, uselocale, wordexp, ...
    "-DBUILD_SEQ",
    "-DBUILD_RAWMIDI",
    "-std=gnu11",
    "-Wno-deprecated-declarations",
    "-Wno-unused-result",
};

// Config machinery + sequencer. Files whose only role is a backend we do not
// build are left out (async's pcm/ctl handlers, etc.); this list grows only as
// far as the snd_seq_open link closure demands.
const sources = [_][]const u8{
    // core: config parsing, dynamic-symbol registry, I/O helpers
    "src/async.c",
    "src/conf.c",
    "src/confeval.c",
    "src/confmisc.c",
    "src/dlmisc.c",
    "src/error.c",
    "src/input.c",
    "src/output.c",
    "src/names.c",
    "src/userfile.c",
    "src/socket.c",
    "src/shmarea.c",
    // control: config card lookups (confmisc) and hook_load_for_all_cards (conf)
    // reach snd_ctl_open / snd_card_next. Only the hw + empty builtin ctl modules
    // are compiled; the optional remap/shm/ext plugins are omitted (ext would drag
    // in the pcm subsystem) via the empty include/ctl_symbols_list.c.
    "src/control/control.c",
    "src/control/control_symbols.c",
    "src/control/control_hw.c",
    "src/control/control_empty.c",
    "src/control/cards.c",
    "src/control/ctlparse.c",
    "src/control/hcontrol.c",
    "src/control/setup.c",
    "src/control/tlv.c",
    "src/control/eld.c",
    // sequencer (the API RtMidi's ALSA backend uses)
    "src/seq/seq.c",
    "src/seq/seq_event.c",
    "src/seq/seq_hw.c",
    "src/seq/seq_midi_event.c",
    "src/seq/seq_old.c",
    "src/seq/seq_symbols.c",
    "src/seq/seqmid.c",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const upstream = b.dependency("alsa_upstream", .{});

    // Upstream's autotools build makes <alsa/...> resolve to include/ via an
    // `alsa -> include` symlink created at build time. The fetched upstream is
    // read-only, so synthesize the same prefix in a generated directory: a copy
    // of include/ placed under alsa/, with our generated headers layered on top
    // (upstream ships none of asoundlib.h/version.h — they are generated).
    const gen = b.addWriteFiles();
    _ = gen.addCopyDirectory(upstream.path("include"), "alsa", .{});
    _ = gen.addCopyFile(b.path("include/config.h"), "alsa/config.h");
    _ = gen.addCopyFile(b.path("include/version.h"), "alsa/version.h");
    _ = gen.addCopyFile(b.path("include/asoundlib.h"), "alsa/asoundlib.h");
    const alsa_prefix = gen.getDirectory();

    const lib = b.addLibrary(.{
        .name = "asound",
        .linkage = .static,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    lib.root_module.addCSourceFiles(.{
        .root = upstream.path("."),
        .files = &sources,
        .flags = &cflags,
    });
    // Include search order: our generated config.h/version.h/asoundlib.h first,
    // then upstream headers for bare "local.h" / "seq.h" / <sound/asound.h>,
    // then the synthesized alsa/ prefix for <alsa/sound/type_compat.h> etc.
    lib.root_module.addIncludePath(b.path("include"));
    lib.root_module.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(alsa_prefix);
    lib.root_module.linkSystemLibrary("dl", .{});
    lib.root_module.linkSystemLibrary("pthread", .{});
    lib.root_module.linkSystemLibrary("m", .{});

    b.installArtifact(lib);
    // Public API for consumers (RtMidi.cpp does `#include <alsa/asoundlib.h>`).
    lib.installHeadersDirectory(upstream.path("include"), "alsa", .{});
    lib.installHeader(b.path("include/asoundlib.h"), "alsa/asoundlib.h");
    lib.installHeader(b.path("include/version.h"), "alsa/version.h");

    // Optional link-closure check: `zig build -Dtest -Dtarget=x86_64-linux-gnu`
    // links a tiny program that opens the sequencer, forcing the linker to
    // resolve the whole snd_seq_open closure against the archive.
    const want_test = b.option(bool, "test", "Build the snd_seq_open link-closure check") orelse false;
    if (want_test) {
        const t = b.addExecutable(.{
            .name = "seq_link_test",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });
        t.root_module.addCSourceFile(.{ .file = b.path("zig-seq-link-test.c"), .flags = &.{} });
        t.root_module.linkLibrary(lib);
        b.installArtifact(t);
    }
}
