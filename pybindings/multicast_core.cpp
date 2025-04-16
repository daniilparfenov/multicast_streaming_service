#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_receiver(py::module &);
void init_sender(py::module &);

PYBIND11_MODULE(multicast_core, m) {
    // Optional docstring
    m.doc() = "multicast_core library";

    init_receiver(m);
    init_sender(m);
}
