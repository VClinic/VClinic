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
