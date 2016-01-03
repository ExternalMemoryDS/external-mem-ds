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
	Root Node block number          sizeof(long)    block no. of the root of BTree
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

template <typename K, typename V, typename CompareFn>
class BTreeNode {

protected:
	long M;
	blocknum_t block_number;
	bool isRoot;
	std::list<K> keys;
	CompareFn cmpl;
	bool isChangedInMem;
	BufferFrame* my_block;

	virtual BTreeNode<K, V, CompareFn>* splitChild(const BTreeNode<K, V, CompareFn>&);

public:
	BTreeNode(blocknum_t block_number, long M): block_number(block_number), M(M) {
		isChangedInMem = false;
	};

	~BTreeNode() {
		if (this->isChangedInMem) {
			this->writeNodetoDisk(my_block);
		}
		delete my_block;
	};

	virtual bool isLeaf();
	virtual void deleteFromNode(const K&);

	// methods for comparing keys
	bool eq(const K& k1, const K& k2) {
		return !(cmpl(k1, k2) && cmpl(k2, k1));
	}

	bool neq(const K& k1, const K& k2) {
		return !(eq(k1, k2));
	}


	// Will use the BufferedFrameReader's static method to read the disk_block and
	// set approriate values and set the 'isSetInMem' to false
	void readNodeFromDisk(BufferFrame*);

	// Will use the BufferedFrameWriter's static method to change the disk_block
	// and set the 'isSetInMem' to false
	void writeNodetoDisk(BufferFrame*);

	// Returns true if no. of keys present == M - 1
	bool isFull() { return (keys.size() == M - 1); }

	// Finds Median of existing Key and new_key
	K& findMedian();

	void setIsChangedInMem(bool new_value) {
		this->isChangedInMem = new_value;
	}

	blocknum_t getBlockNo() {
		return this->block_number;
	}
};

template <typename K, typename V, typename CompareFn>
class InternalNode : BTreeNode<K, V, CompareFn> {
private:
	long M;
	std::vector<blocknum_t> child_block_numbers;

public:
	InternalNode(blocknum_t block_number, long M, bool _isRoot = false) {
		BTreeNode<K, V, CompareFn>(block_number, M);
		this->isRoot = _isRoot;
	};

	blocknum_t findInNode(const K&);	//returns block number of appropriate child node
	void deleteFromNode(const K&);
	bool isLeaf() { return false; }
};

template <typename K, typename V, typename CompareFn>
class TreeLeafNode : BTreeNode<K, V, CompareFn> {
private:
	//array of block, offset pairs
	std::list<blockOffsetPair> value_node_address;
	blocknum_t prev;
	blocknum_t next;

public:
	TreeLeafNode(blocknum_t block_number, long M, bool _isRoot = false) {
		BTreeNode<K, V, CompareFn>(block_number, M);
		this->isRoot = _isRoot;
	};

	blockOffsetPair findInNode(const K&);
    void deleteFromNode(const K&);

    bool isLeaf() { return true; }

    blocknum_t getPrevBlockNo() {
        return this->prev;
    }

    blocknum_t getNextBlockNo() {
        return this->next;
    }

    void setPrevBlockNo(blocknum_t new_prev) {
        this->prev = new_prev;
    }

    void setNextBlockNo(blocknum_t new_next) {
        this->next = new_next;
    }
};

/**
	Some of the fields available in header of BufferedFile

    const size_type key_size;
	const size_type value_size;
	const string Identifier = "BTREE";

	These can be accessed as :
	$buffered_file_object->header->key_size;
*/

template <typename K, typename V, typename CompareFn>
class BTree {

private:
	const size_t Node_type_identifier_size = 1; // in bytes

	BufferedFile* buffered_file_internal;
	BufferedFile* buffered_file_data;

	BTreeNode<K, V, CompareFn>* root;
	size_type sz;
	size_type blocksize;
	long M; // Maximum M keys in the internal nodes

	CompareFn cmpl;

	int calculateM(const size_t blocksize, const size_t key_size);


	// this will read the given block, set the properties accordingly for
	// the new instance, and also set the new nodes my_block bufferframe pointer
	// and then return the block
	BTreeNode<K, V, CompareFn>* getNodeFromBlockNum(blocknum_t);
	/* makes an InternalNode : if first byte of block is 0
	   makes a 	LeafNode     : if fisrt byte of block is 1
	*/

