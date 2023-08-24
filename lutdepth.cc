#include "kernel/register.h"
#include "kernel/utils.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LutdepthPass : Pass {
	LutdepthPass() : Pass("lutdepth", "print depth of LUT chains") {}
	void execute(std::vector<std::string> args, RTLIL::Design *d) override
	{
		bool quiet = false;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-quiet")
				quiet = true;
			else
				break;
		}
		extra_args(args, argidx, d);

		if (!quiet)
			log_header(d, "Executing LUTDEPTH pass. (print LUT network depth)\n");

		for (auto m : d->selected_modules()) {
			SigMap sigmap(m);
			dict<SigBit, Cell*> driver;

			for (auto cell : m->selected_cells())
			if (cell->type == ID($lut))
				driver[sigmap(cell->getPort(ID::Y))] = cell;

			TopoSort<Cell*> sort;

			for (auto cell : m->selected_cells())
			if (cell->type == ID($lut)) {
				sort.node(cell);
				for (auto bit : sigmap(cell->getPort(ID::A)))
				if (driver.count(bit))
					sort.edge(driver.at(bit), cell);
			}

			log_assert(sort.sort());

			int max_depth = 0;
			dict<Cell*, int> depths;
			for (auto cell : sort.sorted) {
				int depth = 1;
				for (auto bit : sigmap(cell->getPort(ID::A)))
				if (driver.count(bit))
					depth = std::max(depth, depths[driver.at(bit)] + 1);
				depths[cell] = depth;
				max_depth = std::max(max_depth, depth);
			}

			if (!quiet)
				log("Maximum depth: %d\n", max_depth);
			else
				log("%d", max_depth);
		}
	}
} LutdepthPass;

PRIVATE_NAMESPACE_END
