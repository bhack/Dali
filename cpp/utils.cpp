#include "utils.h"

using std::vector;
using std::string;
using std::ifstream;
using std::stringstream;
using std::ofstream;
using std::set;
using std::make_shared;

const char* utils::end_symbol          = "**END**";
const char* utils::unknown_word_symbol = "███████";

std::ostream &operator <<(std::ostream &os, const vector<string> &v) {
   if (v.size() == 0) return os << "[]";
   os << "[\"";
   std::copy(v.begin(), v.end() - 1, std::ostream_iterator<string>(os, "\", \""));
   return os << v.back() << "\"]";
}

std::ostream &operator <<(std::ostream &os, const std::unordered_map<string, uint> &v) {
   if (v.size() == 0) return os << "{}";
   os << "{\n";
   for (auto& kv : v) {
       os << "\"" << kv.first << "\" => " << kv.second << ",\n";
   }
   return os << "}";
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const vector<T>& v) {
	if (v.size() == 0) return os << "[]";
	os << "[";
	for (auto& f : v) 
		os << std::fixed
			          << std::setw( 7 ) // keep 7 digits
			          << std::setprecision( 3 ) // use 3 decimals
			          << std::setfill( ' ' ) // pad values with blanks this->w(i,j)
			          << f << " ";
	return os << "]";
}

template std::ostream& operator<< <double>(std::ostream& strm, const vector<double>& a);
template std::ostream& operator<< <float>(std::ostream& strm, const vector<float>& a);
template std::ostream& operator<< <uint>(std::ostream& strm, const vector<uint>& a);
template std::ostream& operator<< <int>(std::ostream& strm, const vector<int>& a);

namespace utils {

	void ensure_directory (std::string& dirname) {
		if (dirname.back() != '/') dirname += "/";
	}

	vector<string> split(const std::string &s, char delim) {
		std::vector<std::string> elems;
	    std::stringstream ss(s);
	    string item;
	    while (std::getline(ss, item, delim))
	    	if (item != "")
	        	elems.push_back(item);
	    return elems;
	}

	template<typename T>
	void tuple_sum(std::tuple<T, T>& A, std::tuple<T, T> B) {
		std::get<0>(A) += std::get<0>(B);
		std::get<1>(A) += std::get<1>(B);
	}

	template void tuple_sum(std::tuple<float, float>&, std::tuple<float, float>);
	template void tuple_sum(std::tuple<double, double>&, std::tuple<double, double>);
	template void tuple_sum(std::tuple<int, int>&, std::tuple<int, int>);
	template void tuple_sum(std::tuple<uint, uint>&, std::tuple<uint, uint>);

	template<typename T>
	void assert_map_has_key(std::unordered_map<string, T>& map, const string& key) {
		if (map.count(key) < 1) {
			stringstream error_msg;
			error_msg << "Map is missing the following key : \"" << key << "\".";
			throw std::runtime_error(error_msg.str());
		}
	}

	template void assert_map_has_key(std::unordered_map<string, string>&, const string&);
	template void assert_map_has_key(std::unordered_map<string, vector<string>>&, const string&);

	vector<string> split_str(const string& original, const string& delimiter) {
		std::vector<std::string> tokens;
		auto delimiter_ptr = delimiter.begin();
		stringstream ss(original);
		int inside = 0;
		std::vector<char> token;
		char ch;
		while (ss) {
			ch = ss.get();
			if (ch == *delimiter_ptr) {
				delimiter_ptr++;
				inside++;
				if (delimiter_ptr == delimiter.end()) {
					tokens.emplace_back(token.begin(), token.end());
					token.clear();
					inside = 0;
					delimiter_ptr = delimiter.begin();
				}
			} else {
				if (inside > 0) {
					token.insert(token.end(), delimiter.begin(), delimiter_ptr);
					delimiter_ptr = delimiter.begin();
					inside = 0;
				} else {
					token.push_back(ch);
				}
			}
		}
		if (inside > 0) {
			token.insert(token.end(), delimiter.begin(), delimiter_ptr);
			tokens.emplace_back(token.begin(), token.end());
			token.clear();
		} else {
			if (token.size() > 0)
				tokens.emplace_back(token.begin(), token.end()-1);
		}
		return tokens;
	}

