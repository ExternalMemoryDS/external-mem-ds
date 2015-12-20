//meta object for B+ Tree, contains meta data
struct meta_obj
{

};

//key node for internal nodes in tree
template<typename T>
struct key_node
{
	T key;
};

//key node for leaf nodes, contains value pointers as offset
template<typename T>
struct key_star_node
{
	T key;
	off_t value;
};

//virtual class, to enable dynamic linking.
//not sure it will be nessesary.
class bptree_node
{

};

//class for internal nodes
template<typename T>
class internal_bptree_node: public bptree_node
{
	int degree;					//degree of the BPTree
	int n;						//current number of keys in node
	key_node<T> *keys;
	bptree_node<T> **children;	//children pointers of type virtual class

public:
	internal_bptree_node(int _degree);
	void insertKey(T _key, off_t _value);
	off_t searchKey(T _key);
	void deleteKey(T _key);
	void splitChild(int index, bptree_node<T> *child);
};

//class for leaf nodes
template<typename T>
class leaf_bptree_node: public bptree_node
{
	int degree;				//degree of the BPTree
	int n;					//current number of keys in node
	key_star_node<T> *keys;

public:
	leaf_bptree_node(int _degree);
	void insertKey(T _key, off_t _value);
	off_t searchKey(T _key);
	void deleteKey(T _key);
};

//class for entire BPTree
template<typename T>
class BPTree
{
	int degree;				//degree of BPTree
	bptree_node<T> * root;	//root of virtual class type

public:
	BPTree(int _degree);
	meta_obj * getMeta();					//return meta object
	void insertElem(T _key, off_t _value);
	off_t searchElem(T _key);
	void deleteElem(T _key);
};