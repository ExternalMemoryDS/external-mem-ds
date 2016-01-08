#include "buffer.h"
#include <stdexcept>
#include <stddef.h>
#include <list>
#include <cstring>
#include <assert.h>

//Type Definitions
typedef long blocknum_t;
typedef long offset_t;
typedef long long int size_type;
typedef bool node_t;

//CONSTANTS
const blocknum_t NULL_BLOCK = -1;
const offset_t NULL_OFFSET = -1;


// OFFSETS
const offset_t NODE_TYPE = 0;
const offset_t PREV_BLOCK = 1;
const offset_t NEXT_BLOCK = 1 + 1 * sizeof(long);
const offset_t PARENT_BLOCK = 1 + 2 * sizeof(long);
const offset_t NUM_KEYS = 1 + 3 * sizeof(long);
const offset_t START_KEYS = 1 + 4 * sizeof(long);



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
	Node-type indentifier           1 byte
	Prev - block no                 sizeof(long)    ONLY IF LEAF NODE
	Next - block no                 sizeof(long)	ONLY IF LEAF NODE
	Parent node block no.           sizeof(long) 	zero/ minus 1
	No. of keys present             sizeof(long)
	M keys                          M * sizeof(K)
	M + 1 pointers                  (M+1)*sizeof(long)  (i.e. block numbers)

                                    Block (internal node)
                                    ---------------------
	Node-type identifier            1 byte          0: Internal, 1: Leaf
	Prev - block no                 sizeof(long)    <empty>
	Next - block no                 sizeof(long)	<empty>
	Parent node block no.           sizeof(long)
	No. of keys present             sizeof(long)
	M keys                          M * sizeof(K)
	M + 1 pointers                  (M+1)*sizeof(long) block numbers

                                    Block (leaf node)
                                    -----------------
	Node-type identifier            1 byte          0: Internal, 1: Leaf
	Prev - block no                 sizeof(long)
	Next - block no                 sizeof(long)
	Parent node block no.           sizeof(long)
	No. of keys present             sizeof(long)
	M keys                          M * sizeof(K)
	M + 1 pointers                  M+1 * (sizeof(long) + sizeof(offset)
                                    (i.e. block nos and offsets but in a different file)

*/

// structure to reference the
// 'value' blocks in data file
struct blockOffsetPair{
	blocknum_t block_number;
	offset_t offset;
};

// The abstract class
template <typename K, typename V, typename CompareFn>
class BTreeNode {

protected:

public:

	long M;
	blocknum_t block_number;
	BufferedFile *buffered_file_internal, *buffered_file_data;
	bool isRoot;
	CompareFn cmpl;

	offset_t START_BLOCKNUMS = START_KEYS + (this->M) * sizeof(K);

	BTreeNode(
		blocknum_t block_number, long M,
		BufferedFile* buff_int, BufferedFile* buff_data,
		bool _isRoot = false
	) : block_number(block_number), M(M) {
			this->buffered_file_internal = buff_int;
			this->buffered_file_data = buff_data;
			this->isRoot = _isRoot;
	};

	virtual void getBlockNumbers(std::list<blocknum_t>& list_inst) =0;
	virtual void setBlockNumbers(std::list<blocknum_t>& list_inst) =0;
	// virtual void replaceKey(const K&, blocknum_t) =0;
	virtual K findMedian() =0;
    virtual blocknum_t getPrevBlockNo() =0;
    virtual blocknum_t getNextBlockNo() =0;
    virtual void setPrevBlockNo(blocknum_t new_prev) =0;
    virtual void setNextBlockNo(blocknum_t new_next) =0;
    virtual void getBlockOffsetPairs(std::list<blockOffsetPair>& list_inst) =0;
	virtual void setBlockOffsetPairs(std::list<blockOffsetPair>& list_inst) =0;

	// virtual void removeKey(const K& remove_key) =0;
	virtual bool isLeaf() =0;
	virtual blockOffsetPair findInNodeLeaf(const K&) =0;
	virtual blocknum_t findInNode(const K&) =0;

	void setIsRoot(bool);
	void deleteFromNode(const K&);
	bool eq(const K& k1, const K& k2);
	bool neq(const K& k1, const K& k2);
	blocknum_t getBlockNo();
	bool isFull();
	blocknum_t getParentBlockNo();
	bool getIsRoot();
	size_t getSize();
	blocknum_t getSmallestKeyBlockNo();
	blocknum_t getLargestKeyBlockNo();
	K getSmallestKey();
	void getKeys(std::list<K>&);
	void setKeys(std::list<K>&);

};

template <typename K, typename V, typename CompareFn>
void BTreeNode<K, V, CompareFn>::setIsRoot(bool new_root_bool) {
	this->isRoot = new_root_bool;
}

// methods for comparing keys
template <typename K, typename V, typename CompareFn>
bool BTreeNode<K, V, CompareFn>::eq(const K& k1, const K& k2) {
	return !(cmpl(k1, k2) && cmpl(k2, k1));
}

template <typename K, typename V, typename CompareFn>
bool BTreeNode<K, V, CompareFn>::neq(const K& k1, const K& k2) {
	return !(eq(k1, k2));
}

// GETTER METHODS
template <typename K, typename V, typename CompareFn>
blocknum_t BTreeNode<K, V, CompareFn>::getBlockNo() {
	return this->block_number;
}

// Returns true if no. of keys present == M - 1
template <typename K, typename V, typename CompareFn>
bool BTreeNode<K, V, CompareFn>::isFull() {
	size_type size = BufferedFrameReader::read<size_type>(
		buffered_file_internal->readBlock(this->block_number),
		NUM_KEYS
	);
	return (size == M - 1);
}

template <typename K, typename V, typename CompareFn>
blocknum_t BTreeNode<K, V, CompareFn>::getParentBlockNo() {
	return BufferedFrameReader::read<blocknum_t>(
		buffered_file_internal->readBlock(this->block_number),
		PARENT_BLOCK
	);
}

template <typename K, typename V, typename CompareFn>
bool BTreeNode<K, V, CompareFn>::getIsRoot() {
	return this->isRoot;
}

// returns the number of keys in the node
// the no. of blockNos will be +1
template <typename K, typename V, typename CompareFn>
size_t BTreeNode<K, V, CompareFn>::getSize() {
	return BufferedFrameReader::read<size_type>(
		buffered_file_internal->readBlock(this->block_number),
		NUM_KEYS
	);
}

template <typename K, typename V, typename CompareFn>
blocknum_t BTreeNode<K, V, CompareFn>::getSmallestKeyBlockNo() {
	return BufferedFrameReader::read<blocknum_t>(
		buffered_file_internal->readBlock(this->block_number),
		START_BLOCKNUMS
	);
}

template <typename K, typename V, typename CompareFn>
blocknum_t BTreeNode<K, V, CompareFn>::getLargestKeyBlockNo() {
	return BufferedFrameReader::read<blocknum_t>(
		buffered_file_internal->readBlock(this->block_number),
		START_BLOCKNUMS + this->getSize() * sizeof(blocknum_t)
	);
}

template <typename K, typename V, typename CompareFn>
K BTreeNode<K, V, CompareFn>::getSmallestKey() {
	return BufferedFrameReader::read<K>(
		buffered_file_internal->readBlock(this->block_number),
		START_KEYS
	);
}

template <typename K, typename V, typename CompareFn>
void BTreeNode<K, V, CompareFn>::getKeys(std::list<K>& list_inst) {
	size_type size = this->getSize();
	offset_t curr;
	for(int i = 0, curr = START_KEYS;
		i < size;
		i++, curr += sizeof(K)
	) {
		list_inst.push_back(
			BufferedFrameReader::read<blocknum_t>(
				buffered_file_internal->readBlock(
					this->block_number
				),
				curr
			)
		);
	}
}


// SETTER METHOD
template <typename K, typename V, typename CompareFn>
void BTreeNode<K, V, CompareFn>::setKeys(std::list<K>& list_inst) {
	size_type size = this->getSize();
	typename std::list<K>::const_iterator key_iter = list_inst.begin();

	for(offset_t curr = START_KEYS;
		key_iter != list_inst.end();
		curr += sizeof(K),
		key_iter++
	) {
		BufferedFrameWriter::write<K>(
			buffered_file_internal->readBlock(
				this->block_number
			),
			curr, *key_iter
		);
	}
}


// Internal Node class
template <typename K, typename V, typename CompareFn>
class InternalNode : public BTreeNode<K, V, CompareFn> {

public:
	InternalNode(
		blocknum_t block_number, long M,
		BufferedFile* int_file, BufferedFile* data_file,
		bool _isRoot = false
	) : BTreeNode<K, V, CompareFn>(block_number, M, int_file, data_file, _isRoot)
	{
		// this->block_number = block_number;
		// this->M = M;
		// this->buffered_file_internal = int_file;
		// this->buffered_file_data = data_file;
		// this->isRoot = _isRoot;
	};

	blocknum_t findInNode(const K&);	//returns block number of appropriate child node
	void deleteFromNode(const K&);
	bool isLeaf() { return false; }

	void getBlockNumbers(std::list<blocknum_t>& list_inst) {
		size_type size = this->getSize();
		offset_t curr;
		for(int i = 0, curr = this->START_BLOCKNUMS;
			i < size;
			i++, curr += sizeof(blocknum_t)
		) {
			list_inst.push_back(
				BufferedFrameReader::read<blocknum_t>(
					this->buffered_file_internal->readBlock(
						this->block_number
					),
					curr
				)
			);
		}
	}


	// SETTER METHOD
	void setBlockNumbers(std::list<blocknum_t>& list_inst) {
		size_type size = this->getSize();
		typename std::list<blocknum_t>::const_iterator block_iter = list_inst.begin();

		for(offset_t curr = this->START_BLOCKNUMS;
			block_iter != list_inst.end();
			curr += sizeof(blocknum_t), block_iter++
		) {
			BufferedFrameWriter::write<blocknum_t>(
				this->buffered_file_internal->readBlock(
					this->block_number
				),
				curr, *block_iter
			);
		}
	}

	// void replaceKey(const K&, blocknum_t);
	K findMedian();

	// never initialised
	// void removeKey(const K& remove_key) {
	// 	return;
	// }
    blocknum_t getPrevBlockNo() {
    	return this->block_number;
    };
    blocknum_t getNextBlockNo() {
    	return this->block_number;
    };
    void setPrevBlockNo(blocknum_t new_prev) {
    	return;
    };
    void setNextBlockNo(blocknum_t new_next) {
    	return;
    };
    void getBlockOffsetPairs(std::list<blockOffsetPair>& list_inst) {
    	return;
    };
	void setBlockOffsetPairs(std::list<blockOffsetPair>& list_inst) {
		return;
	};
	blockOffsetPair findInNodeLeaf(const K&) {
		blockOffsetPair newOne;
		return newOne;
	};


};

// The TreeLeafNode class
template <typename K, typename V, typename CompareFn>
class TreeLeafNode : public BTreeNode<K, V, CompareFn> {

public:
	TreeLeafNode (
		blocknum_t block_number, long M,
		BufferedFile* int_file, BufferedFile* data_file,
		bool _isRoot = false
	)  : BTreeNode<K, V, CompareFn>(block_number, M, int_file, data_file, _isRoot)
	{
		// this->block_number = block_number;
		// this->M = M;
		// this->buffered_file_internal = int_file;
		// this->buffered_file_data = data_file;
		// this->isRoot = _isRoot;
	};

	blockOffsetPair findInNodeLeaf(const K&);
    void deleteFromNode(const K&);

    bool isLeaf() { return true; }

    blocknum_t getPrevBlockNo() {
        return BufferedFrameReader::read<blocknum_t>(
			this->buffered_file_internal->readBlock(this->block_number),
			PREV_BLOCK
		);
    }

    blocknum_t getNextBlockNo() {
        return BufferedFrameReader::read<blocknum_t>(
			this->buffered_file_internal->readBlock(this->block_number),
			NEXT_BLOCK
		);
    }

    void setPrevBlockNo(blocknum_t new_prev) {
        BufferedFrameWriter::write<blocknum_t>(
            this->buffered_file_internal->readBlock(this->block_number),
            PREV_BLOCK,
            new_prev
        );
    }

    void setNextBlockNo(blocknum_t new_next) {
        BufferedFrameWriter::write<blocknum_t>(
            this->buffered_file_internal->readBlock(this->block_number),
            NEXT_BLOCK,
            new_next
        );
    }

    void getBlockOffsetPairs(std::list<blockOffsetPair>& list_inst) {
		size_type size = this->getSize();
		offset_t curr;

		for(int i = 0, curr = this->START_BLOCKNUMS;
			i < size;
			i++, curr += sizeof(blockOffsetPair)
		) {
			list_inst.push_back(
				BufferedFrameReader::read<blockOffsetPair>(
					this->buffered_file_internal->readBlock(
						this->block_number
					),
					curr
				)
			);
		}
	}


	// SETTER METHOD
	void setBlockOffsetPairs(std::list<blockOffsetPair>& list_inst) {
		size_type size = this->getSize();
		typename std::list<blockOffsetPair>::const_iterator block_iter
			= list_inst.begin();

		for(offset_t curr = this->START_BLOCKNUMS;
			block_iter != list_inst.end();
			curr += sizeof(blockOffsetPair), block_iter++
		) {
			BufferedFrameWriter::write<blockOffsetPair>(
				this->buffered_file_internal->readBlock(
					this->block_number
				),
				curr, *block_iter
			);
		}
	}

	// void removeKey(const K& remove_key);
	K findMedian();

	// never initialised
	void getBlockNumbers(std::list<blocknum_t>& list_inst) {
		return;
	};
	void setBlockNumbers(std::list<blocknum_t>& list_inst) {
		return;
	};
	// void replaceKey(const K&, blocknum_t) {
	// 	return;
	// };
	blocknum_t findInNode(const K&) {
		blocknum_t newOne;
		return newOne;
	};

};


// The Main class
template <typename K, typename V, typename CompareFn>
class BTree {

	// OFFSETS for reading header properties
	const offset_t HEADER_STRING = 0;
	const offset_t DS_ID = 4;
	const offset_t ELEM_KEY_SIZE = 12;
	const offset_t ELEM_VALUE_SIZE = 16;
	const offset_t ROOT_BLK_NUM = 20;
	const offset_t TOTAL_BLKS = 20 + sizeof(long);
	const offset_t HEAD_BLK_NO = 20 + 2*sizeof(long);
	const offset_t TAIL_BLK_NO = 20 + 3*sizeof(long);
	const offset_t DATAFILE = 20 + 4*sizeof(long);
	const long Node_type_identifier_size = 1; // in bytes

private:

	BufferedFile* buffered_file_internal;
	BufferedFile* buffered_file_data;

	size_type sz;
	size_type blocksize;
	long M; // Maximum M keys in the internal nodes

	//root block number
	blocknum_t root_block_num;
	CompareFn cmpl;

	int calculateM(const size_t blocksize, const size_t key_size);

	// header-related methods
	void headerInit() {
		BufferFrame *header = buffered_file_internal->readHeader();
		char read_main_string[5], read_iden[9];

		for(int i = 0; i < 4; i++) {
			read_main_string[i] = BufferedFrameReader::read<char>(
				header, i
			);
		}

		for(int i = 0; i < 8; i++) {
			read_iden[i] = BufferedFrameReader::read<char>(
				header, DS_ID + i
			);
		}

		std::string str_ms (read_main_string);
		std::string str_iden(read_iden);

		// this is an un-initialised first time file
		if (str_ms != "RMAD"
			|| str_iden != "BTREE"
		) {
			const char main_str[5] = "RMAD";
			const char iden[6] = "BTREE";

			// for(int i = 0; i < 4; i++) {
			// 	BufferedFrameWriter::write<char>(
			// 		header, i, main_str[i]
			// 	);
			// }

			// for(int i = 0; i < 5; i++) {
			// 	BufferedFrameWriter::write<char>(
			// 		header, DS_ID + i, iden[i]
			// 	);
			// }

			blocknum_t root_block_num
				= buffered_file_internal->allotBlock();

			this->setRootBlockNo(root_block_num);

			// write the key size
			BufferedFrameWriter::write<size_type>(
				header,
				ELEM_KEY_SIZE,
				sizeof(K)
			);

			// write the value size
			BufferedFrameWriter::write<size_type>(
				header,
				ELEM_VALUE_SIZE,
				sizeof(V)
			);

			// write the total blocks as zero
			BufferedFrameWriter::write<long>(
				header,
				TOTAL_BLKS,
				0
			);

			// write the head block number of leaves
			BufferedFrameWriter::write<long>(
				header,
				HEAD_BLK_NO,
				this->root_block_num
			);

			// write the tail block number of leaves
			BufferedFrameWriter::write<long>(
				header,
				TAIL_BLK_NO,
				-1
			);

		} else {
		// if this is already initialised file
			this->root_block_num
				= BufferedFrameReader::read<blocknum_t>(
					header,
					ROOT_BLK_NUM
				);
		}
	}


	void setDataFileName(const char* _dataFileName) {
		BufferFrame *header = buffered_file_internal->readHeader();
		size_t len_of_filename = std::strlen(_dataFileName);

		for(int i = 0; i < len_of_filename; i++) {
			BufferedFrameWriter::write<char>(
				header,
				DATAFILE + i,
				_dataFileName[i]
			);
		}
	}



	// this will read the given block, set the properties accordingly for
	// the new instance, and also set the new nodes my_block bufferframe pointer
	// and then return the block
	BTreeNode<K, V, CompareFn>* getNodeFromBlockNum(blocknum_t);
	/* makes an InternalNode : if first byte of block is 0
	   makes a 	LeafNode     : if fisrt byte of block is 1
	*/

    BTreeNode<K, V, CompareFn>* splitChild(
		BTreeNode<K, V, CompareFn>* child_to_split,
		BTreeNode<K, V, CompareFn>* current_node,
		const K& add_key
	);

    // only called for adding to a TreeLeafNode, so const V&
	void addToNode(const K, const V, BTreeNode<K, V, CompareFn>*);

	void adjustLeaf(BTreeNode<K, V, CompareFn> *parent,
		BTreeNode<K, V, CompareFn> *node_to_adjust
	);

	// methods for comparing keys
	bool eq(const K& k1, const K& k2) {
		return !(cmpl(k1, k2) && cmpl(k2, k1));
	}

	bool neq(const K& k1, const K& k2) {
		return !(eq(k1, k2));
	}

public:
	BTree(const char* pathname, size_type _blocksize = 4096) : blocksize(_blocksize), sz(0) {
		buffered_file_internal = new BufferedFile(pathname);
		// get data file
		// char data_file_name[100];
		// std::strcpy(data_file_name, pathname);
		// std::strcat(data_file_name, "_data");

		buffered_file_data = new BufferedFile("data_file", sizeof(V));

		// will write it to the appropriate position in the header
		this->setDataFileName(
			"data_file"
		);

		M = calculateM(blocksize, sizeof(K));

		printf("M : %ld\n", M);
		this->headerInit();
		// now ready for operation
	}

	~BTree() {
		delete buffered_file_internal;
		delete buffered_file_data;
	}

	V searchElem(const K& key);
	void insertElem(const K& key, const V& value);
	void deleteElem(const K& key);
	void clearTree();

	size_type size() {
		return sz;
	}

	TreeLeafNode<K, V, CompareFn>* getSmallestValueNode() {
		BTreeNode<K, V, CompareFn> *curr, *new_node;
		curr = this->getNodeFromBlockNum(this->root_block_num);

		while (! curr->isLeaf()) {
			new_node = this->getNodeFromBlockNum(
				curr->getSmallestKeyBlockNo()
			);
			delete curr;
			curr = new_node;
		}
		return curr;
	}

	TreeLeafNode<K, V, CompareFn>* getLargestValueNode() {
		BTreeNode<K, V, CompareFn> *curr, *new_node;
		curr = this->getNodeFromBlockNum(this->root_block_num);

		while (! curr->isLeaf()) {
			new_node = this->getNodeFromBlockNum(
				curr->getLargestKeyBlockNo()
			);
			delete curr;
			curr = new_node;
		}
		return curr;
	}

	blocknum_t getRootBlockNo() {
		return this->root_block_num;
	}

	void setRootBlockNo(blocknum_t _rootBlockNum) {
		this->root_block_num = _rootBlockNum;
		BufferFrame* header = buffered_file_internal->readHeader();
		BufferedFrameWriter::write<blocknum_t>(
			header, ROOT_BLK_NUM, _rootBlockNum
		);
	}


	long count(const K&);

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

			std::list<blockOffsetPair> addrList;
			if (start_end) {
				curr = head;
				curr->getBlockOffsetPairs(addrList);
				block_iter = addrList.begin();

				value = BufferedFrameReader::readPtr<V>(
					data_file->readBlock((*block_iter).block_number),
					(*block_iter).offset
				);
			} else {
				curr = tail;
				curr->getBlockOffsetPairs(addrList);

				block_iter = ++((addrList.end()));
			}
		}

		bool operator== (const iterator& rhs);
		bool operator!= (const iterator& rhs) { return !(operator==(rhs)); };
		V operator* ();
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


// Iterator and related methods
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
V BTree<K, V, CompareFn>::iterator::operator*() {
	return value;
}

template <typename K, typename V, typename CompareFn>
typename BTree<K, V, CompareFn>::iterator& BTree<K, V, CompareFn>::iterator::operator++() {
	std::list<blockOffsetPair> addrList;
	BTreeNode<K, V, CompareFn> *new_node;
	curr->getBlockOffsetPairs(addrList);

	// i.e. all keys in this 'curr' have been traversed
	if (++block_iter == addrList.end()) {
		if (curr != tail) {
			new_node = curr->getNodeFromBlockNum(
				curr->getNextBlockNo()
			);
			delete curr;
			curr = new_node;
			addrList.clear();
			curr->getBlockOffsetPairs(addrList);
		} else {
			return nullptr;
		}
		block_iter = addrList.begin();
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
	std::list<blockOffsetPair> addrList;
	BTreeNode<K, V, CompareFn> *new_node;
	curr->getBlockOffsetPairs(addrList);

	// i.e. all keys in this 'curr' have been traversed
	if (block_iter == addrList.begin()) {
		if (curr != head) {
			new_node = curr->getNodeFromBlockNum(
				curr->getPrevBlockNo()
			);
			delete curr;
			curr = new_node;
			addrList.clear();
			curr->getBlockOffsetPairs(addrList);
		} else {
			return nullptr;
		}
		block_iter = --(addrList.end());
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

// B-Tree method - calculateM
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

// Main APIs - start
template <typename K, typename V, typename CompareFn>
V BTree<K, V, CompareFn>::searchElem(const K& search_key) {
	blockOffsetPair valueAddr;
	blocknum_t next_block_num;

	// Node for traversing the tree
	BTreeNode<K, V, CompareFn> *head =
		this->getNodeFromBlockNum(this->getRootBlockNo());

	while (! head->isLeaf()) {

		// always called on an internal node
		next_block_num = head->findInNode(search_key);
		if (next_block_num == NULL_BLOCK) {
			//TODO: exception
		}

		BTreeNode<K, V, CompareFn> *next_node
			= this->getNodeFromBlockNum(next_block_num);

		head = next_node;
	}

	// always called on a leaf node - returns a block-offset pair
	valueAddr = head->findInNodeLeaf(search_key);

	// i.e. the key is not found
	if (valueAddr.block_number == NULL_BLOCK
		|| valueAddr.offset == NULL_OFFSET
	) {
		//TODO: exception
	}

	// otherwise read the value from the block in data file and return
	BufferFrame* buff = buffered_file_data->readBlock(valueAddr.block_number);
	return BufferedFrameReader::read<V>(buff, valueAddr.offset);
};

template <typename K, typename V, typename CompareFn>
void BTree<K, V, CompareFn>::insertElem(const K& new_key, const V& new_value) {
	BufferFrame* disk_block;
	BTreeNode<K, V, CompareFn> *next_node, *temp;
	blocknum_t root_block_num = this->getRootBlockNo();

	temp = this->getNodeFromBlockNum(root_block_num);

	if (this->size() == 0) {
		// i.e. 'first' insert in the tree
		this->addToNode(new_key, new_value, temp);
	} else {
		blockOffsetPair valueAddr;
		blocknum_t next_block_num;

		// Node for traversing the tree
		BTreeNode<K, V, CompareFn> *trav = temp, *par;

		// split the root if needed and make a new root
		if (temp->isFull()) {
			blocknum_t old_root_block_num = this->getRootBlockNo();
			blocknum_t new_root_bnum = buffered_file_internal->allotBlock();

			BTreeNode<K, V, CompareFn> *old_root = temp;
			// New root must be an InternalNode
			BTreeNode<K, V, CompareFn> *new_root
				= new InternalNode<K, V, CompareFn>(
					new_root_bnum, this->M,
					this->buffered_file_internal, this->buffered_file_data,
					true
				);

			std::list<blocknum_t> blockNumList;
			new_root->getBlockNumbers(blockNumList);

			// POSSIBLE BUG HERE
			// Add old root block number as left most child_block_number
			blockNumList.push_back(old_root_block_num);
			new_root->setBlockNumbers(blockNumList);

			// Set new_root to be the root of BTree
			this->setRootBlockNo(new_root_bnum);

			// make isRoot = false in old_root
			old_root->setIsRoot(false);

			// Split old root
			trav = this->splitChild(old_root, new_root, new_key);
		}

		while (trav && ! trav->isLeaf()) {
			next_block_num = trav->findInNode(new_key);
			next_node = this->getNodeFromBlockNum(next_block_num);

			// pro-active splitting
			if (next_node->isFull()) {
				// returns the proper next node

				next_node = this->splitChild(next_node, trav, new_key);

				delete trav;
			} else {
				next_block_num = trav->findInNode(new_key);
				par = trav;
				next_node = this->getNodeFromBlockNum(next_block_num);

				delete trav;
			}
			trav = next_node;
		}

		// here, trav is always a leaf node


		// if key not present, so add the key and the value
		// if key already exists, adds another value
		// with another key instance just next to previous one
		this->addToNode(new_key, new_value, trav);

		// destructor will automatically write it to the block
		// plus our setter methods will mark a node's prop 'isChangedInMem'
		// so that destructor does not write unnecessarily
		delete trav;
	}

	this->sz++;
};


// template <typename K, typename V, typename CompareFn>
// void BTree<K, V, CompareFn>::deleteElem(const K& remove_key) {

// 	std::list<K> key_list;
// 	std::list<blockOffsetPair> block_pair_list;
// 	BTreeNode<K, V, CompareFn>* trav, next_node, temp = nullptr;
// 	K replacement_key;

// 	trav = this->getNodeFromBlockNum(
// 		this->getRootBlockNo()
// 	);

// 	bool contains_key;
// 	blocknum_t next_block_num;

// 	if (trav->isLeaf()) {
// 		// root node is a leaf
// 		trav->removeKey(remove_key);
// 		return;
// 	}


// 	while (trav && ! trav->isLeaf()) {
// 		// method implemented below
// 		contains_key = trav->containsKey(remove_key, next_block_num);

// 		if (contains_key) {
// 			temp = trav;
// 			// need to replace remove_key with least key in right subtree of temp to satisfy c1 convention
// 		}
// 		next_node = this->getNodeFromBlockNum(next_block_num);

// 		if (next_node->isLeaf()) {
// 			next_node->removeKey(remove_key);
// 			this->adjustLeaf(trav, next_node);

// 			if (temp != nullptr) {
// 				std::list<blocknum_t> block_list;
// 				std::list<K> child_keys;
// 				blocknum_t least = trav->getSmallestKeyBlockNo();

// 				BTreeNode<K, V, CompareFn>* first_child = this->getNodeFromBlockNum(least);
// 				replacement_key = first_child->getSmallestKey();

// 				temp->replaceKey(remove_key, replacement_key);
// 				delete temp;
// 			}
// 			break;

// 		} else {
// 			this->adjustInternal(trav, next_node);
// 			// restart delete from current trav
// 			continue;
// 		}
// 	}

// 	// reduce sz
// 	this->sz--;
// }

template <typename K, typename V, typename CompareFn>
long BTree<K, V, CompareFn>::count(const K& find_key) {

	BTreeNode<K, V, CompareFn> *trav, *next_node;
	trav = this->getNodeFromBlockNum(
		this->getRootBlockNo()
	);
	blocknum_t next_block_num;
	long count = 0;

	while(trav && ! trav->isLeaf()) {
		// always called on an internal node
		next_block_num = trav->findInNode(find_key);
		if (next_block_num == NULL_BLOCK) return 0;

		next_node
			= this->getNodeFromBlockNum(next_block_num);
		trav = next_node;
	}

	if (trav && trav->isLeaf()) {
		typename std::list<K>::const_iterator key_iter;

		std::list<K> keyList;
		trav->getKeys(keyList);

		for (
			key_iter = keyList.begin();
			key_iter != keyList.end();
			key_iter++
		) {
			if (this->eq(*key_iter, find_key)) {
				count++;
			} else if (this->cmpl(*key_iter, find_key)) {
				break;
			}
		}
	}

	return count;
};

// Main APIs - end

template <typename K, typename V, typename CompareFn>
blocknum_t InternalNode<K, V, CompareFn>::findInNode(const K& find_key) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blocknum_t>::const_iterator block_iter;

	std::list<K> keyList;
	std::list<blocknum_t> blockList;
	this->getKeys(keyList); this->getBlockNumbers(blockList);

	for (
		key_iter = keyList.begin(), block_iter = blockList.begin();
		key_iter != keyList.end();
		key_iter++, block_iter++
	) {
		if (this->cmpl(*key_iter, find_key)) {
			return *block_iter;
		} else if (this->eq(*key_iter, find_key)) {

			// CRITICAL: since we assume that if the key is equal, we search in the 'right' block
			// same is followed in insertion
			block_iter++;
			return *block_iter;
		}
	}

	return *block_iter;
};

template <typename K, typename V, typename CompareFn>
blockOffsetPair TreeLeafNode<K, V, CompareFn>::findInNodeLeaf(const K& find_key) {
	typename std::list<K>::const_iterator key_iter;
	typename std::list<blockOffsetPair>::const_iterator block_iter;
	blockOffsetPair reqd_block;

	std::list<K> keyList;
	std::list<blockOffsetPair> blockPairList;
	this->getKeys(keyList); this->getBlockOffsetPairs(blockPairList);

	for (
		key_iter = keyList.begin(), block_iter = blockPairList.begin();
		key_iter != keyList.end();
		key_iter++, block_iter++
	) {
		if (this->eq(*key_iter, find_key)) {
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
K InternalNode<K, V, CompareFn>::findMedian() {
	typename std::list<K>::const_iterator key_iterator;

	std::list<K> keyList;
	this->getKeys(keyList);

	// increment till the middle element
	key_iterator = keyList.begin();
	std::advance(
		key_iterator, (int)(this->getSize() / 2)
	);

	// return the middle element
	return *key_iterator;
};

template <typename K, typename V, typename CompareFn>
K TreeLeafNode<K, V, CompareFn>::findMedian() {
	typename std::list<K>::const_iterator key_iterator;

	std::list<K> keyList;
	this->getKeys(keyList);

	// increment till the middle element
	key_iterator = keyList.begin();
	std::advance(
		key_iterator, (int)(this->getSize() / 2)
	);

	// return the middle element
	return *key_iterator;
};

template <typename K, typename V, typename CompareFn>
BTreeNode<K, V, CompareFn>* BTree<K, V, CompareFn>::splitChild(
	BTreeNode<K, V, CompareFn>* child_to_split,
	BTreeNode<K, V, CompareFn>* current_node,
	const K& add_key
) {

	// In this method, 'current_node' refers to the node
	// that initiated the split on its child 'child_to_split'

	// CRITICAL: The new_node is always created on the right

	blocknum_t new_block_num = buffered_file_internal->allotBlock();
	BufferFrame* disk_block_new = buffered_file_internal->readBlock(new_block_num);
	K median_key = child_to_split->findMedian();
	TreeLeafNode<K, V, CompareFn>* new_node;

	std::list<blocknum_t> blockNumList, blockNumList_new;
	std::list<blockOffsetPair> blockPairList, blockPairList_new;
	std::list<K> keyList, keyList_new;

	if (child_to_split->isLeaf()) {
		new_node = new TreeLeafNode<K, V, CompareFn>(
			new_block_num, this->M,
			this->buffered_file_internal,
			this->buffered_file_data,
			current_node->getIsRoot()
		);
		typename std::list<K>::iterator old_key_iter;
		typename std::list<blockOffsetPair>::iterator old_block_iter;

		child_to_split->getKeys(keyList);
		child_to_split->getBlockOffsetPairs(blockPairList);

		//distribute keys
		for (
			old_key_iter = keyList.begin(), old_block_iter = blockPairList.begin();
			old_key_iter != (keyList).end();
			// no increment, read comments below
		) {
			if (this->eq(*old_key_iter, median_key)) {
				//CRITICAL: MEDIAN KEY MUST BE ADDED TO NEW NODE IN CASE OF LEAF NODES
				keyList_new.push_back(*old_key_iter);
				blockPairList_new.push_back(*old_block_iter);

				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = keyList.erase(old_key_iter);
				old_block_iter = blockPairList.erase(old_block_iter);

			} else if (this->cmpl(*old_key_iter, median_key)) {
				keyList_new.push_back(*old_key_iter);
				blockPairList_new.push_back(*old_block_iter);

				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = keyList.erase(old_key_iter);
				old_block_iter = blockPairList.erase(old_block_iter);
			} else {
				old_key_iter++;
				old_block_iter++;
			}
		}

		child_to_split->setKeys(keyList);
		new_node->setKeys(keyList_new);

		child_to_split->setBlockOffsetPairs(blockPairList);
		new_node->setBlockOffsetPairs(blockPairList_new);

		// THE FOLLOWING CODE UPDATES THE POINTERS FOR
		// MAINTAINING A DLL of LEAVES
		// update the prev and next pointers
		new_node->setPrevBlockNo(child_to_split->getBlockNo());
		new_node->setNextBlockNo(child_to_split->getNextBlockNo());

		// update the 'prev' of the (orignally) 'next' of child_to_split
		BufferFrame* disk_block = buffered_file_internal->readBlock(
			child_to_split->getNextBlockNo()
		);
		BufferedFrameWriter::write<blocknum_t>(
			disk_block, PREV_BLOCK, child_to_split->getNextBlockNo()
		);

		// now update the next of child_to_split
		child_to_split->setNextBlockNo(new_node->getBlockNo());
	} else {
		new_node = new TreeLeafNode<K, V, CompareFn>(
			new_block_num, this->M,
			this->buffered_file_internal,
			this->buffered_file_data
		);

		typename std::list<K>::iterator old_key_iter;
		typename std::list<blocknum_t>::iterator old_block_iter;

		child_to_split->getKeys(keyList);
		child_to_split->getBlockNumbers(blockNumList);

		//distribute keys
		for (
			old_key_iter = keyList.begin(), old_block_iter = blockNumList.begin();
			old_key_iter != (keyList).end();
			// no increment, read comments below
		) {
			if (this->eq(*old_key_iter, median_key)) {
				//CRITICAL: MEDIAN KEY MUST BE ADDED TO NEW NODE IN CASE OF LEAF NODES
				keyList_new.push_back(*old_key_iter);
				blockNumList_new.push_back(*old_block_iter);

				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = keyList.erase(old_key_iter);
				old_block_iter = blockNumList.erase(old_block_iter);

			} else if (this->cmpl(*old_key_iter, median_key)) {
				keyList_new.push_back(*old_key_iter);
				blockNumList_new.push_back(*old_block_iter);

				// erase and don't need to increment
				// as it points already to the (previously) next elem in list
				old_key_iter = keyList.erase(old_key_iter);
				old_block_iter = blockNumList.erase(old_block_iter);
			} else {
				old_key_iter++;
				old_block_iter++;
			}
		}

		//add the rightmost block number to new node
		blockNumList_new.push_back(*old_block_iter);

		child_to_split->setKeys(keyList);
		new_node->setKeys(keyList_new);

		child_to_split->setBlockOffsetPairs(blockPairList);
		new_node->setBlockOffsetPairs(blockPairList_new);
	}

	keyList.clear();
	blockNumList.clear();

	current_node->getKeys(keyList);
	current_node->getBlockNumbers(blockNumList);

	typename std::list<K>::iterator curr_key_iter = keyList.begin();
	typename std::list<blocknum_t>::iterator curr_block_iter = blockNumList.begin();

	while (*curr_block_iter != child_to_split->getBlockNo()) {
		curr_block_iter++;
		curr_key_iter++;
	}

	curr_block_iter++;
	curr_key_iter++;

	// adjust keys and pointers in parent node
	keyList.insert(curr_key_iter, median_key);
	keyList.insert(curr_key_iter, new_node->block_number);

	current_node->setKeys(keyList);
	current_node->setBlockNumbers(blockNumList);

	delete current_node;
	delete child_to_split;

	if (cmpl(median_key, add_key)) {
		return child_to_split;
	} else {
		return new_node;
	}
};


template <typename K, typename V, typename CompareFn>
void BTree<K, V, CompareFn>::addToNode(
	const K new_key, const V new_value,
	BTreeNode<K, V, CompareFn> *current_node
) {
	typename std::list<K>::iterator key_iter;
	typename std::list<blockOffsetPair>::iterator block_iter;
	blockOffsetPair new_block_offset;

	std::list<blockOffsetPair> blockPairList;
	std::list<K> keyList;

	current_node->getKeys(keyList);
	current_node->getBlockOffsetPairs(blockPairList);

	new_block_offset.block_number = this->buffered_file_data->allotBlock();
	new_block_offset.offset = 0;

	for (
		key_iter = keyList.begin(), block_iter = blockPairList.begin();
		key_iter != (keyList).end();
		key_iter++, block_iter++
	) {
		if (this->cmpl(*key_iter, new_key)) {
			break;
		} else if (this->eq(*key_iter, new_key)) {

			// CRITICAL: if the key is already present,
			// add another key to its next slot
			// thus allowing for duplicate keys
			block_iter++;
			break;
		}
	}

	// add entries in the resp. lists
	keyList.insert(key_iter, new_key);
	blockPairList.insert(block_iter, new_block_offset);

	current_node->setKeys(keyList);
	current_node->setBlockOffsetPairs(blockPairList);
};

template <typename K, typename V, typename CompareFn>
BTreeNode<K, V, CompareFn>* BTree<K, V, CompareFn>::getNodeFromBlockNum(
	blocknum_t block_number
) {

	bool is_this_root;
	BTreeNode<K, V, CompareFn> *new_node;
	BufferFrame* buff_frame =  this->buffered_file_internal->readBlock(block_number);

	offset_t curr_offset = 0;
	node_t node_type_id = BufferedFrameReader::read<node_t>(buff_frame, curr_offset++);

	if (this->root_block_num == block_number) {
		is_this_root = true;
	} else {
		is_this_root = false;
	}

	if (node_type_id) {
		// make a leaf node
		new_node = new TreeLeafNode<K, V, CompareFn>(
			block_number, this->M,
			this->buffered_file_internal,
			this->buffered_file_data,
			is_this_root
		);

	} else {
		// make an internal node
		new_node = new InternalNode<K, V, CompareFn>(
			block_number, this->M,
			this->buffered_file_internal,
			this->buffered_file_data,
			is_this_root
		);
	}

	return new_node;
};

// /*
// 	DELETION:
// 	(DO NOT WRITE OTHER METHODS TILL "DELETION ENDS" COMMENT)

// 	convention:
// 	C1: if a key is removed it must be removed from both internal and external node
// */


// // NOTE: adjustLeaf and adjustInternal can be made into one method by adding if (leaf) {} else{}
// // if next_node (i.e node_to_adjust) is a leaf node
// template <typename K, typename V, typename CompareFn>
// void BTree<K,V,CompareFn>::adjustLeaf(
// 	BTreeNode<K, V, CompareFn>* parent,
// 	BTreeNode<K, V, CompareFn>* node_to_adjust
// ) {

// 	BTreeNode<K, V, CompareFn>* sibling;
// 	std::list<K> parent_key_list, node_key_list, left_sib_key_list, right_sib_key_list;
// 	std::list<blockOffsetPair> parent_block_list, node_block_list, left_sib_block_list, right_sib_block_list;
// 	parent->getKeys(parent_key_list);
// 	node_to_adjust->getKeys(node_key_list);

// 	K old_key;
// 	blocknum_t node_block_num = node_to_adjust->getBlockNo();

// 	// ?? what is key_list
// 	// also define MIN_KEYS
// 	if (node_key_list.size() < MIN_KEYS) {

// 		if (parent_block_list.front() != node_block_num) {
// 			// find left sibling
// 			left_sibling  = this->getNodeFromBlockNum(
// 				node_to_adjust->getPrevBlockNo()
// 			);
// 		}

// 		if (parent_block_list.back() != node_block_num) {
// 			right_sibling = this->getNodeFromBlockNum(
// 				node_to_adjust->getNextBlockNo()
// 			);
// 		}

// 		//BORROW FROM LEFT CHILD IN trav if such exists
// 		//NOTE: if next_node if the first child of trav, then we MUST use right node


// 		//TODO: check wherever old_key is set!

// 		if (left_sibling != nullptr && left_sibling->getSize() > MIN_KEYS) {
// 					// if left sibling has > MIN_KEYS
// 			node_key_iter = node_key_list.begin();
// 			node_block_iter = node_block_list.begin();

// 			left_sibling->getKeys(left_sib_key_list);
// 			left_sibling->getBlockOffsetPairs(left_sib_block_list);

// 			//need to know old_key
// 			old_key = ;
// 			left_sib_key_iter = left_sib_key_list.end();
// 			left_sib_block_iter = left_sib_block_list.end();
// 			left_sib_key_iter--;
// 			left_sib_block_iter--;

// 			node_key_list.insert(node_key_iter, *left_sib_key_iter);
// 			node_block_list.insert(node_block_iter, *left_sib_block_iter);

// 			node_to_adjust->setKeys(node_key_list);
// 			node_to_adjust->setBlockOffsetPairs(node_block_list);

// 			parent->replaceKey( old_key, *left_sib_key_iter);

// 			left_sib_key_iter = left_sib_key_list.erase(left_sib_key_iter);
// 			left_sib_block_iter = left_sib_block_list.erase(left_sib_block_iter);

// 			left_sibling->setKeys(left_sib_key_list);
// 			left_sibling->setBlockOffsetPairs(left_sib_block_list);

// 		} else if (right_sibling != nullptr && right_sibling->getSize() > MIN_KEYS) {
// 			//NOTE: cant use this condition if next_node is the right most child of trav
// 			// if right sibling has > MIN_KEYS

// 			//assumes that key in parent is same as first key in right child
// 			right_sibling->getKeys(right_sib_key_list);
// 			right_sibling->getBlockOffsetPairs(right_sib_block_list);


// 			right_sib_key_iter = right_sib_key_list.begin();
// 			right_sib_block_iter = right_sib_block_list.begin();
// 			old_key = ;

// 			node_key_list.push_back(*right_sib_key_iter);
// 			node_block_list.push_back(*right_sib_block_iter);

// 			node_to_adjust->setKeys(node_key_list);
// 			node_to_adjust->setBlockOffsetPairs(node_block_list);

// 			right_sib_key_iter = right_sib_key_list.erase(right_sib_key_iter);
// 			right_sib_block_iter = right_sib_block_list.erase(right_sib_block_iter);

// 			parent->replaceKey(old_key, *right_sib_key_iter);

// 			right_sibling->setKeys(right_sib_key_list);
// 			right_sibling->setBlockOffsetPairs(right_sib_block_list);

// 		} else {
// 				// MERGE:
// 			if (parent->isRoot() && parent_key_list.size() < 2) {
// 				// If parent is root and has only one key, 
// 				//then merge parent and both its children into one node, 
// 				// update root block number in ?? ANS: HEADER BLOCK!

// 				//two cases:
// 				//case 1: node_to_adjust is right child of root (if case)
// 				//case 2: node_to_adjust is left child of root   (else case)

// 				if (left_sibling != nullptr) {
// 					//add everyting from nod_to_adjust to left_sib and make left_sibling the root node, discard all other nodes as 
// 					node_key_iter = node_key_list.begin();
// 					node_block_iter = node_block_list.begin();

// 					left_sibling->getKeys(left_sib_key_list);
// 					left_sibling->getBlockOffsetPairs(left_sib_block_list);

// 					while(node_key_iter != node_key_list.end()) {
// 						left_sib_key_list.push_back(*node_key_iter);
// 						left_sib_block_list.push_back(*node_block_iter);

// 						node_key_iter = node_key_list.erase();
// 						node_block_iter = node_block_list.erase();
// 					}

// 					left_sibling->setKeys(left_sib_key_list);
// 					left_sibling->setBlockOffsetPairs(left_sib_block_list);

// 					//TODO: Make left_subling the ROOT NODE

// 				} else {

// 					right_sibling->getKeys(right_sib_key_list);
// 					right_sibling->getBlockOffsetPairs(right_sib_block_list);

// 					typename std::list<K>::iterator right_sib_key_iter = right_sib_key_list.begin();
// 					typename std::list<blockOffsetPair>::iterator right_sib_block_iter = right_sib_block_list.begin();

// 					while(right_sib_key_iter != right_sib_key_list.end()) {
// 						node_key_list.push_back(*right_sib_key_iter);
// 						node_block_list.push_back(*right_sib_block_iter);

// 						right_sib_key_iter = right_sib_key_list.erase();
// 						right_sib_block_iter = right_sib_block_list.erase();
// 					}

// 					node_to_adjust->setKeys(node_key_list);
// 					node_to_adjust->setBlockNumbers(node_block_list);

// 					//TODO: MAKE node_to_adjust the ROOT NODE

// 				}
// 			} else {
// 				//  MERGE: NON-SPECIAL
// 				// if parent is the only internal Node and there are only two nodes 
// 				// merge them to form one leaf node and update root block number in header
// 				// if left sibling doesnt exist, merge with right sibling
// 				// if right sibling doesnt exist, merge with left sibling
// 				// if both exist and both have < MIN_KEYS: is such a state possible in out algo???????
// 				if (left_sibling == nullptr) {
// 					this->mergeHelper(parent, node_to_adjust, right_sibling);
// 				} else {
// 					this->mergeHelper(parent, left_sibling, node_to_adjust);
// 				}

// 			}
// 		}
// 	}
// }

// // if next_node (i.e node_to_adjust) is an internal node
// template <typename K, typename V, typename CompareFn>
// void BTree<K,V,CompareFn>::adjustInternal(
// 	BTreeNode<K, V, CompareFn>* parent, BTreeNode<K, V, CompareFn>* node_to_adjust
// ) {
// 	BTreeNode<K, V, CompareFn>* left_sibling = nullptr, right_sibling = nullptr;

// 	//key lists
// 	std::list<K> parent_key_list, node_key_list, left_sib_key_list, right_sib_key_list;
// 	//block lists
// 	std::list<blocknum_t> parent_block_list, node_block_list, left_sib_block_list, right_sib_block_list;

// 	//key iters
// 	typename std::list<K>::iterator parent_key_iter, node_key_iter, left_sib_key_iter, right_sib_key_iter;

// 	//block iters
// 	typename std::list<blockOffsetPair>::iterator parent_block_iter, node_block_iter, left_sib_block_iter, right_sib_block_iter;


// 	K replacement_key;
// 	blocknum_t replacement_block;

// 	blocknum_t node_block_num = node_to_adjust->getBlockNo();

// 	parent->getKeys(parent_key_list);
// 	node_to_adjust->getKeys(node_key_list);

// 	//bool left_most_child, right_most_child;

// 	if (key_list.size() < MIN_KEYS - 1) {


// 		if (parent_block_list.front() != node_block_num) {
// 			// make left sibling
// 			left_sibling  = this->getNodeFromBlockNum(node_to_adjust->getPrevBlockNo());
// 		}

// 		if (parent_block_list.back() != node_block_num) {
// 			//make right sibling
// 			right_sibling = this->getNodeFromBlockNum(node_to_adjust->getNextBlockNo());
// 		}



// 		//borrow from left child
// 		if ( left_sibling != nullptr &&  left_sibling->getSize() > MIN_KEYS) {
// 			// if left sibling has > MIN_KEYS

// 			node_key_iter = node_key_list.begin();
// 			node_block_iter = node_block_list.begin();

// 			left_sibling->getKeys(left_sib_key_list);
// 			left_sibling->getBlockOffsetPairs(left_sib_block_list);

// 			//CRITICAL:
// 			//TODO: is it possible to do curr = right_node
// 			BTreeNode<K, V, CompareFn>* curr = right_sibling, new_node;

// 			while (! curr->isLeaf()) {
// 				new_node = this->getNodeFromBlockNum(
// 				curr->getSmallestKeyBlockNo()
// 				);
// 				delete curr;
// 				curr = new_node;
// 			}

// 			replacement_key = curr->getSmallestKey();
// 			delete curr;

// 			node_key_list.insert(node_key_iter, replacement_key;

// 			//need to know old_key
// 			old_key = ;
// 			left_sib_key_iter = left_sib_key_list.end();
// 			left_sib_block_iter = left_sib_block_list.end();
// 			left_sib_key_iter--;
// 			left_sib_block_iter--;

// 			node_block_list.insert(node_block_iter, *left_sib_block_iter);

// 			node_to_adjust->setKeys(node_key_list);
// 			node_to_adjust->setBlockOffsetPairs(node_block_list);

// 			BTreeNode<K, V, CompareFn>* curr = right_sibling, new_node;

// 			while (! curr->isLeaf()) {
// 				new_node = this->getNodeFromBlockNum(
// 					curr->getSmallestKeyBlockNo()
// 				);
// 				delete curr;
// 				curr = new_node;
// 			}

// 			replacement_key = curr->getSmallestKey();
// 			delete curr;

// 			parent->replaceKey( old_key, replacement_key);

// 			left_sib_key_iter = left_sib_key_list.erase(left_sib_key_iter);
// 			left_sib_block_iter = left_sib_block_list.erase(left_sib_block_iter);

// 			left_sibling->setKeys(left_sib_key_list);
// 			left_sibling->setBlockOffsetPairs(left_sib_block_list);



// 		} else if ( right_sibling != nullptr && right_sibling->getSize() > MIN_KEYS ) {
// 				//NOTE: cant use this condition if next_node is the right most child of trav
// 				// if right sibling has > MIN_KEYS
// 			right_sibling->getKeys(right_sib_key_list);
// 			right_sibling->getBlockNumbers(right_sib_block_list);	

// 			right_sib_key_iter = right_sib_key_list.begin();
// 			right_sib_block_list = right_sib_block_list.begin();

// 			//CRITICAL:
// 			//TODO: is it possible to do curr = right_node
// 			BTreeNode<K, V, CompareFn>* curr = right_sibling, new_node;

// 			while (! curr->isLeaf()) {
// 				new_node = this->getNodeFromBlockNum(
// 					curr->getSmallestKeyBlockNo()
// 				);
// 				delete curr;
// 				curr = new_node;
// 			}

// 			replacement_key = curr->getSmallestKey();
// 			delete curr;

// 			node_key_list.push_back(replacement_key);
// 			node_block_list.push_back(*right_sib_block_iter);

// 			old_key = ;

// 			node_to_adjust->setKeys(node_key_list);
// 			node_to_adjust->setBlockNumbers(node_block_list);

// 			right_sib_key_iter = right_sib_key_list.erase(right_sib_key_iter);
// 			right_sib_block_iter = right_sib_block_list.erase(right_sib_block_iter);

// 			right_sibling->setKeys(right_sib_key_list);
// 			right_sibling->setBlockNumbers(right_sib_block_list);

// 			//CRITICAL:
// 			//TODO: is it possible to do curr = right_node
// 			BTreeNode<K, V, CompareFn>* curr = right_sibling, new_node;

// 			while (! curr->isLeaf()) {
// 				new_node = this->getNodeFromBlockNum(
// 					curr->getSmallestKeyBlockNo()
// 				);
// 				delete curr;
// 				curr = new_node;
// 			}

// 			replacement_key = curr->getSmallestKey();

// 			delete curr;

// 			parent->replaceKey(old_key, *right_sib_block_iter);


// 		} else {

// 			if ( parent->isRoot() && (parent->getSize() < 2) ) {
// 				// If parent has only one key, then merge parent and both its children into one node
// 				// node_to_adjust is eihter the leftmost child of root or right most child of root
// 				//since num of keys in parent == 1

// 				//first add the only remaining key in root to the left child of root
// 				//then
// 				//add all keys AND blocks from right child of root to left child if root and make left child the root node, discard all other nodes

// 				//two cases:
// 				//case 1: node_to_adjust is right child of root (if case)
// 				//case 2: node_to_adjust is left child of root   (else case)

// 				if(left_sibling != nullptr){
// 					// CASE 1

// 					//first add the only remaining key in root to the left_sibling
// 					//then
// 					//add all keys AND blocks from nod_to_adjust to left_sib and make left_sibling the root node, discard all other nodes




// 					node_key_iter = node_key_list.begin();
// 					node_block_iter = node_block_list.begin();

// 					left_sibling->getKeys(left_sib_key_list);
// 					left_sibling->getBlockOffsetPairs(left_sib_block_list);

// 					//TODO: should it be parent_key_list.front() or *(parent_key_list.front())??
// 					left_sib_key_list.push_back(parent_key_list.front());

// 					while(node_key_iter != node_key_list.end()) {
// 						left_sib_key_list.push_back(*node_key_iter);
// 						left_sib_block_list.push_back(*node_block_iter);

// 						node_key_iter = node_key_list.erase();
// 						node_block_iter = node_block_list.erase();
// 						//
// 					}

// 					left_sib_block_list.push_back(*node_block_iter);
// 					node_block_iter = node_block_list.erase();

// 					left_sibling->setKeys(left_sib_key_list);
// 					left_sibling->setBlockOffsetPairs(left_sib_block_list);

// 					//TODO: make left_sibling the root node

// 				} else {

// 					// CASE 2:

// 					right_sibling->getKeys(right_sib_key_list);
// 					right_sibling->getBlockOffsetPairs(right_sib_block_list);

// 					typename std::list<K>::iterator right_sib_key_iter = right_sib_key_list.begin();
// 					typename std::list<blockOffsetPair>::iterator right_sib_block_iter = right_sib_block_list.begin();

// 					//TODO: should it be parent_key_list.front() or *(parent_key_list.front())??
// 					node_key_list.push_back(parent_key_list.front());

// 					while(right_sib_key_iter != right_sib_key_list.end()) {
// 						node_key_list.push_back(*right_sib_key_iter);
// 						node_block_list.push_back(*right_sib_block_iter);

// 						right_sib_key_iter = right_sib_key_list.erase();
// 						right_sib_block_iter = right_sib_block_list.erase();
// 					}

// 					node_block_list.push_back(*right_sib_block_iter);
// 					right_sib_block_iter = right_sib_block_list.erase();

// 					node_to_adjust->setKeys(node_key_list);
// 					node_to_adjust->setBlockNumbers(node_block_list);

// 					//TODO: make node_to_adjust the root node

// 				}
// 			} else {
// 				// MERGE (NON-SPECIAL):
// 				// if left sibling doesnt exist, merge with right sibling
// 				// if right sibling doesnt exist, merge with left sibling
// 				// if both exist and both have < MIN_KEYS: is such a state possible in out algo???????

// 				if (left_sibling == nullptr) {
// 					this->mergeHelper(parent, node_to_adjust, right_sibling);
// 				} else {
// 					this->mergeHelper(parent, left_sibling, node_to_adjust);
// 				}
// 			}
// 		}
// 	}
// }


// // PARENT IS NOT IN ROOT CRITICAL CONDTITION
// template <typename K, typenameV, typename CompareFn>
// void BTree<K, V, CompareFn>::mergeHelper(
// 	BTreeNode<K, V, CompareFn>& parent,
// 	BTreeNode<K, V, CompareFn>& left_node,
// 	BTreeNode<K, V, CompareFn>& right_node
// ) {
// 	// left and right is relative order in the nodes to be merged
// 	// ASSSUMING THAT PARENT IS NOT IN ROOT CRITICAL CONDITION!!!!!!!!!

// 	std::list<blocknum_t> blockNumList, blockNumList_new;
// 	std::list<K> parent_key_list, left_key_list, right_sib_key_list;

// 	parent->getKeys(parent_key_list);
// 	left_node->getKeys(left_key_list);
// 	right_node->getKeys(right_sib_key_list);

// 	blocknum_t left_node_block_num = left_node->getBlockNo();


// 	if ( left_node->isLeaf() ) {
// 		std::list<blockOffsetPair> right_block_list;

// 		typename std::list<K>::iterator right_key_iter = right_key_list.begin();
// 		typename std::list<blockOffsetPair>::iterator right_block_iter = right_key_list.begin();

// 		while(right_key_iter != right_key_list.end()) {
// 			left_key_list.push_back(*right_key_iter);
// 			left_block_list.push_back(*right_block_iter);

// 			right_key_iter = right_key_list.erase();
// 			right_block_iter = right_block_list.erase();
// 		}

// 		left_node->setKeys(left_key_list);
// 		left_node->setKeys(left_block_list);


// 	} else {
// 		std::list<blocknum_t> right_block_list;

// 		K extra_key;

// 		typename std::list<K>::iterator right_key_iter = right_key_list.begin();
// 		typename std::list<blocknum_t>::iterator right_block_iter = right_key_list.begin();

// 		//CRITICAL:
// 		//TODO: is it possible to do curr = right_node
// 		BTreeNode<K, V, CompareFn>* curr = right_node, new_node;

// 		while (! curr->isLeaf()) {
// 			new_node = this->getNodeFromBlockNum(
// 				curr->getSmallestKeyBlockNo()
// 			);
// 			delete curr;
// 			curr = new_node;
// 		}

// 		// Extra key is required
// 		extra_key = curr->getSmallestKey();
// 		left_key_list.push_back(extra_key);


// 		while(right_key_iter != right_key_list.end()) {
// 			left_key_list.push_back(*right_key_iter);
// 			left_block_list.push_back(*right_block_iter);

// 			right_key_iter = right_key_list.erase();
// 			right_block_iter = right_block_list.erase();
// 		}

// 		//
// 		left_block_list.push_back(*right_block_iter);
// 		right_block_iter = right_block_list.erase();

// 		left_node->setKeys(left_key_list);
// 		left_node->setKeys(left_block_list);

// 	}

// 	left_node->setNextBlockNo(right_node->getNextBlockNo());

// 	BTreeNode<K, V, CompareFn>* succ_node = this->getNodeFromBlockNum(right_node->getNextBlockNo());
// 	succ_node->setPrevBlockNo(left_node_block_num);

// }

// // finds next_block_number and returns true if remove_key is present in this node
// template <typename K, typename V, typename CompareFn>
// bool InternalNode<K,V,CompareFn>::containsKey(const K& key, blocknum_t& next_block_number) {

// 	std::list<K> keyList;
// 	std::list<blocknum_t> blockList;
// 	this->getKeys(keyList); this->getBlockNumbers(blockList);

// 	typename std::list<K>::iterator key_iter;
// 	typename std::list<blocknum_t>::iterator block_iter;

// 	for (
// 		key_iter = keyList.begin(), block_iter = blockList.begin();
// 		key_iter != keyList.end();
// 		key_iter++, block_iter++
// 	) {
// 		if (cmpl(*key_iter, find_key)) {
// 			 next_block_number = *block_iter;
// 			 return false;
// 		} else if (eq(*key_iter, find_key)) {

// 			// CRITICAL: since we assume that if the key is equal, we search in the 'right' block
// 			// same is followed in insertion
// 			block_iter++;
// 			next_block_number = *block_iter;
// 			return true;
// 		}
// 	}

// 	next_block_number = *block_iter;
// 	return false;
// }


// // replaceKey
// template <typename K, typename V, typename CompareFn>
// bool InternalNode<K,V,CompareFn>::replaceKey(const K& key, blocknum_t next_block_number) {
// 	std::list<K> key_list;
// 	std::list<blocknum_t> block_list;

// 	this->getKeys(key_list); this->getBlockNumbers(block_list);

// 	typename std::list<K>::iterator key_iter;
// 	typename std::list<blocknum_t>::iterator block_iter;

// 	for (
// 		key_iter = key_list.begin(), block_iter = block_list.begin();
// 		key_iter != (key_list).end();
// 		// no increment, read comments below
// 	) {
// 		if (eq(*key_iter, *remove_key) {
// 			key_iter = key_list.erase(key_iter);
// 			key_list.insert(key_iter, replacement_key);
// 			break;
// 		} else if (cmpl(*key_iter, *remove_key)) {
// 			// if key > removeKey
// 			break;
// 		} else {
// 			//if key < remove_key
// 			key_iter++;
// 			block_iter++;
// 		}
// 	}

// 	// TODO: if this is done then we store the node even if num_keys < min_threshold, we may need to change this later
// 	// COMMENT: Deven : We are just replacing, do we really need this TODO here ?
// 	this->setKeys(key_list);
// 	this->setBlockOffsetPairs(block_list);
// }

// // For now assuming this a method of TreeLeafNode
// template <typename K, typename V, typename CompareFn>
// void TreeLeafNode<K, V, CompareFn>::removeKey(const K& remove_key) {
// 	// removes all instances of key K including duplicates

// 	std::list<K> key_list;
// 	std::list<blockOffsetPair> block_pair_list;

// 	this->getKeys(key_list); this->getBlockOffsetPairs(block_pair_list);

// 	typename std::list<K>::iterator key_iter;
// 	typename std::list<blockOffsetPair>::iterator block_iter;

// 	for (
// 		key_iter = key_list.begin(), block_iter = block_pair_list.begin();
// 		key_iter != (key_list).end();
// 		// no increment, read comments below
// 	) {
// 		if (eq(*key_iter, remove_key) {
// 			key_iter = key_list.erase(old_key_iter);
// 			block_iter = block_pair_list.erase(old_block_iter);
// 		} else if (cmpl(*key_iter, remove_key)) {
// 			// if key > remove_key
// 			break;
// 		} else {
// 			//if key < remove_key
// 			key_iter++;
// 			block_iter++;
// 		}
// 	}

// 	//TODO: if this is done then we store the node even if num_keys < min_threshold,
// 	// we may need to change this later
// 	this->setKeys(key_list);
// 	this->setBlockOffsetPairs(block_pair_list);
// }

// /*
// 	DELETETION ENDS
// */
