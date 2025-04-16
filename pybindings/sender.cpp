#include "sender.h"

#include <pybind11/pybind11.h>
namespace py = pybind11;

void init_sender(py::module &m) { py::class_<Sender>(m, "Sender").def(py::init<>()); }
