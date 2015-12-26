#include "buffer.h"

template <typename T>
class vector
{
private:
	const size_t block_size;
	const size_t element_size;
	const size_t num_elements_per_block;
	BufferedFile* buffered_file;
	size_type sz;

public:
	typedef long long int size_type;	

	class iterator : public std::iterator < std::random_access_iterator_tag, T, size_type, T*, T& > {
	friend class vector;
	private:
		size_type index;
	public:
		iterator(size_type i) : index(i) {}
		iterator(const iterator& it) : index(it.index) {}
		iterator() : index(0) {}
		bool operator== (const iterator& rhs) { return (index == rhs.index) || (index>=sz && rhs.index>=sz); }
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
		difference_type operator- (const iterator& rhs) { return index - rhs.index; }
	};

	vector(const char* pathname, size_type blocksize) : block_size(blocksize), element_size(sizeof(T)), 
		sz(0), num_elements_per_block(blocksize/(sizeof(T))) {
		buffered_file = new BufferedFile(blksize, pathname);	
	}
	~vector() { delete buffered_file; }
	void push_back(const T& elem);
	void pop_back();
	void clear();
	void insert(iterator position, const T& val);
	void insert(iterator position, std::InputIterator first, std::InputIterator last);
	void erase(iterator first, iterator last);
	T& operator[] (size_type n);
	size_type size() { return sz; }
	iterator begin();
	iterator end();
	
	// const_iterator cbegin();
	// const_iterator cend();
	// reverse_iterator rbegin();
	// reverse_iterator rend();
};

template <typename T>
T& vector<T>::iterator::operator* () {
	long block_number = (index / num_elements_per_block) + 1;
	long block_offset = (index % num_elements_per_block) * element_size;

	void* buff = malloc(block_size);
	buffered_file.readBlock(block_number, buff);

	//not sure if this is the right way do it. Right now this is just a hack!
	T* tmp_ref = (T*) malloc(element_size);
	char* tmp_buff = (char*) buff;
	memcpy(tmp_ref, tmp_buff + block_offset, sizeof(element_size));

	free(buff);

	return *tmp_ref;
}

template <typename T>
bool vector<T>::iterator::operator>= (const vector<T>::iterator& rhs) {
	if(index>=sz && rhs.index>=sz)
		return true;

	return index >= rhs.index;
}

template <typename T>
bool vector<T>::iterator::operator>= (const vector<T>::iterator& rhs) {
	if(index<0 && rhs.index<0)
		return true;

	return index <= rhs.index;
}

template <typename T>
typename vector<T>::iterator vector<T>::begin() {
	iterator iter;
	return iter;	
}

template <typename T>
typename vector<T>::iterator vector<T>::end() {
	iterator iter;
	iter.index = sz;
	return iter;
}

template <typename T>
T& vector<T>::operator[] (vector<T>::size_type n) {
	if(n >= sz)
		throw out_of_range{"vector<T>::operator[]"};

	long block_number = (n / num_elements_per_block) + 1;
	long block_offset = (n % num_elements_per_block) * element_size;

	void* buff = malloc(block_size);
	buffered_file.readBlock(block_number, buff);

	//not sure if this is the right way do it. Right now this is just a hack!
	T* tmp_ref = (T*) malloc(element_size);
	char* tmp_buff = (char*) buff;
	memcpy(tmp_ref, tmp_buff + block_offset, sizeof(element_size));

	free(buff);

	return *tmp_ref;
}

template <typename T>
void vector<T>::push_back(const T& elem) {
	long block_number = (sz / num_elements_per_block) + 1;
	long block_offset = (sz % num_elements_per_block) * element_size + element_size;

	void* buff = malloc(block_size);

	if((block_offset + element_size) > block_size) {
		long new_block = buffered_file.allotBlock();
		buffered_file.readBlock(new_block, buff);
	} else {
		buffered_file.readBlock(block_number, buff);
	}	

	//not sure if this is the right way do it. Right now this is just a hack!	
	char* tmp_buff = (char*) buff;
	memcpy(tmp_buff + block_offset, &T, sizeof(element_size));

	buffered_file.writeBlock(block_number, buff);

	free(buff);

	sz++;
}

template <typename T>
void vector<T>::pop_back() {
	if(sz>0)
		sz--;

	if( sz % num_elements_per_block == 0 )
	{
		buffered_file.deletePage( (sz/num_elements_per_block) + 1 );
	}
}

template <typename T>
void vector<T>::clear() {
	for(auto i = (sz/num_elements_per_block) + 1; i > 0; i--)
	{
		buffered_file.deletePage(i);
	}
	sz = 0;
}

/* incomplete */
template <typename T>
void vector<T>::insert(vector<T>::iterator position, const T& val) {

	if(position.index > size)
		throw out_of_range{"vector<T>::insert()"};

	long block_number = (position.index / num_elements_per_block) + 1;
	long block_offset = (position.index % num_elements_per_block) * element_size;

	long max_block_number = (sz / num_elements_per_block) + 1;
	long max_block_offset = (sz % num_elements_per_block) * element_size;

	void* buff = malloc(block_size);
	buffered_file.readBlock(block_number, buff);

	void* push_block = malloc(block_size - block_offset);

	char* tmp_buff = (char*) buff;
	memcpy(push_block, tmp_buff + block_offset, block_size - block_offset);
}
