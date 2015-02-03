#include <fstream>
#include <iterator>
#include <algorithm>
#include <Eigen>
#include "gzstream.h"
#include "StackedGatedModel.h"
#include "OptionParser/OptionParser.h"
using std::vector;
using std::make_shared;
using std::shared_ptr;
using std::ifstream;
using std::istringstream;
using std::string;
using std::min;
using utils::Vocab;
using utils::from_string;

typedef float REAL_t;
typedef LSTM<REAL_t> lstm;
typedef Graph<REAL_t> graph_t;
typedef Layer<REAL_t> classifier_t;
typedef GatedInput<REAL_t> gate_t;
typedef Mat<REAL_t> mat;
typedef shared_ptr<mat> shared_mat;
typedef float price_t;
typedef Eigen::Matrix<uint, Eigen::Dynamic, Eigen::Dynamic> index_mat;
typedef Eigen::Matrix<REAL_t, Eigen::Dynamic, 1> float_vector;
typedef std::tuple<shared_ptr<index_mat>, shared_eigen_index_vector, shared_eigen_index_vector, shared_ptr<float_vector>> databatch_tuple;

class Product {
	public:
		string sku;
		string name;
		vector<string> description;
		vector<string> categories;
		price_t price;
		Product(string _sku, string _name, vector<string> _description, vector<string> _categories, float _price) :
			sku(_sku), name(_name), description(_description), categories(_categories), price(_price) {}
};

std::ostream& operator<<(std::ostream& strm, const Product& product) {
	return strm << "<#Product sku=\""         << product.sku
			           << "\" name=\""        << product.name
			           << "\" description=\"" << product.description
			           << "\" categories="    << product.categories
			           << " price="           << product.price
			           << " >";
}

class Databatch {
	typedef shared_ptr< index_mat > shared_index_mat;
	typedef shared_ptr< float_vector > shared_float_vector;
	public:
		shared_index_mat data;
		shared_eigen_index_vector codelens;
		shared_eigen_index_vector start_loss;
		shared_float_vector prices;
		Databatch(databatch_tuple databatch) {
			data = std::get<0>(databatch);
			codelens = std::get<1>(databatch);
			start_loss = std::get<2>(databatch);
			prices = std::get<3>(databatch);
		};
		Databatch(
			shared_index_mat _data,
			shared_eigen_index_vector _codelens,
			shared_eigen_index_vector _start_loss,
			shared_float_vector _prices) :
				prices(_prices),
				start_loss(_start_loss), 
				data(_data),
				codelens(_codelens) {};
};

void insert_product_indices_into_matrix(
	Vocab& category_vocab,
	Vocab& word_vocab,
	shared_ptr<index_mat>& mat,
	shared_eigen_index_vector& codelens,
	shared_eigen_index_vector& start_loss,
	Product& product,
	size_t& row) {
	auto description_length = product.description.size();
	auto categories_length  = product.categories.size();
	for (size_t j = 0; j < description_length; j++)
		(*mat)(row, j) = word_vocab.word2index.find(product.description[j]) != word_vocab.word2index.end() ? word_vocab.word2index[product.description[j]] : word_vocab.unknown_word;
	(*mat)(row, description_length) = word_vocab.word2index[utils::end_symbol];
	for (size_t j = 0; j < categories_length; j++)
		(*mat)(row, description_length + j + 1) = category_vocab.word2index[product.categories[j]] + word_vocab.word2index.size();
	// **END** for tokens is the next dimension after all the categories (the last one)
	(*mat)(row, description_length + categories_length + 1) = word_vocab.word2index.size() + category_vocab.word2index.size();
	(*codelens)(row)                                        = categories_length + 1;
	(*start_loss)(row)                                      = description_length;
}

databatch_tuple convert_sentences_to_indices(
	vector<Product>& products,
	Vocab& category_vocab,
	Vocab& word_vocab,
	size_t num_elements,
	vector<size_t>::iterator indices,
	vector<size_t>::iterator lengths_sorted) {

	auto indices_begin = indices;
	auto max_len_example = *std::max_element(lengths_sorted, lengths_sorted + num_elements);
	databatch_tuple databatch;
	std::get<0>(databatch) = make_shared<index_mat>(num_elements, max_len_example);
	std::get<1>(databatch) = make_shared<eigen_index_vector>(num_elements);
	std::get<2>(databatch) = make_shared<eigen_index_vector>(num_elements);
	std::get<3>(databatch) = make_shared<float_vector>(num_elements);
	auto data             = std::get<0>(databatch);
	auto codelens         = std::get<1>(databatch);
	auto start_loss       = std::get<2>(databatch);
	auto prices           = std::get<3>(databatch);
	data->fill(0);
	for (size_t k = 0; k < num_elements; k++) {
		(*prices)(k) = products[*indices].price;
		insert_product_indices_into_matrix(
			category_vocab,
			word_vocab,
			data,
			codelens,
			start_loss,
			products[*indices],
			k);
		indices++;
	}
	return databatch;
}

