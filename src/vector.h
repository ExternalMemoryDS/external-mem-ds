#include "buffer2.h"
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
	BufferedFile<int, blocksize_t, blocksize_t>* buffered_file;
	size_type sz;

public:
	class iterator : public std::iterator<std::random_access_iterator_tag, T, size_type, T*, T&> {
	friend class vector;
	private:
		size_type index;
		vector<T>* vec;
	public:
		iterator(size_type i, vector<T>* v) : index(i), vec(v) {}
		iterator(const iterator& it) : index(it.index), vec(it.vec) {}
		iterator(vector<T>* v) : index(0), vec(v) {}
		iterator() : index(0), vec(nullptr) {}
		bool operator== (const iterator& rhs) { return (index == rhs.index) || (index>=vec->sz && rhs.index>=vec->sz) || (index<0 || rhs.index<0); }
		bool operator!= (const iterator& rhs) { return !(operator==(rhs)); }
		T& operator* ();
		iterator& operator++ () { index++; return *this; }
		iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
		iterator& operator-- () { index--; return *this; }
		iterator operator--(int) { iterator tmp(*this); operator--(); return tmp; }
		iterator operator+ (size_type n) { iterator tmp(*this); tmp.index += n; return tmp; }
		iterator operator- (size_type n) { iterator tmp(*this); tmp.index -= n; return tmp; }
		bool operator> (const iterator& rhs) { return index > rhs.index; }
		bool operator< (const iterator& rhs) { return index < rhs.index; }
		bool operator<= (const iterator& rhs);
		bool operator>= (const iterator& rhs);
		iterator& operator+= (size_type n) { index += n; return *this; }
		iterator& operator-= (size_type n) { index -= n; return *this; }
		T& operator[] (size_type n) { return *(*this+n); }
		typename std::iterator<std::random_access_iterator_tag, T, long long int, T*, T&>::difference_type operator- (const iterator& rhs) { return index - rhs.index; }
	};

	class const_iterator : public std::iterator <std::random_access_iterator_tag, T, size_type, T*, T&> {
	friend class vector;
	private:
		size_type index;
		vector<T>* vec;
	public:
		const_iterator(size_type i, vector<T>* v) : index(i), vec(v) {}
		const_iterator(const iterator& it) : index(it.index), vec(it.vec) {}
		const_iterator(vector<T>* v) : index(0), vec(v) {}
		const_iterator() : index(0), vec(nullptr) {}
		bool operator== (const const_iterator& rhs) { return (index == rhs.index) || (index>=vec->sz && rhs.index>=vec->sz) || (index<0 && rhs.index<0); }
		bool operator!= (const const_iterator& rhs) { return !(operator==(rhs)); }
		const T& operator* () const;
		const_iterator& operator++ () { index++; return *this; }
		const_iterator operator++(int) { const_iterator tmp(*this); operator++(); return tmp; }
		const_iterator& operator-- () { index--; return *this; }
		const_iterator operator--(int) { const_iterator tmp(*this); operator--(); return tmp; }
		const_iterator operator+ (size_type n) { const_iterator tmp(*this); tmp.index += n; return tmp; }
		const_iterator operator- (size_type n) { const_iterator tmp(*this); tmp.index -= n; return tmp; }
		bool operator> (const const_iterator& rhs) { return index > rhs.index; }
		bool operator< (const const_iterator& rhs) { return index < rhs.index; }
		bool operator<= (const const_iterator& rhs);
		bool operator>= (const const_iterator& rhs);
		const_iterator& operator+= (size_type n) { index += n; return *this; }
		const_iterator& operator-= (size_type n) { index -= n; return *this; }
		const T& operator[] (size_type n) const { return *(*this+n); }
		typename std::iterator<std::random_access_iterator_tag, T, long long int, T*, T&>::difference_type operator- (const const_iterator& rhs) { return index - rhs.index; }
	};

	class reverse_iterator : public std::iterator <std::random_access_iterator_tag, T, size_type, T*, T&> {
	friend class vector;
	private:
		size_type index;
		vector<T>* vec;
	public:
		reverse_iterator(size_type i, vector<T>* v) : index(i), vec(v) {}
		reverse_iterator(const reverse_iterator& it) : index(it.index), vec(it.vec) {}
		reverse_iterator(vector<T>* v) : index(v->sz-1), vec(v) {}
		reverse_iterator() : index(-1), vec(nullptr) {}
		bool operator== (const reverse_iterator& rhs) { return (index == rhs.index) || (index>=vec->sz && rhs.index>=vec->sz) || (index<0 && rhs.index<0); }
		bool operator!= (const reverse_iterator& rhs) { return !(operator==(rhs)); }
		T& operator* ();
		reverse_iterator& operator++ () { index--; return *this; }
		reverse_iterator operator++(int) { reverse_iterator tmp(*this); operator++(); return tmp; }
		reverse_iterator& operator-- () { index++; return *this; }
		reverse_iterator operator--(int) { reverse_iterator tmp(*this); operator--(); return tmp; }
		reverse_iterator operator+ (size_type n) { reverse_iterator tmp(*this); tmp.index -= n; return tmp; }
		reverse_iterator operator- (size_type n) { reverse_iterator tmp(*this); tmp.index += n; return tmp; }
		bool operator> (const reverse_iterator& rhs) { return index < rhs.index; }		
		bool operator< (const reverse_iterator& rhs) { return index > rhs.index; }		
		bool operator<= (const reverse_iterator& rhs);
		bool operator>= (const reverse_iterator& rhs);
		reverse_iterator& operator+= (size_type n) { index -= n; return *this; }
		reverse_iterator& operator-= (size_type n) { index += n; return *this; }
		T& operator[] (size_type n) { return *(*this-n); }
		typename std::iterator<std::random_access_iterator_tag, T, long long int, T*, T&>::difference_type operator- (const reverse_iterator& rhs) { return -index+rhs.index; }
	};

	vector(const char* pathname, size_type blocksize) : block_size(blocksize), element_size(sizeof(T)),
		sz(0), num_elements_per_block(blocksize/(sizeof(T))) {
		buffered_file = new BufferedFile<int, block_size, block_size*10>(pathname);
		
		// dirty way to decode the header. reading size from header.
		//BufferFrame* header = buffered_file->readHeader();
		//sz = BufferedFrameReader::read<size_type>(header, sizeof(long));
		
		sz = buffered_file->getHeader();
	}
	
	~vector()
	{
		// update size of vector in header.
		//BufferedFrameWriter::write<size_type>(buffered_file->readHeader(), sizeof(long), sz);

		//buffered_file->writeHeader();

		buffered_file->setHeader(&sz);
		buffered_file->flushHeader();
		
		delete buffered_file;
	}
	
	size_type size() { return sz; }
	
	void push_back(const T& elem);
	void pop_back();
	
	void clear();
	void erase(iterator start, iterator end);
	
	void insert(iterator pos, const T& elem);
	
	template <typename InputIterator>
	void insert(iterator pos, InputIterator first, InputIterator last);
	
	T& operator[] (size_type n);
	
	iterator begin();
	iterator end();
	const_iterator cbegin();
	const_iterator cend();
	reverse_iterator rbegin();
	reverse_iterator rend();
};

