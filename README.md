# toymap: Toy technology mapper

Toymap maps logic networks onto LUTs or simple logic gates. The basic principle is cutting up an And-Inverter Graph representing the desired logic function into small subgraphs such that each subgraph can be implemented with (mapped onto) a LUT or a simple gate. Technology mapping like this is a step in the compilation of logic designs to be programmed onto an FPGA or manufactured as an IC.

Some features:

 * Employs the method of "priority cuts" to come up with candidate cuts of the graph (see the reference below)

 * Attempts to preserve wire names

 * Uses internal representation capable of expressing sequential circuits

     * This isn't useful yet since only acyclic graphs can be mapped and sequential elements are ignored in depth estimation. Import, export and emission of LUTs do handle sequential circuits correctly.

 * Integrates into [Yosys](https://github.com/yosysHQ/yosys)

## Build

    yosys-config --build toymap.so toymap.cc lutdepth.cc post.cc

## References

 * A. Mishchenko, S. Cho, S. Chatterjee, and R. Brayton, "Combinational and sequential mapping with priority cuts", Proc. ICCAD '07, pp. 354-361. [PDF](https://people.eecs.berkeley.edu/~alanmi/publications/2007/iccad07_map.pdf)


## Benchmarks

Following is a result of comparing ABC (invoked with the `abc -lut N` Yosys command) against toymap on the EPFL benchmarks (the `benchmarks/` submodule). On each benchmark LUT4 and LUT6 mappings are attempted, once with ABC, once with toymap, and once with toymap after ABC preprocessed the AIG (labeled abc+toymap). See `run_benchmark.tcl` for detailed commands.

Results as of toymap commit `5a381a6`:


Benchmark | abc LUT4 area | toymap LUT4 area | toymap/abc relative area | abc+toymap LUT4 area | abc+toymap/abc relative area | abc LUT4 depth | toymap LUT4 depth | abc+toymap LUT4 depth | abc LUT6 area | toymap LUT6 area | toymap/abc relative area | abc+toymap LUT6 area | abc+toymap/abc relative area | abc LUT6 depth | toymap LUT6 depth | abc+toymap LUT6 depth | extra toymap args
---|---|---|---|--|--|--|--|--|--|--|--|--|--|--|--|--|--
arithmetic/adder.aig | 339 | 339 | 100.0% | 339 | 100.0% | 85 | 85 | 85 | 274 | 254 | 92.7% | 254 | 92.7% | 51 | 51 | 51 | 
arithmetic/bar.aig | 1156 | 1348 | 116.6% | 1348 | 116.6% | 6 | 6 | 6 | 512 | 512 | 100.0% | 512 | 100.0% | 4 | 4 | 4 | 
arithmetic/div.aig | 6551 | 26085 | 398.2% | 6880 | 105.0% | 1437 | 1443 | 1437 | 5048 | 19983 | 395.9% | 5867 | 116.2% | 860 | 864 | 860 | 
arithmetic/hyp.aig | 64232 | 67522 | 105.1% | 66908 | 104.2% | 8254 | 8259 | 8254 | 44985 | 45057 | 100.2% | 45026 | 100.1% | 4193 | 4198 | 4195 | 
arithmetic/log2.aig | 10661 | 10307 | 96.7% | 10925 | 102.5% | 126 | 135 | 126 | 7880 | 8245 | 104.6% | 8726 | 110.7% | 70 | 77 | 70 | 
arithmetic/max.aig | 1013 | 1077 | 106.3% | 1105 | 109.1% | 67 | 95 | 67 | 799 | 834 | 104.4% | 809 | 101.3% | 40 | 56 | 40 | 
arithmetic/multiplier.aig | 7463 | 9041 | 121.1% | 7970 | 106.8% | 87 | 87 | 87 | 5880 | 6413 | 109.1% | 7041 | 119.7% | 53 | 53 | 53 | 
arithmetic/sin.aig | 1968 | 1928 | 98.0% | 2029 | 103.1% | 56 | 69 | 56 | 1450 | 1446 | 99.7% | 1506 | 103.9% | 36 | 42 | 36 | 
arithmetic/sqrt.aig | 4529 | 8883 | 196.1% | 5065 | 111.8% | 1995 | 2015 | 1995 | 3183 | 5066 | 159.2% | 3700 | 116.2% | 1017 | 1033 | 1017 | 
arithmetic/square.aig | 6248 | 6654 | 106.5% | 6901 | 110.5% | 83 | 84 | 83 | 3928 | 3642 | 92.7% | 3632 | 92.5% | 50 | 50 | 50 | 
random_control/arbiter.aig | 4068 | 4267 | 104.9% | 4267 | 104.9% | 30 | 30 | 30 | 2719 | 2722 | 100.1% | 2722 | 100.1% | 18 | 18 | 18 | 
random_control/cavlc.aig | 269 | 299 | 111.2% | 300 | 111.5% | 6 | 6 | 6 | 107 | 120 | 112.1% | 117 | 109.3% | 4 | 4 | 4 | 
random_control/ctrl.aig | 52 | 54 | 103.8% | 54 | 103.8% | 3 | 3 | 3 | 29 | 28 | 96.6% | 28 | 96.6% | 2 | 2 | 2 | 
random_control/dec.aig | 288 | 288 | 100.0% | 288 | 100.0% | 2 | 2 | 2 | 287 | 272 | 94.8% | 272 | 94.8% | 2 | 2 | 2 | 
random_control/i2c.aig | 434 | 542 | 124.9% | 475 | 109.4% | 5 | 7 | 5 | 303 | 364 | 120.1% | 332 | 109.6% | 3 | 4 | 3 | 
random_control/int2float.aig | 76 | 98 | 128.9% | 86 | 113.2% | 6 | 6 | 6 | 41 | 49 | 119.5% | 46 | 112.2% | 4 | 3 | 4 | 
random_control/mem_ctrl.aig | 14867 | 19047 | 128.1% | 18042 | 121.4% | 36 | 40 | 37 | 9202 | 12449 | 135.3% | 11940 | 129.8% | 22 | 25 | 22 | 
random_control/priority.aig | 169 | 323 | 191.1% | 258 | 152.7% | 43 | 62 | 51 | 127 | 253 | 199.2% | 189 | 148.8% | 26 | 31 | 26 | 
random_control/router.aig | 72 | 103 | 143.1% | 92 | 127.8% | 9 | 18 | 9 | 40 | 67 | 167.5% | 59 | 147.5% | 6 | 11 | 6 | 
random_control/voter.aig | 2236 | 4106 | 183.6% | 2421 | 108.3% | 17 | 23 | 18 | 1461 | 2481 | 169.8% | 1472 | 100.8% | 12 | 17 | 13 | 



## Copyright notice

Except for the `benchmarks/` submodule, the code is:

Copyright 2023 Martin Povišer

No explicit license assigned at this point
