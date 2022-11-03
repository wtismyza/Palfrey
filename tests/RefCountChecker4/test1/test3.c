#define Py_ssize_t int
struct PyObjectstruct;
#define PyObject struct PyObjectstruct
#define NULL 0
PyObject* s;
PyObject* writer;
PyObject* first;
#define PyExc_ValueError "PyExc_ValueError"
#define MODULE_NAME "MODULE_NAME"
/* The item is a new reference. */
static const char *
test3(PyObject *items, PyObject* seq, int i,int indent_level)
{
    PyObject* key, *value;
    PyObject *item = PyList_GET_ITEM(items, i); 
    if (!PyTuple_Check(item) || PyTuple_GET_SIZE(item) != 2) { 
        PyErr_SetString(PyExc_ValueError, "items must return 2-tuples"); 
        goto bail; 
    } 
    key = PyTuple_GET_ITEM(item, 0); 
    value = PyTuple_GET_ITEM(item, 1);
    if (encoder_encode_key_value(s, writer, &first, key, value, indent_level) < 0) 
        goto bail;
    bail:
        return 0;
    
}