template <typename T>
void vector<T>::push_back(const T& elem) {
	long block_number = (sz / num_elements_per_block) + 1;
	long block_offset = (sz % num_elements_per_block) * element_size;
	
	FrameData<block_size>* disk_block;
	
	if(block_offset==0) {
		block_t new_block = buffered_file->allotNewBlock();
		disk_block = buffered_file->readBlock(new_block);
	} else {
		disk_block = buffered_file->readBlock(block_number);
	}
	
	//BufferedFrameWriter::write<T>(disk_block, block_offset, elem);
	disk_block[block_offset] = elem;
	
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

template <typename T>
T& vector<T>::operator[] (vector<T>::size_type n) {
	if(n >= sz)
		throw std::out_of_range{"vector<T>::operator[]"};

	long block_number = (n / num_elements_per_block) + 1;
	long block_offset = (n % num_elements_per_block) * element_size;
	
	FrameData<block_size>* buff = buffered_file->readBlock(block_number);
	
	return &(T)buff[block_offset];
}

template <typename T>
void vector<T>::erase(vector<T>::iterator start, vector<T>::iterator end)
{
	size_type first = start.index;
	size_type last = end.index;
	
	if(first > last)
		return;
	
	if(first >= sz || last>=sz)
		throw std::out_of_range{"vector<T>::erase"};
	
	size_type copy_pos = last + 1;
	size_type num_element_shift = sz - copy_pos;
	
	long first_block_number, first_block_offset;
	long copy_block_number, copy_block_offset;
	long new_size = sz - (last-first) -1;
	const void* copy_data;
	
	FrameData<blksize>* disk_block;
	FrameData<blksize>* copy_block;
	
	while(num_element_shift > 0)
	{
		first_block_number = (first / num_elements_per_block) + 1;
		first_block_offset = (first % num_elements_per_block);
		copy_block_number = (copy_pos / num_elements_per_block) + 1;
		copy_block_offset = (copy_pos % num_elements_per_block);
		
		disk_block = buffered_file->readBlock(first_block_number);
		copy_block = buffered_file->readBlock(copy_block_number);
		
		//copy_data = BufferedFrameReader::readRawData(copy_block, copy_block_offset*element_size);
		if((num_elements_per_block-copy_block_offset) <= num_element_shift)
		{
			if((num_elements_per_block-first_block_offset) <= (num_elements_per_block-copy_block_offset))
			{
				//BufferedFrameWriter::memmove(disk_block, copy_data, first_block_offset * element_size, 
				//							 (num_elements_per_block - first_block_offset) * element_size);

				std::memmove(disk_block[first_block_offset*element_size], copy_data[0], (num_elements_per_block - first_block_offset) * element_size);
				//disk_block[first_block_offset*element_size] = (T)copy_block[]
				
				first += (num_elements_per_block - first_block_offset);
				copy_pos += (num_elements_per_block - first_block_offset);
			}
			else
			{
				//BufferedFrameWriter::memmove(disk_block, copy_data, first_block_offset*element_size,
				//							 (num_elements_per_block - copy_block_offset) * element_size);

				std::memmove(disk_block[first_block_offset*element_size], copy_data[0], (num_elements_per_block - copy_block_offset) * element_size);

				first += (num_elements_per_block - copy_block_offset);
				copy_pos += (num_elements_per_block - copy_block_offset);
			}
		}
		else
		{
			if((num_elements_per_block-first_block_offset) <= num_element_shift)
			{
				//BufferedFrameWriter::memmove(disk_block, copy_data, first_block_offset * element_size, 
				//							 (num_elements_per_block - first_block_offset) * element_size);

				std::memmove(disk_block[first_block_offset*element_size], copy_data[0], (num_elements_per_block - first_block_offset) * element_size);
				
				first += (num_elements_per_block - first_block_offset);
				copy_pos += (num_elements_per_block - first_block_offset);
			}
			else
			{
				//BufferedFrameWriter::memmove(disk_block, copy_data, first_block_offset*element_size,
				//							 (num_element_shift) * element_size);

				std::memmove(disk_block[first_block_offset*element_size], copy_data[0], num_element_shift * element_size);

				first += (num_element_shift);
				//BufferedFrameWriter::memset(disk_block, 0, (first % num_elements_per_block) * element_size,
				//							(num_elements_per_block - (first % num_elements_per_block)) * element_size);

				disk_block->parent_frame->markDirty();
				std::memset(disk_block[(first % num_elements_per_block) * element_size], 0, (num_elements_per_block - (first % num_elements_per_block)) * element_size);

				copy_pos += (num_element_shift);
			}
		}
		num_element_shift = sz - copy_pos;
	}
	buffered_file->deleteBlock(first_block_number + 1);
	sz = new_size;
}

template <typename T>
void vector<T>::insert(vector<T>::iterator pos, const T& elem)
{
	size_type position = pos.index;
	
	if(position > sz)
		throw std::out_of_range{"vector<T>::insert()"};
	
	if(position == sz)
	{
		push_back(elem);
		return;
	}
	
	long insert_block_number = (position / num_elements_per_block) + 1;
	long insert_block_offset = (position % num_elements_per_block) * element_size;
	FrameData<blksize>* disk_block = buffered_file->readBlock(insert_block_number);
	
	long last_block_number = (sz / num_elements_per_block) + 1;
	long last_block_offset = (sz % num_elements_per_block) * element_size;
	
	if(last_block_offset == 0)
		last_block_number -= 1;
	
	T overflow_element = (T)disk_block[(num_elements_per_block-1)*element_size];
	
	//BufferedFrameWriter::memmove( disk_block, 
	//										    BufferedFrameReader::readRawData(disk_block, insert_block_offset ), 
	//										    insert_block_offset + element_size, 
	//										    (num_elements_per_block - (position % num_elements_per_block))*element_size );

	std::memmove(disk_block[insert_block_offset + element_size], disk_block[insert_block_offset], (num_elements_per_block - (position % num_elements_per_block))*element_size);

	
	//BufferedFrameWriter::write<T>(disk_block, insert_block_offset, elem);
	disk_block[insert_block_offset] = elem;
	
	insert_block_number++;
	
	T overflow_element2;
	
	while(insert_block_number <= last_block_number)
	{
		disk_block = buffered_file->readBlock(insert_block_number);
		overflow_element2 = (T)disk_block[(num_elements_per_block-1)*element_size];
		
		//BufferedFrameWriter::memmove( disk_block, 
		//											BufferedFrameReader::readRawData(disk_block, 0), 
		//											element_size, (num_elements_per_block-1)*element_size );

		std::memmove(disk_block[element_size], disk_block[0], (num_elements_per_block-1)*element_size);
		
		//BufferedFrameWriter::write<T>(disk_block, 0, overflow_element);
		disk_block[0] = overflow_element;
		
		overflow_element = overflow_element2;
		
		insert_block_number ++;
	}
	
	if(last_block_offset == 0)
	{
		block_t new_block = buffered_file->allotNewBlock();
		disk_block = buffered_file->readBlock(new_block);
		
		//BufferedFrameWriter::write<T>(disk_block, 0, overflow_element);
		disk_block[0] = overflow_element;
	}
	
	sz ++;
}

template <typename T>
template <typename InputIterator>
void vector<T>::insert(vector<T>::iterator pos, InputIterator first, InputIterator last)
{
	size_type position = pos.index;
	if(position > sz)
		throw std::out_of_range{"vector::insert()"};
	
	auto num_element_insert = std::distance(first, last);
	
	size_type new_size = sz + num_element_insert;
	block_t new_last_block = ((new_size/num_elements_per_block) + 1);
	block_t new_last_offset = new_size%num_elements_per_block;
	block_t last_block = (((sz-1)/num_elements_per_block) + 1);	
	
	if(new_last_block != last_block)
	{
		while(new_last_block != buffered_file->allotNewBlock());
		
		FrameData<blksize>* new_disk_block;
		FrameData<blksize>* copy_disk_block;
		
		const void* copy_data;
		
		long copy_position, copy_block, copy_offset;
		
		while(new_last_block > (((position + num_element_insert)/num_elements_per_block)+1))
		{
		
			new_disk_block = buffered_file->readBlock(new_last_block);
		
			copy_position = ((new_last_block - 1)*num_elements_per_block) - num_element_insert;
			copy_block = (copy_position/num_elements_per_block) + 1;
			copy_offset = copy_position%num_elements_per_block;
		
			if(((num_elements_per_block-copy_offset)+1) < new_last_offset)
			{
				copy_disk_block = buffered_file->readBlock(copy_block+1);
				//copy_data = BufferedFrameReader::readRawData(copy_disk_block, 0);
				copy_data = (void*)copy_disk_block[0];
				
				//BufferedFrameWriter::memmove(new_disk_block, copy_data, 
				//										((num_elements_per_block-copy_offset))*element_size,
				//										(new_last_offset-((num_elements_per_block-copy_offset)))*element_size
				//										);
				std::memmove(new_disk_block[(num_elements_per_block-copy_offset)*element_size], copy_data[0], (new_last_offset-(num_elements_per_block-copy_offset))*element_size);
			}
			
			copy_disk_block = buffered_file->readBlock(copy_block);
			//copy_data = BufferedFrameReader::readRawData(copy_disk_block, copy_offset*element_size);
			copy_data = copy_disk_block[copy_offset*element_size];
			//BufferedFrameWriter::memmove(new_disk_block, copy_data, 0,
			//										(num_elements_per_block-copy_offset)*element_size);
			std::memmove(new_disk_block[0], copy_data, (num_elements_per_block-copy_offset)*element_size);
			
			new_last_block--;
			new_last_offset = num_elements_per_block;
		}
		
		new_disk_block = buffered_file->readBlock(new_last_block);
		long new_block_offset = (position + num_element_insert) % num_elements_per_block;
		long num_left_insert = (num_elements_per_block - new_block_offset + 1);
		
		copy_position = ((new_last_block-1)*num_elements_per_block) - num_element_insert + new_block_offset;
		copy_block = (copy_position/num_elements_per_block) + 1;
		copy_offset = copy_position%num_elements_per_block;
		
		if((num_elements_per_block-copy_offset+1) >= num_left_insert)
		{
			copy_disk_block = buffered_file->readBlock(copy_block);
			//copy_data = BufferedFrameReader::readRawData(copy_disk_block, copy_offset*element_size);
			copy_data = copy_disk_block[copy_offset*element_size];
			//BufferedFrameWriter::memmove(new_disk_block, copy_data, 
			//										   new_block_offset*element_size, 
			//										   num_left_insert*element_size);
			std::memmove(new_disk_block[new_block_offset*element_size], copy_data, num_left_insert*element_size);
		}
		else
		{
			copy_disk_block = buffered_file->readBlock(copy_block+1);
			//copy_data = BufferedFrameReader::readRawData(copy_disk_block, 0);
			copy_data = copy_disk_block[0];
			//BufferedFrameWriter::memmove(new_disk_block, copy_data, 
			//										   (new_block_offset + (num_elements_per_block-copy_offset+1))*element_size,
			//										   (num_left_insert - (new_block_offset + (num_elements_per_block-copy_offset+1)))*element_size);
			std::memmove(new_disk_block[(new_block_offset + (num_elements_per_block-copy_offset+1))*element_size], copy_data, (num_left_insert - (new_block_offset + (num_elements_per_block-copy_offset+1)))*element_size);

			copy_disk_block = buffered_file->readBlock(copy_block);
			//copy_data = BufferedFrameReader::readRawData(copy_disk_block, copy_offset*element_size);
			copy_data = copy_disk_block[copy_offset*element_size];
			//BufferedFrameWriter::memmove(new_disk_block, copy_data,
			//										   new_block_offset*element_size,
			//										(num_element_insert-copy_offset+1)*element_size);
			std::memmove(new_disk_block[new_block_offset*element_size], copy_data, (num_element_insert-copy_offset+1)*element_size);
		}
		
		copy_position = position;
		while(first!=last)
		{
			copy_block = (copy_position/num_elements_per_block) + 1;
			copy_offset = (copy_position%num_elements_per_block) * element_size;
			copy_disk_block = buffered_file->readBlock(copy_block);
			//BufferedFrameWriter::write<T>(copy_disk_block, copy_offset,(T)(*first));
			copy_disk_block[copy_offset] = (T)(*first);
			copy_position ++;
			first ++;
		}
		
	}
	else
	{
		const void* copy_data;
		BufferFrame* disk_block = buffered_file->readBlock(last_block);
		//copy_data = BufferedFrameReader::readRawData(disk_block, 
		//														   (position%num_elements_per_block)*element_size);
		copy_data = disk_block[(position%num_elements_per_block)*element_size];
		//BufferedFrameWriter::memmove(disk_block, copy_data, 
		//										   ((position + num_element_insert)%num_elements_per_block)*element_size,
		//										   ((sz%num_elements_per_block) - (position%num_elements_per_block))*element_size);
		std::memmove(disk_block[((position + num_element_insert)%num_elements_per_block)*element_size], copy_data, ((sz%num_elements_per_block) - (position%num_elements_per_block))*element_size)
		while(first!=last)
		{
				//BufferedFrameWriter::write<T>(disk_block, 
				//											(position%num_elements_per_block)*element_size, 
				//											(T)(*first));
				disk_block[(position%num_elements_per_block)*element_size] = (T)(*first));
				position++;
				first++;
		}
	}
	sz += num_element_insert;
}

