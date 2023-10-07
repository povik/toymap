#include "kernel/register.h"
#include "kernel/utils.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

State invert(State state)
{
	switch (state) {
	case State::S0: return State::S1;
	case State::S1: return State::S0;
	case State::Sx: return State::Sx;
	default: log_assert(false); return State::Sx;
	}
}

Const negate_const(Const val)
{
	std::vector<RTLIL::State> neg_bits;
	for (auto bit : val.bits)
		neg_bits.push_back(invert(bit));
	return Const(neg_bits);
}


Const adjust_lut(Const val, int negated_input)
{
	int mask = 1 << negated_input;
	for (int i = 0; i < (int) val.bits.size(); i++)
	if (!(i & mask))
		std::swap(val.bits[i], val.bits[i ^ mask]);
	return val;
}

struct LutnotPass : Pass {
	LutnotPass() : Pass("lutnot", "absorb NOT gates into LUTs where possible") {}
	void execute(std::vector<std::string> args, RTLIL::Design *d) override
	{
		log_header(d, "Executing LUTNOT pass.\n");

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			break;
		}
		extra_args(args, argidx, d);

		for (auto m : d->selected_whole_modules_warn()) {
			SigMap sigmap(m);
			dict<SigBit, Cell *> lut_driver;
			SigPool foreign_bits;

			for (auto cell : m->cells())
			if (cell->type == ID($lut))
				lut_driver[sigmap(cell->getPort(ID::Y))] = cell;

			for (auto cell : m->cells())
			if (!cell->type.in(ID($lut), ID($_NOT_)))
			for (auto conn : cell->connections())
			for (auto bit : sigmap(conn.second))
				foreign_bits.add(bit);

			for (auto w : m->wires())
			if (w->port_input || w->port_output)
			for (auto bit : sigmap(w))
				foreign_bits.add(bit);

			dict<SigBit, SigBit> replacements;
			std::vector<Cell *> to_remove;

			for (auto cell : m->cells())
			if (cell->type == ID($_NOT_)) {
				SigBit a_bit = sigmap(cell->getPort(ID::A));
				SigBit y_bit = sigmap(cell->getPort(ID::Y));
				if (foreign_bits.check(a_bit) || !lut_driver.count(a_bit))
					continue;

				log_debug("Absorbing _NOT_ cell %s driving %s",
						  log_id(cell->name), log_signal(y_bit));

				Cell *driver = lut_driver.at(a_bit);
				driver->setParam(ID::LUT, negate_const(driver->getParam(ID::LUT)));
				replacements[a_bit] = y_bit;
				to_remove.push_back(cell);
				driver->setPort(ID::Y, y_bit);
			}

			for (auto cell : m->cells())
			if (cell->type == ID($lut))
			for (int i = 0; i < (int) cell->getParam(ID::WIDTH).as_int(); i++)
			if (replacements.count(sigmap(cell->getPort(ID::A)[i]))) {
				SigSpec a_sig = cell->getPort(ID::A);
				a_sig[i] = replacements.at(sigmap(a_sig[i]));
				cell->setPort(ID::A, a_sig);
				cell->setParam(ID::LUT, adjust_lut(cell->getParam(ID::LUT), i));
			}

			for (auto cell : to_remove)
				m->remove(cell);
		}
	}
} LutnotPass;

#include "lutcuts_pmg.h"

std::string hexdump(std::vector<bool> tt)
{
	std::string ret;
	for (int i = 0; i < tt.size(); i += 4) {
		int nibble = 0;
		for (int j = i; j < tt.size() && j < i + 4; j++)
		if (tt[j])
			nibble |= 1 << (j - i);
		ret += nibble < 10 ? '0' + nibble : 'A' - 10 + nibble;
	}
	return ret;
}

std::string bindump(std::vector<bool> tt)
{
	std::string ret;
	for (int i = 0; i < tt.size(); i++)
		ret += tt[i] ? '1' : '0';
	return ret;
}

struct ThruthTable {
	std::vector<int> vars;
	std::vector<bool> values;
	std::vector<bool> dontcares;

	void swap(int a, int b)
	{
		std::swap(vars[a], vars[b]);

		for (int i = 0; i < values.size(); i++) {
			if ((i >> a & 1) || !(i >> b & 1))
				continue;

			int i_ = i & ~(1 << b) | (1 << a);
			std::swap(values[i], values[i_]);
			std::swap(dontcares[i], dontcares[i_]);
		}
	}

