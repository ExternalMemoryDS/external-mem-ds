#include <type_traits>
#include <iostream>
#include <cstring>

using namespace std;

class BufferFrameIOHelper;

class BufferFrame {
friend class BufferFrameIOHelper;
public:
	char data[256];
	bool is_dirty;
	BufferFrame() : is_dirty(false) { };
	BufferFrameIOHelper operator[] (int offset);
};

class BufferFrameIOHelper {
private:
	int offset;
	BufferFrame* frame;

public:
	BufferFrameIOHelper() : offset(-1), frame(nullptr) { };
	BufferFrameIOHelper(int off, BufferFrame* io_frame) : offset(off), frame(io_frame) {};

	template <typename T>
	BufferFrameIOHelper& operator= (const T& rhs)
	{
		frame->is_dirty = true;
		memcpy(frame->data + offset, &rhs, sizeof(rhs));
	}

	template <typename T>
	T convert_function(true_type) const{
		return ((T)(frame->data + offset));
	}

	template <typename T>
	T convert_function(false_type) const{
		return *((T*)(frame->data + offset));
	}

	template <typename T>
	operator T() {
		if(is_pointer<T>::value) {
			frame->is_dirty = true;
		}
		return convert_function<T> (typename is_pointer<T>::type());
	}
};

struct testA {
	int a;
	int b;
	char c;
};

BufferFrameIOHelper BufferFrame::operator[] (int offset) {
	return BufferFrameIOHelper(offset, this);
}

int main() {
	testA m;
	m.a = 1;
	m.b = 1;
	m.c = 'a';

	BufferFrame fr;
	fr[4] = m;
	cout << ((testA)(fr[4])).c <<endl;
	((testA*)fr[4])->c = 'd';
	cout<<boolalpha;
	cout << fr.is_dirty << endl;

	cout << ((testA)(fr[4])).a <<endl;
	cout << ((testA)(fr[4])).b <<endl;
	cout << ((testA)(fr[4])).c <<endl;
	int j = 4 + sizeof(testA);

	((testA*)fr[j])->a = 1;
	((testA*)fr[j])->b = 5;
	((testA*)fr[j])->c = 'a';

	cout << ((testA)(fr[j])).a <<endl;
	cout << ((testA)(fr[j])).b <<endl;
	cout << ((testA)(fr[j])).c <<endl;

	j = 4 + 2*sizeof(testA);

	fr[j] = 10;
	int z = 2 + ((int)fr[j]);

	cout << z << endl;

	return 0;
}
