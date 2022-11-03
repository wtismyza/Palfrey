#define Py_ssize_t int
struct PyObjectstruct;
#define PyObject struct PyObjectstruct
#define NULL 0
PyObject* _filters;
PyObject* Py_None;
PyObject* slotnames;
#define PyExc_ValueError "PyExc_ValueError"
#define MODULE_NAME "MODULE_NAME"
/* The item is a new reference. */
static const char *
test0(PyObject *obj, PyObject* slotnames, int i)
{
    PyObject * name = PyList_GET_ITEM(slotnames, i);
    Py_INCREF(name);
    PyObject * value = PyObject_GetAttr(obj, name);
    Py_DECREF(name);
}