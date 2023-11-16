# allocation-compressibility-tracker

This is a tool for tracking the compressibility of heap allocations on timed intervals for various executables by dynamically linking in `malloc`, `free`, and `mmap` hooks.

## Results

[Here's a Jupyter Notebook with some analysis from this tool.](./results.ipynb)

## Usage

#### Compiling

To build the allocator, run the `build-allocator.sh` script.

```bash
$ git clone https://github.com/adam-mcdaniel/allocation-compressibility-tracker
$ cd allocation-compressibility-tracker
$ ./build-allocator.sh
```

#### Running

To run the compressibility tracker on an executable, use the following environment variables:

|Environment Variable|Value|Purpose|
|:-:|:-:|:-:|
|`LD_PRELOAD`|`/path/to/compiled/libbkmalloc.so`|This dynamically loads an allocator called ["bkmalloc"](https://github.com/kammerdienerb/bkmalloc) created by [Brandom Kammerdiener](https://github.com/kammerdienerb) to call hooks from another dynamic library when allocations are made or freed. I use this to call my hook, which does the actual logic for performing the interval test.|
|`BKMALLOC_OPTS`|`"--hooks-file=/path/to/compiled/hook.so --log-hooks"`|This tells bkmalloc to use my allocation tracker compiled to `hook.so`.|


##### Example Usage

Here is a program `tests/pagetest.exe` being called with the allocation tracker.
I recommend creating a script, like [run.sh](./run.sh) to call the tracker on your executables.

```bash
$ # `sudo` permissions are required to open the pagemap for the process, this part of the tracker can be disabled internally
$ sudo LD_PRELOAD=./libbkmalloc.so BKMALLOC_OPTS="--hooks-file=./hook.so" tests/pagetest.exe
```

#### Adding Tests

You can create a test by inheriting from the `IntervalTest` object, and adding it to the `IntervalTestSuite` for the hook.

In the main hook code, in [src/hook.cpp](src/hook.cpp), the main test is added like so:

```c++
// The compression test which inherits from `IntervalTest` (add your own tests here, too!)
static CompressionTest ct;
// The interval test suite for the hook (don't change)
static IntervalTestSuite its;

class Hooks {
public:
    Hooks() {
        stack_debugf("Hooks constructor\n");

        stack_debugf("Adding test...\n");
        // (Add your test to the hook in the constructor by calling `add_test` with a pointer to your interval test!)
        its.add_test(&ct);
        stack_debugf("Done\n");
    }
```

And that's all!

An interval test has the following methods you can overload to implement your test on an interval.

```c++
    // The name of the test
    virtual const char *name() const {
        return "Base IntervalTest";
    }

    // A virtual method for setting up the test
    // This is run once on startup. Do your initialization here
    virtual void setup() {}
    // A virtual method for cleaning up the test
    // This is run once on shutdown. Do your cleanup here
    virtual void cleanup() {}

    // A virtual method for running the test's interval
    virtual void interval(
        const StackMap<uintptr_t, AllocationSite, TRACKED_ALLOCATION_SITES> &allocation_sites,
        const StackVec<Allocation, TOTAL_TRACKED_ALLOCATIONS> &allocations
    ) {}

    virtual void schedule() {}
```