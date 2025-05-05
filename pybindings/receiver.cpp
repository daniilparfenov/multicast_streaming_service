#include "receiver.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "converters.h"

namespace py = pybind11;
using namespace MulticastLib;

void init_receiver(py::module_& m) {
    py::class_<Receiver>(m, "Receiver")
        .def(py::init<const std::string&, int>())
        .def("start", &Receiver::start)
        .def("stop", &Receiver::stop)
        .def("is_active", &Receiver::isReceiving)
        .def(
            "get_latest_frame", [](Receiver& self) { return matToNumpy(self.getLatestFrame()); },
            "Get latest frame as numpy array")
        .def("getStatistics", &Receiver::getStatistics);
}

void init_receiver_statistics(py::module_& m) {
    py::class_<ReceiverStatistics>(m, "ReceiverStatistics")
    .def_readonly("totalPacketsReceived", &ReceiverStatistics::totalPacketsReceived)
    .def_readonly("totalCorruptedPackets", &ReceiverStatistics::totalCorruptedPackets)
    .def_readonly("totalFramesDecoded", &ReceiverStatistics::totalFramesDecoded)
    .def_readonly("avgFps", &ReceiverStatistics::avgFps);
}