    BTreeNode<K, V, CompareFn>* splitChild(
		BTreeNode<K, V, CompareFn>& child_to_split,
		BTreeNode<K, V, CompareFn>& current_node
	);

    // only called for adding to a TreeLeafNode, so const V&
	void addToNode(const K&, const V&, BTreeNode<K, V, CompareFn>*);

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

	TreeLeafNode<K, V, CompareFn>* getSmallestValueNode() {
		BTreeNode<K, V, CompareFn>* curr = this->root, new_node;

		while (! curr.isLeaf()) {
			new_node = this->getNodeFromBlockNum(
				curr->child_block_numbers.front()
			);
			delete curr;
			curr = new_node;
		}
		return curr;
	}

	TreeLeafNode<K, V, CompareFn>* getLargestValueNode() {
		BTreeNode<K, V, CompareFn>* curr = this->root, new_node;

		while (! curr.isLeaf()) {
			new_node = this->getNodeFromBlockNum(
				curr->child_block_numbers.back()
			);
			delete curr;
			curr = new_node;
		}
		return curr;
	}


	// THE ITERATOR and the CONST_ITERATOR CLASS
	class iterator : public std::iterator<std::random_access_iterator_tag, V, size_type, V*, V&> {
	friend class BTree;
	private:
		TreeLeafNode<K, V, CompareFn>* head, tail, curr;
		V* value;
		BufferedFile* data_file;

		// for iterating in 'curr'
		typename std::list<blockOffsetPair>::const_iterator block_iter;

	public:
		iterator() {
			head = tail = curr = nullptr;
		};

		iterator(BTree<K, V, CompareFn>* B, bool start_end = true) {
			head = B->getSmallestValueNode();
			tail = B->getLargestValueNode();
			data_file = B->buffered_file_data;

			if (start_end) {
				curr = head;
				block_iter = curr->value_node_address.begin();

				value = BufferedFrameReader::readPtr<V>(
					data_file->readBlock((*block_iter).block_number),
					(*block_iter).offset
				);
			} else {
				curr = tail;
				block_iter = ++((curr->value_node_address.end()));
			}
		}

		bool operator== (const iterator& rhs);
		bool operator!= (const iterator& rhs) { return !(operator==(rhs)); };
		V& operator* ();
		iterator& operator++ ();
		iterator& operator-- ();

		// iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
		// iterator operator--(int) { iterator tmp(*this); operator--(); return tmp; }
		// iterator operator+ (size_type n) { iterator tmp(*this); tmp.index += n; return tmp; }
		// iterator operator- (size_type n) { iterator tmp(*this); tmp.index -= n; return tmp; }
		// bool operator> (const iterator& rhs);
		// bool operator< (const iterator& rhs);
		// bool operator<= (const iterator& rhs);
		// bool operator>= (const iterator& rhs);
		// iterator& operator+= (size_type n) { index += n; return *this; }
		// iterator& operator-= (size_type n) { index -= n; return *this; }
		// T& operator[] (size_type n) { return *(*this+n); }
		// typename std::iterator<std::random_access_iterator_tag, T, long long int, T*, T&>::difference_type operator- (const iterator& rhs) { return index - rhs.index; }
	};

	iterator begin();
	iterator end();
};

template <typename K, typename V, typename CompareFn>
typename BTree<K, V, CompareFn>::iterator BTree<K, V, CompareFn>::begin() {
	iterator iter(this);
	return iter;
}

template <typename K, typename V, typename CompareFn>
typename BTree<K, V, CompareFn>::iterator BTree<K, V, CompareFn>::end() {
	iterator iter(this, false);
	return iter;
}

template <typename K, typename V, typename CompareFn>
V& BTree<K, V, CompareFn>::iterator::operator* () {
	return value;
}

template <typename K, typename V, typename CompareFn>
typename BTree<K, V, CompareFn>::iterator& BTree<K, V, CompareFn>::iterator::operator++() {

	BTreeNode<K, V, CompareFn>* new_node;
	// i.e. all keys in this 'curr' have been traversed
	if (++block_iter == curr->value_node_address.end()) {
		if (curr != tail) {
			new_node = curr->getNodeFromBlockNum(
				curr->next
			);
			delete curr;
			curr = new_node;
		} else {
			return nullptr;
		}
		block_iter = curr->value_node_address.begin();
	} else {
		block_iter++;
	}

	value = BufferedFrameReader::readPtr<V>(
		data_file->readBlock(
			(*block_iter).block_number
		),
		(*block_iter).offset
	);

	return *this;
}

