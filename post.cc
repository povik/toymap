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

PRIVATE_NAMESPACE_END
