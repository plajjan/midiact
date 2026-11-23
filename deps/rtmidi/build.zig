const std = @import("std");

// Vendored build of RtMidi as a static library. RtMidi uses other libraries,
// depending on the compilation target:
//
//   macOS    -> CoreMIDI       (-D__MACOSX_CORE__, Core* frameworks)
//   Linux    -> ALSA sequencer (-D__LINUX_ALSA__,  vendored static libasound)
//   Windows  -> WinMM          (-D__WINDOWS_MM__,  winmm)
//
// Only the C wrapper (rtmidi_c.*) needs to be reachable from Acton's ext.c, but
// it depends on the C++ core (RtMidi.*), so both translation units are compiled
// into one static `rtmidi` artifact. Backend libraries/frameworks are linked
// onto the module so they propagate to whatever links this library.
//
// On Linux the ALSA backend is satisfied by the sibling deps/alsa shim (a
// vendored static libasound), so no system libasound / libasound2-dev is needed
// to build. See deps/alsa/build.zig.
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const os_tag = target.result.os.tag;
    const upstream = b.dependency("rtmidi_upstream", .{});

    const lib = b.addLibrary(.{
        .name = "rtmidi",
        .linkage = .static,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    // RtMidi is C++11. The backend define must match the libraries linked below.
    // Web MIDI / JACK / UWP / Android backends are intentionally omitted.
    const cxxflags: []const []const u8 = switch (os_tag) {
        .macos => &.{ "-std=c++11", "-D__MACOSX_CORE__" },
        .windows => &.{ "-std=c++11", "-D__WINDOWS_MM__" },
        else => &.{ "-std=c++11", "-D__LINUX_ALSA__" },
    };

    const sources = [_][]const u8{ "RtMidi.cpp", "rtmidi_c.cpp" };
    lib.root_module.addCSourceFiles(.{
        .root = upstream.path("."),
        .files = &sources,
        .flags = cxxflags,
    });
    lib.root_module.addIncludePath(upstream.path("."));
    lib.root_module.link_libc = true;
    lib.root_module.link_libcpp = true;

    switch (os_tag) {
        .macos => {
            // RtMidi.cpp includes framework headers and regular SDK headers.
            if (std.zig.system.darwin.getSdk(b.allocator, b.graph.io, &target.result)) |sdk| {
                lib.root_module.addSystemFrameworkPath(.{
                    .cwd_relative = b.pathJoin(&.{ sdk, "System/Library/Frameworks" }),
                });
                lib.root_module.addSystemIncludePath(.{
                    .cwd_relative = b.pathJoin(&.{ sdk, "usr/include" }),
                });
            }
            // Framework names propagate to consumers. The native macOS target
            // lets Zig find the framework search path at the final link.
            lib.root_module.linkFramework("CoreServices", .{});
            lib.root_module.linkFramework("CoreAudio", .{});
            lib.root_module.linkFramework("CoreMIDI", .{});
            lib.root_module.linkFramework("CoreFoundation", .{});
        },
        .windows => lib.root_module.linkSystemLibrary("winmm", .{}),
        // Linux and other ALSA hosts: link the vendored static libasound. Its
        // installed headers (<alsa/asoundlib.h>) and archive propagate to
        // whatever links rtmidi, so no system ALSA is required to build.
        else => {
            if (b.lazyDependency("alsa", .{
                .target = target,
                .optimize = optimize,
            })) |alsa_dep| {
                lib.root_module.linkLibrary(alsa_dep.artifact("asound"));
            }
        },
    }

    b.installArtifact(lib);
    lib.installHeader(upstream.path("RtMidi.h"), "RtMidi.h");
    lib.installHeader(upstream.path("rtmidi_c.h"), "rtmidi_c.h");
}
