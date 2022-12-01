#define Py_ssize_t int
struct PyObjectstruct;
#define PyObject struct PyObjectstruct
#define NULL 0
PyObject* _filters;
PyObject* Py_None;
#define PyExc_ValueError "PyExc_ValueError"
#define MODULE_NAME "MODULE_NAME"

/* The item is a new reference. */
static const char *
get_filter(PyObject *category, PyObject *text, Py_ssize_t lineno,
           PyObject *module, PyObject **item)
{
    PyObject *action;
    Py_ssize_t i;
    PyObject *warnings_filters;

    warnings_filters = get_warnings_attr("filters");
    if (warnings_filters == NULL) {
        if (PyErr_Occurred())
            return NULL;
    }
    else {
        Py_DECREF(_filters);
        _filters = warnings_filters;
    }

    if (_filters == NULL || !PyList_Check(_filters)) {
        PyErr_SetString(PyExc_ValueError,
                        MODULE_NAME ".filters must be a list");
        return NULL;
    }

    /* _filters could change while we are iterating over it. */
    for (i = 0; i < PyList_GET_SIZE(_filters); i++) {
        PyObject *tmp_item, *action, *msg, *cat, *mod, *ln_obj;
        Py_ssize_t ln;
        int is_subclass, good_msg, good_mod;

        tmp_item = PyList_GET_ITEM(_filters, i);
        if (!PyTuple_Check(tmp_item) || PyTuple_GET_SIZE(tmp_item) != 5) {
            PyErr_Format(PyExc_ValueError,
                         MODULE_NAME ".filters item %zd isn't a 5-tuple", i);
            return NULL;
        }

        /* Python code: action, msg, cat, mod, ln = item */
        Py_INCREF(tmp_item);
        action = PyTuple_GET_ITEM(tmp_item, 0);
        msg = PyTuple_GET_ITEM(tmp_item, 1);
        cat = PyTuple_GET_ITEM(tmp_item, 2);
        mod = PyTuple_GET_ITEM(tmp_item, 3);
        ln_obj = PyTuple_GET_ITEM(tmp_item, 4);

        good_msg = check_matched(msg, text);
        good_mod = check_matched(mod, module);
        is_subclass = PyObject_IsSubclass(category, cat);
        ln = PyLong_AsSsize_t(ln_obj);
        if (good_msg == -1 || good_mod == -1 || is_subclass == -1 ||
            (ln == -1 && PyErr_Occurred())) {
            Py_DECREF(tmp_item);
            return NULL;
        }

        if (good_msg && is_subclass && good_mod && (ln == 0 || lineno == ln)) {
            *item = tmp_item;
            return _PyUnicode_AsString(action);
        }

        Py_DECREF(tmp_item);
    }

    action = get_default_action();
    if (action != NULL) {
        Py_INCREF(Py_None);
        *item = Py_None;
        return _PyUnicode_AsString(action);
    }

    PyErr_SetString(PyExc_ValueError,
                    MODULE_NAME ".defaultaction not found");
    return NULL;

}
