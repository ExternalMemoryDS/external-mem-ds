#include "buffer.h"
#include <stdexcept>
#include <stddef.h>
#include <list>
#include <cstring>

using namespace	std;

//Type Definitions
typedef long blocknum_t;
typedef long offset_t;
typedef long long int size_type;

//CONSTANTS
const blocknum_t NULL_BLOCK = -1;
const offset_t NULL_OFFSET = -1;
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
	blocknum_t block_number;
	offset_t offset;
};

template <typename K, typename V>
class BTreeNode {

protected:
	long M;
	blocknum_t block_number;
	bool isRoot;
	std::list<K> keys;

	virtual BTreeNode<K, V>* splitChild(const K&);

public:
	BTreeNode(blocknum_t block_number, long M): block_number(block_number), M(M) {
	};

	~BTreeNode();

	virtual bool isLeaf();
	virtual void addToNode(const K&);
	virtual void deleteFromNode(const K&);

	// this method checks if the child where the key K goes
	// after insert requires a split or not
	bool isSplitNeededForAdd(const K&);

	// Will use the BufferedFrameReader's static method to read the disk_block and
	// set approriate values
	void setNodePropFromFrame(BufferFrame*);

	// Will use the BufferedFrameWriter's static method to change the disk_block
	void writeNodePropToFrame(BufferFrame*);

};

template <typename K, typename V>
class InternalNode : BTreeNode<K, V> {
private:
	std::list<blocknum_t> child_block_numbers;

public:
	InternalNode(blocknum_t block_number, long M, bool _isRoot = false) {
		BTreeNode<K, V>(block_number, M);
		this->isRoot = _isRoot;
	};

	blocknum_t findInNode(const K&);	//returns block number of appropriate child node
	void addToNode(const K&);
	void addToNode(const K&, blocknum_t);
	void deleteFromNode(const K&);
	bool isLeaf(){ return false; }
};

template <typename K, typename V>
class TreeLeafNode : BTreeNode<K, V> {
private:
	//array of block, offset pairs
	std::list<blockOffsetPair> value_node_address;
public:
	TreeLeafNode(blocknum_t block_number, long M, bool _isRoot = false) {
		BTreeNode<K, V>(block_number, M);
		this->isRoot = _isRoot;
	};

	blockOffsetPair findInNode(const K&);
	void addToNode(const K&, const V&); //TODO: two arguments or one argument as item <K,V>
	void addToNode(const K&, blockOffsetPair);
 	void deleteFromNode(const K&);

    bool isLeaf(){ return true; }
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

private:
	const size_t Node_type_identifier_size = 1; // in bytes

	BufferedFile* buffered_file_internal;
	BufferedFile* buffered_file_data;

	//bool isRootLeaf;
	BTreeNode<K, V>* root;
	size_type sz;
	size_type blocksize;
	long M; // Maximum M keys in the internal nodes

	int calculateM(const size_t blocksize, const size_t key_size);

	BTreeNode<K, V> * getNodeFromBlockNum(blocknum_t);
	/* makes an InternalNode : if first byte of block is 0
	   makes a 	LeafNode     : if fisrt byte of block is 1
	*/

public:
	BTree(const char* pathname, size_type _blocksize) : blocksize(_blocksize), sz(0) {
		buffered_file_internal = new BufferedFile(pathname, blocksize);

		// get data file
		buffered_file_data = new BufferedFile(strcat(pathname, "_data"), sizeof(V));

		// will write it to the appropriate position in the header
		buffered_file_internal->header->setDataFileName(
			strcat(pathname, "_data")
		);

		root = nullptr;
		M = calculateM(blocksize, sizeof(K));

		// this would have read the header from the file and set all the fields accordingly
		// in the header (might have to make header a friend class of BTree)
		// Two possibilities:
		// 1. if the file was made previously, the header is read and values set in mem
		// 2. if file was not previously used, header class reads required values from BTree class (friend)
		// and then writes it to the header block on the file
		buffered_file_internal->header->init();

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

template <typename K, typename V>
V& BTree<K, V>::searchElem(const K& search_key) {
	blockOffsetPair valueAddr;
	blocknum_t next_block_num;

	// Node for traversing the tree
	BTreeNode<K, V>* head = root;

	while(! head.isLeaf()){

		// always called on an internal node
		next_block_num = head->findInNode(search_key);
		if(next_block_num == NULL_BLOCK) return nullptr;

		BTreeNode<K, V>* next_node = getNodeFromBlockNum(next_block_num);
		head = next_node;
	}

	// always called on a leaf node - returns a block-offset pair
	valueAddr = head->findInNode(search_key);

	// i.e. the key is not found
	if (valueAddr.block_number == NULL_BLOCK
		|| valueAddr.offset == NULL_OFFSET
	) {
		return nullptr;
	}

	// otherwise read the value from the block in data file and return
	BufferFrame* buff = buffered_file_data->readBlock(valueAddr.block_number);
	return BufferedFrameReader::readPtr<V>(buff, valueAddr.offset);
};


template <typename K, typename V>
BTreeNode<K, V>* BTree<K, V>::getNodeFromBlockNum(blocknum_t block_number) {

	BufferFrame* buff = buffered_file_internal->readBlock(block_number);
	BTreeNode<K, V>* new_node = nullptr;

	// read 1st byte from buff
	bool isLeaf = BufferedFrameReader::read<bool>(buff, 0);

	if (isLeaf) {
		new_node = new TreeLeafNode<K, V>(block_number, M);
		// read other properties from block and set them
	} else {
		new_node = new InternalNode<K, V>(block_number, M);
		// read other properties from block and set them
	}

	return new_node;
};

template <typename K, typename V>
blocknum_t InternalNode<K,V>::findInNode(const K& find_key) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blocknum_t>::const_iterator block_iter;

	for (
		key_iter = this->keys.begin(), block_iter = this->child_block_number.begin();
		key_iter != (this->keys).end();
		key_iter++, block_iter++
	){
		if (*key_iter > find_key) {
			return *block_iter;
		}
	}

	return *block_iter;
};

template <typename K, typename V>
blockOffsetPair TreeLeafNode<K,V>::findInNode(const K& find_key) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blockOffsetPair>::const_iterator block_iter;
	blockOffsetPair reqd_block;

