#define Py_ssize_t int
struct PyObjectstruct;
#define PyObject struct PyObjectstruct
#define NULL 0
PyObject* oldextra;
PyObject* self;
PyObject* children;
#define PyExc_ValueError "PyExc_ValueError"
#define MODULE_NAME "MODULE_NAME"
/* The item is a new reference. */
static const char *
test2(PyObject *obj, PyObject* seq, int nchildren)
{
    for (int i = 0; i < PySequence_Fast_GET_SIZE(seq); i++) { 
        PyObject* element = PySequence_Fast_GET_ITEM(seq, i); 
        Py_INCREF(element); 
        if (element_add_subelement(self, element) < 0) { 
            Py_DECREF(seq); 
            Py_DECREF(element); 
            return NULL; 
        } 
        Py_DECREF(element);
    }
}