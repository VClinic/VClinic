# VClinic

A portable and efficient infrastracture for value profilers. The document for API and developing a new tool client is described in [here](https://vclinic.readthedocs.io/en/latest/index.html).

## Pre-request

- Tested System: Ubuntu 20.04 LTS
- Tested Platform: 
    - Intel(R) Xeon(R) CPU E5-2680 v4 @ 2.40GHz
    - ThunderX2 99xx (AArch64)
- Dependencies:
    - CMake >= 3.7
    - GCC/G++

## Building VClinic

Following instructions can clone and build VClinic with multiple example client tools:

```
git clone --recursive https://github.com/VClinic/VClinic.git
cd VClinic
./build.sh
```

## Testing

### Pre-request

- Python2
- GCC/GFortran

### Running benchmarks

We use Nas Parallel Benchmark (NPB 3.4.2) for evaluating the capability as well as the runtime and memory overheads of VClinic. For simplicity, we provide a script to run all the benchmarks with built-in example tools and collect the execution time and peak memory.

```
./run_benchmark.sh
```

During execution, the script will automatically download the NPB 3.4.2 benchmarks and compile for evaluation. After data collection, the raw data is located in `NPB-3.4.2/NPB3.4-OMP/run-<date>/`.