# Investigating Otter crashes

Local Breakpad minidump generation is enabled by default. Configure with
`-DENABLE_CRASH_DUMPS=OFF` to disable it. Dumps and a JSON session metadata file
are written under the application's local data directory, in `crash-dumps`:

- Linux: `~/.local/share/Otter/crash-dumps/`
- Windows: `%LOCALAPPDATA%/Otter/crash-dumps/`

The dump contains the crashed process state. A `session-<pid>-<start-time>.json`
file records the application version, source commit, Qt version, build type, OS,
source tree status, compiler, kernel, CPU architecture, executable path, and
process start time. Otter does not include the active page URL in this metadata
or upload local dumps. A minidump can contain fragments of data held in process
memory, so treat dump files as sensitive.

For useful function names and source lines, keep the matching build's unstripped
binary and build with debug symbols. A typical CMake configuration is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_CRASH_DUMPS=ON
cmake --build build --parallel
```

Keep the exact binary and source revision used for the dump. On Linux, generate
Breakpad symbols from that unstripped binary with `dump_syms`, then run
`minidump_stackwalk <dump-file> <symbols-directory>` to turn a `.dmp` into a
stack trace. The session metadata identifies the source commit and build
configuration to use when resolving symbols.

## QtWebEngine `windows-874` renderer crash

Qt 6.11.2 with Chromium 140.0.7339.225 can crash its renderer when a page
selects the `windows-874` text codec. Pages declaring labels such as
`windows-874`, `TIS-620`, `iso-8859-11`, `cp874`, or `x-IBM874` may trigger it.
The failure is a renderer `int3`/`ud2` trap in `libQt6WebEngineCore`, not an
Otter backtrace.

Qt 6.11.3 with Chromium 140.0.7339.264 fixes the crash. Build Otter against Qt
6.11.3; do not add an ICU `LD_PRELOAD` shim or replace `QtWebEngineProcess`.
Official Qt 6.11.2 still reproduces the fault with its bundled ICU.

## HTTPS pages stay blank with Qt 6.11.3

If HTTPS pages remain blank while HTTP pages load, check the user NSS database
at `~/.pki/nssdb/pkcs11.txt`. An `onepin-opensc-pkcs11.so` module can make the
Qt 6.11.3 certificate verifier hang. Back up the file, remove the OpenSC module
stanza, keep the internal NSS and `libnssckbi.so` entries, and restart Otter.
The NSS root-cert warning can still appear; it is harmless once verification
completes.
