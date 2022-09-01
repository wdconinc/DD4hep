Proximity Focusing RICH
=======================

Example RICH demonstrating `Geant4OpticalTracker` Sensitive Detector plugin usage

![PFRICH](doc/geometry.png)

For testing, it is recommended to follow `../README.md` and use `ctest`. See below for guidance for
running this example standalone.

To use `ctest`, run:
```bash
cd ..  # `pwd` should now be `/DD4hep/examples`
mkdir build
cd build
cmake -DDD4HEP_EXAMPLES="OpticalTracker" .. && make && make install
ctest --output-on-failure   # or use `--verbose` to see all output
```

## Local Development
The following assumes that your current working directory is `DD4hep/examples/OpticalTracker`.

Build with `cmake`, for example:
```bash
cmake -B build -S . -D CMAKE_INSTALL_PREFIX=install
cmake --build build -- install
```

Run a test simulation:
```bash
install/bin/run_test_OpticalTracker.sh \
  python install/examples/OpticalTracker/scripts/richsim.py
```
- See default settings in `scripts/richsim.py`
- Override these settings, or add additional settings by appending `ddsim` options

Draw the hits, to show Cherenkov photon rings:
```bash
root sim.edm4hep.root
```
```cpp
// In ROOT interpreter:
events->Draw("PFRICHHits.position.y:PFRICHHits.position.x","","*");  // hit positions
events->Draw("@PFRICHHits.size()");                                  // raw number of hits per event
```

Test for the expected number of hits:
```bash
root -b -q -l scripts/test_number_of_hits.C
```
