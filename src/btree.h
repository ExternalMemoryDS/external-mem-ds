#include "buffer.h"
#include <stdexcept>
#include <stddef.h>

template <typename K>
class rootNode {

};

template <typename K>
class leafNode {

};

template <typename K>
class internalNode {

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

	rootNode *root;
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

	void search(const K& key);
	void insert(const K& key, const V& value);
	void delete(const K& key);
	void clear();

	size_type size() {
		return sz;
	}
};
