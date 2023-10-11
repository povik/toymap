# toymap: Toy technology mapper

Toymap maps logic networks onto LUTs or simple logic gates. The basic principle is cutting up an And-Inverter Graph representing the desired logic function into small subgraphs such that each subgraph can be implemented with (mapped onto) a LUT or a simple gate. Technology mapping like this is a step in the compilation of logic designs to be programmed onto an FPGA or manufactured as an IC.

Some features:

 * Employs the method of "priority cuts" to come up with candidate cuts of the graph (see the reference below)

 * Attempts to preserve wire names

 * Uses internal representation capable of expressing sequential circuits

     * This isn't useful yet since only acyclic graphs can be mapped and sequential elements are ignored in depth estimation. Import, export and emission of LUTs do handle sequential circuits correctly.

 * Integrates into [Yosys](https://github.com/yosysHQ/yosys)

 * Comes with a pass for basic LUT4 graph rewriting

## Build

    python3 pmgen.py lutcuts.pmg > lutcuts_pmg.h && yosys-config --build toymap.so toymap.cc lutdepth.cc post.cc

## References

 * A. Mishchenko, S. Cho, S. Chatterjee, and R. Brayton, "Combinational and sequential mapping with priority cuts", Proc. ICCAD '07, pp. 354-361. [PDF](https://people.eecs.berkeley.edu/~alanmi/publications/2007/iccad07_map.pdf)

 * S. Jang, B. Chan, K. Chung, and A. Mishchenko, "WireMap: FPGA technology mapping for improved routability". Proc. FPGA '08, pp. 47-55. [PDF](https://people.eecs.berkeley.edu/~alanmi/publications/2008/fpga08_wmap.pdf)

## Benchmarks

Following is a result of comparing ABC (invoked with the `abc -lut N` Yosys command) against toymap on the EPFL benchmarks (the `benchmarks/` submodule). On each benchmark LUT4 and LUT6 mappings are attempted, once with ABC, once with toymap, and once with toymap after ABC preprocessed the AIG (labeled abc+toymap). See `run_benchmark.tcl` for detailed commands.

Results as of toymap commit `8587180`:


Benchmark | abc LUT4 area | toymap LUT4 area | toymap/abc relative area | abc+toymap LUT4 area | abc+toymap/abc relative area | abc LUT4 depth | toymap LUT4 depth | abc+toymap LUT4 depth | abc LUT6 area | toymap LUT6 area | toymap/abc relative area | abc+toymap LUT6 area | abc+toymap/abc relative area | abc LUT6 depth | toymap LUT6 depth | abc+toymap LUT6 depth | extra toymap args (LUT4) | extra toymap args(LUT6)
---|---|---|---|--|--|--|--|--|--|--|--|--|--|--|--|--|--|--
arithmetic/adder.aig | 339 | 339 | 100.0% | 339 | 100.0% | 85 | 85 | 85 | 274 | 268 | 97.8% | 268 | 97.8% | 51 | 51 | 51 |  | 
arithmetic/bar.aig | 1156 | 1280 | 110.7% | 1156 | 100.0% | 6 | 6 | 6 | 512 | 512 | 100.0% | 512 | 100.0% | 4 | 4 | 4 |  | 
arithmetic/div.aig | 6551 | 25931 | 395.8% | 6635 | 101.3% | 1437 | 1443 | 1437 | 5048 | 22014 | 436.1% | 5261 | 104.2% | 860 | 864 | 860 |  | 
arithmetic/hyp.aig | 64232 | 63752 | 99.3% | 63619 | 99.0% | 8254 | 8259 | 8254 | 44985 | 47045 | 104.6% | 47401 | 105.4% | 4193 | 4198 | 4195 |  | 
arithmetic/log2.aig | 10661 | 10075 | 94.5% | 10034 | 94.1% | 126 | 126 | 126 | 7880 | 7747 | 98.3% | 7789 | 98.8% | 70 | 72 | 70 |  | 
arithmetic/max.aig | 1013 | 982 | 96.9% | 997 | 98.4% | 67 | 76 | 67 | 799 | 774 | 96.9% | 785 | 98.2% | 40 | 44 | 40 |  | 
arithmetic/multiplier.aig | 7463 | 7439 | 99.7% | 7428 | 99.5% | 87 | 87 | 87 | 5880 | 5828 | 99.1% | 5981 | 101.7% | 53 | 53 | 53 |  | 
arithmetic/sin.aig | 1968 | 1878 | 95.4% | 1921 | 97.6% | 56 | 60 | 56 | 1450 | 1407 | 97.0% | 1459 | 100.6% | 36 | 36 | 36 |  | 
arithmetic/sqrt.aig | 4529 | 8450 | 186.6% | 4399 | 97.1% | 1995 | 2015 | 1995 | 3183 | 5682 | 178.5% | 3299 | 103.6% | 1017 | 1033 | 1017 |  | 
arithmetic/square.aig | 6248 | 6403 | 102.5% | 6400 | 102.4% | 83 | 84 | 83 | 3928 | 3806 | 96.9% | 3902 | 99.3% | 50 | 50 | 50 |  | 
random_control/arbiter.aig | 4068 | 4222 | 103.8% | 4245 | 104.4% | 30 | 30 | 30 | 2719 | 2722 | 100.1% | 2722 | 100.1% | 18 | 18 | 18 |  | 
random_control/cavlc.aig | 269 | 281 | 104.5% | 279 | 103.7% | 6 | 6 | 6 | 107 | 100 | 93.5% | 99 | 92.5% | 4 | 4 | 4 |  | 
random_control/ctrl.aig | 52 | 50 | 96.2% | 52 | 100.0% | 3 | 3 | 3 | 29 | 28 | 96.6% | 29 | 100.0% | 2 | 2 | 2 |  | 
random_control/dec.aig | 288 | 288 | 100.0% | 288 | 100.0% | 2 | 2 | 2 | 287 | 282 | 98.3% | 272 | 94.8% | 2 | 2 | 2 |  | 
random_control/i2c.aig | 434 | 513 | 118.2% | 457 | 105.3% | 5 | 6 | 5 | 303 | 344 | 113.5% | 312 | 103.0% | 3 | 4 | 3 |  | 
random_control/int2float.aig | 76 | 82 | 107.9% | 81 | 106.6% | 6 | 6 | 6 | 41 | 42 | 102.4% | 41 | 100.0% | 4 | 3 | 4 |  | 
random_control/mem_ctrl.aig | 14867 | 16730 | 112.5% | 16735 | 112.6% | 36 | 40 | 36 | 9202 | 11202 | 121.7% | 11002 | 119.6% | 22 | 25 | 22 |  | 
random_control/priority.aig | 169 | 218 | 129.0% | 202 | 119.5% | 43 | 60 | 36 | 127 | 214 | 168.5% | 174 | 137.0% | 26 | 31 | 26 |  | 
random_control/priority.aig | 169 | 218 | 129.0% | 192 | 113.6% | 43 | 60 | 43 | 127 | 214 | 168.5% | 173 | 136.2% | 26 | 30 | 26 | -target 43 | 
random_control/router.aig | 72 | 95 | 131.9% | 84 | 116.7% | 9 | 10 | 9 | 40 | 73 | 182.5% | 56 | 140.0% | 6 | 7 | 6 |  | 
random_control/voter.aig | 2236 | 3621 | 161.9% | 2441 | 109.2% | 17 | 22 | 18 | 1461 | 2733 | 187.1% | 1498 | 102.5% | 12 | 16 | 13 |  | 

## Credits

 * Hannah Ravensloft (@Ravenslofty) -- suggestions and critique

## Copyright notice

Except for the `benchmarks/` submodule, the code is:

Copyright 2023 Martin Povišer

No explicit license assigned at this point
