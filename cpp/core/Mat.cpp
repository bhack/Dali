#include "Mat.h"

#include "core/MatOps.h"

using namespace Eigen;
using std::vector;
using std::string;
using std::stringstream;
using utils::assert2;
using std::make_shared;

/* MatInternal */

template<typename R>
std::atomic<int> MatInternal<R>::next_matrix(0);

template<typename R>
MatInternal<R>::MatInternal(dim_t n, dim_t d, bool empty) :
        w(n, d),
        dims({n, d}),
        id(next_matrix++) {
    if (empty) {
        w = eigen_mat::Zero(dims[0], dims[1]);
    }
}
template<typename R>
MatInternal<R>::MatInternal(const MatInternal<R>& m) :
        w(m.w),
        dims(m.dims),
        id(m.id) {
}

/* GradInternal */

template<typename R>
GradInternal<R>::GradInternal(dim_t n, dim_t d, bool empty) :
        dw(n, d) {
    if (empty) {
        dw = eigen_mat::Zero(n, d);
    }
}

template<typename R>
GradInternal<R>::GradInternal(const GradInternal<R>& g) :
        dw(g.dw) {
}


/* Mat */

// this does not need to initialize anything once we get rid of w and dw.
template<typename R>
Mat<R>::Mat() : w(NULL, 0, 0), dw(NULL, 0, 0) {
}

template<typename R>
void Mat<R>::point_view_to_internal_memory() {
    // only 2D supported for now.
    assert(dims().size() == 2);
    // Szymon did his research:
    // This is placement new. It does not allocate new memory.
    // It reuses existing memory in place.
    new (&w) eigen_mat_view(m->w.data(), dims(0), dims(1));
    new (&dw) eigen_mat_view(g->dw.data(), dims(0), dims(1));
}

template<typename R>
const vector<dim_t>& Mat<R>::dims() const {
    return m->dims;
}

template<typename R>
const dim_t& Mat<R>::dims(int idx) const {
    return m->dims[idx];
}

template<typename R>
const int& Mat<R>::id() const {
    return m->id;
}

template<typename R>
Mat<R>::Mat (dim_t n, dim_t d, bool empty) : name(nullptr), w(NULL, 0, 0), dw(NULL, 0, 0)  {
    // sometimes we don't need to reset m
    // (for example if it's about to be assigned).
    m = make_shared<MatInternal<R>>(n, d, empty);
    // We always reset the grad calculation
    g = make_shared<GradInternal<R>>(n, d, true);
    point_view_to_internal_memory();

}

template<typename R>
Mat<R>::Mat (string fname) : w(NULL, 0, 0), dw(NULL, 0, 0) {
    // TODO(jonathan): make it work!

    auto arr = cnpy::npy_load(fname);
    vector<uint> npy_dims = {arr.shape[0], arr.shape.size() > 1 ? arr.shape[1] : 1};
    m = make_shared<MatInternal<R>>(npy_dims[0], npy_dims[1], false);
    g = make_shared<GradInternal<R>>(npy_dims[0], npy_dims[1], true);
    point_view_to_internal_memory();

    if (arr.word_size == sizeof(double)) {
        double* loaded_data_double = reinterpret_cast<double*>(arr.data);
        if (arr.fortran_order) {
            Eigen::Map<Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic, Eigen::RowMajor> > wrapped_mat_double_ft(loaded_data_double, dims(0), dims(1));
            w = wrapped_mat_double_ft.cast<R>();
        } else {
            Eigen::Map<Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic, Eigen::ColMajor> > wrapped_mat_double(loaded_data_double, dims(0), dims(1));
            w = wrapped_mat_double.cast<R>();
        }
    } else if (arr.word_size == sizeof(float)) {
        float* loaded_data_float = reinterpret_cast<float*>(arr.data);
        if (arr.fortran_order) {
            Eigen::Map<Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic, Eigen::RowMajor> > wrapped_mat_float_ft(loaded_data_float, dims(0), dims(1));
            w = wrapped_mat_float_ft.cast<R>();
        } else {
            Eigen::Map<Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic, Eigen::ColMajor> > wrapped_mat_float(loaded_data_float, dims(0), dims(1));
            w = wrapped_mat_float.cast<R>();
        }
    } else {
        stringstream error_msg;
        error_msg << "Could not load numpy matrix : \""
           << fname << "\". File dtype (" << arr.word_size << ") not recognized as float or double.";
        throw std::invalid_argument(error_msg.str());
    }
    arr.destruct();
}

