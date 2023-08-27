//
// toymap -- Toy technology mapper of logic networks
//

#define NPRIORITY_CUTS	8
#define CUT_MAXIMUM		6

#include <cstdlib>
#include <vector>
#include <map>

// Include Yosys stuff
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/log.h"
#include "kernel/ff.h"

namespace RTLIL = Yosys::RTLIL;
using Yosys::log;
using Yosys::log_error;
using Yosys::log_id;
using Yosys::ys_debug;
using Yosys::State;

struct AndNode;

struct CoverFaninList;
// A node in the acyclic cover of the cyclic graph
struct CoverNode {
	int lag;
	AndNode *img;

	CoverNode shift(int delta)
	{
		return CoverNode{ lag + delta, img };
	}

	bool operator==(const AndNode *node) const
			{ return node == img && !lag; }
	bool operator<(const CoverNode other) const
			{ return std::tie(lag, img) < std::tie(other.lag, other.img); }
	bool operator==(const CoverNode other) const
			{ return std::tie(lag, img) == std::tie(other.lag, other.img); }

	CoverFaninList fanins();
};

struct CutList {
	CoverNode *array;
	int size;
	int lag_inject;

	CutList(CoverNode *array, int inject=0)
		: array(array), lag_inject(inject)
	{
		CoverNode *p;
		for (p = array; (p < array + CUT_MAXIMUM) && (p->img != NULL); p++);
		size = p - array;
	}

	CutList inject_lag(int delta)
	{
		CutList ret = *this;
		ret.lag_inject += delta;
		return ret;
	}

	class iterator: public std::iterator<std::input_iterator_tag, CoverNode> {
		CoverNode *p;
		int lag_inject;
	public:
		iterator(CoverNode *p,int inject) : p(p), lag_inject(inject) {}
		iterator& operator++() { p++; return *this; }
		bool operator==(const iterator &other) const
			{ return p == other.p && lag_inject == other.lag_inject; }
		int operator-(const iterator &other) const
			{ log_assert(other.lag_inject == lag_inject); return p - other.p; }
		bool operator!=(const iterator &other) const { return !(*this == other); }
		CoverNode operator*() const { return CoverNode{p->lag + lag_inject, p->img}; }
	};
	iterator begin() const { return iterator(array, lag_inject); };
	iterator end()   const { return iterator(array + size, lag_inject); };
};

State invert(State state)
{
	switch (state) {
	case State::S0: return State::S1;
	case State::S1: return State::S0;
	case State::Sx: return State::Sx;
	default: log_assert(false); return State::Sx;
	}
}

struct NodeInput {
	NodeInput() {}

	AndNode *node = NULL;
	struct EdgeFeatures {
		bool negated = false;
		int lag = 0;
		std::vector<State> initvals;

		bool empty() const
		{
			return !negated && lag == 0;
		}

		// Add the features of another edge when the two edges are being
		// chained -- this one is closer to the load and the other one is
		// closer to the driver
		void add(EdgeFeatures other)
		{
			if (other.negated) {
				negated ^= true;
				for (int i = 0; i < lag; i++)
					initvals[i] = invert(initvals[i]);
			}

			initvals.insert(initvals.end(), other.initvals.begin(),
							other.initvals.end());
			lag += other.lag;
		}

		bool crop_const_lag()
		{
			log_assert((int) initvals.size() == lag);
			int i;
			for (i = initvals.size() - 1; i >= 0; i--)
			if (initvals[i] == State::Sx || 
					initvals[i] == State::S0)
				initvals.pop_back();
			else
				break;
			if (i < lag - 1) {
				lag = (int) initvals.size();
				return true;	
			} else {
				return false;
			}
		}

		bool initvals_undef()
		{
			for (auto bit : initvals)
			if (bit != RTLIL::State::Sx)
				return false;
			return true;
		}

		void fixup_initvals()
		{
			initvals = std::vector<RTLIL::State>(lag, RTLIL::State::Sx);
		}
	} feat;

	void negate()
	{
		feat.negated ^= true;
	}

	void delay(State initval)
	{
		feat.lag++;
		feat.initvals.push_back(initval);
	}

	void set_node(AndNode *source)
	{
		feat = {};
		node = source;
	}

	void set_const(int state)
	{
		node = NULL;
		feat = {};
		log_assert(state == 1 || state == 0);
		if (state)
			negate();
	}

	bool is_const()
	{
		return !node && feat.lag == 0;
	}

	bool eval()
	{
		log_assert(is_const());
		return feat.negated;
	}

	std::string describe(int descend);

	bool expand();
	bool tied_to(const NodeInput &other);
	bool eval_conditionally(const NodeInput &other);
	bool assume(const NodeInput &other);

	std::vector<bool> truth_table(CutList cutlist);
};

struct AndNode {
	bool pi = false;
	bool po = false;

	NodeInput ins[2];

	RTLIL::IdString label;
	AndNode() {};

	// Scratch area for algorithms
	bool visited;
	RTLIL::SigBit yw = {};
	union {
		int refs;
		struct {
			CoverNode cut[CUT_MAXIMUM];	
			int area_flow;
			int edge_flow;
			int map_fanouts;
			int fanouts;
		};
		bool has_foreign_cell_users;
		int timedelta;
	};
	int depth_limit;
	int fid; // frontier index
	int depth;

	AndNode *po_fanin()
	{
		log_assert(po);
		log_assert(ins[1].is_const() && ins[1].eval() == 1);
		log_assert(ins[0].node);
		return ins[0].node;
	}