	void change_vars(std::vector<int> new_vars)
	{
		log_assert(new_vars.size() == vars.size());
		int nvars = vars.size();
		int max_var = *std::max_element(vars.begin(), vars.end());
		std::vector<int> vi(max_var + 1);
		for (int i = 0; i < nvars; i++)
			vi[vars[i]] = i;

		std::vector<int> p(new_vars.size());
		for (int i = 0; i < nvars; i++)
			p[vi[new_vars[i]]] = i;

		std::vector<bool> new_values(values.size());
		std::vector<bool> new_dontcares(values.size());
		for (int i = 0; i < values.size(); i++) {
			int i_ = 0;
			for (int j = 0; j < nvars; j++)
				if (i & 1 << j)
					i_ |= 1 << p[j];
			log_assert(i_ < values.size());
			log_assert(i_ >= 0);
			new_values[i_] = values[i];
			new_dontcares[i_] = dontcares[i];
		}

		vars = new_vars;
		values = new_values;
		dontcares = new_dontcares;
	}

	Const to_const()
	{
		Const ret;
		for (int i = 0; i < values.size(); i++)
			ret.bits.push_back(dontcares[i] ? State::Sx : values[i] ? State::S1 : State::S0);
		return ret;
	}

	bool operator==(const ThruthTable &other) const
	{
		return other.vars == vars && other.values == values && other.dontcares == dontcares;
	}
};

struct Lut {
	Cell *cell;
	Const lut;
	std::vector<std::pair<bool, int>> inputs;
};

struct LutNetwork {
	std::vector<Lut> nodes;
	std::vector<int> outs;
	int ninputs;

	void import(SigMap &sigmap, dict<SigBit, Cell *> lut_drivers,
				SigSpec leaves, SigSpec outputs)
	{
		pool<SigBit> frontier(outputs.bits().begin(), outputs.bits().end());
		pool<SigBit> leaves_pool(leaves.bits().begin(), leaves.bits().end());
		std::vector<Cell *> cell_sequence;

		for (auto bit : outputs)
			log_assert(!leaves_pool.count(bit));

		while (!frontier.empty()) {
			SigBit bit = frontier.pop();

			log_assert(lut_drivers.count(bit));
			Cell *cell = lut_drivers.at(bit);
			cell_sequence.push_back(cell);
			for (auto bit : sigmap(cell->getPort(ID::A)))
			if (!leaves_pool.count(bit))
				frontier.insert(bit);
		}

		std::reverse(cell_sequence.begin(), cell_sequence.end());
		dict<SigBit, std::pair<bool, int>> input_map;

		for (int i = 0; i < leaves.size(); i++)
			input_map[leaves.extract(i)] = std::make_pair(true, i);

		int cell_no = 0;
		for (auto cell : cell_sequence) {
			nodes.emplace_back();
			auto &lut = nodes.back();
			lut.cell = cell;
			lut.lut = cell->getParam(ID::LUT);
			for (auto bit : sigmap(cell->getPort(ID::A)))
				lut.inputs.push_back(input_map.at(bit));
			input_map[sigmap(cell->getPort(ID::Y))] = \
					std::make_pair(false, cell_no++);
		}

		for (auto bit : outputs) {
			auto pair = input_map.at(bit);
			log_assert(!pair.first);
			outs.push_back(pair.second);
		}

		ninputs = leaves.size();
	}

	SigSpec emit(Module *m, SigSpec leaves)
	{
		dict<std::pair<bool, int>, SigBit> input_map;

		for (int i = 0; i < leaves.size(); i++)
			input_map[std::make_pair(true, i)] = leaves[i];

		int lut_no = 0;
		for (auto &node : nodes) {
			log_assert(node.cell == NULL);

			SigSpec lut_inputs;
			for (auto in : node.inputs)
				lut_inputs.append(input_map.at(in));

			SigSpec y = m->addWire(NEW_ID, 1);
			node.cell = m->addLut(NEW_ID, lut_inputs, y, node.lut);
			input_map[std::make_pair(false, lut_no++)] = y;
		}

		SigSpec ret;
		for (auto out : outs)
			ret.append(input_map.at(std::make_pair(false, out)));
		return ret;
	}

