/* 

py/pyext - python external object for PD and MaxMSP

Copyright (c)2002-2004 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"

V py::lib_setup()
{
	post("");
	post("py/pyext %s - python script objects, (C)2002-2004 Thomas Grill",PY__VERSION);


    // enable thread support and acquire the global thread lock
    PyEval_InitThreads();

    Py_Initialize();

    // get thread state
    PyThreadState *pythrmain = PyThreadState_Get();
    // get main interpreter state
//	pystate = pythrmain->interp;

    // register/initialize pyext module only once!
	module_obj = Py_InitModule(PYEXT_MODULE, func_tbl);
	module_dict = PyModule_GetDict(module_obj);

	PyModule_AddStringConstant(module_obj,"__doc__",(C *)py_doc);

    post("Python %s",Py_GetVersion());

    // release global lock
    PyEval_ReleaseLock();


    FLEXT_SETUP(pyobj);
	FLEXT_SETUP(pyext);

	post("");
}

FLEXT_LIB_SETUP(py,py::lib_setup)

//PyInterpreterState *py::pystate = NULL;

#if 0 //def FLEXT_THREADS
std::map<flext::thrid_t,PyThreadState *> py::pythrmap;
#endif

//I py::pyref = 0;
PyObject *py::module_obj = NULL;
PyObject *py::module_dict = NULL;


py::py(): 
	module(NULL),
	detach(false),shouldexit(false),thrcount(0),
	stoptick(0)
{
    PyEval_AcquireLock();
    interp = PyThreadState_Get(); //Py_NewInterpreter();
    PyEval_ReleaseLock();

    FLEXT_ADDTIMER(stoptmr,tick);
}

py::~py()
{
	if(thrcount) {
		shouldexit = true;

		// Wait for a certain time
		for(int i = 0; i < (PY_STOP_WAIT/PY_STOP_TICK) && thrcount; ++i) Sleep((F)(PY_STOP_TICK/1000.));

		// Wait forever
		post("%s - Waiting for thread termination!",thisName());
		while(thrcount) Sleep(0.2f);
		post("%s - Okay, all threads have terminated",thisName());
	}

    PyEval_AcquireLock();
    PyThreadState_Swap(interp);
//    Py_EndInterpreter(interp);
    PyEval_ReleaseLock();
}


void py::GetDir(PyObject *obj,AtomList &lst)
{
    if(obj) {
        PY_LOCK
    
        PyObject *pvar  = PyObject_Dir(obj);
	    if(!pvar)
		    PyErr_Print(); // no method found
	    else {
            AtomList *l = GetPyArgs(pvar);
            if(l) { 
                lst = *l; delete l; 
            }
            else
                post("%s - %s: List could not be created",thisName(),GetString(thisTag()));
            Py_DECREF(pvar);
        }

        PY_UNLOCK
    }
}

void py::m__dir(PyObject *obj)
{
    AtomList lst;
    GetDir(obj,lst);
    // dump dir to attribute outlet
    ToOutAnything(GetOutAttr(),thisTag(),lst.Count(),lst.Atoms());
}

V py::m__doc(PyObject *obj)
{
    if(obj) {
        PY_LOCK

		PyObject *docf = PyDict_GetItemString(obj,"__doc__"); // borrowed!!!
		if(docf && PyString_Check(docf)) {
			post("");
			post(PyString_AsString(docf));
		}

        PY_UNLOCK
	}
}


V py::SetArgs(I argc,const t_atom *argv)
{
	// script arguments
	C **sargv = new C *[argc+1];
	for(int i = 0; i <= argc; ++i) {
		sargv[i] = new C[256];
		if(!i) 
			strcpy(sargv[i],thisName());
		else
			GetAString(argv[i-1],sargv[i],255);
	}

	// the arguments to the module are only recognized once! (at first use in a patcher)
	PySys_SetArgv(argc+1,sargv);

	for(int j = 0; j <= argc; ++j) delete[] sargv[j];
	delete[] sargv;
}

V py::ImportModule(const C *name)
{
	if(!name) return;

	module = PyImport_ImportModule((C *)name);
	if (!module) {
		PyErr_Print();
		dict = NULL;
	}
	else
		dict = PyModule_GetDict(module); // borrowed

}


V py::ReloadModule()
{
	if(module) {
		PyObject *newmod = PyImport_ReloadModule(module);
		if(!newmod) {
			PyErr_Print();
			// old module still exists?!
//			dict = NULL;
		}
		else {
			Py_XDECREF(module);
			module = newmod;
			dict = PyModule_GetDict(module); // borrowed
		}
	}
	else 
		post("%s - No module to reload",thisName());
}

V py::GetModulePath(const C *mod,C *dir,I len)
{
#if FLEXT_SYS == FLEXT_SYS_PD
	// uarghh... pd doesn't show it's path for extra modules

	C *name;
	I fd = open_via_path("",mod,".py",dir,&name,len,0);
	if(fd > 0) close(fd);
	else name = NULL;

	// if dir is current working directory... name points to dir
	if(dir == name) strcpy(dir,".");
#elif FLEXT_SYS == FLEXT_SYS_MAX
	// how do i get the path in Max/MSP?
    short path;
    long type;
    char smod[256];
    strcat(strcpy(smod,mod),".py");
    if(!locatefile_extended(smod,&path,&type,&type,-1))
        path_topathname(path,NULL,dir);
    else 
        // not found
        *dir = 0;
#else
	*dir = 0;
#endif
}

V py::AddToPath(const C *dir)
{
	if(dir && *dir) {
		PyObject *pobj = PySys_GetObject("path");
		if(pobj && PyList_Check(pobj)) {
			int i,n = PyList_Size(pobj);
			for(i = 0; i < n; ++i) {
				PyObject *pt = PyList_GetItem(pobj,i);
				if(PyString_Check(pt) && !strcmp(dir,PyString_AsString(pt))) break;
			}
			if(i == n) { // string is not yet existent in path
				PyObject *ps = PyString_FromString(dir);
				PyList_Append(pobj,ps);
			}
		}
		PySys_SetObject("path",pobj);
	}
}
