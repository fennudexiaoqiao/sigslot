# sigslot

Lightweight signal/slot header-only library (local repo).

Build with CMake
 - Default builds a static library. To select library type set `SIGSLOT_LIBRARY_TYPE` to `STATIC`, `SHARED` or `INTERFACE`.

Examples:

As subdirectory (inside your project's top-level CMakeLists):

 - add_subdirectory(path/to/sigslot)
 - target_link_libraries(your_target PRIVATE sigslot::sigslot)

Standalone configure & build (example):

```powershell
cmake -S . -B build -DSIGSLOT_LIBRARY_TYPE=STATIC
cmake --build build --config Release
```

If `INTERFACE` is chosen the target `sigslot` is an interface target and only headers are used.