	std::string dump(std::pair<bool, int> id)
	{
		bool leaf;
		int index;
		std::tie(leaf, index) = id;

		if (leaf) {
			return stringf("%c", 'a' + index);
		} else {
			auto &node = nodes[index];
			std::vector<bool> lut_binary;
			for (auto bit : node.lut)
				lut_binary.push_back(bit != State::S0);
			std::string args;
			for (auto input : node.inputs) {
				if (!args.empty())
					args += ",";
				args += dump(input);
			}
			return hexdump(lut_binary) + "(" + args + ")";
		}
	}

	std::string dump_output()
	{
		return dump(std::make_pair(false, outs.front()));
	}

	ThruthTable thruth_table()
	{
		std::vector<bool> cell_states(nodes.size(), false);
		std::vector<bool> ret(1 << ninputs, false);

		log_assert(outs.size() == 1);
		for (int i = 0; i < (1 << ninputs); i++) {
			for (int j = 0; j < nodes.size(); j++) {
				auto &node = nodes[j];

				int idx = 0;
				for (auto k = 0; k < node.inputs.size(); k++) {
					bool leaf; int in_idx;
					std::tie(leaf, in_idx) = node.inputs[k];
					if (leaf ? i & (1 << in_idx) : cell_states[in_idx])
						idx |= 1 << k;
				}
				cell_states[j] = (node.lut[idx] != State::S0);
			}
			ret[i] = cell_states.back();
		}

		ThruthTable ret_table;
		ret_table.values = ret;
		for (int i = 0; i < ninputs; i++)
			ret_table.vars.push_back(i);
		ret_table.dontcares = std::vector<bool>(ret.size(), false);
		return ret_table;
	}
};

bool matches(std::vector<bool>::iterator val1,
			 std::vector<bool>::iterator dc1,
			 std::vector<bool>::iterator val2,
			 std::vector<bool>::iterator dc2,
			 int size)
{
	for (int i = 0; i < size; i++) {
		if (!*dc1 && !*dc2 && *val1 != *val2)
			return false;

		val1++; val2++; dc1++; dc2++;
	}
	return true;
}

void adjust(std::vector<bool>::iterator val1,
			std::vector<bool>::iterator dc1,
			std::vector<bool>::iterator val2,
			std::vector<bool>::iterator dc2,
			int size)
{
	for (int i = 0; i < size; i++) {
		if (*dc1 && !*dc2) {
			*dc1 = false;
			*val1 = *val2;
		}
		val1++; val2++; dc1++; dc2++;
	}
}

struct Fragment {
	std::vector<bool> values;
	std::vector<bool> dontcares;
	uint32_t bs_high; // bound set high mask
	uint32_t bs_low; // bound set low mask

	bool operator<(const Fragment &other) const
	{
		return std::tie(values, dontcares) < std::tie(other.values, other.dontcares);
	}
};

std::vector<Fragment> find_fragments(ThruthTable &table, int bn)
{
	int nfrags = 1 << bn;
	int fraglen = table.values.size() / nfrags;
	std::vector<Fragment> found;
	log_assert(nfrags * fraglen == table.values.size());

	uint32_t bs_mask = (1 << bn) - 1;

	for (int i = 0; i < nfrags; i++) {
		std::vector<bool>::iterator val = table.values.begin() + i * fraglen;
		std::vector<bool>::iterator dc = table.dontcares.begin() + i * fraglen;

		for (auto &other : found) {
			if (matches(other.values.begin(), other.dontcares.begin(), val, dc, fraglen)) {
				adjust(other.values.begin(), other.dontcares.begin(), val, dc, fraglen);
				other.bs_high |= (uint32_t) i;
				other.bs_low |= bs_mask & ~(uint32_t) i;
				goto match;
			}
		}

		found.push_back(Fragment{std::vector<bool>(val, val + fraglen),
								    std::vector<bool>(dc, dc + fraglen),
								    (uint32_t) i, bs_mask & ~(uint32_t) i});

	match:
		;
	}

	return found;
}

#define LUT_SIZE 4

static bool search_shared = false;

