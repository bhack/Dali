#include "dali/mat/math/memory/SynchronizedMemory.h"
#include "dali/mat/math/memory/LazyTensor.h"
#include "dali/mat/math/memory/TensorOps.h"

using mshadow::AllocSpace;
using mshadow::FreeSpace;
using mshadow::Shape2;
using mshadow::Tensor;
using mshadow::Copy;

template<typename R, int dimension>
SynchronizedMemory<R, dimension>::SynchronizedMemory(int n, int d, PreferredDevice _preferred_device) :
#ifdef DALI_USE_CUDA
    mem_gpu(Shape2(n, d)),
    allocated_gpu(false),
    gpu_fresh(false),
#endif
    allocated_cpu(false),
    mem_cpu(Shape2(n, d)),
    cpu_fresh(false),
    preferred_device(_preferred_device) {
}

template<typename R, int dimension>
SynchronizedMemory<R,dimension>::SynchronizedMemory(const SynchronizedMemory& other) :
#ifdef DALI_USE_CUDA
        allocated_gpu(false),
        mem_gpu(other.mem_gpu.shape_),
        gpu_fresh(false),
#endif
        allocated_cpu(false),
        mem_cpu(other.mem_cpu.shape_),
        cpu_fresh(false),
        preferred_device(other.preferred_device) {
    if (other.cpu_fresh) {
        const auto& data_source = other.cpu_data();
        copy_data_from(data_source);
#ifdef DALI_USE_CUDA
    } else if (other.gpu_fresh) {
        const auto& data_source = other.gpu_data();
        copy_data_from(data_source);
#endif
    } else {
        // data was not initialized on the source
        // so we also choose not to initialize.
        return;
    }
}

template<typename R, int dimension>
unsigned int SynchronizedMemory<R,dimension>::number_of_elements() const {
    unsigned int dim = 1;
    for (int i = 0; i < dimension;i++) {
        dim *= mem_cpu.shape_.shape_[i];
    }
    return dim;
}

template<typename R, int dimension>
R SynchronizedMemory<R, dimension>::sum() const {
    #ifdef DALI_USE_CUDA
        if (should_compute_on_gpu({std::cref(*this)})) {
            return TensorOps::sum(gpu_data(), number_of_elements() );
        } else {
            return TensorOps::sum(cpu_data(), number_of_elements() );
        }
    #else
        return TensorOps::sum(cpu_data(), number_of_elements());
    #endif
}

template<typename R, int dimension>
bool SynchronizedMemory<R,dimension>::operator==(const SynchronizedMemory<R,dimension>& other) const {
    #ifdef DALI_USE_CUDA
        if (should_compute_on_gpu({std::cref(*this), std::cref(other)})) {
            return TensorOps::equals(gpu_data(), other.gpu_data(), number_of_elements());
        }
    #endif
    return TensorOps::equals(cpu_data(), other.cpu_data(), number_of_elements());
}

template<typename R, int dimension>
bool SynchronizedMemory<R,dimension>::allclose(const SynchronizedMemory<R,dimension>& other, R tol) const {
    #ifdef DALI_USE_CUDA
        if (should_compute_on_gpu({std::cref(*this), std::cref(other)})) {
            return TensorOps::allclose(gpu_data(), other.gpu_data(), number_of_elements(), tol);
        }
    #endif
    return TensorOps::allclose(cpu_data(), other.cpu_data(), number_of_elements(), tol);
}

#ifdef DALI_USE_CUDA
template<typename R, int dimension>
LazyTensor<
    typename SynchronizedMemory<R,dimension>::cpu_tensor_t,
    typename SynchronizedMemory<R,dimension>::gpu_tensor_t,
    R,
    mshadow::expr::type::kRValue> SynchronizedMemory<R,dimension>::wrapper() {
    return LazyTensor<
        cpu_tensor_t,
        gpu_tensor_t,
        R,
        mshadow::expr::type::kRValue
        >(*this);
}
#else
template<typename R, int dimension>
LazyTensor<
    typename SynchronizedMemory<R,dimension>::cpu_tensor_t,
    R, mshadow::expr::type::kRValue> SynchronizedMemory<R,dimension>::wrapper() {
    return LazyTensor<
        cpu_tensor_t,
        R,
        mshadow::expr::type::kRValue>(*this);
}
#endif

template<typename R, int dimension>
mshadow::Shape<dimension> SynchronizedMemory<R,dimension>::shape() const {
    return mem_cpu.shape_;
}

#ifdef DALI_USE_CUDA
    template<typename R, int dimension>
    PreferredDevice SynchronizedMemory<R,dimension>::tie_breaker_device = DEVICE_GPU;
#endif

template<typename R, int dimension>
SynchronizedMemory<R,dimension>::~SynchronizedMemory() {
    if (allocated_cpu)
        FreeSpace(&mem_cpu);
#ifdef DALI_USE_CUDA
    if (allocated_gpu)
        FreeSpace(&mem_gpu);
#endif
}

template<typename R, int dimension>
const typename SynchronizedMemory<R,dimension>::cpu_tensor_t & SynchronizedMemory<R,dimension>::cpu_data() const {
    to_cpu();
    return mem_cpu;
}

