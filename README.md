# Prototype Deephaven Julia Client

This is an experimental demonstration of how a Julia client for
[Deephaven Community Core](https://github.com/deephaven/deephaven-core)
might work.

It wraps the C++ client.

## Building

### Building `libcxxwrap-julia`

First, follow the instructions to build `libcxxwrap-julia`
(repeated here for convenience, but also available in full [on the repo](https://github.com/JuliaInterop/libcxxwrap-julia)).

Note that you need to change `home/user/path/to/julia` to the path to your Julia install.

```bash
git clone https://github.com/JuliaInterop/libcxxwrap-julia.git
mkdir libcxxwrap-julia-build
cd libcxxwrap-julia-build
cmake -DJulia_PREFIX=home/user/path/to/julia ../libcxxwrap-julia
cmake --build . --config Release
cd ..
```

### Building the C++ client

Follow the instructions to build the Deephaven C++ client located
[here](https://github.com/deephaven/deephaven-core/tree/main/cpp-client).

The `CMAKE_PREFIX_PATH` environment variable is relevant and needs to remain set in the next step.

### Building the wrapper

From the base of the repository, run the following commands to build the C++ wrapper that the Julia client will use.
Ensure that the `CMAKE_PREFIX_PATH` variable is set the same as it was when building the C++ client.
This step has to be rerun every time the wrapper changes.

Due to some build system issue, you will also need to specify the path to the C++ client source to cmake.
Change the `-DDHCLIENT_DIR` argument as appropriate.

```bash
mkdir build && cd build
export JlCxx_DIR=../libcxxwrap-julia-build/
cmake -DDHCLIENT_DIR=/some/path/to/deephaven-core/cpp-client/deephaven/ .. && make
cd ..
```

### Running the Julia client

First, ensure that [the CxxWrap package](https://github.com/JuliaInterop/CxxWrap.jl) is installed.
Then, simply run the script:

```bash
julia main.jl
```