void implement_varchoices(ThruthTable table, std::vector<int> vars, LutNetwork &net)
{
	int log2 = table.vars.size();
	log_assert(1 << log2 == table.values.size());
	log_assert(1 << log2 == table.dontcares.size());

	std::vector<std::pair<bool, int>> lut_inputs;

	auto sep = std::find(vars.begin(), vars.end(), -1);
	for (auto it = (log2 <= LUT_SIZE) ? vars.begin() : sep - LUT_SIZE;
			it != sep; it++) {
		if (*it < net.ninputs) {
			lut_inputs.push_back(std::make_pair(true, *it));
		} else {
			log_assert(*it - net.ninputs < net.nodes.size());
			lut_inputs.push_back(std::make_pair(false, *it - net.ninputs));
		}
	}

	if (log2 <= LUT_SIZE) {
		net.nodes.push_back(Lut{
			NULL,
			table.to_const(),
			lut_inputs
		});
		net.outs.push_back(net.nodes.size() - 1);
		return;
	}

	table.change_vars(std::vector<int>(vars.begin(), sep));

	int bn = LUT_SIZE, fn = log2 - bn;
	std::vector<Fragment> fragments = find_fragments(table, bn);
	log_assert(fragments.size() <= 4);
	int nluts = ceil_log2(fragments.size());

	// Find if there's a shared variable
	int shared;
	for (shared = 0; shared < bn; shared++) {
		int nhighs = 0, nlows = 0;
		for (auto &frag : fragments) {
			if (frag.bs_high & 1 << shared)
				nhighs++;
			if (frag.bs_low & 1 << shared)
				nlows++;
		}

		if (nlows <= 1 << (nluts - 1) && nhighs <= 1 << (nluts - 1))
			break;
	}

	if (!search_shared || shared == bn)
		shared = -1;
	else
		nluts -= 1;

	ThruthTable sub;
	for (int i = 0; i < fn; i++)
		sub.vars.push_back(table.vars[i]);
	for (int i = 0; i < nluts; i++)
		sub.vars.push_back(net.ninputs + net.nodes.size() + i);
	if (shared != -1)
		sub.vars.push_back(table.vars[fn + shared]);

	std::sort(fragments.begin(), fragments.end());

	if (shared != -1) {
		// First that part of the thruth table for the shared variable being low
		int pad = 0;
		for (auto &frag : fragments) {
			if (!(frag.bs_low & 1 << shared))
				continue;
			sub.values.insert(sub.values.end(), frag.values.begin(), frag.values.end());
			sub.dontcares.insert(sub.dontcares.end(), frag.dontcares.begin(), frag.dontcares.end());
			pad++;
		}
		for (; pad < 1 << nluts; pad++) {
			sub.values.insert(sub.values.end(), 1 << fn, true);
			sub.dontcares.insert(sub.dontcares.end(), 1 << fn, true);
		}

		// Then the other part
		pad = 0;
		for (auto &frag : fragments) {
			if (!(frag.bs_high & 1 << shared))
				continue;
			sub.values.insert(sub.values.end(), frag.values.begin(), frag.values.end());
			sub.dontcares.insert(sub.dontcares.end(), frag.dontcares.begin(), frag.dontcares.end());
			pad++;
		}
		for (; pad < 1 << nluts; pad++) {
			sub.values.insert(sub.values.end(), 1 << fn, true);
			sub.dontcares.insert(sub.dontcares.end(), 1 << fn, true);
		}

		log_assert(sub.values.size() == 1 << (fn + nluts + 1));
	} else {
		for (auto &frag : fragments) {
			sub.values.insert(sub.values.end(), frag.values.begin(), frag.values.end());
			sub.dontcares.insert(sub.dontcares.end(), frag.dontcares.begin(), frag.dontcares.end());
		}
		for (int i = 0; i < (1 << nluts) - fragments.size(); i++) {
			sub.values.insert(sub.values.end(), 1 << fn, true);
			sub.dontcares.insert(sub.dontcares.end(), 1 << fn, true);
		}	
	}

	std::vector<int> f_indices;
	int fraglen = 1 << fn;
	for (int i = 0; i < (1 << bn); i++) {
		std::vector<bool> f(table.values.begin() + i*fraglen,	
							table.values.begin() + (i + 1)*fraglen);
		std::vector<bool> f_dc(table.dontcares.begin() + i*fraglen,	
							table.dontcares.begin() + (i + 1)*fraglen);

		int j = 0;
		bool found = false;
		for (auto &frag : fragments) {
			if (shared != -1 && !(((i & (1 << shared)) ? frag.bs_high : frag.bs_low) & 1 << shared))
				continue;

			if (matches(frag.values.begin(), frag.dontcares.begin(),
						f.begin(), f_dc.begin(), fraglen)) {
				found = true;
				break;
			}
			j++;
		}
		log_assert(shared != -1 || found);
		log_assert(shared == -1 || found);
		f_indices.push_back(j);
	}

	for (int i = 0; i < nluts; i++) {
		Const lut;

		for (auto fi : f_indices)
			lut.bits.push_back(fi & 1 << i ? State::S1 : State::S0);

		net.nodes.push_back(Lut{
			NULL,
			lut,
			lut_inputs
		});
	}

	implement_varchoices(sub, std::vector<int>(sep + 1, vars.end()), net);
}

