#include "MatOps.h"

#include "dali/mat/Index.h"
#include "dali/mat/Mat.h"
#include "dali/mat/Tape.h"

using std::vector;
using std::string;
using utils::assert2;
using utils::MS;

#define GRAD(X) if (!(X).constant) (X).dw()

template<typename R>
Mat<R> MatOps<R>::eltmul_broadcast(
        Mat<R> matrix1,
        Mat<R> matrix2) {
    assert2(matrix1.dims(0) == matrix2.dims(0) && matrix2.dims(1) == 1,
            MS() << "Matrices " << matrix1 << " and " << matrix2
                 << " cannot be element multiplied with broadcast,"
                 << " they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = (matrix1.w().array().colwise() * matrix2.w().col(0).array()).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += ((out.dw()).array().colwise() * (matrix2.w()).col(0).array()).matrix();
            GRAD(matrix2).noalias() += ((matrix1.w()).array() * (out.dw()).array()).matrix().rowwise().sum();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::eltmul(
    Mat<R> matrix1,
    Mat<R> matrix2) {
    if (matrix1.dims(1) != matrix2.dims(1) && (matrix1.dims(1) == 1 || matrix2.dims(1) == 1)) {
        if (matrix1.dims(1) == 1) {
            return eltmul_broadcast(matrix2, matrix1);
        }
        return eltmul_broadcast(matrix1, matrix2);
    }
    assert2(matrix1.dims(0) == matrix2.dims(0) && matrix1.dims(1) == matrix2.dims(1),
            "Matrices cannot be element-wise multiplied, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = (matrix1.w().array() * matrix2.w().array()).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += ((matrix2.w()).array() * (out.dw()).array()).matrix();
            GRAD(matrix2).noalias() += ((matrix1.w()).array() * (out.dw()).array()).matrix();
        });
    return out;
}


template<typename R>
Mat<R> MatOps<R>::eltmul(
    Mat<R> matrix,
    R alpha) {

    auto out = Mat<R>::empty_like(matrix);
    out.w() = (matrix.w().array() * alpha).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, alpha, out]() {
            GRAD(matrix).noalias() += (alpha * (out.dw()).array()).matrix();
        });
    return out;
}