template<typename R, int dimension>
typename SynchronizedMemory<R,dimension>::cpu_tensor_t & SynchronizedMemory<R,dimension>::mutable_cpu_data() {
    to_cpu();
#ifdef DALI_USE_CUDA
    gpu_fresh = false;
#endif
    return mem_cpu;
}

#ifdef DALI_USE_CUDA
    template<typename R, int dimension>
    const Tensor<mshadow::gpu, dimension, R>& SynchronizedMemory<R,dimension>::gpu_data() const {
        to_gpu();
        return mem_gpu;
    }

    template<typename R, int dimension>
    Tensor<mshadow::gpu, dimension, R>& SynchronizedMemory<R,dimension>::mutable_gpu_data() {
        to_gpu();
        cpu_fresh = false;
        return mem_gpu;
    }
#endif

template<typename R, int dimension>
bool SynchronizedMemory<R,dimension>::prefers_cpu() const {
    return preferred_device == DEVICE_CPU;
}

template<typename R, int dimension>
bool SynchronizedMemory<R,dimension>::prefers_gpu() const {
    return preferred_device == DEVICE_GPU;
}

#ifdef DALI_USE_CUDA
    template<typename R, int dimension>
    void SynchronizedMemory<R,dimension>::to_gpu() const {
        if (!gpu_fresh) {
            if (!allocated_gpu) {
                AllocSpace(&mem_gpu, false);
                allocated_gpu = true;
            }
            if (cpu_fresh) {
                Copy(mem_gpu, mem_cpu);
            }
            gpu_fresh = true;
        }
    }
#endif

template<typename R, int dimension>
void SynchronizedMemory<R,dimension>::to_cpu() const {
    if (!cpu_fresh) {
        if (!allocated_cpu) {
            AllocSpace(&mem_cpu, false);
            allocated_cpu = true;
        }
#ifdef DALI_USE_CUDA
        if (gpu_fresh) {
            Copy(mem_cpu, mem_gpu);
        }
#endif
        cpu_fresh = true;
    }
}

template<typename R, int dimension>
template<typename SourceType>
void SynchronizedMemory<R,dimension>::copy_data_from(SourceType& data_source) {
    if (prefers_cpu()) {
        AllocSpace(&mem_cpu, false);
        allocated_cpu = true;
        Copy(mem_cpu, data_source);
        cpu_fresh = true;
    } else {
#ifdef DALI_USE_CUDA
        AllocSpace(&mem_gpu, false);
        allocated_gpu = true;
        Copy(mem_gpu, data_source);
        gpu_fresh = true;
#endif
    }
}

template<typename R, int dimension>
bool should_compute_on_gpu(
        std::initializer_list<std::reference_wrapper<const SynchronizedMemory<R,dimension>>> sts) {

#ifdef DALI_USE_CUDA
    if (sts.size() == 1) {
        const auto& mat = (*sts.begin()).get();
        return (mat.prefers_gpu() && (mat.gpu_fresh || !mat.cpu_fresh && !mat.gpu_fresh));
    }
    bool everybody_cpu = true;
    bool everybody_gpu = true;
    for (auto& st : sts) {
        everybody_gpu = everybody_gpu && st.get().prefers_gpu();
        everybody_cpu = everybody_cpu && st.get().prefers_cpu();
    }
    if (everybody_cpu) {
        return false;
    } else if (everybody_gpu) {
        return true;
    } else {
        return SynchronizedMemory<R,dimension>::tie_breaker_device == DEVICE_GPU;
    }
#else
    return false;
#endif
}

template<typename R, int dimension>
bool should_compute_on_gpu(
        const std::vector<std::reference_wrapper<const SynchronizedMemory<R,dimension>>>& sts) {

#ifdef DALI_USE_CUDA
    if (sts.size() == 1) {
        const auto& mat = (*sts.begin()).get();
        return (mat.prefers_gpu() && (mat.gpu_fresh || !mat.cpu_fresh && !mat.gpu_fresh));
    }
    bool everybody_cpu = true;
    bool everybody_gpu = true;
    for (auto& st : sts) {
        everybody_gpu = everybody_gpu && st.get().prefers_gpu();
        everybody_cpu = everybody_cpu && st.get().prefers_cpu();
    }
    if (everybody_cpu) {
        return false;
    } else if (everybody_gpu) {
        return true;
    } else {
        return SynchronizedMemory<R,dimension>::tie_breaker_device == DEVICE_GPU;
    }
#else
    return false;
#endif
}

template bool should_compute_on_gpu(
        std::initializer_list<std::reference_wrapper<const SynchronizedMemory<float,2>>>);
template bool should_compute_on_gpu(
        std::initializer_list<std::reference_wrapper<const SynchronizedMemory<double,2>>>);

template bool should_compute_on_gpu(
        const std::vector<std::reference_wrapper<const SynchronizedMemory<float,2>>>& );
template bool should_compute_on_gpu(
        const std::vector<std::reference_wrapper<const SynchronizedMemory<double,2>>>& );

template class SynchronizedMemory<float,2>;
template class SynchronizedMemory<double,2>;