std::pair<std::vector<int>, int> explore_varchoices(ThruthTable table, int varcounter=-1)
{
	if (varcounter == -1)
		varcounter = table.vars.size();

	int nvars = table.vars.size();
	log_assert(1 << nvars == table.values.size());
	log_assert(1 << nvars == table.dontcares.size());

	if (nvars <= LUT_SIZE)
		return std::make_pair(table.vars, 1);

	int bn = LUT_SIZE, fn = nvars - bn;
	int level = bn - 1;
	std::vector<int> p(bn, 0);

	std::vector<int> best_vars;
	int best_nluts = std::numeric_limits<int>::max();

	int niters = 1;
	while (level >= 0) {
		log_assert(level < p.size());
		niters++;
		table.swap(fn + level, p[level]);

		std::vector<Fragment> fragments = find_fragments(table, bn);
		if (fragments.size() <= 4) {
			int nluts = ceil_log2(fragments.size());

			// Find if there's a shared variable
			int shared;
			for (shared = 0; shared < bn; shared++) {
				int nhighs = 0, nlows = 0;
				for (auto &frag : fragments) {
					if (frag.bs_high & 1 << shared)
						nhighs++;
					if (frag.bs_low & 1 << shared)
						nlows++;
				}

				if (nlows <= 1 << (nluts - 1) && nhighs <= 1 << (nluts - 1))
					break;
			}

			if (!search_shared || shared == bn)
				shared = -1;
			else
				nluts -= 1;

			ThruthTable sub;
			for (int i = 0; i < fn; i++)
				sub.vars.push_back(table.vars[i]);
			for (int i = 0; i < nluts; i++)
				sub.vars.push_back(varcounter + i);
			if (shared != -1)
				sub.vars.push_back(table.vars[fn + shared]);

			std::sort(fragments.begin(), fragments.end());

			if (shared != -1) {
				// First that part of the thruth table for the shared variable being low
				int pad = 0;
				for (auto &frag : fragments) {
					if (!(frag.bs_low & 1 << shared))
						continue;
					sub.values.insert(sub.values.end(), frag.values.begin(), frag.values.end());
					sub.dontcares.insert(sub.dontcares.end(), frag.dontcares.begin(), frag.dontcares.end());
					pad++;
				}
				for (; pad < 1 << nluts; pad++) {
					sub.values.insert(sub.values.end(), 1 << fn, true);
					sub.dontcares.insert(sub.dontcares.end(), 1 << fn, true);
				}

				// Then the other part
				pad = 0;
				for (auto &frag : fragments) {
					if (!(frag.bs_high & 1 << shared))
						continue;
					sub.values.insert(sub.values.end(), frag.values.begin(), frag.values.end());
					sub.dontcares.insert(sub.dontcares.end(), frag.dontcares.begin(), frag.dontcares.end());
					pad++;
				}
				for (; pad < 1 << nluts; pad++) {
					sub.values.insert(sub.values.end(), 1 << fn, true);
					sub.dontcares.insert(sub.dontcares.end(), 1 << fn, true);
				}
			} else {
				for (auto &frag : fragments) {
					sub.values.insert(sub.values.end(), frag.values.begin(), frag.values.end());
					sub.dontcares.insert(sub.dontcares.end(), frag.dontcares.begin(), frag.dontcares.end());
				}
				for (int i = 0; i < (1 << nluts) - fragments.size(); i++) {
					sub.values.insert(sub.values.end(), 1 << fn, true);
					sub.dontcares.insert(sub.dontcares.end(), 1 << fn, true);
				}	
			}

			int sub_nluts;
			std::vector<int> sub_vars;
			std::tie(sub_vars, sub_nluts) = explore_varchoices(sub, varcounter + nluts);

			if (sub_nluts != std::numeric_limits<int>::max() &&
					sub_nluts + nluts < best_nluts) {
				best_nluts = sub_nluts + nluts;
				best_vars.clear();
				best_vars.insert(best_vars.end(), table.vars.begin(), table.vars.end());
				best_vars.push_back(-1);
				best_vars.insert(best_vars.end(), sub_vars.begin(), sub_vars.end());
			}
		}

		p[level]++;
		if (p[level] == fn) {
			level--;
		} else {
			for (; level < bn - 1; level++)
				p[level + 1] = p[level];
		}
	}

	return std::make_pair(best_vars, best_nluts);
}

