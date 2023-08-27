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

Results as of toymap commit `ab52ce4`:


Benchmark | abc LUT4 area | toymap LUT4 area | toymap/abc relative area | abc+toymap LUT4 area | abc+toymap/abc relative area | abc LUT4 depth | toymap LUT4 depth | abc+toymap LUT4 depth | abc LUT6 area | toymap LUT6 area | toymap/abc relative area | abc+toymap LUT6 area | abc+toymap/abc relative area | abc LUT6 depth | toymap LUT6 depth | abc+toymap LUT6 depth | extra toymap args
---|---|---|---|--|--|--|--|--|--|--|--|--|--|--|--|--|--
arithmetic/adder.aig | 339 | 339 | 100.0% | 339 | 100.0% | 85 | 85 | 85 | 274 | 254 | 92.7% | 254 | 92.7% | 51 | 51 | 51 | 
arithmetic/bar.aig | 1156 | 1351 | 116.9% | 1404 | 121.5% | 6 | 6 | 6 | 512 | 512 | 100.0% | 512 | 100.0% | 4 | 4 | 4 | 
arithmetic/div.aig | 6551 | 27292 | 416.6% | 9360 | 142.9% | 1437 | 1443 | 1439 | 5048 | 22353 | 442.8% | 8127 | 161.0% | 860 | 864 | 863 | 
arithmetic/hyp.aig | 64232 | 81157 | 126.3% | 80814 | 125.8% | 8254 | 8259 | 8255 | 44985 | 48939 | 108.8% | 48854 | 108.6% | 4193 | 4199 | 4195 | 
arithmetic/log2.aig | 10661 | 11636 | 109.1% | 12451 | 116.8% | 126 | 135 | 126 | 7880 | 9418 | 119.5% | 10226 | 129.8% | 70 | 77 | 70 | 
arithmetic/max.aig | 1013 | 1132 | 111.7% | 1401 | 138.3% | 67 | 96 | 67 | 799 | 840 | 105.1% | 817 | 102.3% | 40 | 56 | 40 | 
arithmetic/multiplier.aig | 7463 | 9710 | 130.1% | 9747 | 130.6% | 87 | 87 | 87 | 5880 | 7402 | 125.9% | 7531 | 128.1% | 53 | 53 | 53 | 
arithmetic/sin.aig | 1968 | 2133 | 108.4% | 2168 | 110.2% | 56 | 69 | 56 | 1450 | 1595 | 110.0% | 1571 | 108.3% | 36 | 42 | 36 | 
arithmetic/sqrt.aig | 4529 | 10792 | 238.3% | 6890 | 152.1% | 1995 | 2015 | 1995 | 3183 | 5680 | 178.4% | 4274 | 134.3% | 1017 | 1033 | 1017 | 
arithmetic/square.aig | 6248 | 6966 | 111.5% | 7371 | 118.0% | 83 | 84 | 83 | 3928 | 3807 | 96.9% | 3811 | 97.0% | 50 | 50 | 50 | 
random_control/arbiter.aig | 4068 | 4265 | 104.8% | 4267 | 104.9% | 30 | 30 | 30 | 2719 | 2722 | 100.1% | 2722 | 100.1% | 18 | 18 | 18 | 
random_control/cavlc.aig | 269 | 305 | 113.4% | 306 | 113.8% | 6 | 6 | 6 | 107 | 121 | 113.1% | 122 | 114.0% | 4 | 4 | 4 | 
random_control/ctrl.aig | 52 | 61 | 117.3% | 55 | 105.8% | 3 | 3 | 3 | 29 | 28 | 96.6% | 28 | 96.6% | 2 | 2 | 2 | 
random_control/dec.aig | 288 | 288 | 100.0% | 288 | 100.0% | 2 | 2 | 2 | 287 | 280 | 97.6% | 272 | 94.8% | 2 | 2 | 2 | 
random_control/i2c.aig | 434 | 552 | 127.2% | 486 | 112.0% | 5 | 7 | 5 | 303 | 383 | 126.4% | 348 | 114.9% | 3 | 4 | 3 | 
random_control/int2float.aig | 76 | 100 | 131.6% | 88 | 115.8% | 6 | 6 | 6 | 41 | 49 | 119.5% | 47 | 114.6% | 4 | 3 | 4 | 
random_control/mem_ctrl.aig | 14867 | 19892 | 133.8% | 18813 | 126.5% | 36 | 40 | 37 | 9202 | 13572 | 147.5% | 13434 | 146.0% | 22 | 25 | 22 | 
random_control/priority.aig | 169 | 365 | 216.0% | 278 | 164.5% | 43 | 62 | 51 | 127 | 253 | 199.2% | 190 | 149.6% | 26 | 31 | 26 | 
random_control/router.aig | 72 | 99 | 137.5% | 96 | 133.3% | 9 | 18 | 9 | 40 | 75 | 187.5% | 65 | 162.5% | 6 | 11 | 6 | 
random_control/voter.aig | 2236 | 4180 | 186.9% | 2432 | 108.8% | 17 | 23 | 18 | 1461 | 2725 | 186.5% | 1525 | 104.4% | 12 | 17 | 13 | 



## Copyright notice

Except for the `benchmarks/` submodule, the code is:

Copyright 2023 Martin Povišer

No explicit license assigned at this point