template<typename R>
Mat<R>::Mat (dim_t n, dim_t d, R std) :
        Mat<R>(n, d, false) {
    std::default_random_engine generator;
    std::normal_distribution<R> distribution(0.0, std);
    std::random_device rd;
    generator.seed(rd());
    auto randn = [&] (int) {return distribution(generator);};
    w = eigen_mat::NullaryExpr(dims(0), dims(1), randn);
}

template<typename R>
Mat<R>::Mat (dim_t n, dim_t d, R lower, R upper) :
         Mat<R>(n, d, false) {
    std::default_random_engine generator;
    std::uniform_real_distribution<R> distribution(lower, upper);
    std::random_device rd;
    generator.seed(rd());
    auto randn = [&] (int) {return distribution(generator);};
    w = eigen_mat::NullaryExpr(dims(0), dims(1), randn);
}

template<typename R>
Mat<R>::Mat (const Mat<R>& other, bool copy_w, bool copy_dw) :
        name(other.name), w(NULL, 0, 0), dw(NULL, 0, 0) {
    if (copy_w) {
        // This copies memory using copy constructor
        m = make_shared<MatInternal<R>>(*other.m);
    } else {
        // This does not. (only shared_ptr is copied).
        m = other.m;
    }

    if (copy_dw) {
        this->g = make_shared<GradInternal<R>>(*other.g);
    } else {
        g = other.g;
    }
    point_view_to_internal_memory();
}

template<typename R>
Mat<R> Mat<R>::shallow_copy(const Mat<R>& m) {
        return Mat(m, false, true);
}

template<typename R>
void Mat<R>::set_name(string& _name) {
        name = std::make_shared<string>(_name);
}

template<typename R>
void Mat<R>::set_name(char * _name) {
        name = std::make_shared<string>(_name);
}

template<typename R>
void Mat<R>::set_name(const char * _name) {
        name = std::make_shared<string>(_name);
}

template<typename R>
void Mat<R>::print() const {
    for (int i = 0; i < dims(0) ; ++i) {
            std::cout << (i == 0 ? "[" : " ");
            for (int j = 0; j < dims(1); ++j) {
                    std::cout << std::fixed
                              << std::setw( 7 ) // keep 7 digits
                              << std::setprecision( 3 ) // use 3 decimals
                              << std::setfill( ' ' ) // pad values with blanks this->w(i,j)
                              << this->w(i,j) << " ";
            }
            std::cout << (i == dims(0)-1 ? "]" : "\n");
    }
    std::cout << std::endl;
}

template<typename R>
void Mat<R>::grad() {
    assert2(dims(0) == 1 && dims(1) == 1,
            "Grad only works on a \"scalar\" matrix, a 1x1 matrix. "
            "Call G.sum or G.mean before using grad.");
    dw(0) += 1;
}

template<typename R>
void Mat<R>::npy_save (string fname, string mode) {
    cnpy::npy_save(fname, w.data(), dims().data(), dims().size(), mode);
}

template<typename R>
unsigned int Mat<R>::number_of_elements() const {
    unsigned int dim = 1;
    for (auto& n : dims())
        dim *= n;
    return dim;
}



template<typename R>
Mat<R> Mat<R>::eltmul_broadcast(Mat<R> matrix2) {
    return MatOps<R>::eltmul_broadcast(*this, matrix2);
}

template<typename R>
Mat<R> Mat<R>::eltmul(Mat<R> matrix2) {
    return MatOps<R>::eltmul(*this, matrix2);

}


template<typename R>
Mat<R> Mat<R>::eltmul(R alpha) {
    return MatOps<R>::eltmul(*this, alpha);

}


template<typename R>
Mat<R> Mat<R>::eltmul_broadcast_rowwise(
        Mat<R> row_vector) {
    return MatOps<R>::eltmul_broadcast_rowwise(*this, row_vector);

}

template<typename R>
Mat<R> Mat<R>::eltmul_rowwise(
        Mat<R> matrix2) {
    return MatOps<R>::eltmul_rowwise(*this, matrix2);

}

template<typename R>
Mat<R> Mat<R>::add(
        Mat<R> matrix2) {
    return MatOps<R>::add(*this, matrix2);
}


template<typename R>
Mat<R> Mat<R>::sub(
        Mat<R> matrix2) {
    return MatOps<R>::sub(*this, matrix2);

}