	for (
		key_iter = this->keys.begin(), block_iter = this->value_node_address.begin();
		key_iter != (this->keys).end();
		key_iter++, block_iter++
	){
		if (*key_iter == find_key) {
			reqd_block.block_number = (*block_iter).block_number;
			reqd_block.offset = (*block_iter).offset;
			return reqd_block;
		}
	}

	reqd_block.block_number = NULL_BLOCK;
	reqd_block.offset = NULL_OFFSET;

	return reqd_block;
};


template <typename K, typename V>
void BTree<K, V>::insertElem(const K& new_key, const V& new_value) {
	BufferFrame* disk_block;

	if (this->root == nullptr) {
		// i.e. 'first' insert in the tree
		long root_block_num = buffered_file_internal->header->root_block_num;
		disk_block = buffered_file_internal->readBlock(root_block_num);
		this->root = new TreeLeafNode<K, V>(root_block_num, this->M, true);
		this->root->addToNode(new_key, new_value);

		this->root->writeNodePropToFrame(disk_block);
	} else {

		blockOffsetPair valueAddr;
		blocknum_t next_block_num;

		// Node for traversing the tree
		BTreeNode<K, V>* trav = this->root;

		// split the root if needed and make a new root
		if (this->root->isSplitNeededForAdd(new_key)) {
			long new_root_bnum = buffered_file_internal->allotBlock();
			BTreeNode<K, V>* t = new TreeLeafNode(new_root_bnum, this->M, true);
			trav = t->splitChild(K, true);
			this->root = t;
		}

		BTreeNode<K, V>* next_node;

		while(trav && ! trav->isLeaf()) {

			// pro-active splitting
			if (trav->isSplitNeededForAdd(new_key)) {
				// returns the proper next node
				trav = trav->splitChild(K, true);
			} else {
				next_block_num = trav->findInNode(new_key);
				new_node = getNodeFromBlockNum(next_block_num);
				delete trav;
				trav = next_node;
			}
		}

		// always called on a leaf node - returns a block-offset pair
		valueAddr = trav->findInNode(new_key);

		// i.e. the key is not found
		if (valueAddr.block_number == NULL_BLOCK
			|| valueAddr.offset == NULL_OFFSET
		) {
			// key not present, so add the key and the value
			trav->addToNode(new_key, new_value);
		} else {
			// key already exists, just update the value
			trav->updateValue(new_key, new_value);
		}
	}
};

template <typename K, typename V>
BTreeNode<K, V>* splitChild(const K& new_key) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blocknum_t>::const_iterator block_iter;
	typename std::list<blockOffsetPair>::const_iterator blockOffset_iter;

	BTreeNode<K, V>* new_node;
	BufferFrame* disk_block;
	long new_block_num;

	new_block_num = buffered_file_internal->allotBlock();
	disk_block_new = buffered_file_internal->readBlock(new_block_num);
	disk_block_old = buffered_file_internal->readBlock(this->block_number);

	if (this->isLeaf()) {
		new_node = new TreeLeafNode<K, V>(new_block_num, this->M, this->isRoot);

		this->addToNode(new_key);
		for(
			int i = 0, key_iter = this->keys.begin(), block_iter = this->value_node_address.begin();
			i <= (this->keys).size() / 2, key_iter != (this->keys).end();
			key_iter++, block_iter++
		) {
			new_node->addToNode(*key_iter, *block_iter);
			this->deleteFromNode(*key_iter, *block_iter);
			this->writeNodePropToFrame(disk_block_old);
			new_node->writeNodePropToFrame(disk_block_new);
		}
	} else {
		new_node = new InternalNode<K, V>(new_block_num, this->M, this->isRoot);

		for(
			int i = 0, key_iter = this->keys.begin(), block_iter = this->child_block_numbers.begin();
			i <= (this->keys).size() / 2, key_iter != (this->keys).end();
			key_iter++, block_iter++
		) {
			new_node->addToNode(*key_iter, *block_iter);
			this->deleteFromNode(*key_iter, *block_iter);
			this->writeNodePropToFrame(disk_block_old);
			new_node->writeNodePropToFrame(disk_block_new);
		}
	}

	return new_node;
};
