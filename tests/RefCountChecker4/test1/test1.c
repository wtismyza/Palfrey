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
test1(PyObject *obj, PyObject* slotnames, int nchildren)
{
    int length;
    PyObject* c;
    for (int i = 0; i < nchildren; i++) { 
        PyObject *child = PyList_GET_ITEM(children, i);
        if (!Element_Check(child)) {
            raise_type_error(child);
            length = i; 
            dealloc_extra(oldextra); 
            return NULL; 
        } 
        Py_INCREF(child); 
        c = child; 
    }
}