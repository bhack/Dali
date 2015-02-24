#include "Backward.h"

using std::stringstream;
using std::string;
using std::vector;

#define CLIP(X, V) (X).array().min(V).max(-V).matrix()

template<typename T> Backward<T>::Backward (
	shared_mat x,
	shared_mat _out,
	uint _type) : type(_type), matrices(0), out(_out) {
	matrices.emplace_back(x);
}

template<typename T> Backward<T>::Backward (
	shared_mat x,
	shared_mat _out,
	int index,
	uint _type) : type(_type), matrices(0), out(_out), ix(index) {
	matrices.emplace_back(x);
}

template<typename T> Backward<T>::Backward (
	shared_mat x,
	shared_mat _out,
	index_std_vector& _indices,
	uint _type)  : type(_type), out(_out), matrices(0), num_indices(_indices.size()) {
	indices = _indices.data();
	matrices.emplace_back(x);
}

template<typename T> Backward<T>::Backward (
	shared_mat x,
	shared_mat _out,
	eigen_index_block _indices,
	uint _type)  : type(_type), out(_out), matrices(0), num_indices(_indices.rows()) {
	matrices.emplace_back(x);
	indices = _indices.data();
}

template<typename T> Backward<T>::Backward (
	std::initializer_list<shared_mat> _matrices,
	shared_mat _out,
	uint _type)  : type(_type), matrices(_matrices), out(_out) {}

template<typename T> Backward<T>::Backward (
	const vector<shared_mat>& _matrices,
	shared_mat _out,
	uint _type)  : type(_type), matrices(_matrices), out(_out) {}

template<typename T>
std::ostream& operator<<(std::ostream& strm, const Backward<T>& a) {
	// TODO make this output the entire vector of matrices:
	if (a.matrices.size() > 1) {
		return strm << "<#Backward matrix1=" << *(a.matrices[0]) << ", matrix2=" << *(a.matrices[1]) << ", out=" << *(a.out) << ", type=\""<< a.op_type() << "\">";
	}
	return strm << "<#Backward matrix1=" << *(a.matrices[1]) << ", out=" << *(a.out) << ", type=\""<< a.op_type() << "\">";
}

template std::ostream& operator<< <double>(std::ostream& strm, const Backward<double>& a);
template std::ostream& operator<< <float>(std::ostream& strm, const Backward<float>& a);

template<typename T>
string Backward<T>::op_type () const {
	switch(this->type) {
		case utils::ops::add:
			return "add";
		case utils::ops::sub:
			return "sub";
		case utils::ops::eltmul:
			return "eltmul";
		case utils::ops::eltmul_rowwise:
			return "eltmul_rowwise";
		case utils::ops::square:
			return "square";
		case utils::ops::tanh:
			return "tanh";
		case utils::ops::sigmoid:
			return "sigmoid";
		case utils::ops::relu:
			return "relu";
		case utils::ops::mul:
			return "mul";
		case utils::ops::row_pluck:
			return "row_pluck";
		case utils::ops::rows_pluck:
			return "rows_pluck";
		case utils::ops::add_broadcast:
			return "add_broadcast";
		case utils::ops::sub_broadcast:
			return "sub_broadcast";
		case utils::ops::sub_broadcast_reversed:
			return "sub_broadcast_reversed";
		case utils::ops::eltmul_broadcast:
			return "eltmul_broadcast";
		case utils::ops::eltmul_broadcast_rowwise:
			return "eltmul_broadcast_rowwise";
		case utils::ops::mul_with_bias:
			return "mul_with_bias";
		case utils::ops::mul_add_mul_with_bias:
			return "mul_add_mul_with_bias";
		case utils::ops::mul_add_broadcast_mul_with_bias:
			return "mul_add_broadcast_mul_with_bias";
		case utils::ops::transpose:
			return "transpose";
		case utils::ops::sum:
			return "sum";
		case utils::ops::mean:
			return "mean";
		default:
			return "?";
			break;
	}
}

template<typename T>
void Backward<T>::backward_rows_pluck() {
	auto index_ptr = indices;
	for (int i = 0; i < num_indices; ++i) {
		// for each row do the same operation as for row_pluck:
		matrices[0]->dw.row(*index_ptr).noalias() += out->dw.col(i).transpose();
		index_ptr++;
	}
}

