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

Results as of toymap commit `f83a169`:


Benchmark | abc LUT4 area | toymap LUT4 area | toymap/abc relative area | abc+toymap LUT4 area | abc+toymap/abc relative area | abc LUT4 depth | toymap LUT4 depth | abc+toymap LUT4 depth | abc LUT6 area | toymap LUT6 area | toymap/abc relative area | abc+toymap LUT6 area | abc+toymap/abc relative area | abc LUT6 depth | toymap LUT6 depth | abc+toymap LUT6 depth | extra toymap args
---|---|---|---|--|--|--|--|--|--|--|--|--|--|--|--|--|--
arithmetic/adder.aig | 339 | 339 | 100.0% | 339 | 100.0% | 85 | 85 | 85 | 274 | 254 | 92.7% | 254 | 92.7% | 51 | 51 | 51 | 
arithmetic/bar.aig | 1156 | 1348 | 116.6% | 1348 | 116.6% | 6 | 6 | 6 | 512 | 512 | 100.0% | 512 | 100.0% | 4 | 4 | 4 | 
arithmetic/div.aig | 6551 | 26146 | 399.1% | 6868 | 104.8% | 1437 | 1443 | 1437 | 5048 | 19981 | 395.8% | 5789 | 114.7% | 860 | 864 | 860 | 
arithmetic/hyp.aig | 64232 | 67430 | 105.0% | 66879 | 104.1% | 8254 | 8259 | 8254 | 44985 | 45070 | 100.2% | 44925 | 99.9% | 4193 | 4198 | 4195 | 
arithmetic/log2.aig | 10661 | 10918 | 102.4% | 10905 | 102.3% | 126 | 126 | 126 | 7880 | 8538 | 108.4% | 8682 | 110.2% | 70 | 72 | 70 | 
arithmetic/max.aig | 1013 | 1018 | 100.5% | 1105 | 109.1% | 67 | 76 | 67 | 799 | 778 | 97.4% | 808 | 101.1% | 40 | 44 | 40 | 
arithmetic/multiplier.aig | 7463 | 9042 | 121.2% | 7970 | 106.8% | 87 | 87 | 87 | 5880 | 6950 | 118.2% | 7043 | 119.8% | 53 | 53 | 53 | 
arithmetic/sin.aig | 1968 | 1945 | 98.8% | 2039 | 103.6% | 56 | 60 | 56 | 1450 | 1464 | 101.0% | 1508 | 104.0% | 36 | 36 | 36 | 
arithmetic/sqrt.aig | 4529 | 8909 | 196.7% | 5065 | 111.8% | 1995 | 2015 | 1995 | 3183 | 5064 | 159.1% | 3700 | 116.2% | 1017 | 1033 | 1017 | 
arithmetic/square.aig | 6248 | 6924 | 110.8% | 6902 | 110.5% | 83 | 84 | 83 | 3928 | 3643 | 92.7% | 3639 | 92.6% | 50 | 50 | 50 | 
random_control/arbiter.aig | 4068 | 4267 | 104.9% | 4267 | 104.9% | 30 | 30 | 30 | 2719 | 2722 | 100.1% | 2722 | 100.1% | 18 | 18 | 18 | 
random_control/cavlc.aig | 269 | 306 | 113.8% | 302 | 112.3% | 6 | 6 | 6 | 107 | 120 | 112.1% | 121 | 113.1% | 4 | 4 | 4 | 
random_control/ctrl.aig | 52 | 55 | 105.8% | 55 | 105.8% | 3 | 3 | 3 | 29 | 28 | 96.6% | 28 | 96.6% | 2 | 2 | 2 | 
random_control/dec.aig | 288 | 288 | 100.0% | 288 | 100.0% | 2 | 2 | 2 | 287 | 282 | 98.3% | 272 | 94.8% | 2 | 2 | 2 | 
random_control/i2c.aig | 434 | 546 | 125.8% | 476 | 109.7% | 5 | 6 | 5 | 303 | 364 | 120.1% | 334 | 110.2% | 3 | 4 | 3 | 
random_control/int2float.aig | 76 | 97 | 127.6% | 86 | 113.2% | 6 | 6 | 6 | 41 | 51 | 124.4% | 46 | 112.2% | 4 | 3 | 4 | 
random_control/mem_ctrl.aig | 14867 | 19061 | 128.2% | 18154 | 122.1% | 36 | 40 | 36 | 9202 | 12443 | 135.2% | 11976 | 130.1% | 22 | 25 | 22 | 
random_control/priority.aig | 169 | 325 | 192.3% | 258 | 152.7% | 43 | 62 | 51 | 127 | 249 | 196.1% | 188 | 148.0% | 26 | 31 | 26 | 
random_control/router.aig | 72 | 101 | 140.3% | 90 | 125.0% | 9 | 10 | 9 | 40 | 74 | 185.0% | 59 | 147.5% | 6 | 7 | 6 | 
random_control/voter.aig | 2236 | 3924 | 175.5% | 2421 | 108.3% | 17 | 23 | 18 | 1461 | 2569 | 175.8% | 1472 | 100.8% | 12 | 16 | 13 | 


## Copyright notice

Except for the `benchmarks/` submodule, the code is:

Copyright 2023 Martin Povišer

No explicit license assigned at this point