template<typename R>
Mat<R> Mat<R>::add_broadcast(Mat<R> matrix2) {
    return MatOps<R>::add_broadcast(*this, matrix2);
}

template<typename R>
Mat<R> Mat<R>::sub_broadcast(Mat<R> matrix2) {
    return MatOps<R>::sub_broadcast(*this, matrix2);
}

template<typename R>
Mat<R> Mat<R>::sub_broadcast_reversed(Mat<R> matrix2) {
    return MatOps<R>::sub_broadcast_reversed(*this, matrix2);
}

template<typename R>
Mat<R> Mat<R>::square() {
    return MatOps<R>::square(*this);
}

template<typename R>
Mat<R> Mat<R>::sigmoid() {
    return MatOps<R>::sigmoid(*this);
}


template<typename R>
Mat<R> Mat<R>::steep_sigmoid(R aggressiveness) {
    return MatOps<R>::steep_sigmoid(*this, aggressiveness);
}

template<typename R>
Mat<R> Mat<R>::sum() {
    return MatOps<R>::sum(*this);
}


template<typename R>
Mat<R> Mat<R>::mean() {
    return MatOps<R>::mean(*this);
}


template<typename R>
Mat<R> Mat<R>::log() {
    return MatOps<R>::log(*this);
}

template<typename R>
Mat<R> Mat<R>::exp() {
    return MatOps<R>::exp(*this);
}

template<typename R>
Mat<R> Mat<R>::T() {
    return MatOps<R>::transpose(*this);
}

template<typename R>
Mat<R> Mat<R>::tanh() {
    return MatOps<R>::tanh(*this);
}

template<typename R>
Mat<R> Mat<R>::relu() {
    return MatOps<R>::relu(*this);
}

template<typename R>
Mat<R> Mat<R>::mul(Mat<R> other) const {
    return MatOps<R>::mul(*this, other);
}

template<typename R>
Mat<R> Mat<R>::rows_pluck(
        Indexing::Index indices) {
    return MatOps<R>::rows_pluck(*this, indices);

}

template<typename R>
Mat<R> Mat<R>::rows_cols_pluck(
        Indexing::Index row_indices,
        Indexing::Index col_indices) {
    return MatOps<R>::rows_cols_pluck(*this, row_indices, col_indices);
}

template<typename R>
Mat<R> Mat<R>::row_pluck(
        int row) {
    return MatOps<R>::row_pluck(*this, row);
}

template<typename R>
void Mat<R>::npy_save (FILE * fp) {
    std::vector<char> header = cnpy::create_npy_header(w.data(),dims().data(),dims().size());
    fwrite(&header[0],sizeof(char),header.size(),fp);
    fwrite(w.data(),sizeof(R), number_of_elements(), fp);
}

template<typename R>
void Mat<R>::npy_load(cnpy::NpyArray& arr) {

    m = make_shared<MatInternal<R>>(arr.shape[0], arr.shape.size() > 1 ? arr.shape[1] : 1);
    point_view_to_internal_memory();

    if (arr.word_size == sizeof(double)) {
        double* loaded_data_double = reinterpret_cast<double*>(arr.data);
        if (arr.fortran_order) {
            Eigen::Map<Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic, Eigen::RowMajor> > wrapped_mat_double_ft(loaded_data_double, dims(0), dims(1));
            w = wrapped_mat_double_ft.cast<R>();
        } else {
            Eigen::Map<Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic, Eigen::ColMajor> > wrapped_mat_double(loaded_data_double, dims(0), dims(1));
            w = wrapped_mat_double.cast<R>();
        }
    } else if (arr.word_size == sizeof(float)) {
        float* loaded_data_float = reinterpret_cast<float*>(arr.data);
        if (arr.fortran_order) {
            Eigen::Map<Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic, Eigen::RowMajor> > wrapped_mat_float_ft(loaded_data_float, dims(0), dims(1));
            w = wrapped_mat_float_ft.cast<R>();
        } else {
            Eigen::Map<Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic, Eigen::ColMajor> > wrapped_mat_float(loaded_data_float, dims(0), dims(1));
            w = wrapped_mat_float.cast<R>();
        }
    } else {
        throw std::invalid_argument("Could not load numpy matrix : not recognized as float or double.");
    }
}

template<typename R>
void Mat<R>::npy_load(FILE * fp) {
    auto arr = cnpy::load_the_npy_file(fp);
    npy_load(arr);
    arr.destruct();
}