vector<Databatch> create_labeled_dataset(vector<Product>& products,
	Vocab& category_vocab,
	Vocab& word_vocab,
	size_t subpieces) {

	vector<Databatch> dataset;
	vector<size_t> lengths = vector<size_t>(products.size());
	for (size_t i = 0; i != lengths.size(); ++i) lengths[i] = products[i].description.size() + products[i].categories.size() + 2;
	vector<size_t> lengths_sorted(lengths);

	auto shortest = utils::argsort(lengths);
	std::sort(lengths_sorted.begin(), lengths_sorted.end());
	size_t piece_size = ceil(((float)lengths.size()) / (float)subpieces);
	size_t so_far = 0;

	auto shortest_ptr = lengths_sorted.begin();
	auto end_ptr = lengths_sorted.end();
	auto indices_ptr = shortest.begin();

	while (shortest_ptr != end_ptr) {
		dataset.emplace_back( convert_sentences_to_indices(
			products,
			category_vocab,
			word_vocab,
			min(piece_size, lengths.size() - so_far),
			indices_ptr,
			shortest_ptr) );
		shortest_ptr += min(piece_size, lengths.size() - so_far);
		indices_ptr  += min(piece_size, lengths.size() - so_far);
		so_far       = min(so_far + piece_size, lengths.size());
	}
	return dataset;
}

#include <set>
vector<string> get_category_vocabulary(vector<Product>& products) {
	std::set<string> categories;
	string word;
	for (auto& product : products)
		for (auto& category : product.categories)
			categories.insert(category);
	vector<string> list;
	for (auto& key_val : categories)
		list.emplace_back(key_val);
	return list;
}

vector<string> get_vocabulary(vector<Product>& products, int min_occurence) {
	std::unordered_map<string, uint> word_occurences;
	string word;
	for (auto& product : products)
		for (auto& word : product.description) word_occurences[word] += 1;
	vector<string> list;
	for (auto& key_val : word_occurences)
		if (key_val.second >= min_occurence)
			list.emplace_back(key_val.first);
	list.emplace_back(utils::end_symbol);
	return list;
}

template<typename T>
void stream_to_products(T& ostream, vector<Product>& products) {
	string line;
	string sku;
	string name;
	vector<string> description;
	vector<string> categories;
	price_t price;
	int args = 0;
	while (std::getline(ostream, line)) {
		if (args == 0) {
			sku = line;
		} else if (args == 1) {
			name = line;
		} else if (args == 2) {
			istringstream ss(line);
			string word;
			while (ss >> word) description.push_back(word);
		} else if (args == 3) {
			istringstream ss(line);
			string category;
			while (ss >> category) categories.push_back(category);
		} else if (args == 4) {
			istringstream ss(line);
			ss >> price;
		}
		args++;
		if (args == 5) {
			products.emplace_back(sku, name, description, categories, price);
			args = 0;
			categories.clear();
			description.clear();
		}
	}
}

/**
Load products from textfile
into memory, and create a vector of
Product objects.
*/
vector<Product> get_products(const string& filename) {
	vector<Product> products;
	if (utils::is_gzip(filename)) {
		igzstream infilegz(filename.c_str());
		stream_to_products(infilegz, products);
	} else {
		ifstream infile(filename);
		stream_to_products(infile, products);
	}
	return products;
}

template<typename T>
void tuple_sum(std::tuple<T, T>& A, std::tuple<T, T> B) {
	std::get<0>(A) += std::get<0>(B);
	std::get<1>(A) += std::get<1>(B);
}

