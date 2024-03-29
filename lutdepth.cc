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
		bool write_attrs = false;
		int target = 0;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-quiet")
				quiet = true;
			else if (args[argidx] == "-target" && argidx + 1 < args.size())
				target = atoi(args[++argidx].c_str());
			else if (args[argidx] == "-write_attrs")
				write_attrs = true;
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
			dict<Cell*, int> envelope;
			for (auto cell : sort.sorted) {
				int depth = 1;
				for (auto bit : sigmap(cell->getPort(ID::A)))
				if (driver.count(bit))
					depth = std::max(depth, depths[driver.at(bit)] + 1);
				depths[cell] = depth;
				max_depth = std::max(max_depth, depth);
			}

			int module_target;
			if (target && target >= max_depth) {
				module_target = target;
			} else {
				if (target)
					log_warning("User-specified depth target of %d unattainable\n", target);
				module_target = max_depth;
			}

			for (auto cell : sort.sorted)
				envelope[cell] = module_target;

			for (auto it = sort.sorted.rbegin(); it != sort.sorted.rend(); it++) {
				int depth = envelope[*it];
				for (auto bit : sigmap((*it)->getPort(ID::A)))
				if (driver.count(bit))
					envelope[driver[bit]] = std::min(envelope[driver[bit]], depth - 1);
			}

			if (write_attrs)
			for (auto cell : sort.sorted) {
				cell->attributes[ID(depth)] = depths[cell];
				cell->attributes[ID(depth_envelope)] = envelope[cell];
				cell->set_bool_attribute(ID(critical), depths[cell] == envelope[cell]);
			}

			if (!quiet)
				log("Maximum depth: %d\n", max_depth);
			else
				log("%d", max_depth);
		}
	}
} LutdepthPass;

PRIVATE_NAMESPACE_END
