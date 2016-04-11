#include "property_extractor.h"

#include "dali/array/array.h"

std::string ShapeProperty::name = "shape";
std::string DTypeProperty::name = "dtype";

std::vector<int> ShapeProperty::extract(const Array& x) {
    return x.shape();
}

DType DTypeProperty::extract(const Array& x) {
    return x.dtype();
}