struct LutrewriteOncePass : Pass {
	LutrewriteOncePass() : Pass("lutrewrite_once", "rewrite local patches of LUT network") {}
	void execute(std::vector<std::string> args, RTLIL::Design *d) override
	{
		log_header(d, "Executing LUTREWRITE_ONCE pass. (rewrite local patches of LUT network)\n");

		int max_nluts = 20, max_nouterfans = 1, max_nleaves = 9,
			lutsize = 4;
		bool select_root = false;
		float w_cutoff = 1.01;
		search_shared = false;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-lut" && argidx + 1 < args.size())
				lutsize = atoi(args[++argidx].c_str());
			else if (args[argidx] == "-luts" && argidx + 1 < args.size())
				max_nluts = atoi(args[++argidx].c_str());
			else if (args[argidx] == "-outerfans" && argidx + 1 < args.size())
				max_nouterfans = atoi(args[++argidx].c_str());
			else if (args[argidx] == "-leaves" && argidx + 1 < args.size())
				max_nleaves = atoi(args[++argidx].c_str());
			else if (args[argidx] == "-w" && argidx + 1 < args.size())
				w_cutoff = atof(args[++argidx].c_str());
			else if (args[argidx] == "-root")
				select_root = true;
			else if (args[argidx] == "-shared")
				search_shared = true;
			else
				break;
		}
		extra_args(args, argidx, d);

		bool did_something = false;

