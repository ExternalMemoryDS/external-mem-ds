#include "buffer.h"

//Type Definitions
typedef long blocknum_t;
typedef long offset_t;
typedef long long int size_type;

/*
	Burst Trie = Nodes + Buckets
	Nodes:
		Contain 256 pointers (blocknbr, offset) and other parameters.
		1. Internal Nodes
		2. Leaf Nodes
	Buckets:
		Contains pointers (offset) to strings and the "string\0value"s.

*/

struct blockOffsetPair{
	blocknum_t block_number;
	offset_t offset;
};

template <typename V, size_type internal_blksize, size_type data_blksize>
class BTrie
{
private:
	BTrieNode* root;
	BufferedFile *buffered_file_internal, *buffered_file_bucket;

protected:
public:
	BTrie(const char* pathname);
	~BTrie();
};

class BTrieNode
{
private:
	bool isLeaf;
	blockOffsetPair child[256];

protected:
public:
	BTrieNode();
	~BTrieNode();
};

template <typename V>
class Bucket
{
private:
	char startChar;
	char endChar;
	bool isPure;
	
protected:
public:
	Bucket();
	~Bucket();
};