	std::vector<bool> truth_table()
	{
		return truth_table(CutList{cut, 0});
	}

	std::vector<bool> truth_table(CutList cutlist, bool negate=false)
	{
		for (auto it = cutlist.begin(); it != cutlist.end(); ++it)
		if (*it == this) {
			int index = it - cutlist.begin();
			std::vector<bool> ret;
			for (int i = 0; i < (1 << cutlist.size); i++)
				ret.push_back((((i >> index) & 1) ^ negate) != 0);
			return ret;
		}

		log_assert(!pi && "spilled cut");
		std::vector<bool> a = ins[0].truth_table(cutlist);
		std::vector<bool> b = ins[1].truth_table(cutlist);
		log_assert(a.size() == 1 << cutlist.size && b.size() == 1 << cutlist.size);
		std::vector<bool> ret;
		for (int i = 0; i < (1 << cutlist.size); i++)
			ret.push_back((a[i] && b[i]) ^ negate);
		return ret;
	}

	void combine_label(RTLIL::IdString other_label)
	{
		if (other_label.empty())
			return;

		if (label.empty()) {
			label = other_label;
			return;
		}

		if (std::make_pair(other_label.isPublic(), -other_label.size())
					> std::make_pair(label.isPublic(), -label.size()))
			label = other_label;
	}

	bool expand();

	struct FaninList {
		AndNode *node;
		class iterator: public std::iterator<std::input_iterator_tag, AndNode *> {
			AndNode *node; int pos;
		public:
			iterator(AndNode *node, int pos)
				: node(node), pos(pos) {}
			iterator& operator++() { pos++; return *this; }
			bool operator==(const iterator &other) const
				{ return std::tie(node, pos) == std::tie(other.node, other.pos); }
			bool operator!=(const iterator &other) const
				{ return !(*this == other); }
			AndNode *operator*() const
				{ return node->ins[pos].node; }
		};
		iterator begin() const
			{ return iterator(node, node->pi ? 0 : (node->ins[0].node == NULL)); };
		iterator end()   const
			{ return node->pi ? begin() : iterator(node, (node->ins[1].node == NULL) ? 1 : 2); };
	};

	FaninList fanins()
	{
		return FaninList{ this };
	}
};

struct CoverFaninList {
	CoverNode node;
	class iterator: public std::iterator<std::input_iterator_tag, CoverNode> {
		CoverNode node; int pos;
	public:
		iterator(CoverNode node, int pos)
			: node(node), pos(pos) {}
		iterator& operator++() { pos++; return *this; }
		bool operator==(const iterator &other) const
			{ return std::tie(node, pos) == std::tie(other.node, other.pos); }
		bool operator!=(const iterator &other) const
			{ return !(*this == other); }
		CoverNode operator*() const
			{ return CoverNode{node.lag + node.img->ins[pos].feat.lag,
							   node.img->ins[pos].node}; }
	};
	iterator begin() const
		{ return iterator(node, node.img->pi ? 0 : (node.img->ins[0].node == NULL)); };
	iterator end()   const
		{ return node.img->pi ? begin() : iterator(node, (node.img->ins[1].node == NULL) ? 1 : 2); };
};

CoverFaninList CoverNode::fanins()
{
	return CoverFaninList{ *this };
}

#if 0
std::string NodeInput::describe(int descend)
{
	std::string ret;
	if (feat.negated) ret += "~";
	if (feat.lag) {
		ret += "[";
		for (auto it = feat.initvals.rbegin(); it != feat.initvals.rend(); it++)
		switch (*it) {
		case State::S1:
			ret += "1";
			break;
		case State::S0:
			ret += "0";
			break;
		default:
			ret += "x";
			break;
		}
		ret += "]";
	}

	if (!node) {
		ret += "0";
	} else if (descend && !node->pi) {
		std::string in0 = node->ins[0].describe(descend - 1);
		std::string in1 = node->ins[1].describe(descend - 1);
		ret += Yosys::stringf("(%s && %s)", in0.c_str(), in1.c_str());
	} else {
		ret += Yosys::stringf("%s", node->label.c_str());
	}

	return ret;
}
#endif

std::vector<bool> NodeInput::truth_table(CutList cutlist)
{
	log_assert(feat.initvals_undef());

	if (!node) {
		return std::vector<bool>(1 << cutlist.size, feat.negated);
	} else {
		return node->truth_table(cutlist.inject_lag(-feat.lag), feat.negated);
	}
}

bool NodeInput::expand()
{
	if (is_const())
		return false;
	if (!node)
		return feat.crop_const_lag();

	if (node->pi)
		return false;

	NodeInput *in0, *in1;
	in0 = &node->ins[0];
	in1 = &node->ins[1];
	if (!in0->is_const())
		std::swap(in0, in1);
	if (!in0->is_const())
		return false;
	if (in0->eval()) {
		if (in1->node == node)
			return false;
		feat.add(in1->feat);
		node = in1->node;
	} else {
		set_const(feat.negated);
	}
	return true;
}

bool NodeInput::tied_to(const NodeInput &other)
{
	return node && other.node &&
		node == other.node && feat.lag == other.feat.lag;
}

bool NodeInput::eval_conditionally(const NodeInput &other)
{
	log_assert(tied_to(other));
	return !(feat.negated ^ other.feat.negated);
}

