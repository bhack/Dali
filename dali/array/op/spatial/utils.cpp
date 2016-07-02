#include "utils.h"
#include "dali/utils/assert2.h"
#include "dali/utils/print_utils.h"

namespace internal {
    DataFormatDimMapping::DataFormatDimMapping(const std::string& data_format) {
        n_dim = data_format.find('N');
        c_dim = data_format.find('C');
        h_dim = data_format.find('H');
        w_dim = data_format.find('W');

        const auto MISS = std::string::npos;
        ASSERT2(n_dim != MISS && c_dim != MISS &&
                h_dim != MISS && w_dim != MISS &&
                data_format.size() == 4,
                utils::MS() << "data_format must be a permutation of letters N,C,H,W (was "
                            << data_format << ").");
    }

    template<typename Container>
    Container function_info_helper(
                const std::vector<int>& input_shape,
                const int& window_h,
                const int& window_w,
                const int& stride_h,
                const int& stride_w,
                const PADDING_T& padding,
                const DataFormatDimMapping& mapping) {
        Container info;

        info.batch_size         = input_shape[mapping.n_dim];
        info.in_channels        = input_shape[mapping.c_dim];
        info.in_h               = input_shape[mapping.h_dim];
        info.in_w               = input_shape[mapping.w_dim];

        if (padding == PADDING_T_SAME) {
            info.out_h = internal::int_ceil(info.in_h, stride_h);
            info.out_w = internal::int_ceil(info.in_w, stride_w);
        } else if (padding == PADDING_T_VALID) {
            info.out_h = internal::int_ceil(info.in_h - window_h + 1, stride_h);
            info.out_w = internal::int_ceil(info.in_w - window_w + 1, stride_w);
        } else {
            ASSERT2(false, utils::MS() << "Unrecognized value of padding passed to Pool2dFunction (got " << padding << ").");
        }

        if (padding == PADDING_T_SAME) {
            info.padding_h = std::max(0, (info.out_h - 1) * stride_h + window_h - info.in_h);
            info.padding_w = std::max(0, (info.out_w - 1) * stride_w + window_w - info.in_w);
        } else if (padding == PADDING_T_VALID) {
            info.padding_h = 0;
            info.padding_w = 0;
        }

        info.odd_padding_h = info.padding_h % 2;
        info.odd_padding_w = info.padding_w % 2;

        info.padding_h /= 2;
        info.padding_w /= 2;

        return info;
    }


    PoolFunctionInfo compute_pool_info(
            const std::vector<int>& input_shape,
            const int& window_h,
            const int& window_w,
            const int& stride_h,
            const int& stride_w,
            const PADDING_T& padding,
            const std::string& data_format) {
        internal::check_data_format(data_format);
        DataFormatDimMapping mapping(data_format);
        return function_info_helper<PoolFunctionInfo>(
                input_shape, window_h, window_w, stride_h, stride_w, padding, mapping);
    }

    ConvFunctionInfo compute_conv_info(
            const std::vector<int>& input_shape,
            const std::vector<int>& filters_shape,
            const int& stride_h,
            const int& stride_w,
            const PADDING_T& padding,
            const std::string& data_format) {
        internal::check_data_format(data_format);
        DataFormatDimMapping mapping(data_format);

        int filter_h = filters_shape[mapping.h_dim];
        int filter_w = filters_shape[mapping.w_dim];

        ConvFunctionInfo ret = function_info_helper<ConvFunctionInfo>(
                input_shape, filter_h, filter_w, stride_h, stride_w, padding, mapping);

        ASSERT2_EQ(ret.in_channels, filters_shape[mapping.c_dim],
            "Conv2d input and filters need to have the same number of input channels"
        );

        ret.filter_h     = filter_h;
        ret.filter_w     = filter_w;
        ret.out_channels = filters_shape[mapping.n_dim];
        return ret;
    }

}  // namespace internal
