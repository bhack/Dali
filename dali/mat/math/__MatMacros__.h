#ifndef DALI_MAT_MATH___MAT_MACROS___H
#define DALI_MAT_MATH___MAT_MACROS___H

#include <mshadow/tensor.h>

#include "dali/mat/math/SynchronizedTensor.h"


#define GRAD(X) if (!(X).constant) GET_GRAD(X)

#define GET_MAT_ST(X) ((X).w()->w)
#define GET_GRAD_ST(X) ((X).dw()->dw)

template<typename Device, int ndims, typename R, typename R2>
inline void tensor_fill(mshadow::Tensor<Device, ndims, R>& ts, R2 filler) {
    mshadow::MapExp<mshadow::sv::saveto>(&ts, mshadow::expr::ScalarExp<R>((R)filler));
}

template<typename R, typename R2>
inline void tensor_fill(SynchronizedTensor<R>& t, R2 filler) {
    if (t.prefers_gpu()) {
        tensor_fill(t.mutable_gpu_data(), filler);
    } else {
        tensor_fill(t.mutable_cpu_data(), filler);
    }
}



#endif
