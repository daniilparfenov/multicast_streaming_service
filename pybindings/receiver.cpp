#include "receiver.h"

#include <pybind11/pybind11.h>
namespace py = pybind11;

void init_receiver(py::module &m) { py::class_<Receiver>(m, "Receiver").def(py::init<>()); }
