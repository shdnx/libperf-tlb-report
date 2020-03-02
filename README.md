# What is this

This is a simple C library for collecting and reporting on TLB-related hardware performance counters. It's built for Linux x86-64, using the Linux `perf_event_open()` system call.

# Configuration and building

Configure using CMake and then build using your favoite build engine like GNU Make or Ninja. For example:

```bash
cd libperf-tlb-report
mkdir build
cd build
cmake -G Ninja -D STANDALONE=ON -D USE_GROUPS=OFF ..
ninja
```

The following cmake options are available:

## `STANDALONE`

Whether to register a constructor and destructor to automatically call `perfevents_init()` and `perfevents_finalize()`.

If `OFF`, you need to call these functions yourself in the application you're linking it to. Include `libperf-tlb-report.h` for the function prototypes.

Defaults to `ON`.

## `USE_GROUPS`

Whether to collect all performance events in a single group.

In theory, from what I understand, we would want to do this, so that the performance event counters represent the same period of code execution. In reality, this seems to cause the events to be almost never actually run. Couldn't find anything online that'd explain this or propose a fix.

Because of this, this option defaults to `OFF`.

# Usage

Configure and build as per the above to create a single static library `libperf-tlb-report.a`. Link your application to this statically, and you're done.

# Resources

 - The only good resouce on the topic I found: the [manpage for `perf_event_open()`](http://man7.org/linux/man-pages/man2/perf_event_open.2.html).
 - [StackOverflow discussion on using groups](https://stackoverflow.com/a/42092180/128240)

# License

The 3-Clause BSD License, see `LICENSE`.