int main(int argc, char *argv[]) {
	auto parser = optparse::OptionParser()
	    .usage("usage: %prog [dataset_path] [min_occurence] [subsets] [input_size] [epochs] [stack_size] [report_frequency]")
	    .description(
	    	"Sparkfun Dataset Prediction\n"
	    	"---------------------------\n"
	    	"Use StackedLSTMs to predict SparkFun categories in"
	    	" sequential fashion. Moreover, use an Multi Layer Perceptron "
	    	" reading hidden LSTM activations to predict pricing."
	    	" Final network can read product description and predict it's category"
	    	" and price, or provide a topology for the products on SparkFun's website:\n"
	    	" > https://www.sparkfun.com "
	    	"\n"
	    	" @author Jonathan Raiman\n"
	    	" @date January 31st 2015"
	    	);
	parser.set_defaults("subsets", "10");
	parser
		.add_option("-s", "--subsets")
		.help("Break up dataset into how many minibatches ? \n(Note: reduces batch sparsity)").metavar("INT");
	parser.set_defaults("min_occurence", "2");
	parser
		.add_option("-m", "--min_occurence")
		.help("How often a word must appear to be included in the Vocabulary \n"
			"(Note: other words replaced by special **UNKNONW** word)").metavar("INT");
	parser.set_defaults("epochs", "5");
	parser
		.add_option("-e", "--epochs")
		.help("How many training loops through the full dataset ?").metavar("INT");
	parser.set_defaults("input_size", "100");
	parser
		.add_option("-i", "--input_size")
		.help("Size of the word vectors").metavar("INT");
	parser.set_defaults("report_frequency", "1");
	parser
		.add_option("-r", "--report_frequency")
		.help("How often (in epochs) to print the error to standard out during training.").metavar("INT");
	parser.set_defaults("dataset", "sparkfun_dataset.txt");
	parser
		.add_option("-d", "--dataset")
		.help("Where to fetch the product data . "
			"\n(Note: Data format is:\nsku\nname\ndescription\ncategories\nprice)").metavar("FILE");
	parser.set_defaults("stack_size", "4");
	parser
		.add_option("-stack", "--stack_size")
		.help("How many LSTMs should I stack ?").metavar("INT");
	parser.set_defaults("hidden", "100");
	parser
		.add_option("-h", "--hidden")
		.help("How many Cells and Hidden Units should each LSTM have ?").metavar("INT");
	parser.set_defaults("decay_rate", "0.95");
	parser
		.add_option("-decay", "--decay_rate")
		.help("What decay rate should RMSProp use ?").metavar("FLOAT");
	parser.set_defaults("rho", "0.95");
	parser
		.add_option("--rho")
		.help("What rho / learning rate should the Solver use ?").metavar("FLOAT");

	parser.set_defaults("memory_penalty", "0.3");
	parser
		.add_option("--memory_penalty")
		.help("L1 Penalty on Input Gate activation.").metavar("FLOAT");

	parser.set_defaults("save", "");
	parser.add_option("--save")
		.help("Where to save the model to ?").metavar("FOLDER");

	parser.set_defaults("load", "");
	parser.add_option("--load")
		.help("Where to load the model from ?").metavar("FOLDER");

	optparse::Values& options = parser.parse_args(argc, argv);

	int min_occurence    = from_string<int>(options["min_occurence"]);
	int stack_size       = from_string<int>(options["stack_size"]);
	int subsets          = from_string<int>(options["subsets"]);
	int input_size       = from_string<int>(options["input_size"]);
	int epochs           = from_string<int>(options["epochs"]);
	int report_frequency = from_string<int>(options["report_frequency"]);
	int hidden_size      = from_string<int>(options["hidden"]);
	REAL_t rho           = from_string<REAL_t>(options["rho"]);
	REAL_t decay_rate    = from_string<REAL_t>(options["decay_rate"]);
	REAL_t memory_penalty = from_string<REAL_t>(options["memory_penalty"]);
	std::string load_location(options["load"]);
	std::string dataset_path(options["dataset"]);
	std::string save_destination(options["save"]);
	if (min_occurence <= 0) min_occurence = 1;
	if (stack_size <= 0)    stack_size = 1;

	// Collect Dataset from File:
	auto products       = get_products(dataset_path);
	auto index2word     = get_vocabulary(products, min_occurence);
	auto index2category = get_category_vocabulary(products);
	Vocab word_vocab(index2word);
	Vocab category_vocab(index2category, false);
	auto dataset = create_labeled_dataset(products, category_vocab, word_vocab, subsets);
	
	memory_penalty = memory_penalty / dataset[0].data->cols();

	std::cout << "Loaded Dataset"                                    << std::endl;
	std::cout << "Load location         = " << ((load_location != "") ? load_location : "N/A") << std::endl;
	std::cout << "Save location         = " << ((save_destination != "") ? save_destination : "N/A") << std::endl;

	// Construct the model
	auto vocab_size = word_vocab.index2word.size() + index2category.size() + 1;
	auto output_size = index2category.size() + 1;
	auto model = (load_location != "") ?
		StackedGatedModel<REAL_t>::load(load_location) :
		StackedGatedModel<REAL_t>(
			vocab_size,
			input_size,
			hidden_size,
			stack_size,
			output_size,
			memory_penalty);
	if (load_location != "") {
		std::cout << "Loaded Model" << std::endl;
	} else {
		std::cout << "Vocabulary size       = " << index2word.size()     << std::endl;
		std::cout << "Category size         = " << index2category.size() << std::endl;
		std::cout << "Number of Products    = " << products.size()       << std::endl;
		std::cout << "Number of Minibatches = " << dataset.size()        << std::endl;
		std::cout << "Rho                   = " << rho                   << std::endl;
		std::cout << "Memory Penalty        = " << memory_penalty        << std::endl;

		std::cout << "Constructed Stacked LSTMs" << std::endl;
	}

	// Store all parameters in a vector:
	auto parameters = model.parameters();

	//Gradient descent optimizer:
	Solver::AdaDelta<REAL_t> solver(parameters, rho, 1e-9, 5.0);
	// Solver::SGD<REAL_t> solver(5.0);

	// Main training loop:
	for (auto i = 0; i < epochs; ++i) {
		std::tuple<REAL_t, REAL_t> cost(0.0, 0.0);
		for (auto& minibatch : dataset) {
			auto G = graph_t(true);      // create a new graph for each loop
			tuple_sum(cost, model.cost_fun(
				G,
				minibatch.data,               // the sequence to predict
				minibatch.start_loss,
				minibatch.codelens,
				word_vocab.index2word.size()
			));
			// backpropagate
			G.backward();
			// solve it.
			solver.step(parameters, 0.0);
			// solver.step(parameters, rho, 0.0);
		}
		if (i % report_frequency == 0) {
			std::cout << "epoch (" << i << ") KL error = " << std::get<0>(cost)
			                         << ", Memory cost = " << std::get<1>(cost) << std::endl;

			auto& random_batch = dataset[utils::randint(0, std::min(3, (int) dataset.size() - 1))]; 
			auto random_example_index = utils::randint(0, random_batch.data->rows() - 1);
			auto reconstruction = model.reconstruct_fun(
				random_batch.data->row(random_example_index).head((*random_batch.start_loss)(random_example_index) + 1),
				category_vocab,
				(*random_batch.codelens)(random_example_index),
				word_vocab.index2word.size());

			std::cout << "Reconstruction \"";
			for (int j = 0; j < (*random_batch.start_loss)(random_example_index); j++) {
				std::cout << word_vocab.index2word[(*random_batch.data)(random_example_index, j)] << " ";
			}
			std::cout << "\"\n => ";
			for (auto& cat : reconstruction) {
				std::cout << (
					(cat < category_vocab.index2word.size()) ?
						category_vocab.index2word[cat] :
						(
							(cat == category_vocab.index2word.size()) ?
								"**END**" :
								"??"
						)
					) << ", ";
			}
			std::cout << std::endl;
		}
	}

	if (save_destination != "") {
		model.save(save_destination);
		std::cout << "Saved Model in \"" << save_destination << "\"" << std::endl;
	}

	std::cout <<"\n"
	          << "Final Results\n"
	          << "=============\n" << std::endl;
	for (auto& minibatch : dataset) {
		for (int i = 0; i < minibatch.data->rows(); i++) {

			auto reconstruction = model.reconstruct_fun(
				minibatch.data->row(i).head((*minibatch.start_loss)(i) + 1),
				category_vocab,
				(*minibatch.codelens)(i),
				word_vocab.index2word.size());

			std::cout << "Reconstruction \"";
			for (int j = 0; j < (*minibatch.start_loss)(i); j++) {
				std::cout << word_vocab.index2word[(*minibatch.data)(i, j)] << " ";
			}
			std::cout << "\"\n => ";
			for (auto& cat : reconstruction) {
				std::cout << (
					(cat < category_vocab.index2word.size()) ?
						category_vocab.index2word[cat] :
						(
							(cat == category_vocab.index2word.size()) ?
								"**END**" :
								"??"
						)
					) << ", ";
			}
			std::cout << std::endl;
		}

	}


	return 0;
}