template<typename T>
void Backward<T>::backward_mul_add_mul_with_bias() {
	auto bias = matrices.back();
	bias->dw.noalias() += out->dw.rowwise().sum();
	#ifdef DEBUG_RECCURENTJS
		if (bias->n > 200) {
			std::cout << bias->w.col(0).head(20) << std::endl;
			std::cout << bias->dw.col(0).head(20) << std::endl;
			std::cout << "clipping the decoder" << std::endl;
		}
		DEBUG_ASSERT_NOT_NAN(bias->dw);
	#endif
	auto matrices_ptr = matrices.begin();
	while (matrices_ptr != (matrices.end() - 1)) {
		(*matrices_ptr)->dw.noalias()     += (out->dw) * (*(matrices_ptr+1))->w.transpose();
		(*(matrices_ptr+1))->dw.noalias() += (*matrices_ptr)->w.transpose() * (out->dw);
		matrices_ptr+=2;
	}
	/**
	More explicity we are doing this:
	// first multiply:
	matrices[0]->dw.noalias() += (out->dw) * ((matrices[1]->w).transpose());
	matrices[1]->dw.noalias() += matrices[0]->w.transpose() * (out->dw);
	// second multiply:
	matrices[2]->dw.noalias() += (out->dw) * ((matrices[3]->w).transpose());
	matrices[3]->dw.noalias() += matrices[2]->w.transpose() * (out->dw);
	**/
}

template<typename T>
void Backward<T>::backward_rows_pluck(T clip_val) {
	auto index_ptr = indices;
	for (int i = 0; i < num_indices; ++i) {
		// for each row do the same operation as for row_pluck:
		matrices[0]->dw.row(*index_ptr).noalias() += CLIP(out->dw.col(i).transpose(), clip_val);
		index_ptr++;
	}
}

template<typename T>
void Backward<T>::backward_mul_add_mul_with_bias(T clip_val) {
	auto bias = matrices.back();
	bias->dw.noalias() += CLIP(out->dw.rowwise().sum(), clip_val);
	#ifdef DEBUG_RECCURENTJS
		if (bias->n > 200) {
			std::cout << bias->w.col(0).head(20) << std::endl;
			std::cout << bias->dw.col(0).head(20) << std::endl;
			std::cout << "clipping the decoder" << std::endl;
		}
		DEBUG_ASSERT_NOT_NAN(bias->dw);
	#endif

	auto matrices_ptr = matrices.begin();
	while (matrices_ptr != (matrices.end() - 1)) {
		(*matrices_ptr)->dw.noalias()     += CLIP(((out->dw) * (*(matrices_ptr+1))->w.transpose()), clip_val);
		(*(matrices_ptr+1))->dw.noalias() += CLIP(((*matrices_ptr)->w.transpose() * (out->dw)), clip_val);
		matrices_ptr+=2;
	}
	/**
	More explicity we are doing this:
	// first multiply:
	matrices[0]->dw.noalias() += (out->dw) * ((matrices[1]->w).transpose());
	matrices[1]->dw.noalias() += matrices[0]->w.transpose() * (out->dw);
	// second multiply:
	matrices[2]->dw.noalias() += (out->dw) * ((matrices[3]->w).transpose());
	matrices[3]->dw.noalias() += matrices[2]->w.transpose() * (out->dw);
	**/
}

