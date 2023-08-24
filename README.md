# toymap: Toy technology mapper

Toymap maps logic networks onto LUTs or simple logic gates. The basic principle is cutting up an And-Inverter Graph representing the desired logic function into small subgraphs such that each subgraph can be implemented with (mapped onto) a LUT or a simple gate. Technology mapping like this is a step in the compilation of logic designs to be programmed onto an FPGA or manufactured as an IC.

Some features:

 * Employs the method of "priority cuts" to come up with candidate cuts of the graph (see the reference below)

 * Attempts to preserve wire names

 * Uses internal representation capable of expressing sequential circuits

     * This isn't useful yet since only acyclic graphs can be mapped and sequential elements are ignored in depth estimation. Import, export and emission of LUTs do handle sequential circuits correctly.

 * Integrates into [Yosys](https://github.com/yosysHQ/yosys)

## Build

    yosys-config --build toymap.so toymap.cc lutdepth.cc

## References

 * A. Mishchenko, S. Cho, S. Chatterjee, and R. Brayton, "Combinational and sequential mapping with priority cuts", Proc. ICCAD '07, pp. 354-361. [PDF](https://people.eecs.berkeley.edu/~alanmi/publications/2007/iccad07_map.pdf)


## Benchmarks

Following is a result of comparing ABC (invoked with the `abc -lut N` Yosys command) against toymap on the EPFL benchmarks (the `benchmarks/` submodule). On each benchmark LUT4 and LUT6 mappings are attempted, once with ABC, once with toymap, and once with toymap after ABC preprocessed the AIG (labeled abc+toymap). See `run_benchmark.tcl` for detailed commands.

Results as of toymap commit `f1f44b3`:


Benchmark | abc LUT6 area | toymap LUT6 area | toymap/abc relative area | abc+toymap LUT6 area | abc+toymap/abc relative area | abc LUT6 depth | toymap LUT6 depth | abc+toymap LUT6 depth | abc LUT4 area | toymap LUT4 area | toymap/abc relative area | abc+toymap LUT4 area | abc+toymap/abc relative area | abc LUT4 depth | toymap LUT4 depth | abc+toymap LUT4 depth | extra toymap args
---|---|---|---|--|--|--|--|--|--|--|--|--|--|--|--|--|--
arithmetic/adder.aig | 339 | 339 | 100.0% | 339 | 100.0% | 85 | 85 | 85 | 274 | 254 | 92.7% | 254 | 92.7% | 51 | 51 | 51 | 
arithmetic/bar.aig | 1156 | 1354 | 117.1% | 1406 | 121.6% | 6 | 6 | 6 | 512 | 512 | 100.0% | 512 | 100.0% | 4 | 4 | 4 | 
arithmetic/div.aig | 6551 | 27315 | 417.0% | 9361 | 142.9% | 1437 | 1443 | 1439 | 5048 | 22367 | 443.1% | 8083 | 160.1% | 860 | 866 | 864 | -no_exact_area
arithmetic/hyp.aig | 64232 | 81013 | 126.1% | 80572 | 125.4% | 8254 | 8259 | 8255 | 44985 | 48902 | 108.7% | 48843 | 108.6% | 4193 | 4199 | 4195 | 
arithmetic/log2.aig | 10661 | 11627 | 109.1% | 12445 | 116.7% | 126 | 135 | 126 | 7880 | 9340 | 118.5% | 10196 | 129.4% | 70 | 77 | 70 | 
arithmetic/max.aig | 1013 | 1131 | 111.6% | 1401 | 138.3% | 67 | 96 | 67 | 799 | 840 | 105.1% | 817 | 102.3% | 40 | 56 | 40 | 
arithmetic/multiplier.aig | 7463 | 9706 | 130.1% | 9751 | 130.7% | 87 | 87 | 87 | 5880 | 7419 | 126.2% | 7530 | 128.1% | 53 | 53 | 53 | 
arithmetic/sin.aig | 1968 | 2133 | 108.4% | 2168 | 110.2% | 56 | 69 | 56 | 1450 | 1595 | 110.0% | 1570 | 108.3% | 36 | 42 | 36 | 
arithmetic/sqrt.aig | 4529 | 10792 | 238.3% | 6933 | 153.1% | 1995 | 2015 | 1995 | 3183 | 5674 | 178.3% | 4238 | 133.1% | 1017 | 1033 | 1017 | -no_exact_area
arithmetic/square.aig | 6248 | 6970 | 111.6% | 7371 | 118.0% | 83 | 84 | 83 | 3928 | 3805 | 96.9% | 3810 | 97.0% | 50 | 50 | 50 | 
random_control/arbiter.aig | 4068 | 4267 | 104.9% | 4265 | 104.8% | 30 | 30 | 30 | 2719 | 2722 | 100.1% | 2722 | 100.1% | 18 | 18 | 18 | 
random_control/cavlc.aig | 269 | 305 | 113.4% | 306 | 113.8% | 6 | 6 | 6 | 107 | 121 | 113.1% | 122 | 114.0% | 4 | 4 | 4 | 
random_control/ctrl.aig | 52 | 61 | 117.3% | 55 | 105.8% | 3 | 3 | 3 | 29 | 28 | 96.6% | 28 | 96.6% | 2 | 2 | 2 | 
random_control/dec.aig | 288 | 288 | 100.0% | 288 | 100.0% | 2 | 2 | 2 | 287 | 280 | 97.6% | 272 | 94.8% | 2 | 2 | 2 | 
random_control/i2c.aig | 434 | 552 | 127.2% | 486 | 112.0% | 5 | 7 | 5 | 303 | 385 | 127.1% | 344 | 113.5% | 3 | 4 | 3 | 
random_control/int2float.aig | 76 | 100 | 131.6% | 88 | 115.8% | 6 | 6 | 6 | 41 | 49 | 119.5% | 47 | 114.6% | 4 | 3 | 4 | 
random_control/mem_ctrl.aig | 14867 | 19972 | 134.3% | 18870 | 126.9% | 36 | 40 | 37 | 9202 | 13626 | 148.1% | 13545 | 147.2% | 22 | 25 | 22 | 
random_control/priority.aig | 169 | 369 | 218.3% | 282 | 166.9% | 43 | 62 | 51 | 127 | 259 | 203.9% | 192 | 151.2% | 26 | 31 | 26 | -no_exact_area
random_control/router.aig | 72 | 99 | 137.5% | 96 | 133.3% | 9 | 18 | 9 | 40 | 75 | 187.5% | 65 | 162.5% | 6 | 11 | 6 | 
random_control/voter.aig | 2236 | 4177 | 186.8% | 2432 | 108.8% | 17 | 23 | 18 | 1461 | 2741 | 187.6% | 1525 | 104.4% | 12 | 17 | 13 | 



## Copyright notice

Except for the `benchmarks/` submodule, the code is:

Copyright 2023 Martin Povišer

No explicit license assigned at this point
