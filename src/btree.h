#include "buffer.h"
#include <stdexcept>
#include <stddef.h>

/*
	Actually a B+Tree but using BTree in class names for readability


        Class BTreeNode (Abstract)
              /           \
             /             \
		LeafNode      InternalNode

	Class BTree HAS A BTreeNode root
	BTreeLeaf contains pointers(block nos and offsets) to item records
	The leaves will form a doubly linked list

*/

/*

	The B-Tree on the disk will be stored as one file with
	all the internal nodes and leaf ndoes
	and another one with the data which is pointed to by the leaves

	The structure of the B-Tree key file:


	Part of the file                Size            Comments
	==========================================================================
                                    HEADER
                                    ------
	“RMAD”                          4 bytes         Just for fun! :P
	“BTREE”                         8 bytes         Identifies the data structure
	Element Key size                4 bytes         generally from sizeof(K)
	Element Value size              4 bytes         generally from sizeof(V)
	Root Node block number          sizeof(long)    block no. of the root of Btree
	Total blocks allocated          <temp>          <temp>
	Head block no.                  sizeof(long)    traversing DLL in leaves
	Tail block no.                  sizeof(long)    traversing DLL in leaves
	data_file’s name                32 bytes

                                    Root Node
                                    ---------
	No of empty slots               sizeof(long)
	M keys                          M * sizeof(K)
	M + 1 pointers                  (M+1)*sizeof(long)  (i.e. block numbers)

                                    Block (internal node)
                                    ---------------------
	Node-type identifier            1 byte          0: Internal, 1: Leaf
	Parent node block no.           sizeof(long)
	No of empty slots               sizeof(int)
	M keys                          M * sizeof(K)
	M + 1 pointers                  (M+1)*sizeof(long) block numbers

                                Block (leaf node)
                                -----------------
	Node-type identifier            1 byte          0: Internal, 1: Leaf
	Prev - block no                 sizeof(long)
	Next - block no                 sizeof(long)
	Parent node block no.           sizeof(long)
	No. of empty slots              sizeof(long)
	M keys                          M * sizeof(K)
	M + 1 pointers                  M+1 * (sizeof(long) + sizeof(offset)
                                    (i.e. block nos and offsets but in a different file)

*/

struct blockOffsetPair{
	long blockNumber;
	long offset;
};

template <typename K, typename V>
class BTreeNode{
protected:
	long M;
	long block_number;
	bool isRoot;
	K keys[M];
	int curr_keys; 	// Current number of keys in the node

	virtual void splitInternal();

public:
	BTreeNode(long block_number, long M): block_number(block_number), M(M){};
	//virtual V& findInNode(const K&);
	~BTreeNode();
	virtual void addToNode(const K&);
	virtual void deleteFromNode(const K&);
};

template <typename K, typename V>
class InternalNode : BTreeNode{
private:
	//array of block numbers
	long child_block_numbers[M+1];
public:
	long findInNode(const K&);	//returns block number of appropriate child node
	void addToNode(const K&);
	void deleteFromNode(const K&);
};

template <typename K, typename V>
class TreeLeafNode : BTreeNode{
private:
	//array of block, offset pairs
	blockOffsetPair value_node_address[M+1];
public:
	blockOffsetPair& findInNode(const K&);
	void addToNode(const K&, const V&); //TODO: two arguments or one argument as item <K,V>
 	void deleteFromNode(const K&);
};
/**
	Some of the fields available in header of BufferedFile

 	const size_type key_size;
	const size_type value_size;
	const string Identifier = "BTREE";

	These can be accessed as :
	$buffered_file_object->header->key_size;

 */


template <typename K, typename V>
class BTree
{
public:
	typedef long long int size_type;
private:
	BufferedFile* buffered_file_internal;
	BufferedFile* buffered_file_data;
	bool isRootLeaf;
	BTreeNode *root;
	size_type sz;
	long M; // Maximum M keys in the internal nodes

public:
	BTree(const char* pathname, size_type blocksize) : block_size(blocksize),
		element_size(sizeof(T)), sz(0)
	) {

		// this would have read the header from the file and set all the fields accordingly
		buffered_file_internal = new BufferedFile(pathname, block_size, block_size*3);

		// get data file
		buffered_file_data = new BufferedFile(pathname + "_data", sizeof(V), sizeof(V)*3);

		// will write it to the appropriate position in the header
		buffered_file_internal->header->setDataFileName(
			pathname + "_data"
		);

		root = NULL;

		// now ready for operation
	}

	~BTree() {
		delete buffered_file_internal;
		delete buffered_file_data;
	}

	V& search(const K& key);
	void insert(const K& key, const V& value);
	void delete(const K& key);
	void clear();

	size_type size() {
		return sz;
	}
};

