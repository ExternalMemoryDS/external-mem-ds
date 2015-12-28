#include "vector.h"
#include <random>
#include <functional>
#include <iostream>

#define NUM_INSERT 3

int main()
{
    vector<int> exvec("./randvec", (size_t) 4096);
    
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(1,1000);
    
    auto dice = std::bind ( distribution, generator );
    
    for(auto i = 1; i<=NUM_INSERT; i++)
        exvec.push_back(dice());
    
    vector<int>::iterator it = exvec.begin();
    for(;it!=exvec.end();++it)
        std::cout<<*it<<std::endl;
    return 0;
}