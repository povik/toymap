yosys -import

proc lutarea {} {
	# In some edge cases toymap can leave behind a NOT gate which
	# might need to be converted into a single-input LUT in an
	# actual implementation of the network. Do that here so we
	# have a fair comparison of the LUT counts.
	techmap -map +/gate2lut.v -D LUT_WIDTH=1
	set stat [yosys tee -q -s result.json stat -json]
	return [dict get $stat modules \\top num_cells_by_type \$lut]
}

proc lutdepth {} {
	set stat [yosys tee -q -s result.string lutdepth -quiet]
	return $stat
}

proc bench {name extra4 extra6} {
	read_aiger -module_name top "benchmarks/$name"
	design -save aig

	design -load aig
	abc -lut 6
	set abc_area6 [lutarea]
	set abc_depth6 [lutdepth]

	design -load aig
	abc -lut 4
	set abc_area4 [lutarea]
	set abc_depth4 [lutdepth]

	design -load aig
	toymap -lut 6 -balance -depth_cuts -emit_luts {*}$extra6
	opt_lut
	lutrewrite -lut 6 -leaves 10 {*}$extra6
	lutrewrite -lut 6 -shared -leaves 10 {*}$extra6
	opt_lut
	set toymap_area6 [lutarea]
	set toymap_depth6 [lutdepth]

	design -load aig
	toymap -lut 4 -balance -depth_cuts -emit_luts {*}$extra4
	opt_lut
	lutrewrite -lut 4 {*}$extra4
	lutrewrite -lut 4 -shared {*}$extra4
	opt_lut
	set toymap_area4 [lutarea]
	set toymap_depth4 [lutdepth]

	design -load aig
	abc -g aig
	aigmap
	opt_clean
	toymap -lut 6 -balance -depth_cuts -emit_luts {*}$extra6
	opt_lut
	lutrewrite -lut 6 -leaves 10 -lut 6 {*}$extra6
	lutrewrite -lut 6 -shared -leaves 10 -lut 6 {*}$extra6
	opt_lut
	set abc_toymap_area6 [lutarea]
	set abc_toymap_depth6 [lutdepth]

	design -load aig
	abc -g aig
	aigmap
	opt_clean
	toymap -lut 4 -balance -depth_cuts -emit_luts {*}$extra4
	opt_lut
	lutrewrite -lut 4 {*}$extra4
	lutrewrite -lut 4 -shared {*}$extra4
	opt_lut
	set abc_toymap_area4 [lutarea]
	set abc_toymap_depth4 [lutdepth]

	set p1 "$abc_area4 | $toymap_area4 | [format "%.1f" [expr (100.0 * $toymap_area4 / $abc_area4)]]%"
	set p2 "$abc_toymap_area4 | [format "%.1f" [expr (100.0 * $abc_toymap_area4 / $abc_area4)]]%"
	set p3 "$abc_depth4 | $toymap_depth4 | $abc_toymap_depth4"
	set p4 "$abc_area6 | $toymap_area6 | [format "%.1f" [expr (100.0 * $toymap_area6 / $abc_area6)]]%"
	set p5 "$abc_toymap_area6 | [format "%.1f" [expr (100.0 * $abc_toymap_area6 / $abc_area6)]]%"
	set p6 "$abc_depth6 | $toymap_depth6 | $abc_toymap_depth6"
	puts stdout "$name | $p1 | $p2 | $p3 | $p4 | $p5 | $p6 | $extra4 | $extra6"

	design -reset
	design -delete aig
}

bench "arithmetic/adder.aig" {} {}
bench "arithmetic/bar.aig" {} {}
bench "arithmetic/div.aig" {} {}
bench "arithmetic/hyp.aig" {} {}
bench "arithmetic/log2.aig" {} {}
bench "arithmetic/max.aig" {} {}
bench "arithmetic/multiplier.aig" {} {}
bench "arithmetic/sin.aig" {} {}
bench "arithmetic/sqrt.aig" {} {}
bench "arithmetic/square.aig" {} {}

bench "random_control/arbiter.aig" {} {}
bench "random_control/cavlc.aig" {} {}
bench "random_control/ctrl.aig" {} {}
bench "random_control/dec.aig" {} {}
bench "random_control/i2c.aig" {} {}
bench "random_control/int2float.aig" {} {}
bench "random_control/mem_ctrl.aig" {} {}
bench "random_control/priority.aig" {} {}
bench "random_control/priority.aig" {-target 43} {}
bench "random_control/router.aig" {} {}
bench "random_control/voter.aig" {} {}