bool NodeInput::assume(const NodeInput &other)
{
	if (is_const())
		return false;

	if (tied_to(other)) {
		set_const(eval_conditionally(other));
		return true;
	}

	NodeInput *in0, *in1;
	in0 = &node->ins[0];
	in1 = &node->ins[1];
	if (in0->is_const() || !in0->tied_to(other))
		std::swap(in0, in1);
	if (in0->is_const() || !in0->tied_to(other))
		return false;
	if (in0->eval_conditionally(other)) {
		EdgeFeatures save = feat;
		*this = *in1;
		feat.add(save);
	} else {
		set_const(feat.negated);
	}
	return true;
}

bool AndNode::expand()
{
	if (pi) return false;
	bool did = false;
	while (ins[0].expand()) did = true;
	while (ins[1].expand()) did = true;
	while (ins[0].assume(ins[1]) || ins[1].assume(ins[0]))
		did = true;
	return did;
}

struct Network {
	std::vector<AndNode*> nodes;
	bool impure_module = false;
	int frontier_size = 0;
	int max_cut = 0;
	bool no_exact_area = false;

	Network() {}
	~Network() {
		for (auto node : nodes)
			delete node;
	}

	Network (const Network&) = delete;
	Network& operator= (const Network&) = delete;
	Network(Network&& other) {

		impure_module = other.impure_module;
		frontier_size = other.frontier_size;
		max_cut = other.max_cut;
		no_exact_area = other.no_exact_area;
	}

	void yosys_import(RTLIL::Module *m, bool import_ff=false)
	{
		Yosys::SigMap sigmap(m);

		Yosys::dict<RTLIL::SigBit, AndNode*> wire_nodes;

		AndNode *node;
		wire_nodes[RTLIL::State::S0] = node = new AndNode();
		node->has_foreign_cell_users = false;
		node->visited = false;
		nodes.push_back(node);
		node->ins[0].set_const(0);
		node->ins[1].set_const(0);
		wire_nodes[RTLIL::State::S1] = node = new AndNode();
		node->has_foreign_cell_users = false;
		node->visited = false;
		nodes.push_back(node);
		node->ins[0].set_const(1);
		node->ins[1].set_const(1);

		// uh oh
		wire_nodes[RTLIL::State::Sx] = node = new AndNode();
		node->has_foreign_cell_users = false;
		node->visited = false;
		nodes.push_back(node);
		node->ins[0].set_const(0);
		node->ins[1].set_const(0);

		// Generate nodes for all wires, these will be pruned
		// based on usage later
		for (auto wire : m->wires())
		for (int i = 0; i < wire->width; i++) {
			RTLIL::SigBit bit(wire, i);
			RTLIL::SigBit mapped = sigmap(bit);

			if (!mapped.wire)
				continue;

			if (wire_nodes.count(mapped)) {
				node = wire_nodes.at(mapped);
			} else {
				wire_nodes[mapped] = node = new AndNode();
				node->has_foreign_cell_users = false;
				node->visited = false;
				node->yw = mapped;
				nodes.push_back(node);
			}

			if (wire->port_input)
				node->pi = true;
			if (wire->port_output)
				node->has_foreign_cell_users = true;

			if (bit.wire->width == 1)
				node->combine_label(bit.wire->name);
			else
				node->combine_label(Yosys::stringf("%s[%d]",
						bit.wire->name.c_str(), bit.offset));
		}

		Yosys::pool<RTLIL::IdString> known_cells;
		if (import_ff)
			known_cells = {ID($_AND_), ID($_NOT_), ID($ff)};
		else
			known_cells = {ID($_AND_), ID($_NOT_)};

		for (auto cell : m->cells())
		if (!known_cells.count(cell->type))
		for (auto &conn : cell->connections_) {
			for (auto bit : sigmap(conn.second))
			if (bit.wire) {
				wire_nodes.at(bit)->has_foreign_cell_users = true;
				log_assert(wire_nodes.at(bit)->yw.wire);
			}
		}

		Yosys::FfInitVals initvals;
		if (import_ff)
			initvals.set(&sigmap, m);

		std::vector<RTLIL::Cell *> imported_cells;

		for (auto cell : m->cells()) {
			if (cell->type == ID($ff) && import_ff) {
				for (int i = 0; i < cell->getParam(Yosys::ID::WIDTH).as_int(); i++) {
					RTLIL::SigBit q = sigmap(cell->getPort(Yosys::ID::Q)[i]);
					AndNode *node = wire_nodes.at(q);
					NodeInput *ins = node->ins;

					if (node->has_foreign_cell_users) {
						node->po = true;
						log_assert(node->yw.wire);
					}

					ins[0].set_node(wire_nodes.at(sigmap(cell->getPort(Yosys::ID::D)[i])));
					ins[0].delay(initvals(q));
					ins[1].set_const(1);
				}
				imported_cells.push_back(cell);
			} else if (cell->type.in(ID($_AND_), ID($_NOT_))) {
				AndNode *node = wire_nodes.at(sigmap(cell->getPort(Yosys::ID::Y)));

				if (node->has_foreign_cell_users) {
					node->po = true;
					log_assert(node->yw.wire);

					AndNode *indirect_node = new AndNode();
					node->ins[0].set_node(indirect_node);
					node->ins[1].set_const(1);

					node = indirect_node;
					node->has_foreign_cell_users = false;
					node->visited = false;
					nodes.push_back(node);
				}

				NodeInput *ins = node->ins;

				if (cell->type == ID($_AND_)) {
					ins[0].set_node(wire_nodes.at(sigmap(cell->getPort(Yosys::ID::A))));
					ins[0].node->visited = true;
					ins[1].set_node(wire_nodes.at(sigmap(cell->getPort(Yosys::ID::B))));
					ins[1].node->visited = true;
				} else if (cell->type == ID($_NOT_)) {
					ins[0].set_node(wire_nodes.at(sigmap(cell->getPort(Yosys::ID::A))));
					ins[0].node->visited = true;
					ins[0].negate();
					ins[1].set_const(1);
				}
				imported_cells.push_back(cell);
			} else {
				// There are foreign cells in the module
				impure_module = true;
			}
		}

		for (auto cell : imported_cells)
			m->remove(cell);

		for (auto node : nodes)
		if (node->has_foreign_cell_users && node->visited && !node->po)
			node->pi = true;

		clean(false);
		compact();
		clean(false);
		log("Imported %lu nodes\n", nodes.size());
	}

