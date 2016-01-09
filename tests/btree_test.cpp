#include "btree.h"
#include <random>
#include <functional>
#include <iostream>
#include <vector>

#define NUM_INSERT 29

struct test_Comparator {
	test_Comparator() {}
	int operator()(int x, int y) const { return x > y; }

};

int main()
{
	BTree<int, int, test_Comparator>* btree_test = new BTree<int, int, test_Comparator>("./btree_test", 512);

	std::default_random_engine generator;
	std::uniform_int_distribution<int> distribution(1,1000);

	auto dice = std::bind ( distribution, generator );

	// test_Comparator cmpl;
	// std::cout << "CMPL : " << cmpl(7, 4) << std::endl;

	// for(auto i = 1; i <= NUM_INSERT; i++) {
	// 	btree_test->insertElem(i, i + 1);
	// 	std::cout << "INSERTED ELEMENT K : " << i << " VALUE : " << (i+1) << std::endl;
	// }

	std::cout << "ELEMENT 4 : " << btree_test->searchElem(4) << std::endl;
	std::cout << "ELEMENT 14 : " << btree_test->searchElem(14) << std::endl;
	// std::cout << "ELEMENT 400 : " << btree_test->searchElem(400) << std::endl;

	std::cout << "SIZE : " << btree_test->size() << std::endl;
	std::cout << std::endl;

	delete btree_test;
	return 0;
}
