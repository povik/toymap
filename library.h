#ifndef __LIBRARY_H__
#define __LIBRARY_H__

#include "kernel/log.h"
#include <vector>

struct LutLibrary {
	struct LutVariety {
		int width;
		int cost;
		std::vector<int> delays;

		int arrival(int in, int t)
		{
			log_assert(in < width);
			return t + delays[in];
		}

		bool operator<(const LutVariety &other) const
		{
			return cost < other.cost;
		}
	};

	std::vector<LutVariety> varieties;
	std::vector<LutVariety*> by_width;

	static LutLibrary academic_luts(int k)
	{
		LutLibrary lib;
		lib.add(k, 1, std::vector<int>(k, 1));
		return lib;
	}

	LutVariety &lookup(int width)
	{
		log_assert(width > 0);
		log_assert(width <= (int) by_width.size());
		return *by_width[width - 1];
	}

	const LutVariety &lookup(int width) const
	{
		log_assert(width > 0);
		log_assert(width <= (int) by_width.size());
		return *by_width[width - 1];
	}

	int max_width()
	{
		return (int) by_width.size();
	}

	void add(int width, int cost, std::vector<int> delays)
	{
		log_assert((int) delays.size() == width);
		varieties.emplace_back();
		LutVariety &variety = varieties.back();
		variety.width = width;
		variety.cost = cost;
		variety.delays = delays;
		index();
	}

	void index()
	{
		int max_width = 0;
		for (auto &v : varieties)
			max_width = std::max(max_width, v.width);

		std::sort(varieties.begin(), varieties.end());

		by_width.clear();
		for (int j = 1; j <= max_width; j++) {
			bool found = false;
			for (auto &v : varieties) {
				if (v.width >= j) {
					found = true;
					by_width.push_back(&v);
					break;
				}
			}
			log_assert(found);
		}
	}
};

#endif /* __LIBRARY_H__ */
