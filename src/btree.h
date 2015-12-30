#include "buffer.h"
#include <stdexcept>
#include <stddef.h>
#include <vector>
#include <cstring>


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

struct blockOffsetPair {
	long blockNumber;
	long offset;
};

template <typename K, typename V>
class BTreeNode {
protected:
	long M;
	long block_number;
	bool isRoot;
	std::vector<K> keys;
	int curr_keys; 	// Current number of keys in the node

	virtual void splitInternal();

public:
	BTreeNode(long block_number, long M): block_number(block_number), M(M){
		curr_keys = 0;
		keys.reserve(keys.size() + M);
	};
	//virtual V& findInNode(const K&);
	~BTreeNode();
	virtual void addToNode(const K&);
	virtual void deleteFromNode(const K&);
};

template <typename K, typename V>
class InternalNode : BTreeNode<K, V> {
private:
	//array of block numbers
	int M;
	std::vector<long> child_block_numbers;
public:
	long findInNode(const K&);	//returns block number of appropriate child node
	void addToNode(const K&);
	void deleteFromNode(const K&);
};

template <typename K, typename V>
class TreeLeafNode : BTreeNode<K, V> {
private:
	//array of block, offset pairs
	std::vector<blockOffsetPair> value_node_address;
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
class BTree {
public:
	typedef long long int size_type;
private:
	const size_t Node_type_identifier_size = 1; // in bytes

	BufferedFile* buffered_file_internal;
	BufferedFile* buffered_file_data;

	bool isRootLeaf;
	BTreeNode<K, V>* root;
	size_type sz;
	size_type blocksize;
	long M; // Maximum M keys in the internal nodes

	int calculateM(const size_t blocksize, const size_t key_size);

public:
	BTree(const char* pathname, size_type _blocksize) : blocksize(_blocksize), sz(0) {

		// this would have read the header from the file and set all the fields accordingly
		buffered_file_internal = new BufferedFile(pathname, blocksize, blocksize*3);

		// get data file
		buffered_file_data = new BufferedFile(strcat(pathname, "_data"), sizeof(V), sizeof(V)*3);

		// will write it to the appropriate position in the header
		buffered_file_internal->header->setDataFileName(
			strcat(pathname, "_data")
		);

		root = nullptr;
		M = calculateM(blocksize, sizeof(K));

		// now ready for operation
	}

	~BTree() {
		delete buffered_file_internal;
		delete buffered_file_data;
	}

	V& searchElem(const K& key);
	void insertElem(const K& key, const V& value);
	void deleteElem(const K& key);
	void clearTree();

	size_type size() {
		return sz;
	}
};

template <typename K, typename V>
int BTree<K, V>::calculateM(const size_t blocksize, const size_t key_size) {

	/*
		sizeof(Node-type identifier) = 1 byte (A)
		sizeof(block_number) = sizeof(long)
		sizeof(no_of_empty_slots) = sizeof(long)
		sizeof(keys) = from user’s input or Sizeof(K)
		sizeof(block_number_data_file) = sizeof(long)
		sizeof(offset) = size we might restrict the block size to a fixed no. BL_MAX,
		we can always get the offset size to be (Log2(BL_MAX)/ 8) OR
		for now sizeof(int)

		Block_size = we know already as supplied by the user.

		So, M can be found out as:
		Block_size = sizeof(Node-type identifier)
		+ sizeof(block_number) + sizeof(No of empty slots)
		+ M*sizeof(keys) + (M+1)*(sizeof(block_number) + sizeof(offset))

		find the greatest integer M that satisfies the above relation.
	*/

	int long_size = sizeof(long);
	int int_size = sizeof(int);

	int M = (blocksize - Node_type_identifier_size - 3 * long_size - int_size)
		/ (int_size + long_size + key_size);

	return M;
};