		for (auto m : select_root ? d->selected_modules()
						: d->selected_whole_modules_warn()) {
			lutcuts_pm pm(m);
			pm.setup(m->cells());
			dict<SigBit, Cell *> lut_drivers;
			for (auto cell : m->cells())
				lut_drivers[pm.sigmap(cell->getPort(ID::Y))] = cell;

			pm.ud_lut_cuts.max_nluts = max_nluts;
			pm.ud_lut_cuts.max_nouterfans = max_nouterfans;
			pm.ud_lut_cuts.max_nleaves = max_nleaves;

			TopoSort<Cell*> sort;
			for (auto cell : select_root ? m->selected_cells() : m->cells())
			if (cell->type == ID($lut)) {
				sort.node(cell);
				for (auto bit : pm.sigmap(cell->getPort(ID::A)))
				if (lut_drivers.count(bit))
					sort.edge(lut_drivers.at(bit), cell);
			}
			log_assert(sort.sort());

			int ncuts = 0, save = 0;
			for (auto root : sort.sorted) {
				if (root->type != ID($lut))
					continue;

				int depth = 1;
				for (auto i : pm.sigmap(root->getPort(ID::A)))
				if (lut_drivers.count(i) && lut_drivers.at(i)->attributes.count(ID(depth)))
					depth = std::max(depth, lut_drivers.at(i)->attributes[ID(depth)].as_int() + 1);
				root->attributes[ID(depth)] = depth;

				pm.ud_lut_cuts.root = root;

				pm.run_lut_cuts([&](){
					auto const &st = pm.st_lut_cuts;
					int nleaves = st.leaves.size();
					int nouterfans = st.outerfans.size();
					float weight = ((float) st.nluts - nouterfans + 1) / ((nleaves + lutsize - 3) / (lutsize - 1));

					if (weight < w_cutoff)
						return;

					ncuts++;

					if (0) {
						SigBit y = root->getPort(ID::Y);
						std::string leaves_select;
						for (auto bit : st.leaves)
							leaves_select += stringf("w:%s ", log_id(bit.wire->name));
						log_debug("  select -set leaves %s%%%%; show w:%s %%ci*:@leaves\n\n",
								  leaves_select.c_str(), log_id(y.wire->name));
					}

					LutNetwork old_net;
					old_net.import(pm.sigmap, lut_drivers, st.leaves, st.outerfans.export_all());

					std::vector<int> new_vars;
					int new_nluts;
					auto table = old_net.thruth_table();
					std::tie(new_vars, new_nluts) = explore_varchoices(table);
					if (new_nluts < st.nluts) {
						LutNetwork new_net;
						new_net.ninputs = old_net.ninputs;
						implement_varchoices(table, new_vars, new_net);

						if (1) {
							log_debug("W: %1.2f nluts: %d -> %d\n", weight, st.nluts, new_nluts);
							std::string dump;
							dump = old_net.dump_output();
							log_debug("Old: %s\n", dump.c_str());
							dump = new_net.dump_output();
							log_debug("New: %s\n", dump.c_str());
						}

						int new_depth = -1;
						if (root->attributes.count(ID(depth_envelope))) {
							int envelope = root->attributes[ID(depth_envelope)].as_int();

							dict<std::pair<bool, int>, int> depth_map;

							for (int i = 0; i < nleaves; i++)
							if (lut_drivers.count(st.leaves[i]) &&
									lut_drivers.at(st.leaves[i])->attributes.count(ID(depth)))
								depth_map[std::make_pair(true, i)] = lut_drivers[st.leaves[i]]->attributes[ID(depth)].as_int();

							for (int i = 0; i < new_net.nodes.size(); i++) {
								int depth = 1;
								for (auto in : new_net.nodes[i].inputs)
									depth = std::max(depth, depth_map[in] + 1);
								depth_map[std::make_pair(false, i)] = depth;
							}

							new_depth = depth_map[std::make_pair(false, new_net.outs.front())];

							if (new_depth > envelope) {
								log_debug("Rejected due to depth\n");
								return;
							}
						}

						save += st.nluts - new_nluts;

						log_assert(new_net.thruth_table() == old_net.thruth_table());
						log_assert(new_net.nodes.size() == new_nluts);
						// Wooo!

						for (auto node : old_net.nodes)
							pm.blacklist(node.cell);

						SigBit y = root->getPort(ID::Y);
						root->setPort(ID::Y, m->addWire(NEW_ID, 1));

						m->connect(y, new_net.emit(m, st.leaves));
						if (new_depth != -1)
							new_net.nodes.back().cell->attributes[ID(depth)] = new_depth;
						lut_drivers[pm.sigmap(y)] = new_net.nodes.back().cell;
						did_something = true;
					}
				});
			}

			log("Visited %d cuts. Saved %d LUTs.\n", ncuts, save);
		}

		if (did_something)
			d->scratchpad_set_bool("opt.did_something", true);
	}
} LutrewriteOncePass;

struct LutrewritePass : Pass {
	LutrewritePass() : Pass("lutrewrite", "rewrite local patches of LUT network") {}
	void execute(std::vector<std::string> args, RTLIL::Design *d) override
	{
		log_header(d, "Executing LUTREWRITE pass. (rewrite local patches of LUT network)\n");

		std::string passdown_args = "";
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-lut" && argidx + 1 < args.size())
				passdown_args += stringf("-lut %s ", args[++argidx].c_str());
			else if (args[argidx] == "-luts" && argidx + 1 < args.size())
				passdown_args += stringf("-luts %s ", args[++argidx].c_str());
			else if (args[argidx] == "-outerfans" && argidx + 1 < args.size())
				passdown_args += stringf("-outerfans %s ", args[++argidx].c_str());
			else if (args[argidx] == "-leaves" && argidx + 1 < args.size())
				passdown_args += stringf("-leaves %s ", args[++argidx].c_str());
			else if (args[argidx] == "-w" && argidx + 1 < args.size())
				passdown_args += stringf("-w %s ", args[++argidx].c_str());
			else if (args[argidx] == "-shared")
				passdown_args += "-shared ";
			else
				break;
		}
		extra_args(args, argidx, d);

		while (true) {
			d->scratchpad_unset("opt.did_something");

			Pass::call(d, "lutdepth -write_attrs");
			Pass::call(d, stringf("lutrewrite_once %s", passdown_args.c_str()));
			Pass::call(d, "opt_clean");

			if (!d->scratchpad_get_bool("opt.did_something"))
				break;
		}		
	}
} LutrewritePass;

PRIVATE_NAMESPACE_END