	/**
	Text To Map
	-----------

	Read a text file, extract all key value pairs and
	ignore markdown decoration characters such as =, -,
	and #

	Inputs
	------

	std::string fname : the file to read

	Outputs
	-------

	std::unordered_map<string, std::vector<string> > map : the extracted key value pairs.

	**/
	std::unordered_map<string, std::vector<string>> text_to_map(const string& fname) {
		ifstream infile(fname);
		string line;
		const char space = ' ';
		std::unordered_map<string, std::vector<string>> map;
		while (std::getline(infile, line)) {
			if (*line.begin() != '=' && *line.begin() != '-' && *line.begin() != '#') {
				const auto tokens = utils::split(line, space);
				if (tokens.size() > 1) {
					auto ptr = tokens.begin() + 1;
					while( ptr != tokens.end()) {
						map[tokens[0]].emplace_back(*(ptr++));
					}
				}
			}
		}
		return map;
	}

	void map_to_file(const std::unordered_map<string, std::vector<string>>& map, const string& fname) {
		ofstream fp;
		fp.open(fname.c_str(), std::ios::out);
		for (auto& kv : map) {
			fp << kv.first;
			for (auto& v: kv.second)
				fp << " " << v;
			fp << "\n";
		}
	}

	/**	
	Load Labeled Corpus
	-------------------

	Read text file line by line and extract pairs of labeled text
	by splitting on the first space character. Whatever is before
	the first space is the label and is stored second in the returned
	pairs, and after the space is the example, and this is returned
	first in the pairs.

	Inputs
	------

	const std::string& fname : labeled corpus file

	Outputs
	-------

	std::vector<std::pair<std::string, std::string>> pairs : labeled corpus pairs

	**/
	vector<std::pair<string, string>> load_labeled_corpus(const string& fname) {
		ifstream fp(fname.c_str());
		string l;
		const char space = ' ';
		vector<std::pair<string, string>> pairs;
		string::size_type n;
		while (std::getline(fp, l)) {
			n = l.find(space);
			pairs.emplace_back(std::piecewise_construct,
				std::forward_as_tuple(l.begin() + n + 1, l.end()),
				std::forward_as_tuple(l.begin(), l.begin() + n)
			);
		}
		return pairs;
	}

	vector<string> tokenize(const string& s) {
		stringstream ss(s);
		std::istream_iterator<string> begin(ss);
		std::istream_iterator<string> end;
		return vector<string>(begin, end);
	}

	vector<std::pair<vector<string>, string>> load_tokenized_labeled_corpus(const string& fname) {
		ifstream fp(fname.c_str());
		string l;
		const char space = ' ';
		vector<std::pair<vector<string>, string>> pairs;
		string::size_type n;
		while (std::getline(fp, l)) {
			n = l.find(space);
			pairs.emplace_back(std::piecewise_construct,
				std::forward_as_tuple(tokenize(string(l.begin() + n + 1, l.end()))),
				std::forward_as_tuple(l.begin(), l.begin() + n)
			);
		}
		return pairs;

	}

	vector<string> get_vocabulary(const tokenized_labeled_dataset& examples, int min_occurence) {
		std::unordered_map<string, uint> word_occurences;
		string word;
		for (auto& example : examples)
			for (auto& word : example.first) word_occurences[word] += 1;
		vector<string> list;
		for (auto& key_val : word_occurences)
			if (key_val.second >= min_occurence)
				list.emplace_back(key_val.first);
		list.emplace_back(utils::end_symbol);
		return list;
	}

	vector<string> get_label_vocabulary(const tokenized_labeled_dataset& examples) {
		std::set<string> labels;
		string word;
		for (auto& example : examples)
			labels.insert(example.second);
		return vector<string>(labels.begin(), labels.end());
	}

	std::vector<string> get_lattice_vocabulary(const OntologyBranch::shared_branch lattice) {
		std::vector<std::string> index2label;
		index2label.emplace_back(end_symbol);
		for (auto& kv : *lattice->lookup_table)
			index2label.emplace_back(kv.first);
		return index2label;
	}


	// Trimming text from StackOverflow:
	// http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring

	// trim from start
	std::string &ltrim(std::string &s) {
	        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	        return s;
	}

	// trim from end
	std::string &rtrim(std::string &s) {
	        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	        return s;
	}

	// trim from both ends
	std::string &trim(std::string &s) {
	        return ltrim(rtrim(s));
	}

