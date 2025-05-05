#include "sender.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "converters.h"

namespace py = pybind11;
using namespace MulticastLib;

void init_sender(py::module_& m) {
    py::class_<Sender>(m, "Sender")
        .def(py::init<const std::string&, int>())
        .def("start_stream", &Sender::startStream)
        .def("stop_stream", &Sender::stopStream)
        .def(
            "get_preview_frame", [](Sender& self) { return matToNumpy(self.getPreviewFrame()); },
            "Get last captured frame as numpy array")
        .def("get_active_client_count", &Sender::getActiveClientCount,
             "Get the number of active clients");
}
