#include "buffer.h"
#include <stdexcept>
#include <stddef.h>

/*
	Actually a B+Tree but using BTree in class names for readability	


			Class BTreeNode (Abstract)
			  /			  \
			 /		 	   \
		LeafNode  	InternalNode

	Class BTree HAS A BTreeNode root
	BTreeLeaf contains pointers to item records
	The Item records will form a doubly linked list

*/


template <typename K, typename V>
class BTreeNode{
private:
	virtual void splitInternal();

protected:
	bool isRoot;

public:
	virtual V& findInNode(const K&);
	virtual void addToNode(const K&);
	virtual void deleteFromNode(const K&);
};

template <typename K, typename V>
class InternalNode : BTreeNode{
private:
	//array of blocknumbers
public:
	V& findInNode(const K&);
	void addToNode(const K&);
	void deleteFromNode(const K&);
};

template <typename K, typename V>
class TreeLeafNode : BTreeNode{
private:
	//array of block, offset pairs
public:
	V& findInNode(const K&);
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