template <typename T>
typename vector<T>::iterator vector<T>::begin() {
	iterator iter(this);
	return iter;
}

template <typename T>
typename vector<T>::iterator vector<T>::end() {
	iterator iter(this);
	iter.index = sz;
	return iter;
}

template <typename T>
typename vector<T>::const_iterator vector<T>::cbegin() {
	const_iterator iter(this);
	return iter;
}

template <typename T>
typename vector<T>::const_iterator vector<T>::cend() {
	const_iterator iter(this);
	iter.index = sz;
	return iter;
}

template <typename T>
typename vector<T>::reverse_iterator vector<T>::rbegin() {
	reverse_iterator iter(this);
	return iter;
}

template <typename T>
typename vector<T>::reverse_iterator vector<T>::rend() {
	reverse_iterator iter(this);
	iter.index = -1;
	return iter;
}

template <typename T>
T& vector<T>::iterator::operator* () {
	return (*vec)[index];
}

template <typename T>
bool vector<T>::iterator::operator>= (const vector<T>::iterator& rhs) {
	if(index>=vec->sz && rhs.index>=vec->sz)
		return true;

	return index >= rhs.index;
}

template <typename T>
bool vector<T>::iterator::operator<= (const vector<T>::iterator& rhs) {
	if(index<0 && rhs.index<0)
		return true;

	return index <= rhs.index;
}

template <typename T>
const T& vector<T>::const_iterator::operator* () const {
	return (*vec)[index];
}

template <typename T>
bool vector<T>::const_iterator::operator>= (const vector<T>::const_iterator& rhs) {
	if(index>=vec->sz && rhs.index>=vec->sz)
		return true;

	return index >= rhs.index;
}

template <typename T>
bool vector<T>::const_iterator::operator<= (const vector<T>::const_iterator& rhs) {
	if(index<0 && rhs.index<0)
		return true;

	return index <= rhs.index;
}

template <typename T>
T& vector<T>::reverse_iterator::operator* () {
	return (*vec) [index];
}

template <typename T>
bool vector<T>::reverse_iterator::operator>= (const vector<T>::reverse_iterator& rhs) {
	if(index<0 && rhs.index<0)
		return true;

	return index <= rhs.index;
}

template <typename T>
bool vector<T>::reverse_iterator::operator<= (const vector<T>::reverse_iterator& rhs) {
	if((index>=vec->sz && rhs.index>=vec->sz))
		return true;

	return index >= rhs.index;
}
