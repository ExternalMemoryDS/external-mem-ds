#include "vector.h"
#include <random>
#include <functional>
#include <iostream>
#include <vector>

#define NUM_INSERT 18

int main()
{
	vector<int> exvec("./randvec", (size_t) 16);
	
	std::default_random_engine generator;
	std::uniform_int_distribution<int> distribution(1,1000);
	
	auto dice = std::bind ( distribution, generator );
	
	for(auto i = 1; i<=NUM_INSERT; i++)
		exvec.push_back(dice());
	
	std::cout << exvec.size() << std::endl;
	std::cout << std::endl;
	
	for (auto it = 0; it < exvec.size(); it++)
	{
		std::cout << exvec[it] << std::endl;
	}
	
	std::cout << std::endl;
	
	//exvec.insert(2, 5);
	std::vector<int> ins_vec;
	for(auto i = 1; i<=1; i++)
		ins_vec.push_back(dice());
	exvec.insert(vector<int>::iterator(16,&exvec), ins_vec.begin(), ins_vec.end());
	
	for (auto it = 0; it < exvec.size(); it++)
	{
		std::cout << exvec[it] << std::endl;
	}
	
	//exvec.clear();
	
	std::cout << std::endl;
	std::cout << exvec.size() << std::endl;
        
	exvec.erase(vector<int>::iterator(2,&exvec), vector<int>::iterator(15,&exvec));
	
	std::cout << std::endl;
	
	for (auto it = exvec.begin(); it != exvec.end(); it++)
	{
		std::cout << *it << std::endl;		
	}
	
	std::cout << std::endl;
	
	for (auto it = exvec.rbegin(); it != exvec.rend(); it++)
	{
		std::cout << *it << std::endl;
		*it = 3;
	}
	
	std::cout << std::endl;
	
	for (auto it = exvec.cbegin(); it != exvec.cend(); it++)
	{
		std::cout << *it << std::endl;
// 		*it = 5;
	}
	
	std::cout << std::endl;
	std::cout << exvec.size() << std::endl;
	
	return 0;
}