	// From this StackOverflow:
	// http://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
	bool makedirs(const char* path, mode_t mode) {
	  // const cast for hack
	  char* p = const_cast<char*>(path);

	  // Do mkdir for each slash until end of string or error
	  while (*p != '\0') {
	    // Skip first character
	    p++;

	    // Find first slash or end
	    while(*p != '\0' && *p != '/') p++;

	    // Remember value from p
	    char v = *p;

	    // Write end of string at p
	    *p = '\0';

	    // Create folder from path to '\0' inserted at p
	    if(mkdir(path, mode) == -1 && errno != EEXIST) {
	      *p = v;
	      return false;
	    }

	    // Restore path to it's former glory
	    *p = v;
	  }
	  return true;
	}

	/**
	randint
	-------

	Sample integers from a uniform distribution between (and including)
	lower and upper int values.

	Inputs
	------
	int lower
	int upper

	Outputs
	-------

	int sample

	**/
	int randint(int lower, int upper) {
		if (lower == upper) return lower;
		std::default_random_engine generator;
		std::uniform_int_distribution<int> distribution(lower, upper);
		std::random_device rd;
		generator.seed(rd());
		return distribution(generator);
	}

	void Vocab::construct_word2index() {
		uint i = 0;
		for (auto& s : index2word)
			word2index[s] = i++;
	}
	void Vocab::add_unknown_word() {
		index2word.emplace_back(unknown_word_symbol);
		word2index[unknown_word_symbol] = index2word.size() - 1;
		unknown_word = index2word.size() - 1;
	}
	Vocab::Vocab() : unknown_word(-1) {add_unknown_word();};
	Vocab::Vocab(vector<string>& _index2word) : index2word(_index2word), unknown_word(-1) {
		construct_word2index();
		add_unknown_word();
	}
	Vocab::Vocab(vector<string>& _index2word, bool _unknown_word) : index2word(_index2word), unknown_word(-1) {
		construct_word2index();
		if (_unknown_word) add_unknown_word();
	}

	template<typename T>
    T from_string(const std::string& s) {
	    std::istringstream stream (s);
	    T t;
	    stream >> t;
	    return t;
	}

	template float from_string<float>(const std::string& s);
	template int from_string<int>(const std::string& s);
	template uint from_string<uint>(const std::string& s);
	template double from_string<double>(const std::string& s);
	template long from_string<long>(const std::string& s);

	bool is_gzip(const std::string& filename) {
		const unsigned char gzip_code = 0x1f;
		const unsigned char gzip_code2 = 0x8b;
		unsigned char ch;
		std::ifstream file;
		file.open(filename);
		if (!file) return false;
		file.read(reinterpret_cast<char*>(&ch), 1);
		if (ch != gzip_code)
			return false;
		if (!file) return false;
		file.read(reinterpret_cast<char*>(&ch), 1);
		if (ch != gzip_code2)
			return false;
		return true;
	}