template<typename R>
void Mat<R>::npy_load(string fname) {
    auto arr = cnpy::npy_load(fname);
    npy_load(arr);
    arr.destruct();
}

template<typename R>
Mat<R>::~Mat() {}

template<typename R>
Mat<R> Mat<R>::RandMat(dim_t n, dim_t d, R std) {
    // is in fact using C++ 11 's rvalue, move operator,
    // so no copy is made.
    return Mat(n, d, std);
}

template<typename R>
Mat<R> Mat<R>::Empty(dim_t n, dim_t d) {
    // use an empty matrix and modify
    // it so as to not incur the filling
    // with zeros cost.
    return Mat(n, d, true);
}






template<typename R>
std::ostream& operator<<(std::ostream& strm, const Mat<R>& a) {
    if (a.name != 0) {
        return strm << "<#Mat name=\"" << *a.name<< "\" n=" << a.dims(0) << ", d=" << a.dims(1) << ">";
    } else {
        return strm << "<#Mat n=" << a.dims(0) << ", d=" << a.dims(1) << ">";
    }
}

template std::ostream& operator<< <double>(std::ostream& strm, const Mat<double>& a);
template std::ostream& operator<< <float>(std::ostream& strm, const Mat<float>& a);

template <typename R>
std::size_t std::hash<Mat<R>>::operator()(const Mat<R>& k) const {
    return k.id();
}

template std::size_t std::hash<Mat<float>>::operator()(const Mat<float>& k)   const;
template std::size_t std::hash<Mat<double>>::operator()(const Mat<double>& k) const;

template <typename R>
bool operator!=(const Mat<R>& A, const Mat<R>& B) {
    return A.id() != B.id();
}

template bool operator!=(const Mat<float>&, const Mat<float>&);
template bool operator!=(const Mat<double>&, const Mat<double>&);

template <typename R>
bool operator==(const Mat<R>& A, const Mat<R>& B) {
    return A.id() == B.id();
}

template bool operator==<float>(const Mat<float>&, const Mat<float>&);
template bool operator==<double>(const Mat<double>&, const Mat<double>&);

template<typename R>
int argmax(std::shared_ptr<Mat<R>> A) {
    int i = 0;
    R current_max = -std::numeric_limits<R>::infinity();
    auto ptr = A->w.data();
    for (int j = 0; j < A->number_of_elements(); j++) {
        if (*ptr > current_max) {
            current_max = *ptr;
            i = j;
        }
        ptr++;
    }
    return i;
}

template<typename R>
int argmax_slice(std::shared_ptr<Mat<R>> A, int min, int max) {
    int i = 0;
    R current_max = -std::numeric_limits<R>::infinity();
    auto ptr = A->w.data();
    for (int j = min; j < max; j++) {
        if (*ptr > current_max) {
            current_max = *ptr;
            i = j;
        }
        ptr++;
    }
    return i;
}

template int argmax(std::shared_ptr<Mat<float>>);
template int argmax(std::shared_ptr<Mat<double>>);
template int argmax_slice(std::shared_ptr<Mat<float>>, int, int);
template int argmax_slice(std::shared_ptr<Mat<double>>, int, int);

template<typename R>
void utils::save_matrices(vector<std::shared_ptr<Mat<R>>>& parameters, string dirname) {
    utils::ensure_directory(dirname);
    const char * c_dirname = dirname.c_str();
    utils::makedirs(c_dirname);
    int i = 0;
    for (auto& param : parameters) {
        stringstream param_location;
        param_location << dirname << "/param_" << i << ".npy";
        param->npy_save(param_location.str());
        i++;
    }
}

template<typename R>
void utils::load_matrices(vector< std::shared_ptr<Mat<R>> >& parameters, string dirname) {
    utils::ensure_directory(dirname);
    int i = 0;
    for (auto& param : parameters) {
        stringstream param_location;
        param_location << dirname << "/param_" << i << ".npy";
        param->npy_load(param_location.str());
        i++;
    }
}

template void utils::save_matrices(vector< std::shared_ptr<Mat<float>> >&, string);
template void utils::save_matrices(vector< std::shared_ptr<Mat<double>> >&, string);
template void utils::load_matrices(vector< std::shared_ptr<Mat<float>> >&, string);
template void utils::load_matrices(vector< std::shared_ptr<Mat<double>> >&, string);

template class Mat<float>;
template class Mat<double>;
template class MatInternal<float>;
template class MatInternal<double>;
template class GradInternal<float>;
template class GradInternal<double>;
