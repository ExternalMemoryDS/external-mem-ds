#include "buffer.h"

//Type Definitions
typedef long blocknum_t;
typedef long offset_t;
typedef long long int size_type;

const offset_t NODE_TYPE = 0;
const offset_t PARENT_BLOCK = 1;
const offset_t NUM_KEYS = 1 + 1 * sizeof(long);
const offset_t START_KEYS = 1 + 2 * sizeof(long);

const offset_t HEADER_STRING = sizeof(long);
const offset_t DS_ID = 4 + HEADER_STRING;
const offset_t ELEM_VALUE_SIZE = 5 + DS_ID;
const offset_t ROOT_BLK_NUM = sizeof(long) + ELEM_VALUE_SIZE;
const offset_t TOTAL_BLKS = ROOT_BLK_NUM + sizeof(long);
const offset_t DATAFILE = ROOT_BLK_NUM + 2*sizeof(long);
const long Node_type_identifier_size = 1; // in bytes

/*
	Burst Trie = Nodes + Buckets
	Nodes:
		Contain 256 pointers (blocknbr, offset) and other parameters.
		1. Internal Nodes
		2. Leaf Nodes
	Buckets:
		Contains pointers (offset) to strings and the "string\0value"s.

*/


/*

	The B-Trie on the disk will be stored as one file with
	all the internal nodes and leaf nodes (class BtrieNode)
	and another one(bucket) with the remaing part of string
	which is pointed to by the leaves

	The structure of the B-Trie internal nodes file:


	Part of the file                Size            Comments
	==========================================================================
                                    HEADER
                                    ------
	“RMAD”                          4 bytes                 Just for fun! :P
	“BTRIE”                         8 bytes                 Identifies the data structure
	Element Value size              4 bytes                 generally from sizeof(V)
	Root Node block number          sizeof(blockOffsetPair) block no. of the root of BTree
	Total blocks allocated          <temp>                  <temp>
	data_file’s name                32 bytes

                                    Root Node (1 block = M nodes)
                                    -----------------------------
	Node-type indentifier           1 byte
	Parent node block no.           sizeof(long) 	              zero/ minus 1
	No. of keys present             sizeof(long)
	256 keys                        256 * sizeof(K)
	256 pointers                    256 * sizeof(blockOffsetPair)

                                    Internal node (1 block = M nodes)
                                    ---------------------------------
	Node-type identifier            1 byte                        0: Internal, 1: Leaf
	Parent node block no.           sizeof(long)
	No. of keys present             sizeof(long)
	256 keys                        256 * sizeof(K)
	256 pointers                    256 * sizeof(blockOffsetPair)

                                    Leaf node (1 block = M nodes)
                                    -----------------------------
	Node-type identifier            1 byte                        0: Internal, 1: Leaf
	Prev - block no                 sizeof(long)
	Next - block no                 sizeof(long)
	Parent node block no.           sizeof(long)
	No. of keys present             sizeof(long)
	256 keys                        256 * sizeof(K)
	256 pointers                    256 * sizeof(blockOffsetPair)
                                    (i.e. block nos and offsets, but in a different file)

                                    Bucket
                                    ------

*/


struct blockOffsetPair {
	blocknum_t block_number;
	offset_t offset;
};

template <typename V, size_type internal_blksize, size_type data_blksize>
class BTrie
{

private:
	BTrieNode* root;
	BufferedFile *buffered_file_internal, *buffered_file_bucket;

	// no of nodes in a block
	int M;
	int calculateM();

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


	BufferFrame *header = buffered_file_internal->readHeader();
		char read_main_string[5], read_iden[6];

		bool ret_val = true;

		for(int i = 0; i < 4; i++) {
			read_main_string[i] = BufferedFrameReader::read<char>(
				header, HEADER_STRING + i
			);
		}

		for(int i = 0; i < 5; i++) {
			read_iden[i] = BufferedFrameReader::read<char>(
				header, DS_ID + i
			);
		}

		std::string ms(read_main_string);
		std::string ri(read_iden);
		std::string to_write_ms("RMAD");
		std::string to_write_id("BTRIE");

		// this is an un-initialised first time file
		if ( ms != "RMAD"
			|| ri != "BTRIE"
		) {
			for(int i = 0; i < 4; i++) {
				BufferedFrameWriter::write<char>(
					header, HEADER_STRING + i,
					to_write_ms[i]
				);
			}

			for(int i = 0; i < 5; i++) {
				BufferedFrameWriter::write<char>(
					header, DS_ID + i,
					to_write_id[i]
				);
			}


			blocknum_t root_block_num
				= buffered_file_internal->allotBlock();

			this->setRootBlockNo(root_block_num);

			BufferFrame* block = buffered_file_internal->readBlock(
				root_block_num
			);

			this->root_block_num = root_block_num;

			BufferedFrameWriter::write<bool>(
				block,
				NODE_TYPE,
				true
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

		} else {
			// if this is already initialised file
			this->root_block_num
				= BufferedFrameReader::read<blocknum_t>(
					header,
					ROOT_BLK_NUM
				);

			ret_val = false;
		}

protected:
public:
	BTrie(const char* pathname) {
		this->M = calculateM();

		buffered_file_internal = new BufferedFile(pathname, blocksize);

		buffered_file_bucket = new BufferedFile("bucket_file", sizeof(V));

		if (this->headerInit()) {
			// will write it to the appropriate position in the header
			this->setDataFileName(
				"bucket_file"
			);
		}

		// now ready for operation

	};
	~BTrie();

	V search(const char* search_str);
	void insert(const char* insert_str, V new_val);
	void delete(const char* delete_str);
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