	template <typename T>
	vector<size_t> argsort(const vector<T> &v) {
		// initialize original index locations
		vector<size_t> idx(v.size());
		for (size_t i = 0; i != idx.size(); ++i) idx[i] = i;

		// sort indexes based on comparing values in v
		sort(idx.begin(), idx.end(),
		   [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

		return idx;
	}
	template vector<size_t> argsort(const vector<size_t>&v);
	template vector<size_t> argsort(const vector<float>&v);
	template vector<size_t> argsort(const vector<double>&v);
	template vector<size_t> argsort(const vector<int>&v);
	template vector<size_t> argsort(const vector<uint>&v);
	template vector<size_t> argsort(const vector<std::string>&v);

	template<typename T>
	T sigmoid_operator<T>::operator () (T x) const { return 1.0 / (1.0 + exp(-x)); }

	template<typename T>
	T tanh_operator<T>::operator() (T x) const { return std::tanh(x); }

	template<typename T>
	T relu_operator<T>::operator() (T x) const { return std::max(x, (T) 0.0); }

	template<typename T>
	T sign_operator<T>::operator() (T x) const { return x > 0.0 ? 1.0 : 0.0; }

	template<typename T>
	T dtanh_operator<T>::operator() (T x) const { return 1.0 - x*x; }

	template <class T> inline void hash_combine(std::size_t & seed, const T & v) {
	  std::hash<T> hasher;
	  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	std::size_t get_random_id() {
		std::size_t seed = 0;
		std::default_random_engine generator;
		std::random_device rd;
		std::uniform_int_distribution<long> randint(0, std::numeric_limits<long>::max());
		generator.seed(rd());
		hash_combine(seed, randint(generator));
		hash_combine(seed, std::time(NULL));
		return seed;
	}

	template<typename T>
	void assign_cli_argument(char * source, T& target, T default_val, std::string variable_name ) {
		using std::cerr;
		using std::istringstream;
		// Takes an input, a default value, and tries to extract from a character sequence
		// an assignment. If it fails it notifies the user and switches back to the default.
		// Default is copied so a copy is an original is always available
		// for fallback (even if target and default originated from the same place).
		istringstream ss(source);
		if (!(ss >> target)) {
		    cerr << "Invalid " << variable_name << " => \""<< source << "\"\n"
		         << "Using default (" << default_val << ") instead\n";
		    target = default_val;
		}
	}

	template<typename T>
	void assign_cli_argument(char * source, T& target, std::string variable_name ) {
		using std::cerr;
		using std::istringstream;
		T default_val = target;
		// Takes an input, a default value, and tries to extract from a character sequence
		// an assignment. If it fails it notifies the user and switches back to the default.
		// Default is copied so a copy is an original is always available
		// for fallback (even if target and default originated from the same place).
		istringstream ss(source);
		if (!(ss >> target)) {
		    cerr << "Invalid " << variable_name << " => \""<< source << "\"\n"
		         << "Using default (" << default_val << ") instead\n";
		    target = default_val;
		}
	}

	template struct sigmoid_operator<float>;
	template struct tanh_operator<float>;
	template struct relu_operator<float>;
	template struct sign_operator<float>;
	template struct dtanh_operator<float>;
	// template struct clip_operator<float>;

	template void assign_cli_argument<int>(char*,int&,int,std::string);
	template void assign_cli_argument<float>(char*,float&,float,std::string);
	template void assign_cli_argument<double>(char*,double&,double,std::string);
	template void assign_cli_argument<long>(char*,long&,long,std::string);
	template void assign_cli_argument<uint>(char*,uint&,uint,std::string);
	template void assign_cli_argument<std::string>(char*,std::string&, std::string, std::string);

	template void assign_cli_argument<int>(char*,int&,std::string);
	template void assign_cli_argument<float>(char*,float&,std::string);
	template void assign_cli_argument<double>(char*,double&,std::string);
	template void assign_cli_argument<long>(char*,long&,std::string);
	template void assign_cli_argument<uint>(char*,uint&,std::string);
	template void assign_cli_argument<std::string>(char*,std::string&,std::string);

	template struct sigmoid_operator<double>;
	template struct tanh_operator<double>;
	template struct relu_operator<double>;
	template struct sign_operator<double>;
	template struct dtanh_operator<double>;

	void OntologyBranch::save(string fname, std::ios_base::openmode mode) {
		auto hasher = std::hash<OntologyBranch>();
		set<size_t> visited_list;
		vector<shared_branch> open_list;

		ofstream fp;
		fp.open(fname.c_str(), mode);
		open_list.push_back(shared_from_this());

		while (open_list.size() > 0) {
			auto el = open_list[0];
			open_list.erase(open_list.begin());
			if (visited_list.count(hasher(*el)) == 0) {
				visited_list.insert(hasher(*el));
				for (auto& child : el->children) {
					fp << el->name << "->" << child->name << "\n";
					open_list.emplace_back(child);
				}
				for (auto& parent : el->parents)
					open_list.emplace_back(parent.lock());
			}
		}
	}

	void OntologyBranch::compute_max_depth() {
		_max_depth = 0;
		for (auto&v : children)
			if (v->max_depth() + 1 > _max_depth)
				_max_depth = v->max_depth() + 1;
	}

	int& OntologyBranch::max_depth() {
		if (_max_depth > -1) return _max_depth;
		else {
			compute_max_depth();
			return _max_depth;
		}
	}

	std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> OntologyBranch::random_path_to_root(const string& nodename) {
		auto node = lookup_table->at(nodename);
		auto up_node = node;
		uint direction;
		std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> path_pair;
		// path_pair.first = vector<OntologyBranch::shared_branch>();
		while ( &(*up_node) != this) {
			direction = randint(0, up_node->parents.size()-1);
			path_pair.first.emplace_back(up_node);
			path_pair.second.emplace_back(direction);
			up_node = up_node->parents[direction].lock();
		}
		return path_pair;
	}

	std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> OntologyBranch::random_path_to_root(const string& nodename, const int offset) {
		auto node = lookup_table->at(nodename);
		auto up_node = node;
		uint direction;
		std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> path_pair;
		// path_pair.first = vector<OntologyBranch::shared_branch>();
		while ( &(*up_node) != this) {
			direction = randint(0, up_node->parents.size()-1);
			path_pair.first.emplace_back(up_node);
			path_pair.second.emplace_back(direction + offset);
			up_node = up_node->parents[direction].lock();
		}
		return path_pair;
	}


	std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> OntologyBranch::random_path_from_root(const string& nodename) {
		auto node = lookup_table->at(nodename);
		auto up_node = node;
		uint direction;
		std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> path_pair;
		// path_pair.first = vector<OntologyBranch::shared_branch>();
		while ( &(*up_node) != this) {
			direction = randint(0, up_node->parents.size()-1);
			path_pair.first.emplace(path_pair.first.begin(), up_node);
			path_pair.second.emplace(path_pair.second.begin(), direction);
			up_node = up_node->parents[direction].lock();
		}
		return path_pair;
	}

	std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> OntologyBranch::random_path_from_root(const string& nodename, const int offset) {
		auto node = lookup_table->at(nodename);
		auto up_node = node;
		uint direction;
		std::pair<vector<OntologyBranch::shared_branch>, vector<uint>> path_pair;
		// path_pair.first = vector<OntologyBranch::shared_branch>();
		while ( &(*up_node) != this) {
			direction = randint(0, up_node->parents.size()-1);
			path_pair.first.emplace(path_pair.first.begin(), up_node);
			path_pair.second.emplace(path_pair.second.begin(), direction + offset);
			up_node = up_node->parents[direction].lock();
		}
		return path_pair;
	}

	void OntologyBranch::add_lattice_edge(
		const std::string& parent,
		const std::string& child,
		std::shared_ptr<std::map<std::string, OntologyBranch::shared_branch>>& map,
		std::vector<OntologyBranch::shared_branch>& parentless) {
		if (map->count(child) == 0)
			(*map)[child] = make_shared<OntologyBranch>(child);

		if (map->count(parent) == 0) {
			(*map)[parent] = make_shared<OntologyBranch>(parent);
			parentless.emplace_back((*map)[parent]);
		}
		(*map)[child]->add_parent((*map)[parent]);
	}

	std::vector<OntologyBranch::shared_branch> OntologyBranch::load(string fname) {
		auto branch_map = make_shared<std::map<std::string, OntologyBranch::shared_branch>>();
		std::vector<OntologyBranch::shared_branch> roots;
		std::vector<OntologyBranch::shared_branch> parentless;

		ifstream fp;
		fp.open(fname.c_str(), std::ios::in);
		const string right_arrow = "->";
		const string left_arrow = "<-";
		string line;

		while (std::getline(fp, line)) {
			auto tokens = utils::split_str(line, right_arrow);
			if (tokens.size() >= 2) {
				for (int i = 0; i < tokens.size()-1; i++)
					add_lattice_edge(trim(tokens[i]), trim(tokens[i+1]), branch_map, parentless);
			} else {
				tokens = utils::split_str(line, left_arrow);
				if (tokens.size() >= 2)
					for (int i = 0; i < tokens.size()-1; i++)
						add_lattice_edge(trim(tokens[i+1]), trim(tokens[i]), branch_map, parentless);
			}
		}

		for (auto& k : parentless)
			if (k->parents.size() == 0) {
				roots.emplace_back(k);
				k->lookup_table = branch_map;
			}

		return roots;
	}

	OntologyBranch::OntologyBranch(const string& _name) : name(_name), _max_depth(-1) {}

	void OntologyBranch::add_parent(OntologyBranch::shared_branch parent) {
		parents.emplace_back(parent);
		parent->add_child(shared_from_this());
	}

	void OntologyBranch::add_child(OntologyBranch::shared_branch child) {
		children.emplace_back(child);
	}
}

std::ostream& operator<<(std::ostream& strm, const utils::OntologyBranch& a) {
	strm << "<#OntologyBranch name=\"" << a.name << "\"";
	if (a.children.size() > 0) {
		strm << " children={ ";
		for (auto& v : a.children)
			strm  << *v << ", ";
		strm << "}";
	}
	return strm << ">";
}

std::size_t std::hash<utils::OntologyBranch>::operator()(const utils::OntologyBranch& k) const {
	size_t seed = 0;
	std::hash<std::string> str_hasher;
	seed ^= str_hasher(k.name) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}