	void yosys_perimeter(RTLIL::Module *m)
	{
		(void) m;
		for (auto node : nodes)
		if (!node->po && !node->pi)
			node->yw = RTLIL::SigBit();
		else
			log_assert(node->yw.wire);
	}

	void yosys_wires(RTLIL::Module *m, bool mapping_only=false)
	{
		for (auto node : nodes) {
			if (mapping_only && !node->map_fanouts)
				continue;
			if (node->yw.wire)
				continue;
			RTLIL::Wire *w;
			RTLIL::IdString label = node->label;
			if (label.empty())
				label = NEW_ID;
			while (m->wire(label))
				label = Yosys::stringf("%s_", label.c_str());
			w = m->addWire(label, 1);
			node->yw = RTLIL::SigBit(w, 0);
		}
	}

	void yosys_export(RTLIL::Module *m)
	{
		yosys_perimeter(m);
		yosys_wires(m);

		for (auto node : nodes) {
			if (node->pi) continue;
			NodeInput *nin = node->ins;
			RTLIL::SigBit yin[2];

			if (nin[0].feat.negated && !nin[1].feat.negated)
				std::swap(nin[0], nin[1]);

			for (int j = 0; j < 2; j++) {
				if (nin[j].node)
					yin[j] = nin[j].node->yw;
				else
					yin[j] = RTLIL::State::S0;

				log_assert(nin[j].feat.lag == (int) nin[j].feat.initvals.size());
				for (auto it = nin[j].feat.initvals.rbegin();
						it != nin[j].feat.initvals.rend(); it++) {
					RTLIL::SigBit q = m->addWire(NEW_ID, 1);
					m->addFf(NEW_ID, yin[j], q);
					if (*it != State::Sx)
						q.wire->attributes[RTLIL::ID::init] =
							RTLIL::Const(*it != State::S0 ? 1 : 0, 1);
					yin[j] = q;
				}
				if (nin[j].feat.negated)
					yin[j] = m->NotGate(NEW_ID, yin[j]);
			}
			m->addAndGate(NEW_ID, yin[0], yin[1], node->yw);
		}
	}

	int clean(bool verbose=true)
	{
		std::vector<AndNode*> used;
		for (auto node : nodes)
		if (node->po || node->pi) {
			node->visited = true;
			used.push_back(node);
		} else {
			node->visited = false;
		}

		for (int i = 0; i < (int) used.size(); i++) {
			for (auto in : used[i]->fanins()) {
				if (!in->visited) {
					used.push_back(in);
					in->visited = true;
				}
			}
		}

		int nremoved = 0;
		for (auto node : nodes)
		if (!node->visited) {
			nremoved++;
			delete node;
		}

		std::reverse(used.begin(), used.end());
		used.swap(nodes);

		if (verbose)
			log("Removed %d unused nodes\n", nremoved);
		return nremoved;
	}

	void compact(bool verbose=false)
	{
		bool did = false;
		while (true) {
			did = false;
			for (auto node : nodes)
				did |= node->expand();
			if (!did)
				break;
		}
		clean(verbose);
	}

	void check_sort()
	{
		for (auto node : nodes)
			node->visited = false;

		for (auto node : nodes) {
			if (!node->pi) {
				if (node->ins[0].node)
					log_assert(node->ins[0].node->visited);
				if (node->ins[1].node)
					log_assert(node->ins[1].node->visited);
			}
			node->visited = true;
		}
	}

	void tsort()
	{
		std::vector<AndNode*> order;

		for (auto node : nodes)
			node->refs = 0;
		for (auto node : nodes)
		for (auto fanin : node->fanins())
			fanin->refs++;

		for (auto node : nodes)
		if (!node->refs)
			order.push_back(node);

		for (int i = 0; i < (int) order.size(); i++)
		for (auto fanin : order[i]->fanins()) {
			fanin->refs--;
			if (!fanin->refs)
				order.push_back(fanin);
			log_assert(fanin->refs >= 0);
		}

		std::reverse(order.begin(), order.end());
		log_assert(order.size() == nodes.size());
		nodes.swap(order);
		check_sort();
	}

	void check_frontier()
	{
		AndNode **cache = new AndNode*[frontier_size];
		for (auto node : nodes) {
			cache[node->fid] = node;
			AndNode *n1 = node->ins[0].node;
			AndNode *n2 = node->ins[1].node;
			if (n1)
				log_assert(cache[n1->fid] == n1);
			if (n2)
				log_assert(cache[n2->fid] == n2);
		}
		delete[] cache;
	}

