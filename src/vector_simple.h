#include "buffer.h"
#include <iterator>
#include <stdexcept>
#include <stddef.h>

template <typename T>
class vector
{
public:
	typedef long long int size_type;
private:
	const size_t block_size;
	const size_t element_size;
	const size_t num_elements_per_block;
	BufferedFile* buffered_file;
	size_type sz;

public:        
	vector(const char* pathname, size_type blocksize) : block_size(blocksize), element_size(sizeof(T)),
		sz(0), num_elements_per_block(blocksize/(sizeof(T))) {
		buffered_file = new BufferedFile(pathname, block_size, block_size*3);
		
		// dirty way to decode the header. reading size from header.
		const void* tmp_header = buffered_file->readHeader();
		sz = *((size_type*)(((char*)tmp_header) + sizeof(long)));
	}
	
	~vector()
	{
		// update size of vector in header.
		void* tmp_header = malloc(block_size);
		memcpy(tmp_header, buffered_file->readHeader(), block_size);
		//*((size_type*)(((char*)tmp_header) + sizeof(long))) = sz;
		char* tmp_header_char = (char*) tmp_header;
		memcpy(tmp_header_char + sizeof(long), &sz, sizeof(size_type));
		buffered_file->writeHeader(tmp_header);
		free(tmp_header);
		
		delete buffered_file;
		
	}
	
	void push_back(const T& elem);
	void pop_back();
	void clear();
	const T& operator[] (size_type n) const;
	size_type size() { return sz; }
};

template <typename T>
const T& vector<T>::operator[] (vector<T>::size_type n) const {
	if(n >= sz)
		throw std::out_of_range{"vector<T>::operator[]"};

	long block_number = (n / num_elements_per_block) + 1;
	long block_offset = (n % num_elements_per_block) * element_size;

	const void* buff = buffered_file->readBlock(block_number);

	//not sure if this is the right way do it. Right now this is just a hack!	
	char* tmp_buff = (char*) buff;

	return *((T*)(tmp_buff+block_offset));
}

template <typename T>
void vector<T>::push_back(const T& elem) {
	long block_number = (sz / num_elements_per_block) + 1;
	long block_offset = (sz % num_elements_per_block) * element_size;

	const void* disk_block;

	if(block_offset==0) {
		long new_block = buffered_file->allotBlock();
		disk_block = buffered_file->readBlock(new_block);
	} else {
		disk_block = buffered_file->readBlock(block_number);
	}

	//not sure if this is the right way do it. Right now this is just a hack!	
	void* tmp_buff = malloc(block_size);
	memcpy(tmp_buff, disk_block, block_size);
	char* tmp_buff_char = (char*) tmp_buff;
	memcpy(tmp_buff_char + block_offset, &elem, element_size);
	//*((T*) (((char*) tmp_buff) + block_offset)) = elem;

	buffered_file->writeBlock(block_number, tmp_buff);

	free(tmp_buff);

	sz++;
}

template <typename T>
void vector<T>::pop_back() {
	if(sz>0)
		sz--;

	if( sz % num_elements_per_block == 0 )
	{
		buffered_file->deleteBlock( (sz/num_elements_per_block) + 1 );
	}
}

template <typename T>
void vector<T>::clear() {
	for(auto i = (sz/num_elements_per_block) + 1; i > 0; i--)
	{
		buffered_file->deleteBlock(i);
	}
	sz = 0;
}