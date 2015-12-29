#include "vector.h"
#include <random>
#include <functional>
#include <iostream>

#define NUM_INSERT 1024

int main()
{
	vector<int> exvec("./randvec", (size_t) 4096);
	
	std::default_random_engine generator;
	std::uniform_int_distribution<int> distribution(1,1000);
	
	auto dice = std::bind ( distribution, generator );
	
	for(auto i = 1; i<=NUM_INSERT; i++)
		exvec.push_back(dice());
	
	std::cout << exvec.size() << std::endl;
	std::cout << std::endl;
	
        exvec.insert(2, 5);
        
	for (auto it = 0; it < exvec.size(); it++)
	{
		std::cout << exvec[it] << std::endl;
	}
	
	//exvec.clear();
	
	std::cout << std::endl;
	std::cout << exvec.size() << std::endl;
	
	return 0;
}