	void frontier()
	{
		frontier_size = 1; // first item is special (used for PO scratch)
		std::vector<int> free_indices;

		for (auto node : nodes) {
			node->fid = 0;
			node->visited = false;
		}

		for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
			for (auto node : (*it)->fanins()) {
				log_assert(!node->visited);
				if (!node->fid) {
					if (free_indices.empty())
						free_indices.push_back(frontier_size++);
					node->fid = free_indices.back();
					free_indices.pop_back();
				}
			}

			// free our index
			if ((*it)->fid != 0)
				free_indices.push_back((*it)->fid);
			(*it)->visited = true;
		}

		check_frontier();
		log("Frontier is %d wide at its peak\n", frontier_size);
	}

	void walk_mapping()
	{
		for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
			AndNode *node = *it;
			log_assert(node->map_fanouts >= 0);
			log_assert(node->map_fanouts <= node->po ? 1 : 0);

			if (node->map_fanouts)
				deref_cut(node);
			node->map_fanouts = 0;
		}

		for (auto node : nodes)
		if (node->po)
		if (!node->map_fanouts++)
			ref_cut(node);

		int area = 0, support_area = 0;
		for (auto node : nodes) {
			log_assert(!node->po || (node->ins[0].node && node->ins[1].is_const()));
			if (node->pi || node->po)
				continue;
			if (node->map_fanouts)
				area++;
			if (node->map_fanouts == 1)
				support_area++;
		}

		log("Mapping: Area is %d nodes (of those %d are single-fanout)\n",
			area, support_area);
	}

	void apply_timedelta()
	{
		for (auto node : nodes)
		for (int i = 0; i < 2; i++) {
			if (node->pi)
				continue;

			log_assert(node->ins[i].feat.initvals_undef());
			if (node->ins[i].node)
				node->ins[i].feat.lag += node->timedelta -
								node->ins[i].node->timedelta;
			else
				node->ins[i].feat.lag = 0;
			node->ins[i].feat.fixup_initvals();
		}
	}

	// Debugging tool
	void scramble_lag()
	{
		tsort();

		for (auto node : nodes) {
			if (node->pi) {
				node->timedelta = 0;
				continue;
			}

			int mindelta = -2;
			for (auto fanin : CoverNode{0, node}.fanins())
				mindelta = std::max(mindelta, fanin.img->timedelta - fanin.lag);

			node->timedelta = mindelta + (rand() % 2);
		}

		apply_timedelta();
	}

	void trivial_cuts()
	{
		for (auto node : nodes) {
			node->map_fanouts = 0;
			int pos = 0;
			for (auto fanin : CoverNode{0, node}.fanins())
				node->cut[pos++] = fanin;
			if (pos < CUT_MAXIMUM)
				node->cut[pos].img = NULL;
		}

		walk_mapping();
	}

	static void deref_cut(AndNode *node)
	{
		if (!node->pi)
		for (auto cut_node : CutList{node->cut}) {
			log_assert(cut_node.img != node);
			log_assert(cut_node.img->map_fanouts >= 1);
			if (!--cut_node.img->map_fanouts)
				deref_cut(cut_node.img);
		}
	}

	static int ref_cut(AndNode *node)
	{
		int sum = 0;
		if (!node->pi)
		for (auto cut_node : CutList{node->cut}) {
			log_assert(cut_node.img != node);
			if (!cut_node.img->map_fanouts++)
				sum += 1 + ref_cut(cut_node.img);
			log_assert(cut_node.img->map_fanouts >= 1);
		}
		return sum;
	}

	template<typename CutEvaluation>
	void cuts(bool consider_previous_cut=true)
	{
		log_assert(max_cut <= CUT_MAXIMUM);
		struct NodeCache {
			int ps_len;
			struct PriorityCut {
				CoverNode cut[CUT_MAXIMUM];
			} ps[NPRIORITY_CUTS];
			AndNode *mark;
		};

		NodeCache *cache = new NodeCache[frontier_size];

		// Go over the nodes in topological order
		for (auto node : nodes) {
			// Clear the cache
			NodeCache *lcache = &cache[node->fid];
			lcache->ps_len = 0;
			lcache->mark = node;

			AndNode *n1 = node->ins[0].node;
			AndNode *n2 = node->ins[1].node;

			int lag1 = node->ins[0].feat.lag;
			int lag2 = node->ins[1].feat.lag;

			// Find the best NPRIORITY_CUTS cuts according
			// to CutEvaluation
			std::map<std::pair<CutEvaluation, int>, int> leaderboard;

			if (node->pi) {
				// PI has no non-trivial cut
				lcache->ps_len = 1;
				lcache->ps[0].cut[0] = CoverNode{0, node};
				lcache->ps[0].cut[1] = CoverNode{0, NULL};

				// Selected cut is empty
				node->cut[0].img = NULL;
				continue;
			}

			if (node->po) {
				// PO has empty cache
				lcache->ps_len = 0;

				// Selected cut is the trivial one
				node->cut[0] = CoverNode{0, node->ins[0].node};
				node->cut[1] = CoverNode{0, NULL};
				continue;
			}

			log_assert(n1 && n2);

			CoverNode t1_nodes[2] = { {0, n1}, {0, NULL} };
			CoverNode t2_nodes[2] = { {0, n2}, {0, NULL} };
			CutList t1(t1_nodes, 0);
			CutList t2(t2_nodes, 0);

			CoverNode working_cut[CUT_MAXIMUM];	

			log_assert(cache[n1->fid].mark == n1);
			log_assert(cache[n2->fid].mark == n2);

			if (consider_previous_cut) {
				lcache->ps_len++;
				std::copy(node->cut, node->cut + CUT_MAXIMUM, lcache->ps[0].cut);
				leaderboard[
					std::make_pair(CutEvaluation(CutList(node->cut), node),
								   std::numeric_limits<int>::max())] = 0;
				log_assert(!leaderboard.empty());
			}

			for (int i = -1; i < cache[n1->fid].ps_len; i++)
			for (int j = -1; j < cache[n2->fid].ps_len; j++) {
				CutList n1_cut = (i == -1) ? t1 : cache[n1->fid].ps[i].cut;
				CutList n2_cut = (j == -1) ? t2 : cache[n2->fid].ps[j].cut;

				std::map<CoverNode, bool> cook;
				for (auto cut_node : n1_cut.inject_lag(lag1)) cook[cut_node] = true;
				for (auto cut_node : n2_cut.inject_lag(lag2)) cook[cut_node] = true;

				int cutlen = 0;
				int hash = 0;
				for (auto pair : cook) {
					if (cutlen >= max_cut)
						goto reject;
					working_cut[cutlen++] = pair.first;
					hash += (int) (long long) pair.first.img;
				}
				if (0) { reject: continue; }
				if (cutlen < CUT_MAXIMUM)
					working_cut[cutlen] = CoverNode{0, NULL};

				auto working_eval = std::make_pair(CutEvaluation(CutList(working_cut), node), hash);

				if (working_eval.first.reject(node) || leaderboard.count(working_eval))
					continue;

				int slot;
				if (lcache->ps_len < NPRIORITY_CUTS) {
					// Slot is assured
					leaderboard[working_eval] = slot = lcache->ps_len++;
				} else {
					// Post the new cut on the leaderboard, and if that sinks one
					// of the earlier cached cuts below the cutoff, reuse the slot
					// in the `lcache->ps` array for the new cut
					leaderboard[working_eval] = -1;
					slot = (leaderboard.rbegin())->second;
					leaderboard.erase(std::prev(leaderboard.end()));

					if (slot != -1)
						leaderboard[working_eval] = slot;
				}
 
				if (slot == -1)
					continue;

				std::copy(working_cut, working_cut + CUT_MAXIMUM, lcache->ps[slot].cut);
			}

			log_assert(!leaderboard.empty());
			CoverNode *best_cut = lcache->ps[leaderboard.begin()->second].cut;

			if (node->map_fanouts)
				deref_cut(node);

			std::copy(best_cut, best_cut + CUT_MAXIMUM, node->cut);
			leaderboard.begin()->first.first.select_on(node);

			if (node->map_fanouts)
				ref_cut(node);
		}

		delete[] cache;
	}

	struct DepthEval {
		int depth;
		int cut_width;
		int area_flow;
		int edge_flow;

		DepthEval(CutList cutlist, AndNode *node, bool area_flow2=false)
		{
			depth = 0;
			cut_width = 0;
			for (auto cut_node : cutlist) {
				depth = std::max(depth, cut_node.img->depth + 1);
				cut_width++;
			}

			if (area_flow2) {
				area_flow = compute_area_flow(cutlist, node);
			} else {
				area_flow = 100;
				for (auto cut_node : cutlist)
					area_flow += cut_node.img->area_flow;
				area_flow /= std::max(1, node->map_fanouts);
			}

			edge_flow = 100 * cut_width;
			for (auto cut_node : cutlist)
				edge_flow += cut_node.img->edge_flow;
			edge_flow /= std::max(1, node->map_fanouts);
		}

		static int compute_area_flow(CutList cutlist, AndNode *node, bool top=true)
		{
			int area_flow = 100;

			if (node->visited || node->pi)
				return 0;

			node->visited = true;

			for (auto cut_node : cutlist) {
				if (cut_node.img->visited)
					continue;

				if (!cut_node.img->map_fanouts) {
					area_flow += compute_area_flow(CutList{cut_node.img->cut}, cut_node.img, false);
				} else {
					cut_node.img->visited = true;
					area_flow += cut_node.img->area_flow;
				}
			}
			area_flow /= std::max(1, node->map_fanouts);

			if (top)
				clear_area_flow_visited(cutlist, node);

			return area_flow;
		}

		static void clear_area_flow_visited(CutList cutlist, AndNode *node)
		{
			if (!node->visited || node->pi)
				return;

			for (auto cut_node : cutlist) {
				if (!cut_node.img->map_fanouts)
					clear_area_flow_visited(CutList{cut_node.img->cut}, cut_node.img);
				else
					cut_node.img->visited = false;
			}

			node->visited = false;
		}

		bool operator<(const DepthEval other) const
			{ return std::tie(depth, cut_width, area_flow, edge_flow)
						< std::tie(other.depth, other.cut_width, other.area_flow, other.edge_flow); }
		bool reject(AndNode *root) const	{ (void) root; return false; }
		void select_on(AndNode *node) const
			{ node->depth = depth; node->area_flow = area_flow; node->edge_flow = edge_flow; }
	};

	struct DepthEval2 : public DepthEval {
		DepthEval2(CutList cutlist, AndNode *node)
			: DepthEval(cutlist, node) {}
		bool operator<(const DepthEval2 other) const
			{ return std::tie(depth, area_flow, edge_flow, cut_width)
						< std::tie(other.depth, other.area_flow, other.edge_flow, other.cut_width); }
	};

	struct DepthEvalInitial : public DepthEval {
		DepthEvalInitial(CutList cutlist, AndNode *node)
			: DepthEval(cutlist, node, false)
		{
			area_flow = 100;
			for (auto cut_node : cutlist)
				area_flow += cut_node.img->area_flow;
			area_flow /= std::max(1, node->fanouts);
			edge_flow = 100 * cut_width;
			for (auto cut_node : cutlist)
				edge_flow += cut_node.img->edge_flow;
			edge_flow /= std::max(1, node->fanouts);
		}
	};

	struct AreaFlowEval : public DepthEval {
		int fanin_refs;
		AreaFlowEval(CutList cutlist, AndNode *node)
				: DepthEval(cutlist, node) {}
		bool reject(AndNode *node) const 
			{ return depth > node->depth_limit; }
		void select_on(AndNode *node) const
			{ log_assert(!reject(node)); DepthEval::select_on(node); }
		bool operator<(const AreaFlowEval other) const
			{ return std::tie(area_flow, edge_flow, cut_width, depth)
						< std::tie(other.area_flow, other.edge_flow, other.cut_width, other.depth); }
	};

	struct ExactAreaEval : public AreaFlowEval {
		int exact_area;
		ExactAreaEval(CutList cutlist, AndNode *node)
				: AreaFlowEval(cutlist, node) {
			exact_area = calc_exact_area(cutlist, node);
		}

		static int calc_exact_area(CutList cutlist, AndNode *node) {
			if (node->pi)
				return 0;
			int ret;

			if (node->map_fanouts)
				deref_cut(node);

			ret = 1;
			for (auto cut_node : cutlist)
			if (!cut_node.img->map_fanouts++)
				ret += 1 + ref_cut(cut_node.img);

			for (auto cut_node : cutlist)
			if (!--cut_node.img->map_fanouts)
				deref_cut(cut_node.img);

			if (node->map_fanouts)
				ref_cut(node);

			return ret;
		}
		bool reject(AndNode *node) const 
			{ return depth > node->depth_limit; }
		bool operator<(const ExactAreaEval other) const
			{ return std::tie(exact_area, cut_width, depth)
						< std::tie(other.exact_area, other.cut_width, other.depth); }
	};

	void spread_depth_limit(int po_depth)
	{
		for (auto node : nodes)
			node->depth_limit = std::numeric_limits<int>::max();
		for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
			if ((*it)->po) {
				(*it)->po_fanin()->depth_limit = po_depth;
			} else {
				for (auto cut_fanin : CutList{(*it)->cut})
					cut_fanin.img->depth_limit =
							std::min(cut_fanin.img->depth_limit,
							 		 (*it)->depth_limit - 1);
			}
		}
	}

	void depth_cuts()
	{
		tsort();
		frontier();

		for (auto node : nodes) {
			node->depth = 0;
			node->area_flow = 0;
			node->edge_flow = 0;
			node->map_fanouts = 0;
			node->fanouts = 0;
			node->visited = false;
		}

		for (auto node : nodes) {
			if (node->po)
				node->fanouts++;
			for (auto fanin : node->fanins())
				fanin->fanouts++;
		}

		cuts<DepthEvalInitial>(false);

		// `map_fanouts` is a reference counter on each AIG node for the number of times
		// that node is used as a fanin in the current mapping draft. Now when the initial cuts
		// (and therefore the mapping) have been selected, we need to walk the mapping once to
		// set the initial reference counts. Subsequent calls to `cuts<>()` will update the
		// reference counts if the selected cut on a node that is part of the mapping changes.
		walk_mapping();

		cuts<DepthEval>();
		cuts<DepthEval2>();

		int target_depth = 0;
		for (auto node : nodes)
		if (node->po)
			target_depth = std::max(target_depth, node->po_fanin()->depth);
		log("Mapping: Depth will be %d\n", target_depth);

		// Call `walk_mapping()` again to print the current area
		walk_mapping();
		log("Mapping: Performing area recovery\n");

		spread_depth_limit(target_depth);
		cuts<AreaFlowEval>();
		spread_depth_limit(target_depth);
		cuts<AreaFlowEval>();

		if (!no_exact_area) {
			spread_depth_limit(target_depth);
			cuts<ExactAreaEval>();
			spread_depth_limit(target_depth);
			cuts<ExactAreaEval>();
		}

		// Walk the mapping once more to (1) check the `map_fanouts` counters for consistence;
		// and (2) print out the final mapping area.
		walk_mapping();
	}

	void dump_cuts()
	{
		for (auto node : nodes) {
			if (node->pi) {
				log("Node %s: PI\n", node->label.c_str());
				continue;
			}
			log("Node %s: (depth %d)\n", node->label.c_str(), node->depth);
			for (auto cut_node : CutList{node->cut})
				log("\t%s (lag %d)\n", cut_node.img->label.c_str(), cut_node.lag);
		}
	}

	void emit_luts(RTLIL::Module *m, bool gate2=false)
	{
		yosys_perimeter(m);
		yosys_wires(m, true);

		for (auto node : nodes) {
			if (!node->map_fanouts || node->pi)
				continue;
			RTLIL::SigSpec yin;
			for (auto cut_node : CutList{node->cut, 0}) {
				RTLIL::SigBit ybit = cut_node.img->yw;
				log_assert(cut_node.lag >= 0);
				for (int i = 0; i < cut_node.lag; i++) {
					RTLIL::SigBit q = m->addWire(NEW_ID, 1);
					m->addFf(NEW_ID, ybit, q);
					ybit = q;
				}
				yin.append(ybit);
			}

			if (yin.size() == 0) {
				m->connect(node->yw, RTLIL::SigBit(node->truth_table()[0]));
				continue;
			}

			if (gate2 && yin.size() == 2) {
				auto tt = node->truth_table();

				if (!tt[2] && tt[1]) {
					std::swap(tt[2], tt[1]);
					std::swap(yin[1], yin[0]);
				}

				switch ((tt[3] << 3) | (tt[2] << 2) | (tt[1] << 1) | tt[0]) {
					case 0b1111:
						m->connect(node->yw, RTLIL::State::S1); continue;
					case 0b1110:
						m->addOrGate(NEW_ID, yin[0], yin[1], node->yw); continue;
					case 0b1101:
						m->addOrnotGate(NEW_ID, yin[1], yin[0], node->yw); continue;
					case 0b1100:
						m->connect(node->yw, yin[1]); continue;
					case 0b1001:
						m->addXnorGate(NEW_ID, yin[0], yin[1], node->yw); continue;
					case 0b1000:
						m->addAndGate(NEW_ID, yin[0], yin[1], node->yw); continue;
					case 0b0111:
						m->addNandGate(NEW_ID, yin[0], yin[1], node->yw); continue;
					case 0b0110:
						m->addXorGate(NEW_ID, yin[0], yin[1], node->yw); continue;
					case 0b0101:
						m->addNotGate(NEW_ID, yin[0], node->yw); continue;
					case 0b0100:
						m->addAndnotGate(NEW_ID, yin[1], yin[0], node->yw); continue;
					case 0b0001:
						m->addNorGate(NEW_ID, yin[0], yin[1], node->yw); continue;
					case 0b0000:
						m->connect(node->yw, RTLIL::State::S0); continue;
				}
				log_assert(false && "unreachable");
			}

			if (yin.size() > 1) {
				m->addLut(NEW_ID, yin, node->yw, node->truth_table());
				continue;
			}

			log_assert(yin.size() == 1);
			auto tt = node->truth_table();
			switch (tt[1] << 1 | tt[0]) {
			case 0b00:
				m->connect(node->yw, RTLIL::State::S0);
				break;
			case 0b10:
				m->connect(node->yw, yin);
				break;
			case 0b01:
				m->addNotGate(NEW_ID, yin[0], node->yw);
				break;
			case 0b11:
				m->connect(node->yw, RTLIL::State::S0);
				break;
			}
		}
	}
};

