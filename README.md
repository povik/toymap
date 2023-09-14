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

Results as of toymap commit `c09be49`:


Benchmark | abc LUT4 area | toymap LUT4 area | toymap/abc relative area | abc+toymap LUT4 area | abc+toymap/abc relative area | abc LUT4 depth | toymap LUT4 depth | abc+toymap LUT4 depth | abc LUT6 area | toymap LUT6 area | toymap/abc relative area | abc+toymap LUT6 area | abc+toymap/abc relative area | abc LUT6 depth | toymap LUT6 depth | abc+toymap LUT6 depth | extra toymap args
---|---|---|---|--|--|--|--|--|--|--|--|--|--|--|--|--|--
arithmetic/adder.aig | 339 | 339 | 100.0% | 339 | 100.0% | 85 | 85 | 85 | 274 | 261 | 95.3% | 261 | 95.3% | 51 | 51 | 51 | 
arithmetic/bar.aig | 1156 | 1408 | 121.8% | 1284 | 111.1% | 6 | 6 | 6 | 512 | 512 | 100.0% | 512 | 100.0% | 4 | 4 | 4 | 
arithmetic/div.aig | 6551 | 25183 | 384.4% | 6840 | 104.4% | 1437 | 1443 | 1437 | 5048 | 21580 | 427.5% | 6375 | 126.3% | 860 | 864 | 860 | 
arithmetic/hyp.aig | 64232 | 67388 | 104.9% | 67206 | 104.6% | 8254 | 8258 | 8254 | 44985 | 49020 | 109.0% | 49132 | 109.2% | 4193 | 4198 | 4195 | 
arithmetic/log2.aig | 10661 | 10074 | 94.5% | 10044 | 94.2% | 126 | 126 | 126 | 7880 | 8592 | 109.0% | 8353 | 106.0% | 70 | 72 | 70 | 
arithmetic/max.aig | 1013 | 972 | 96.0% | 992 | 97.9% | 67 | 76 | 67 | 799 | 774 | 96.9% | 803 | 100.5% | 40 | 44 | 40 | 
arithmetic/multiplier.aig | 7463 | 7431 | 99.6% | 7430 | 99.6% | 87 | 87 | 87 | 5880 | 5898 | 100.3% | 5950 | 101.2% | 53 | 53 | 53 | 
arithmetic/sin.aig | 1968 | 1894 | 96.2% | 1909 | 97.0% | 56 | 60 | 56 | 1450 | 1416 | 97.7% | 1471 | 101.4% | 36 | 36 | 36 | 
arithmetic/sqrt.aig | 4529 | 8514 | 188.0% | 5162 | 114.0% | 1995 | 2015 | 1995 | 3183 | 5761 | 181.0% | 3755 | 118.0% | 1017 | 1033 | 1017 | 
arithmetic/square.aig | 6248 | 6418 | 102.7% | 6402 | 102.5% | 83 | 84 | 83 | 3928 | 3942 | 100.4% | 3947 | 100.5% | 50 | 50 | 50 | 
random_control/arbiter.aig | 4068 | 4211 | 103.5% | 4214 | 103.6% | 30 | 30 | 30 | 2719 | 2722 | 100.1% | 2722 | 100.1% | 18 | 18 | 18 | 
random_control/cavlc.aig | 269 | 293 | 108.9% | 283 | 105.2% | 6 | 6 | 6 | 107 | 118 | 110.3% | 119 | 111.2% | 4 | 4 | 4 | 
random_control/ctrl.aig | 52 | 54 | 103.8% | 57 | 109.6% | 3 | 3 | 3 | 29 | 28 | 96.6% | 29 | 100.0% | 2 | 2 | 2 | 
random_control/dec.aig | 288 | 288 | 100.0% | 288 | 100.0% | 2 | 2 | 2 | 287 | 272 | 94.8% | 286 | 99.7% | 2 | 2 | 2 | 
random_control/i2c.aig | 434 | 522 | 120.3% | 460 | 106.0% | 5 | 6 | 5 | 303 | 351 | 115.8% | 329 | 108.6% | 3 | 4 | 3 | 
random_control/int2float.aig | 76 | 92 | 121.1% | 84 | 110.5% | 6 | 6 | 6 | 41 | 51 | 124.4% | 46 | 112.2% | 4 | 3 | 4 | 
random_control/mem_ctrl.aig | 14867 | 17404 | 117.1% | 17050 | 114.7% | 36 | 40 | 36 | 9202 | 11920 | 129.5% | 11477 | 124.7% | 22 | 25 | 22 | 
random_control/priority.aig | 169 | 221 | 130.8% | 206 | 121.9% | 43 | 34 | 29 | 127 | 225 | 177.2% | 178 | 140.2% | 26 | 31 | 26 | 
random_control/router.aig | 72 | 97 | 134.7% | 85 | 118.1% | 9 | 10 | 9 | 40 | 77 | 192.5% | 60 | 150.0% | 6 | 7 | 6 | 
random_control/voter.aig | 2236 | 3646 | 163.1% | 2441 | 109.2% | 17 | 23 | 18 | 1461 | 2773 | 189.8% | 1501 | 102.7% | 12 | 16 | 13 | 

## Credits

 * Hannah Ravensloft (@Ravenslofty) -- suggestions and critique

## Copyright notice

Except for the `benchmarks/` submodule, the code is:

Copyright 2023 Martin Povišer

No explicit license assigned at this point