template<typename T>
void Backward<T>::operator ()() {
	switch(this->type) {
		case utils::ops::add:
			for (auto& matrix : matrices) matrix->dw.noalias() += out->dw;
			break;
		case utils::ops::sub:
			matrices[0]->dw.noalias() += out->dw;
			matrices[1]->dw.noalias() -= out->dw;
			break;
		case utils::ops::add_broadcast:
			matrices[0]->dw.noalias() += out->dw;
			matrices[1]->dw.noalias() += out->dw.rowwise().sum();
			break;
		case utils::ops::sub_broadcast:
			matrices[0]->dw.noalias() += out->dw;
			matrices[1]->dw.noalias() -= out->dw.rowwise().sum();
			break;
		case utils::ops::sub_broadcast_reversed:
			matrices[0]->dw.noalias() -= out->dw;
			matrices[1]->dw.noalias() += out->dw.rowwise().sum();
			break;
		case utils::ops::eltmul:
			matrices[0]->dw.noalias() += ((matrices[1]->w).array() * (out->dw).array()).matrix();
			matrices[1]->dw.noalias() += ((matrices[0]->w).array() * (out->dw).array()).matrix();
			break;
		case utils::ops::eltmul_rowwise:
			matrices[0]->dw.noalias() += ((matrices[1]->w).transpose().array() * (out->dw).array()).matrix();
			matrices[1]->dw.noalias() += ((matrices[0]->w).array() * (out->dw).array()).matrix().transpose();
			break;
		case utils::ops::eltmul_broadcast:
			matrices[0]->dw.noalias() += ((out->dw).array().colwise() * (matrices[1]->w).col(0).array()).matrix();
			matrices[1]->dw.noalias() += ((matrices[0]->w).array() * (out->dw).array()).matrix().rowwise().sum();
			break;
		case utils::ops::eltmul_broadcast_rowwise:
			matrices[0]->dw.noalias() += ((out->dw).array().rowwise() * (matrices[1]->w).row(0).array()).matrix();
			matrices[1]->dw.noalias() += (((matrices[0]->w).array() * (out->dw).array()).matrix().colwise().sum()).matrix();
			break;
		case utils::ops::square:
			matrices[0]->dw.noalias() += 2.0 * ((matrices[0]->w).array() * (out->dw).array()).matrix();
			break;
		case utils::ops::sigmoid:
			matrices[0]->dw.noalias() += (((out->w).array() - out->w.array().square()).max(1e-9) * out->dw.array()).matrix();
			break;
		case utils::ops::mul:
			matrices[0]->dw.noalias() += (out->dw) * ((matrices[1]->w).transpose());
			matrices[1]->dw.noalias() += matrices[0]->w.transpose() * (out->dw);
			break;
		case utils::ops::relu:
			matrices[0]->dw.noalias() += (out->w.unaryExpr(utils::sign_operator<T>()).array() * out->dw.array()).matrix();
			break;
		case utils::ops::tanh:
			matrices[0]->dw.noalias() += (out->w.unaryExpr(utils::dtanh_operator<T>()).array() * out->dw.array()).matrix();
			break;
		case utils::ops::row_pluck:
			matrices[0]->dw.row(ix).noalias() += out->dw.col(0).transpose();
			break;
		case utils::ops::rows_pluck:
			// number of rows:
			backward_rows_pluck();
			break;
		case utils::ops::mul_with_bias:
			matrices[0]->dw.noalias() += (out->dw) * ((matrices[1]->w).transpose());
			matrices[1]->dw.noalias() += matrices[0]->w.transpose() * (out->dw);
			matrices[2]->dw.noalias() += out->dw.rowwise().sum().matrix();
			break;
		case utils::ops::mul_add_mul_with_bias:
			backward_mul_add_mul_with_bias();
			break;
		case utils::ops::mul_add_broadcast_mul_with_bias:
			// first multiply:
			// broadcasting input means taking outer product here:
			matrices[0]->dw += ((out->dw).rowwise().sum() * ((matrices[1]->w).transpose()));
			// broadcasting output means sum after the reverse product here:
			matrices[1]->dw.noalias() += (matrices[0]->w.transpose() * (out->dw)).rowwise().sum();
			// second multiply:
			matrices[2]->dw.noalias() += (out->dw) * ((matrices[3]->w).transpose());

			matrices[3]->dw.noalias() += matrices[2]->w.transpose() * (out->dw);
			// bias vector:
			matrices[4]->dw.noalias() += out->dw.rowwise().sum();
			break;
		case utils::ops::transpose:
			matrices[0]->dw.noalias() += (out->dw).transpose();
			break;
		case utils::ops::sum:
			matrices[0]->dw.array() += out->dw(0);
			break;
		case utils::ops::mean:
			matrices[0]->dw.array() += (1.0 / (matrices[0]->n * matrices[0]->d)) * out->dw(0);
			break;
		default:
			stringstream error_msg;
			error_msg << "NotImplemented: Do not know how to backpropagate for this type => "
			   << op_type() << " (" << type << ")";
			throw std::invalid_argument(error_msg.str());
			break;
	}
}