template <typename K, typename V, typename CompareFn>
bool BTree<K, V, CompareFn>::iterator::operator== (const iterator& rhs) {
	return (
		(rhs.curr == curr)
		&& (rhs.head == head)
		&& (rhs.tail == tail)
		&& (*rhs == *this)
	);
}


template <typename K, typename V, typename CompareFn>
typename BTree<K, V, CompareFn>::iterator& BTree<K, V, CompareFn>::iterator::operator--() {

	BTreeNode<K, V, CompareFn>* new_node;
	// i.e. all keys in this 'curr' have been traversed
	if (block_iter == curr->value_node_address.begin()) {
		if (curr != head) {
			new_node = curr->getNodeFromBlockNum(
				curr->prev
			);
			delete curr;
			curr = new_node;
		} else {
			return nullptr;
		}
		block_iter = --(curr->value_node_address.end());
	} else {
		block_iter--;
	}

	value = BufferedFrameReader::readPtr<V>(
		data_file->readBlock(
			(*block_iter).block_number
		),
		(*block_iter).offset
	);

	return *this;
}

template <typename K, typename V, typename CompareFn>
int BTree<K, V, CompareFn>::calculateM(const size_t blocksize, const size_t key_size) {

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

template <typename K, typename V, typename CompareFn>
V& BTree<K, V, CompareFn>::searchElem(const K& search_key) {
	blockOffsetPair valueAddr;
	blocknum_t next_block_num;

	// Node for traversing the tree
	BTreeNode<K, V, CompareFn>* head = root;

	while (! head.isLeaf()) {

		// always called on an internal node
		next_block_num = head->findInNode(search_key);
		if (next_block_num == NULL_BLOCK) return nullptr;

		BTreeNode<K, V, CompareFn>* next_node = getNodeFromBlockNum(next_block_num);
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


template <typename K, typename V, typename CompareFn>
BTreeNode<K, V, CompareFn>* BTree<K, V, CompareFn>::getNodeFromBlockNum(blocknum_t block_number) {

	BufferFrame* buff = buffered_file_internal->readBlock(block_number);
	BTreeNode<K, V, CompareFn>* new_node = nullptr;

	// read 1st byte from buff
	bool isLeaf = BufferedFrameReader::read<bool>(buff, 0);

	if (isLeaf) {
		new_node = new TreeLeafNode<K, V, CompareFn>(block_number, M);
		// read other properties from block and set them
	} else {
		new_node = new InternalNode<K, V, CompareFn>(block_number, M);
		// read other properties from block and set them
	}
	new_node->my_block = buff;

	return new_node;
};

template <typename K, typename V, typename CompareFn>
blocknum_t InternalNode<K, V, CompareFn>::findInNode(const K& find_key) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blocknum_t>::const_iterator block_iter;

	for (
		key_iter = this->keys.begin(), block_iter = this->child_block_number.begin();
		key_iter != (this->keys).end();
		key_iter++, block_iter++
	) {
		if (cmpl(*key_iter, find_key)) {
			return *block_iter;
		} else if (eq(*key_iter, find_key)) {

			// CRITICAL: since we assume that if the key is equal, we search in the 'right' block
			// same is followed in insertion
			block_iter++;
			return *block_iter;
		}
	}

	return *block_iter;
};

template <typename K, typename V, typename CompareFn>
blockOffsetPair TreeLeafNode<K, V, CompareFn>::findInNode(const K& find_key) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blockOffsetPair>::const_iterator block_iter;
	blockOffsetPair reqd_block;

	for (
		key_iter = this->keys.begin(), block_iter = this->value_node_address.begin();
		key_iter != (this->keys).end();
		key_iter++, block_iter++
	) {
		if (eq(*key_iter, find_key)) {
			reqd_block.block_number = (*block_iter).block_number;
			reqd_block.offset = (*block_iter).offset;
			return reqd_block;
		}
	}

	reqd_block.block_number = NULL_BLOCK;
	reqd_block.offset = NULL_OFFSET;

	return reqd_block;
};


template <typename K, typename V, typename CompareFn>
void BTree<K, V, CompareFn>::insertElem(const K& new_key, const V& new_value) {
	BufferFrame* disk_block;
	BTreeNode<K, V, CompareFn>* next_node;

	if (this->root == nullptr) {
		// i.e. 'first' insert in the tree
		blocknum_t root_block_num = buffered_file_internal->header->root_block_num;
		disk_block = buffered_file_internal->readBlock(root_block_num);
		this->root = new TreeLeafNode<K, V, CompareFn>(root_block_num, this->M, true);
		this->addToNode(new_key, new_value, root);

		this->root->writeNodetoDisk(disk_block);
	} else {
		blockOffsetPair valueAddr;
		blocknum_t next_block_num;

		// Node for traversing the tree
		BTreeNode<K, V, CompareFn>* trav = this->root, par;

		// split the root if needed and make a new root
		if (this->root->isFull()) {
			blocknum_t old_root_block_num = this->root->block_number;
			blocknum_t new_root_bnum = buffered_file_internal->allotBlock();

			BTreeNode<K, V, CompareFn>* old_root = this->root;
			//New root must be an InternalNode
			BTreeNode<K, V, CompareFn>* new_root = new InternalNode<K, V, CompareFn>(new_root_bnum, this->M, true);

			// CHECK IF THIS IS CORRECT
			// Add old root block number as left most child_block_number
			new_root->child_block_numbers.push_back(old_root_block_num);

			// Set new_root to be the root of BTree
			this->root = new_root;

			//TODO: UPDATE HEADER BLOCK ON DISK INDICATING NEW ROOT

			// make isRoot = false in old_root
			old_root->isRoot = false;

			// Split old root
			trav = this->splitChild(old_root, new_root);
			//TODO: WRITING TO DISK MAY BE REQUIRED (DEPENDS IF WRITING TO DISK IS )
		}

		BTreeNode<K, V, CompareFn>* next_node;

		while (trav && ! trav->isLeaf()) {
			next_block_num = trav->findInNode(new_key);
			next_node = this->getNodeFromBlockNum(next_block_num);

			// pro-active splitting
			if (next_node->isFull()) {
				// returns the proper next node
				next_node = this->splitChild(next_node, trav);

				// destructor will automatically write it to the block
				// plus our setter methods will mark a node's prop 'isChangedInMem'
				// so that destructor does not write unnecessarily
				delete trav;
			} else {
				next_block_num = trav->findInNode(new_key);
				par = trav;
				next_node = getNodeFromBlockNum(next_block_num);

				// destructor will automatically write it to the block
				// plus our setter methods will mark a node's prop 'isChangedInMem'
				// so that destructor does not write unnecessarily
				delete trav;
			}
			trav = next_node;
		}

		// always called on a leaf node - returns a block-offset pair
		valueAddr = trav->findInNode(new_key);

		// i.e. the key is not found
		if (valueAddr.block_number == NULL_BLOCK
			|| valueAddr.offset == NULL_OFFSET
		) {
			// key not present, so add the key and the value
			this->addToNode(new_key, new_value, trav);
		} else {
			// key already exists, just update the value
			trav->updateValue(new_key, new_value);
		}

		// destructor will automatically write it to the block
		// plus our setter methods will mark a node's prop 'isChangedInMem'
		// so that destructor does not write unnecessarily
		delete trav;
	}

	this->sz++;
};

template <typename K, typename V, typename CompareFn>
K& BTreeNode<K, V, CompareFn>::findMedian() {
	typename std::list<K>::const_iterator key_iterator;

	// increment till the middle element
	key_iterator = this->keys.begin();
	std::advance(
		key_iterator, (int)(this->keys.size() / 2)
	);

	// return the middle element
	return *key_iterator;
};

template <typename K, typename V, typename CompareFn>
BTreeNode<K, V, CompareFn>* BTree<K, V, CompareFn>::splitChild(
	BTreeNode<K, V, CompareFn>& child_to_split,
	BTreeNode<K, V, CompareFn>& current_node
) {

	// In this method, 'current_node' refers to the node
	// that initiated the split on its child 'child_to_split'

	// CRITICAL: The new_node is always created on the right

	blocknum_t new_block_num = buffered_file_internal->allotBlock();
	BufferFrame* disk_block_new = buffered_file_internal->readBlock(new_block_num);
	K median_key = child_to_split->findMedian();
	TreeLeafNode<K, V, CompareFn>* new_node;

	if (child_to_split->isLeaf()) {
		new_node = new TreeLeafNode<K, V, CompareFn>(new_block_num, this->M);
		typename std::list<K>::const_iterator old_key_iter;
		typename std::list<blockOffsetPair>::const_iterator old_block_iter;

		//distribute keys
		for (
			old_key_iter = child_to_split->keys.begin(), old_block_iter = child_to_split->value_node_address.begin();
			old_key_iter != (child_to_split->keys).end();
			// no increment, read comments below
		) {
			if (eq(*old_key_iter, median_key)) {
				//CRITICAL: MEDIAN KEY MUST BE ADDED TO NEW NODE IN CASE OF LEAF NODES
				new_node->keys.push_back(*old_key_iter);
				new_node->value_node_address.push_back(*old_block_iter);

				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = child_to_split->keys.erase(old_key_iter);
				old_block_iter = child_to_split->value_node_address.erase(old_block_iter);

			} else if (cmpl(*old_key_iter, median_key)) {
				new_node->keys.push_back(*old_key_iter);
				new_node->value_node_address.push_back(*old_block_iter);

				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = child_to_split->keys.erase(old_key_iter);
				old_block_iter = child_to_split->value_node_address.erase(old_block_iter);
			} else {
				old_key_iter++;
				old_block_iter++;
			}
		}

		// THE FOLLOWING CODE UPDATES THE POINTERS FOR
		// MAINTAINING A DLL of LEAVES
		// update the prev and next pointers
		new_node->setPrevBlockNo(child_to_split->getBlockNo());
		new_node->setNextBlockNo(child_to_split->getNextBlockNo());

		// update the 'prev' of the (orignally) 'next' of child_to_split
		BufferFrame* disk_block = buffered_file_internal->readBlock(
			child_to_split->getNextBlockNo()
		);
		BufferedFrameWriter::write<blocknum_t>(disk_block, 1, child_to_split->getNextBlockNo());

		// now update the next of child_to_split
		child_to_split->setNextBlockNo(new_node->getBlockNo());
	} else {
		new_node = new InternalNode<K, V, CompareFn>(new_block_num, this->M);

		typename std::list<K>::const_iterator old_key_iter;
		typename std::list<blocknum_t>::const_iterator old_block_iter;

		//distribute keys
		for (
			old_key_iter = child_to_split->keys.begin(), old_block_iter = child_to_split->child_block_numbers.begin();
			old_key_iter != (child_to_split->keys).end();
			// No increment required, read comments below
		) {
			if (eq(*old_key_iter, median_key)) {
				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = this->keys.erase(old_key_iter);
			} else if (cmpl(*old_key_iter, median_key)) {
				new_node->keys.push_back(*old_key_iter);
				new_node->child_block_numbers.push_back(*old_block_iter);

				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = child_to_split->keys.erase(old_key_iter);
				old_block_iter = child_to_split->child_block_numbers.erase(old_block_iter);
			} else {
				old_key_iter++, old_block_iter++;
			}
		}

		//add the rightmost block number to new node
		new_node->child_block_numbers.push_back(*old_block_iter);
	}


	typename std::list<K>::const_iterator curr_key_iter = current_node->keys.begin();
	typename std::list<blocknum_t>::const_iterator curr_block_iter = current_node->child_block_numbers.begin();

	while (*curr_block_iter != child_to_split->block_number) {
		curr_block_iter++;
		curr_key_iter++;
	}

	curr_block_iter++;
	curr_key_iter++;

	// adjust keys and pointers in parent node
	current_node->keys.insert(curr_key_iter, median_key);
	current_node->child_block_numbers.insert(curr_key_iter, new_node->block_number);

	// mark changed
	current_node->setIsChangedInMem(true);
	child_to_split->setIsChangedInMem(true);

	delete current_node;
	delete child_to_split;

	return new_node;
};


template <typename K, typename V, typename CompareFn>
void BTree<K, V, CompareFn>::addToNode(
	const K& new_key, const V& new_value,
	BTreeNode<K, V, CompareFn>* current_node
) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blockOffsetPair>::const_iterator block_iter;
	blockOffsetPair new_block_offset;

	new_block_offset.block_number = this->buffered_file_data->allotBlock();
	new_block_offset.offset = 0;

	for (
		key_iter = current_node->keys.begin(), block_iter = current_node->value_node_address.begin();
		key_iter != (current_node->keys).end();
		key_iter++, block_iter++
	) {
		if (cmpl(*key_iter, new_key)) {
			break;
		} else if (eq(*key_iter, new_key)) {

			// CRITICAL: since we assume that if the key is equal, we search in the 'right' block
			// same is followed in insertion
			block_iter++;
			break;
		}
	}

	// add entries in the resp. lists
	current_node->keys.insert(block_iter, new_key);
	current_node->value_node_address.insert(block_iter, new_block_offset);

	current_node->setIsChangedInMem(true);

};