template<typename R>
Mat<R> MatOps<R>::eltmul_broadcast_rowwise(
    Mat<R> matrix1,
    Mat<R> row_vector) {
    if (matrix1.dims(1) != row_vector.dims(1) || row_vector.dims(0) != 1)
        throw std::invalid_argument("Matrices A and B^T cannot be element multiplied with broadcast, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = (matrix1.w().array().rowwise() * row_vector.w().row(0).array()).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, row_vector, out]() {
            GRAD(matrix1).noalias() += ((out.dw()).array().rowwise() * (row_vector.w()).row(0).array()).matrix();
            GRAD(row_vector).noalias() += (((matrix1.w()).array() * (out.dw()).array()).matrix().colwise().sum()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::eltmul_rowwise(
    Mat<R> matrix1,
    Mat<R> matrix2) {

    if (matrix1.dims(0) != matrix2.dims(1) || matrix1.dims(1) != matrix2.dims(0))
        throw std::invalid_argument("Matrices A and B^T cannot be element-wise multiplied, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = (matrix1.w().array() * matrix2.w().transpose().array()).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += ((matrix2.w()).transpose().array() * (out.dw()).array()).matrix();
            GRAD(matrix2).noalias() += ((matrix1.w()).array() * (out.dw()).array()).matrix().transpose();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::add(
        Mat<R> matrix1,
        Mat<R> matrix2) {
    if (matrix1.dims(1) != matrix2.dims(1) && (matrix1.dims(1) == 1 || matrix2.dims(1) == 1)) {
        if (matrix1.dims(1) == 1) {
            return add_broadcast(matrix2, matrix1);
        }
        return add_broadcast(matrix1, matrix2);
    }
    if (matrix1.dims(0) != matrix2.dims(0) || matrix1.dims(1) != matrix2.dims(1))
        throw std::invalid_argument("Matrices cannot be added, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = matrix1.w() + matrix2.w();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += out.dw();
            GRAD(matrix2).noalias() += out.dw();
        });
    return out;
}


template<typename R>
Mat<R> MatOps<R>::sub(
        Mat<R> matrix1,
        Mat<R> matrix2) {
    if (matrix1.dims(1) != matrix2.dims(1) && (matrix1.dims(1) == 1 || matrix2.dims(1) == 1)) {
        if (matrix1.dims(1) == 1) {
            return sub_broadcast_reversed(matrix2, matrix1);
        }
        return sub_broadcast(matrix1, matrix2);
    }
    if (matrix1.dims(0) != matrix2.dims(0) || matrix1.dims(1) != matrix2.dims(1))
        throw std::invalid_argument("Matrices cannot be added, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = matrix1.w() - matrix2.w();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += out.dw();
            GRAD(matrix2).noalias() -= out.dw();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::add(
        Mat<R> matrix1,
        R alpha) {
    auto out = Mat<R>::empty_like(matrix1);
    out.w().array() = matrix1.w().array() + alpha;
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, out]() {
            GRAD(matrix1).noalias() += out.dw();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::add_broadcast(Mat<R> matrix1, Mat<R> matrix2) {
    // broadcast matrix 2:
    if (matrix1.dims(0) != matrix2.dims(0) || matrix2.dims(1) != 1)
            throw std::invalid_argument("Matrices cannot be added with broadcast, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = (matrix1.w().colwise() + matrix2.w().col(0)).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += out.dw();
            GRAD(matrix2).noalias() += out.dw().rowwise().sum();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::sub_broadcast(Mat<R> matrix1, Mat<R> matrix2) {
    // broadcast matrix 2:
    if (matrix1.dims(0) != matrix2.dims(0) || matrix2.dims(1) != 1)
        throw std::invalid_argument("Matrices cannot be substracted with broadcast, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = (matrix1.w().colwise() - matrix2.w().col(0)).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += out.dw();
            GRAD(matrix2).noalias() -= out.dw().rowwise().sum();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::sub_broadcast_reversed(Mat<R> matrix1, Mat<R> matrix2) {
    // broadcast matrix 2:
    if (matrix1.dims(0) != matrix2.dims(0) || matrix2.dims(1) != 1)
        throw std::invalid_argument("Matrices cannot be substracted with broadcast, they do not have the same dimensions.");
    auto out = Mat<R>::empty_like(matrix1);
    out.w() = ((-matrix1.w()).colwise() + matrix2.w().col(0)).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out] () {
            GRAD(matrix1).noalias() -= out.dw();
            GRAD(matrix2).noalias() += out.dw().rowwise().sum();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::add(std::initializer_list<Mat<R>> matrices) {
    auto matrices_vector = vector<Mat<R>>(matrices);
    return add(matrices_vector);
}

template<typename R>
Mat<R> MatOps<R>::add(const std::vector<Mat<R>>& matrices) {
    auto out = Mat<R>::empty_like(*matrices.begin());
    for (auto& matrix : matrices)
        out.w() += matrix.w();
    if (graph::backprop_enabled)
        graph::emplace_back([matrices, out]() {
            for (auto& matrix : matrices) {
                GRAD(matrix).noalias() += out.dw();
            }
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::square(Mat<R> matrix) {
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().array().square();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out]() {
            GRAD(matrix).noalias() += 2.0 * ((matrix.w()).array() * (out.dw()).array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::sqrt(Mat<R> matrix) {
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().array().sqrt();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out]() {
            GRAD(matrix).noalias() += 0.5 * ((out.w()).array().inverse() * (out.dw()).array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::elt_inv(Mat<R> matrix) {
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().array().inverse();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out]() {
            GRAD(matrix).noalias() += -((out.w()).array().square() * (out.dw()).array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::fill(Mat<R> matrix, R filler) {
    auto out = Mat<R>::empty_like(matrix);
    out.w().fill(filler);
    return out;
}

template<typename R>
Mat<R> MatOps<R>::pow(Mat<R> matrix, R other) {
    if (other == (R) -1.0) {
        return MatOps<R>::elt_inv(matrix);
    } else if (other == (R) 0.0){
        return MatOps<R>::fill(matrix, 1.0);
    } else if (other == (R)0.5) {
        return MatOps<R>::sqrt(matrix);
    } else if (other == (R)1.0) {
        return matrix;
    } else if (other == (R)2.0) {
        return MatOps<R>::square(matrix);
    }
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().array().pow(other);
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out, other]() {
            GRAD(matrix).noalias() += other * ((matrix.w()).array().pow(other - 1.0) * (out.dw()).array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::sigmoid(Mat<R> matrix) {
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().unaryExpr(utils::sigmoid_operator<R>());
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out](){
            GRAD(matrix).noalias() += (((out.w()).array() - out.w().array().square()) * out.dw().array()).matrix();
        });
    return out;
}


template<typename R>
Mat<R> MatOps<R>::softmax_no_grad(Mat<R> matrix, R temperature) {
    auto out = Mat<R>::empty_like(matrix);
    auto layer_max = matrix.w().colwise().maxCoeff().array().matrix();
    auto exped_distributions = (matrix.w().rowwise() - layer_max.row(0)).array().exp().matrix();

    auto total_distribution = exped_distributions.colwise().sum().array().matrix();
    out.w() = (exped_distributions.array().rowwise() / total_distribution.row(0).array());
    return out;
}

template<typename R>
Mat<R> MatOps<R>::softmax(Mat<R> matrix, R temperature) {
    Mat<R> out = MatOps<R>::softmax_no_grad(matrix, temperature);
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, temperature, out](){
            GRAD(matrix).noalias() += (((out.w()).array() - out.w().array().square())/temperature * out.dw().array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::steep_sigmoid(Mat<R> matrix, R aggressiveness) {
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().unaryExpr(utils::steep_sigmoid_operator<R>(aggressiveness));
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out, aggressiveness](){
            GRAD(matrix).noalias() += (aggressiveness * ((out.w()).array() - out.w().array().square()) * out.dw().array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::sum(Mat<R> matrix) {
    Mat<R> out (1,1, false);
    out.w()(0) = matrix.w().array().sum();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out]() {
            GRAD(matrix).array() += out.dw()(0);
        });
    return out;
}


template<typename R>
Mat<R> MatOps<R>::mean(Mat<R> matrix) {
    Mat<R> out (1,1, false);
    out.w()(0) = matrix.w().array().mean();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out](){
            GRAD(matrix).array() += (1.0 / (matrix.number_of_elements())) * out.dw()(0);
        });
    return out;
}



template<typename R>
Mat<R> MatOps<R>::sigmoid_binary_cross_entropy(Mat<R> matrix, R t) {
    assert(0 <= t && t <= 1);
    assert(matrix.dims().size() > 1);
    auto out = Mat<R>::empty_like(matrix);

    auto sigmoided_input = std::make_shared<typename Mat<R>::eigen_mat>(
        matrix.w().array().unaryExpr(utils::sigmoid_operator<R>())
    );

    out.w() = -(
                          t  * ( sigmoided_input->array()   + EPS      ).log()
                + ( 1.0 - t) * ( 1.00000001 - sigmoided_input->array() ).log()
    ).matrix();

    if (graph::backprop_enabled)
        graph::emplace_back([matrix, t, out, sigmoided_input](){
            GRAD(matrix).array() += (sigmoided_input->array() - t) * out.dw().array();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::binary_cross_entropy(Mat<R> matrix, R t) {
    assert(0 <= t && t <= 1);
    assert(matrix.dims().size() > 1);
    Mat<R> out =  Mat<R>(
        matrix.dims(0),
        matrix.dims(1),
        false);

    auto x = matrix.w().array();

    out.w() = (-(t * (x + EPS).log() + (1.0-t) * (1.0 - x + EPS).log())).matrix();

    DEBUG_ASSERT_NOT_NAN(out.w());

    if (graph::backprop_enabled)
        graph::emplace_back([matrix, t, out](){
            auto x = matrix.w().array();
            GRAD(matrix).array() += (t-x) / (x*(x- 1.0) + EPS)* out.dw().array();
            DEBUG_ASSERT_NOT_NAN(matrix.dw());
        });

    return out;
}

template<typename R>
Mat<R> MatOps<R>::cross_entropy(Mat<R> matrix, uint answer_idx) {
    DEBUG_ASSERT_BOUNDS(matrix.w(),0.0,1.0 + EPS);
    assert(matrix.dims().size() > 1);
    Mat<R> out =  Mat<R>(1, 1, false);

    auto x = matrix.w().array();

    out.w()(0,0) = - std::log(x(answer_idx, 0) + EPS);

    DEBUG_ASSERT_NOT_NAN(out.w());

    if (graph::backprop_enabled)
        graph::emplace_back([matrix, answer_idx, out](){
            auto x = matrix.w().array();
            GRAD(matrix)(answer_idx, 0) += -1.0/(x(answer_idx, 0) + EPS) * out.dw()(0,0);
        });
    return out;
}


template<typename R>
Mat<R> MatOps<R>::softmax_cross_entropy(Mat<R> matrix, uint answer_idx) {
    Mat<R> out =  Mat<R>(1, 1, false);

    Mat<R> probs = softmax(matrix);
    out.w()(0,0) = -std::log(probs.w()(answer_idx, 0));

    if (graph::backprop_enabled)
        graph::emplace_back([matrix, probs, answer_idx, out](){
            GRAD(matrix) += probs.w()* out.dw()(0,0);
            // write gradients into log probabilities
            GRAD(matrix)(answer_idx, 0) -= 1 * out.dw()(0,0);
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::log(Mat<R> matrix) {
    assert(matrix.dims().size() > 1);
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().array().log();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out](){
            GRAD(matrix).noalias() += ((1.0 / (matrix.w()).array()) * (out.dw()).array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::exp(Mat<R> matrix) {
    assert(matrix.dims().size() > 1);
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().array().exp();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out](){
            GRAD(matrix).noalias() += ((out.w()).array() * (out.dw()).array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::hstack(Mat<R> matrix1, Mat<R> matrix2) {
    if (matrix1.dims(0) != matrix2.dims(0))
        throw std::invalid_argument("Matrices cannot be joined -- they do not have the same number of rows.");
    Mat<R> out (
        matrix1.dims(0),
        matrix1.dims(1) + matrix2.dims(1),
        false
    );
    out.w().block(0,0, matrix1.dims(0), matrix1.dims(1)) = matrix1.w();
    out.w().block(0,matrix1.dims(1), matrix2.dims(0), matrix2.dims(1)) = matrix2.w();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += out.dw().block(0,0, matrix1.dims(0), matrix1.dims(1));
            GRAD(matrix2).noalias() += out.dw().block(0,matrix1.dims(1), matrix2.dims(0), matrix2.dims(1));
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::hstack(std::initializer_list<Mat<R>> matrices) {
    vector<Mat<R>> matrices_vector(matrices);
    return hstack(matrices_vector);
}

template<typename R>
Mat<R> MatOps<R>::hstack(const std::vector<Mat<R>>& matrices) {
    int n = -1;
    int d_total = 0;
    for (auto& mat : matrices) {
        if (n == -1) {
            n = mat.dims(0);
        } else {
            if (mat.dims(0) != n) {
                throw std::invalid_argument("Matrices cannot be joined -- they do not have the same number of rows.");
            }
        }
        d_total+= mat.dims(1);
    }
    Mat<R> out (
        n,
        d_total,
        false
    );
    int offset = 0;
    for (auto& mat : matrices) {
        out.w().block(0, offset, mat.dims(0), mat.dims(1)) = mat.w();
        offset += mat.dims(1);
    }
    if (graph::backprop_enabled)
        graph::emplace_back([matrices, out]() {
            int offset = 0;
            for (auto & mat : matrices) {
                GRAD(mat).noalias() += out.dw().block(0, offset, mat.dims(0), mat.dims(1));
                offset += mat.dims(1);
            }
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::vstack(Mat<R> matrix1, Mat<R> matrix2) {
    if (matrix1.dims(1) != matrix2.dims(1))
        throw std::invalid_argument("Matrices cannot be horizontally stacked -- they do not have the same number of cols.");
    Mat<R> out (
        matrix1.dims(0) + matrix2.dims(0),
        matrix1.dims(1),
        false
    );
    out.w().block(0,0, matrix1.dims(0), matrix1.dims(1)) = matrix1.w();
    out.w().block(matrix1.dims(0),0, matrix2.dims(0), matrix2.dims(1)) = matrix2.w();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out]() {
            GRAD(matrix1).noalias() += out.dw().block(0,0, matrix1.dims(0), matrix1.dims(1));
            GRAD(matrix2).noalias() += out.dw().block(matrix1.dims(0),0, matrix2.dims(0), matrix2.dims(1));
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::vstack(std::initializer_list<Mat<R>> matrices) {
    vector<Mat<R>> matrices_vector(matrices);
    return vstack(matrices_vector);
}

template<typename R>
Mat<R> MatOps<R>::vstack(const std::vector<Mat<R>>& matrices) {
    assert(matrices.size() > 0);
    assert(matrices[0].dims().size() > 1);
    int d = matrices[0].dims(1);
    int n_total = 0;
    for (auto& mat : matrices) {
        if (mat.dims(1) != d) {
            throw std::invalid_argument("Matrices cannot be horizontally stacked -- they do not have the same number of cols.");
        }
        n_total += mat.dims(0);
    }
    Mat<R> out (
        n_total,
        d,
        false
    );
    int offset = 0;
    for (auto& mat : matrices) {
        out.w().block(offset, 0, mat.dims(0), mat.dims(1)) = mat.w();
        offset += mat.dims(0);
    }
    if (graph::backprop_enabled)
        graph::emplace_back([matrices, out]() {
            int offset = 0;
            for (auto & mat : matrices) {
                GRAD(mat).noalias() += out.dw().block(offset,0, mat.dims(0), mat.dims(1));
                offset += mat.dims(0);
            }
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::transpose(Mat<R> matrix) {
    assert(matrix.dims().size() > 1);
    Mat<R> out (
        matrix.dims(1),
        matrix.dims(0),
        false);
    out.w() = matrix.w().transpose();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out](){
            GRAD(matrix).noalias() += (out.dw()).transpose();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::tanh(Mat<R> matrix) {
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().unaryExpr(utils::tanh_operator<R>());
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out](){
            GRAD(matrix).noalias() += (out.w().unaryExpr(utils::dtanh_operator<R>()).array() * out.dw().array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::relu(Mat<R> matrix) {
    auto out = Mat<R>::empty_like(matrix);
    out.w() = matrix.w().unaryExpr(utils::relu_operator<R>());
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out](){
            GRAD(matrix).noalias() += (out.w().unaryExpr(utils::sign_operator<R>()).array() * out.dw().array()).matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::mul(
    Mat<R> matrix1,
    Mat<R> matrix2) {
    if (matrix1.dims(1) != matrix2.dims(0))
        throw std::invalid_argument("matmul dimensions misaligned.");
    Mat<R> out (
        matrix1.dims(0),
        matrix2.dims(1),
        false);
    out.w() = matrix1.w() * matrix2.w();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, out](){
            GRAD(matrix1).noalias() += (out.dw()) * ((matrix2.w()).transpose());
            GRAD(matrix2).noalias() += matrix1.w().transpose() * (out.dw());
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::mul_with_bias(
    Mat<R> matrix1,
    Mat<R> matrix2,
    Mat<R> bias) {
    if (matrix1.dims(1) != matrix2.dims(0))
            throw std::invalid_argument("matmul dimensions misaligned.");
    if (matrix1.dims(0) != bias.dims(0) || bias.dims(1) != 1)
            throw std::invalid_argument("Matrices cannot be added with broadcast, they do not have the same dimensions.");
    Mat<R> out (
            matrix1.dims(0),
            matrix2.dims(1),
            false);
    out.w() = ((matrix1.w() * matrix2.w()).colwise() + bias.w().col(0)).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, matrix2, bias, out]() {
            GRAD(matrix1).noalias() += (out.dw()) * ((matrix2.w()).transpose());
            GRAD(matrix2).noalias() += matrix1.w().transpose() * (out.dw());
            GRAD(bias).noalias()    += out.dw().rowwise().sum().matrix();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::mul_add_broadcast_mul_with_bias(
    Mat<R> matrix1,
    Mat<R> input_to_1,
    Mat<R> matrix2,
    Mat<R> input_to_2,
    Mat<R> bias) {
    if (matrix1.dims(1) != input_to_1.dims(0))
        throw std::invalid_argument("matmul 1 dimensions misaligned.");
    if (matrix2.dims(1) != input_to_2.dims(0))
        throw std::invalid_argument("matmul 2 dimensions misaligned.");
    if (matrix2.dims(0) != bias.dims(0) || matrix1.dims(0) != bias.dims(0) || input_to_1.dims(1) != 1 || bias.dims(1) != 1)
        throw std::invalid_argument("Matrices cannot be added with broadcast, they do not have the same dimensions.");
    Mat<R> out (
            matrix1.dims(0),
            input_to_2.dims(1),
            false);
    // both input to 1 and bias are columns,
    // so we add both of those before adding the true matrix
    // product in broadcasted form
    out.w() = (
          (
              (
                  (matrix2.w() * input_to_2.w())
              )
          ).colwise() + (bias.w()+ (matrix1.w() * input_to_1.w())).col(0)
      ).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, input_to_1, matrix2, input_to_2, bias, out] () {
            // first multiply:
            // broadcasting input means taking outer product here:
            GRAD(matrix1) += ((out.dw()).rowwise().sum() * ((input_to_1.w()).transpose()));
            // broadcasting output means sum after the reverse product here:
            GRAD(input_to_1).noalias() += (matrix1.w().transpose() * (out.dw())).rowwise().sum();
            // second multiply:
            GRAD(matrix2).noalias() += (out.dw()) * ((input_to_2.w()).transpose());

            GRAD(input_to_2).noalias() += matrix2.w().transpose() * (out.dw());
            // bias vector:
            GRAD(bias).noalias() += out.dw().rowwise().sum();
        });
    return out;
}


template<typename R>
Mat<R> MatOps<R>::mul_add_mul_with_bias(std::initializer_list<Mat<R>> matrices) {
    vector<Mat<R>> matrices_vector(matrices);
    return mul_add_mul_with_bias(matrices_vector);
}

template<typename R>
Mat<R> MatOps<R>::mul_add_mul_with_bias(const vector<Mat<R>>& matrices) {
    Mat<R> out(
            matrices[0].dims(0),
            matrices[1].dims(1),
            true);
    DEBUG_ASSERT_MAT_NOT_NAN(out)
    auto matrices_ptr = matrices.begin();
    while (matrices_ptr != (matrices.end() - 1)) {
        DEBUG_ASSERT_MAT_NOT_NAN(*matrices_ptr)
        DEBUG_ASSERT_MAT_NOT_NAN(*(matrices_ptr + 1))
        DEBUG_ASSERT_MAT_NOT_NAN(out)
        out.w() += (*matrices_ptr).w() * (*(matrices_ptr + 1)).w();
        DEBUG_ASSERT_MAT_NOT_NAN(out)
        matrices_ptr+=2;
    }

    DEBUG_ASSERT_NOT_NAN((*(matrices.begin() + matrices.size() - 1)).w());
    out.w().colwise() += (*(matrices.begin() + matrices.size() - 1)).w().col(0);
    if (graph::backprop_enabled)
        graph::emplace_back([matrices, out](){
            auto matrices_ptr = matrices.begin();
            while (matrices_ptr != (matrices.end() - 1)) {
                GRAD((*matrices_ptr)).noalias()   += (out.dw()) * (*(matrices_ptr+1)).w().transpose();
                GRAD(*(matrices_ptr+1)).noalias() += (*matrices_ptr).w().transpose() * (out.dw());
                matrices_ptr+=2;
            }
            auto bias = *(matrices.begin() + matrices.size() - 1);
            GRAD(bias).noalias() += out.dw().rowwise().sum();
        });

    DEBUG_ASSERT_NOT_NAN(out.w());
    return out;
}

// operation of the form (A * x + B * y) + C, called with mul_add_mul_with_bias(A, x, B, y, C)
template<typename R>
Mat<R> MatOps<R>::mul_add_mul_with_bias(
    Mat<R> matrix1,
    Mat<R> input_to_1,
    Mat<R> matrix2,
    Mat<R> input_to_2,
    Mat<R> bias) {
    DEBUG_ASSERT_NOT_NAN(bias.w());
    if (matrix1.dims(1) != input_to_1.dims(0))
        throw std::invalid_argument("matmul 1 dimensions misaligned.");
    if (matrix2.dims(1) != input_to_2.dims(0))
        throw std::invalid_argument("matmul 2 dimensions misaligned.");
    if (matrix2.dims(0) != bias.dims(0) || matrix1.dims(0) != bias.dims(0) || bias.dims(1) != 1)
        throw std::invalid_argument("Matrices cannot be added with broadcast, they do not have the same dimensions.");
    if (input_to_1.dims(1) != input_to_2.dims(1)) {
        if (input_to_1.dims(1) == 1) {
            return mul_add_broadcast_mul_with_bias(matrix1, input_to_1, matrix2, input_to_2, bias);
        }
        return mul_add_broadcast_mul_with_bias(matrix2, input_to_2, matrix1, input_to_1, bias);
    }
    Mat<R> out (
            matrix1.dims(0),
            input_to_1.dims(1),
            false);
    out.w() = (
                  (
                      (
                          (matrix1.w() * input_to_1.w()) +
                          (matrix2.w() * input_to_2.w())
                      )
                  ).colwise() + bias.w().col(0)
              ).matrix();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix1, input_to_1, matrix2, input_to_2, bias, out](){
            // first multiply:
            // broadcasting input means taking outer product here:
            GRAD(matrix1)               += (out.dw() * (input_to_1.w()).transpose());
            // broadcasting output means sum after the reverse product here:
            GRAD(input_to_1).noalias() += matrix1.w().transpose() * (out.dw());
            // second multiply:
            GRAD(matrix2).noalias()     += (out.dw()) * (input_to_2.w()).transpose();

            GRAD(input_to_2).noalias()  += matrix2.w().transpose() * (out.dw());
            // bias vector:
            GRAD(bias).noalias()        += out.dw().rowwise().sum();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::rows_pluck(
        Mat<R> matrix,
        Indexing::Index indices
        ) {
    Mat<R> out (
        matrix.dims(1),
        indices.size(),
        false);

    for (std::size_t offset = 0; offset < indices.size(); ++offset) {
        out.w().col(offset) = matrix.w().row(indices[offset]).transpose();
    }
    if (graph::backprop_enabled) {
        graph::emplace_back([matrix, out, indices](){
            auto index_ptr = indices.data();
            for (std::size_t i = 0; i < out.dims(1); ++i) {
                // for each row do the same operation as for row_pluck:
                GRAD(matrix).row(*index_ptr).noalias() += out.dw().col(i).transpose();
                index_ptr++;
            }
        });
    }
    return out;
}

template<typename R>
Mat<R> MatOps<R>::dropout(
    Mat<R> matrix,
    R drop_prob) {

    assert(0.0 <= drop_prob && drop_prob <= 1.0);

    // no dropout happens.
    if (drop_prob < 1e-6)
        return matrix;

    auto out = Mat<R>::empty_like(matrix);

    auto bool_mat = std::make_shared<Eigen::Matrix<R, Eigen::Dynamic, Eigen::Dynamic>>(matrix.dims(0), matrix.dims(1));

    std::default_random_engine generator;
    std::bernoulli_distribution distribution(1.0 - drop_prob);
    std::random_device rd;
    generator.seed(rd());

    auto data_ptr = matrix.w().data();
    auto out_ptr  = out.w().data();
    auto bool_ptr = bool_mat->data();

    for (int i = 0; i < matrix.number_of_elements();++i) {
        (*bool_ptr) = distribution(generator) ? 1.0 : 0.0;
        (*out_ptr) = (*bool_ptr) > 0 ? *data_ptr : 0.0;
        out_ptr++;
        data_ptr++;
        bool_ptr++;
    }

    if (graph::backprop_enabled) {
        graph::emplace_back([matrix, out, bool_mat](){
            GRAD(matrix) += (out.dw().array() * (*bool_mat).array()).matrix();
        });
    }
    return out;
}

template<typename R>
Mat<R> MatOps<R>::dropout_normalized(
    Mat<R> matrix,
    R drop_prob) {

    assert(0.0 <= drop_prob && drop_prob <= 1.0);

    // no dropout happens.
    if (drop_prob < 1e-6)
        return matrix;

    auto out = Mat<R>::empty_like(matrix);

    auto bool_mat = std::make_shared<Eigen::Matrix<R, Eigen::Dynamic, Eigen::Dynamic>>(matrix.dims(0), matrix.dims(1));

    std::default_random_engine generator;
    std::bernoulli_distribution distribution(1.0 - drop_prob);
    std::random_device rd;
    generator.seed(rd());

    auto data_ptr = matrix.w().data();
    auto out_ptr  = out.w().data();
    auto bool_ptr = bool_mat->data();

    R normalized_drop_prob = 1.0 / (1.0 - drop_prob);
    for (unsigned int i = 0; i < matrix.number_of_elements();++i) {
        (*bool_ptr) = distribution(generator) ? normalized_drop_prob : 0.0;
        (*out_ptr) = (*bool_ptr) > 0 ? *data_ptr : 0.0;
        out_ptr++;
        data_ptr++;
        bool_ptr++;
    }

    if (graph::backprop_enabled) {
        graph::emplace_back([matrix, out, bool_mat](){
            GRAD(matrix) += (out.dw().array() * (*bool_mat).array()).matrix();
        });
    }
    return out;
}

template<typename R>
Mat<R> MatOps<R>::fast_dropout(Mat<R> matrix) {
    auto out = Mat<R>::empty_like(matrix);

    auto randn_mat = std::make_shared<Eigen::Matrix<R, Eigen::Dynamic, Eigen::Dynamic>>(matrix.dims(0), matrix.dims(1));

    std::default_random_engine generator;
    std::normal_distribution<R> distribution(1.0, 1.0);
    std::random_device rd;
    generator.seed(rd());

    auto data_ptr = matrix.w().data();
    auto out_ptr  = out.w().data();
    auto randn_ptr = randn_mat->data();

    for (unsigned int i = 0; i < matrix.number_of_elements();++i) {
        (*randn_ptr) = distribution(generator);
        (*out_ptr) = (*randn_ptr) * *data_ptr;
        out_ptr++;
        data_ptr++;
        randn_ptr++;
    }

    if (graph::backprop_enabled) {
        graph::emplace_back([matrix, out, randn_mat](){
            GRAD(matrix) += (out.dw().array() * (*randn_mat).array()).matrix();
        });
    }
    return out;
}

template<typename R>
Mat<R> MatOps<R>::rows_cols_pluck(
        Mat<R> matrix,
        Indexing::Index row_indices,
        Indexing::Index col_indices) {
    if (row_indices.size() != col_indices.size())
        throw std::invalid_argument("Cannot pluck column row pairs, not the same amount of row and column indices.");
        Mat<R> out (
            1,
            row_indices.size(),
            false);
        for (int offset = 0; offset < row_indices.size(); ++offset)
            out.w()(offset) = matrix.w()(row_indices[offset], col_indices[offset]);
    if (graph::backprop_enabled && !matrix.constant) {
        graph::emplace_back([matrix, out, row_indices, col_indices](){
            auto row_index_ptr = row_indices.data();
            auto col_index_ptr = col_indices.data();
            for (int i = 0; i < out.dims(1); ++i) {
                // for each row do the same operation as for row_pluck:
                matrix.dw()(*row_index_ptr, *col_index_ptr) += out.dw()(i);
                row_index_ptr++;
                col_index_ptr++;
            }
        });
    }
    return out;
}

template<typename R>
Mat<R> MatOps<R>::row_pluck(
        Mat<R> matrix,
        int row) {
    Mat<R> out (matrix.dims(1), 1, false);
    out.w() = matrix.w().row(row).transpose();
    if (graph::backprop_enabled)
        graph::emplace_back([matrix, out, row]() {
            GRAD(matrix).row(row).noalias() += out.dw().col(0).transpose();
        });
    return out;
}

template<typename R>
Mat<R> MatOps<R>::consider_constant(
        Mat<R> matrix
        ) {
    // perform a copy of the matrix that references
    // everything and owns nothing. A true nomad.
    Mat<R> out(matrix, false, false);
    out.constant = true;
    return out;
}

template class MatOps<float>;
template class MatOps<double>;