/**
Clipped backpropagation step
**/
template<typename T>
void Backward<T>::operator ()(T clip_val) {
	switch(this->type) {
		case utils::ops::add:
			for (auto& matrix : matrices) matrix->dw.noalias() += CLIP(out->dw, clip_val);
			break;
		case utils::ops::sub:
			matrices[0]->dw.noalias() += CLIP(out->dw, clip_val);
			matrices[1]->dw.noalias() -= CLIP(out->dw, clip_val);
			break;
		case utils::ops::add_broadcast:
			matrices[0]->dw.noalias() += CLIP(out->dw, clip_val);
			matrices[1]->dw.noalias() += CLIP(out->dw.rowwise().sum(), clip_val);
			break;
		case utils::ops::sub_broadcast:
			matrices[0]->dw.noalias() += CLIP(out->dw, clip_val);
			matrices[1]->dw.noalias() -= CLIP(out->dw.rowwise().sum(), clip_val);
			break;
		case utils::ops::sub_broadcast_reversed:
			matrices[0]->dw.noalias() -= CLIP(out->dw, clip_val);
			matrices[1]->dw.noalias() += CLIP(out->dw.rowwise().sum(), clip_val);
			break;
		case utils::ops::eltmul:
			matrices[0]->dw.noalias() += CLIP(((matrices[1]->w).array() * (out->dw).array()).matrix(), clip_val);
			matrices[1]->dw.noalias() += CLIP(((matrices[0]->w).array() * (out->dw).array()).matrix(), clip_val);
			break;
		case utils::ops::eltmul_rowwise:
			matrices[0]->dw.noalias() += CLIP(((matrices[1]->w).transpose().array() * (out->dw).array()).matrix(), clip_val);
			matrices[1]->dw.noalias() += CLIP((((matrices[0]->w).array() * (out->dw).array()).matrix().transpose()), clip_val);
			break;
		case utils::ops::eltmul_broadcast:
			matrices[0]->dw.noalias() += CLIP(((out->dw).array().colwise() * (matrices[1]->w).col(0).array()).matrix(), clip_val);
			matrices[1]->dw.noalias() += CLIP(((matrices[0]->w).array() * (out->dw).array()).matrix().rowwise().sum(), clip_val);
			break;
		case utils::ops::eltmul_broadcast_rowwise:
			matrices[0]->dw.noalias() += CLIP(((out->dw).array().rowwise() * (matrices[1]->w).row(0).array()).matrix(), clip_val);
			matrices[1]->dw.noalias() += CLIP((((matrices[0]->w).array() * (out->dw).array()).matrix().colwise().sum()).matrix(), clip_val);
			break;
		case utils::ops::square:
			matrices[0]->dw.noalias() += CLIP(2.0 * ((matrices[0]->w).array() * (out->dw).array()).matrix(), clip_val);
			break;
		case utils::ops::sigmoid:
			matrices[0]->dw.noalias() += CLIP((((out->w).array() - out->w.array().square()).max(1e-9) * out->dw.array()).matrix(), clip_val);
			break;
		case utils::ops::mul:
			matrices[0]->dw.noalias() += CLIP(((out->dw) * ((matrices[1]->w).transpose())), clip_val);
			matrices[1]->dw.noalias() += CLIP((matrices[0]->w.transpose() * (out->dw)), clip_val);
			break;
		case utils::ops::relu:
			matrices[0]->dw.noalias() += CLIP((out->w.unaryExpr(utils::sign_operator<T>()).array() * out->dw.array()).matrix(), clip_val);
			break;
		case utils::ops::tanh:
			matrices[0]->dw.noalias() += CLIP((out->w.unaryExpr(utils::dtanh_operator<T>()).array() * out->dw.array()).matrix(), clip_val);
			break;
		case utils::ops::row_pluck:
			matrices[0]->dw.row(ix).noalias() += CLIP(out->dw.col(0).transpose(), clip_val);
			break;
		case utils::ops::rows_pluck:
			// number of rows:
			backward_rows_pluck(clip_val);
			break;
		case utils::ops::mul_with_bias:
			matrices[0]->dw.noalias() += CLIP((out->dw) * (matrices[1]->w).transpose(), clip_val);
			matrices[1]->dw.noalias() += CLIP(matrices[0]->w.transpose() * (out->dw), clip_val);
			matrices[2]->dw.noalias() += CLIP(out->dw.rowwise().sum().matrix(), clip_val);
			break;
		case utils::ops::mul_add_mul_with_bias:
			backward_mul_add_mul_with_bias(clip_val);
			break;
		case utils::ops::mul_add_broadcast_mul_with_bias:
			// first multiply:
			// broadcasting input means taking outer product here:
			matrices[0]->dw           += CLIP((out->dw).rowwise().sum() * ((matrices[1]->w).transpose()), clip_val);
			// broadcasting output means sum after the reverse product here:
			matrices[1]->dw.noalias() += CLIP((matrices[0]->w.transpose() * (out->dw)).rowwise().sum(), clip_val);
			// second multiply:
			matrices[2]->dw.noalias() += CLIP(out->dw * (matrices[3]->w).transpose(), clip_val);

			matrices[3]->dw.noalias() += CLIP(matrices[2]->w.transpose() * (out->dw), clip_val);
			// bias vector:
			matrices[4]->dw.noalias() += CLIP(out->dw.rowwise().sum(), clip_val);
			break;
		case utils::ops::transpose:
			matrices[0]->dw.noalias() += CLIP((out->dw).transpose(), clip_val);
			break;
		case utils::ops::sum:
			matrices[0]->dw.array() += out->dw(0);
			break;
		case utils::ops::mean:
			matrices[0]->dw.array() += (1.0 / (matrices[0]->n * matrices[0]->d)) * out->dw(0);
			break;
		default:
			stringstream error_msg;
			error_msg << "NotImplemented: Do not know how to backpropagate for this type => "
			   << op_type() << " (" << type << ")";
			throw std::invalid_argument(error_msg.str());
			break;
	}
}

template class Backward<float>;
template class Backward<double>;