USING_YOSYS_NAMESPACE
struct ToymapPass : Pass {
	ToymapPass() : Pass("toymap", "toy technology mapping") {}
	void help() override
	{
		log("\n");
		log("    toymap [options] [selection]\n");
		log("\n");
		log("        -ff          do import $ff cells\n");
		log("        -lut N       set maximum LUT arity to N\n");
		log("        -depth_cuts  find mapping by selecting depth-minimizing cuts\n");
		log("                     followed by passes of area recovery\n");
		log("        -no_exact_area  disable exact area pass in area recovery\n");
		log("        -emit_luts   emit LUT mapping\n");
		log("        -emit_gate2  emit 2-input gate mapping\n");
		log("\n");
		log("Examples of use:\n");
		log("\n");
		log("    toymap -lut 4 -depth_cuts -emit_luts\n");
		log("    toymap -lut 2 -depth_cuts -emit_gate2\n");
		log("\n");
		log("Order of options matters (operations are performed in order). Toymap is\n");
		log("allowed to crash if used the wrong way.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *d) override
	{
		log_header(d, "Executing TOYMAP pass.\n");

		std::vector<std::string> commands;

		size_t argidx;

		bool import_ff = false;
		bool no_exact_area = false;
		int lut = 4;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-ff")
				import_ff = true;
			if (args[argidx] == "-no_exact_area")
				no_exact_area = true;
			else if (args[argidx] == "-lut" && argidx + 1 < args.size())
				lut = atoi(args[++argidx].c_str());
			else if (args[argidx][0] == '-')
				commands.push_back(args[argidx]);
			else
				break;
		}
		extra_args(args, argidx, d);

		for (auto m : d->selected_whole_modules_warn()) {
			log("Working on module %s\n", m->name.c_str());

			Network net;
			net.max_cut = lut;
			net.no_exact_area = no_exact_area;
			net.yosys_import(m, import_ff);
			bool emitted = false;
			for (auto cmd : commands) {
				if      (cmd == "-trivial_cuts")  net.trivial_cuts();
				else if (cmd == "-scramble_lag")  net.scramble_lag();
				else if (cmd == "-depth_cuts")    net.depth_cuts();
				else if (cmd == "-dump_cuts")     net.dump_cuts();
				else if (cmd == "-emit_luts")   { net.emit_luts(m); emitted = true; }
				else if (cmd == "-emit_gate2")  { net.emit_luts(m, true); emitted = true; }
				else log_error("Unknown command: %s\n", cmd.c_str());
 			}
 			if (!emitted)
 				net.yosys_export(m);
		}
	}
} ToymapPass;
