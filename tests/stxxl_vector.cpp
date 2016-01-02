#include "stxxl.h"

#define NUM_INSERT 1000000
typedef stxxl::VECTOR_GENERATOR<int>::result stxxl_vector;

int main()
{
	stxxl::syscall_file* outputfile = new stxxl::syscall_file("persistent_vector.data", stxxl::file::RDWR|stxxl::file::CREAT | stxxl::file::DIRECT);
	stxxl_vector v(outputfile);

	std::default_random_engine generator;
	std::uniform_int_distribution<int> distribution(1,1000);
	
	auto dice = std::bind ( distribution, generator );
	
	for(auto i = 1; i<=NUM_INSERT; i++)
		v.push_back(dice());

	std::cout << v.size() << std::endl;
	std::cout << std::endl;

	for (auto it = 0; it < v.size(); it++)
	{
		std::cout << v[it] << std::endl;
	}

	for (auto it = 0; it < v.size(); it++)
		v.pop_back();

	std::cout << std::endl;
}