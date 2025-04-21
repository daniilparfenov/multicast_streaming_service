#ifndef CONVERTERS_H
#define CONVERTERS_H

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <opencv2/opencv.hpp>

namespace py = pybind11;

inline py::object matToNumpy(const cv::Mat& mat) {
    if (mat.empty()) return py::none();

    py::buffer_info buffer_info(mat.data, sizeof(unsigned char),
                                py::format_descriptor<unsigned char>::format(), 3,
                                {mat.rows, mat.cols, mat.channels()},
                                {sizeof(unsigned char) * mat.cols * mat.channels(),
                                 sizeof(unsigned char) * mat.channels(), sizeof(unsigned char)});

    return py::array_t<unsigned char>(buffer_info);
}

#endif  // CONVERTERS_H
