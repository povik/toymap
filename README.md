# toymap: Toy technology mapper

Toymap maps logic networks onto LUTs or simple logic gates. The basic principle is cutting up an And-Inverter Graph representing the desired logic function into small subgraphs such that each subgraph can be implemented with (mapped onto) a LUT or a simple gate. Technology mapping like this is a step in the compilation of logic designs to be programmed onto an FPGA or manufactured as an IC.

Some features:

 * Employs the method of "priority cuts" to come up with candidate cuts of the graph (see the reference below)

 * Attempts to preserve wire names

 * Uses internal representation capable of expressing sequential circuits

     * This isn't useful yet since only acyclic graphs can be mapped and sequential elements are ignored in depth estimation. Import, export and emission of LUTs do handle sequential circuits correctly.

 * Integrates into [Yosys](https://github.com/yosysHQ/yosys)

## Build

    python3 pmgen.py lutcuts.pmg > lutcuts_pmg.h && yosys-config --build toymap.so toymap.cc lutdepth.cc post.cc

## References

 * A. Mishchenko, S. Cho, S. Chatterjee, and R. Brayton, "Combinational and sequential mapping with priority cuts", Proc. ICCAD '07, pp. 354-361. [PDF](https://people.eecs.berkeley.edu/~alanmi/publications/2007/iccad07_map.pdf)

 * S. Jang, B. Chan, K. Chung, and A. Mishchenko, "WireMap: FPGA technology mapping for improved routability". Proc. FPGA '08, pp. 47-55. [PDF](https://people.eecs.berkeley.edu/~alanmi/publications/2008/fpga08_wmap.pdf)

## Benchmarks

Following is a result of comparing ABC (invoked with the `abc -lut N` Yosys command) against toymap on the EPFL benchmarks (the `benchmarks/` submodule). On each benchmark LUT4 and LUT6 mappings are attempted, once with ABC, once with toymap, and once with toymap after ABC preprocessed the AIG (labeled abc+toymap). See `run_benchmark.tcl` for detailed commands.

Results as of toymap commit `8ac2aa5`:


Benchmark | abc LUT4 area | toymap LUT4 area | toymap/abc relative area | abc+toymap LUT4 area | abc+toymap/abc relative area | abc LUT4 depth | toymap LUT4 depth | abc+toymap LUT4 depth | abc LUT6 area | toymap LUT6 area | toymap/abc relative area | abc+toymap LUT6 area | abc+toymap/abc relative area | abc LUT6 depth | toymap LUT6 depth | abc+toymap LUT6 depth | extra toymap args
---|---|---|---|--|--|--|--|--|--|--|--|--|--|--|--|--|--
arithmetic/adder.aig | 339 | 339 | 100.0% | 339 | 100.0% | 85 | 85 | 85 | 274 | 261 | 95.3% | 261 | 95.3% | 51 | 51 | 51 | 
arithmetic/bar.aig | 1156 | 1408 | 121.8% | 1284 | 111.1% | 6 | 6 | 6 | 512 | 512 | 100.0% | 512 | 100.0% | 4 | 4 | 4 | 
arithmetic/div.aig | 6551 | 25976 | 396.5% | 6837 | 104.4% | 1437 | 1443 | 1437 | 5048 | 21515 | 426.2% | 6286 | 124.5% | 860 | 864 | 860 | 
arithmetic/hyp.aig | 64232 | 67618 | 105.3% | 67390 | 104.9% | 8254 | 8259 | 8254 | 44985 | 48993 | 108.9% | 49032 | 109.0% | 4193 | 4198 | 4195 | 
arithmetic/log2.aig | 10661 | 10125 | 95.0% | 10076 | 94.5% | 126 | 126 | 126 | 7880 | 8609 | 109.3% | 8353 | 106.0% | 70 | 72 | 70 | 
arithmetic/max.aig | 1013 | 1005 | 99.2% | 1024 | 101.1% | 67 | 76 | 67 | 799 | 774 | 96.9% | 803 | 100.5% | 40 | 44 | 40 | 
arithmetic/multiplier.aig | 7463 | 7460 | 100.0% | 7467 | 100.1% | 87 | 87 | 87 | 5880 | 5891 | 100.2% | 5953 | 101.2% | 53 | 53 | 53 | 
arithmetic/sin.aig | 1968 | 1905 | 96.8% | 1937 | 98.4% | 56 | 60 | 56 | 1450 | 1417 | 97.7% | 1470 | 101.4% | 36 | 36 | 36 | 
arithmetic/sqrt.aig | 4529 | 8522 | 188.2% | 5215 | 115.1% | 1995 | 2015 | 1995 | 3183 | 5778 | 181.5% | 3766 | 118.3% | 1017 | 1033 | 1017 | 
arithmetic/square.aig | 6248 | 6409 | 102.6% | 6408 | 102.6% | 83 | 84 | 83 | 3928 | 3931 | 100.1% | 3946 | 100.5% | 50 | 50 | 50 | 
random_control/arbiter.aig | 4068 | 4245 | 104.4% | 4245 | 104.4% | 30 | 30 | 30 | 2719 | 2722 | 100.1% | 2722 | 100.1% | 18 | 18 | 18 | 
random_control/cavlc.aig | 269 | 298 | 110.8% | 295 | 109.7% | 6 | 6 | 6 | 107 | 118 | 110.3% | 119 | 111.2% | 4 | 4 | 4 | 
random_control/ctrl.aig | 52 | 54 | 103.8% | 58 | 111.5% | 3 | 3 | 3 | 29 | 28 | 96.6% | 29 | 100.0% | 2 | 2 | 2 | 
random_control/dec.aig | 288 | 288 | 100.0% | 288 | 100.0% | 2 | 2 | 2 | 287 | 275 | 95.8% | 272 | 94.8% | 2 | 2 | 2 | 
random_control/i2c.aig | 434 | 534 | 123.0% | 470 | 108.3% | 5 | 6 | 5 | 303 | 351 | 115.8% | 329 | 108.6% | 3 | 4 | 3 | 
random_control/int2float.aig | 76 | 94 | 123.7% | 85 | 111.8% | 6 | 6 | 6 | 41 | 51 | 124.4% | 46 | 112.2% | 4 | 3 | 4 | 
random_control/mem_ctrl.aig | 14867 | 18091 | 121.7% | 17444 | 117.3% | 36 | 40 | 36 | 9202 | 11919 | 129.5% | 11472 | 124.7% | 22 | 25 | 22 | 
random_control/priority.aig | 169 | 329 | 194.7% | 269 | 159.2% | 43 | 62 | 51 | 127 | 225 | 177.2% | 177 | 139.4% | 26 | 31 | 26 | 
random_control/router.aig | 72 | 99 | 137.5% | 88 | 122.2% | 9 | 10 | 9 | 40 | 76 | 190.0% | 60 | 150.0% | 6 | 7 | 6 | 
random_control/voter.aig | 2236 | 3719 | 166.3% | 2441 | 109.2% | 17 | 23 | 18 | 1461 | 2769 | 189.5% | 1501 | 102.7% | 12 | 16 | 13 | 

## Credits

 * Hannah Ravensloft (@Ravenslofty) -- suggestions and critique

## Copyright notice

Except for the `benchmarks/` submodule, the code is:

Copyright 2023 Martin Povišer

No explicit license assigned at this point
