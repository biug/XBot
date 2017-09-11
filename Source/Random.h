#pragma once

#include <random>

namespace XBot
{

class Random
{
private:
	std::minstd_rand _rng;

public:
	Random();

	int index(int n);

	static Random & Instance();
};

}
