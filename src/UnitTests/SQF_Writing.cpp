#include "stdafx.h"
#include "CppUnitTest.h"
#include "CppUnitTestLogger.h"
#include <Python.h>

// TODO: Fix this horrible mess
#define LOG_ERROR(a)
#include "../Pythia/SQFWriter.h"
//#include "../Pythia/SQFWriter.cpp"  // I don't know why I cannot make VS use pythia.lib :(
#include "../Pythia/ExceptionFetcher.h"
//#include "../Pythia/ExceptionFetcher.cpp"
#include "../Pythia/ResponseWriter.h"
//#include "../Pythia/ResponseWriter.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SQF_Writing_Test
{
    PyObject *python_eval(const char *str)
    {
        // Note: Of course, this function leaks memory.
        // Don't use in production code!
        PyObject* main_module = PyImport_AddModule("__main__");
        PyObject* global_dict = PyModule_GetDict(main_module);
        PyObject* local_dict = PyDict_New();
        PyObject* code = Py_CompileString(str, "pyscript", Py_eval_input);
        PyObject* result = PyEval_EvalCode(code, global_dict, local_dict);

        if (!result)
        {
            std::string error = PyExceptionFetcher().getError();
        }

        return result;
    }

    void python_to_sqf(const char *python, const char *sqf)
    {
        PyObject *obj = python_eval(python);
        TestResponseWriter writer;
        writer.initialize();
        SQFWriter::encode(obj, &writer);
        writer.finalize();
        std::string output = writer.getResponse();
        Assert::AreEqual(sqf, output.c_str());
    }

    TEST_CLASS(SQFGeneratorUnitTest)
    {
        // Note: SQFWriter operates on real, valid PyObject objects, that were
        // created inside python by python code.
        // Hence, the tests test the conversion of valid objects to SQF
        // and there are no "[1, 2" tests (missing closing bracket)

        //Logger::WriteMessage("In StringParsing");
        public:
        TEST_CLASS_INITIALIZE(init)
        {
            Py_Initialize();
        }

        TEST_CLASS_CLEANUP(deinit)
        {
            Py_Finalize();
        }

        TEST_METHOD(PythonNoneParsing)
        {
            python_to_sqf("None", "nil");
        }

        TEST_METHOD(PythonBooleanParsing)
        {

            python_to_sqf("True", "True");
            python_to_sqf("False", "False");
        }

        TEST_METHOD(PythonIntegerParsing)
        {
            python_to_sqf("0", "0");
            python_to_sqf("-0", "0");
            python_to_sqf("250", "250");
            python_to_sqf("1000", "1000");
            python_to_sqf("-5", "-5");

            // "long long" overflow
            python_to_sqf("9223372036854775807", "9223372036854775807");
            python_to_sqf("9223372036854775808", "OVERFLOW!");
            python_to_sqf("-9223372036854775808", "-9223372036854775808");
            python_to_sqf("-9223372036854775809", "OVERFLOW!");
        }

        TEST_METHOD(PythonFloatParsing)
        {
            python_to_sqf("0.0", "0");
            python_to_sqf("-0.0", "0");
            python_to_sqf("1.5", "1.5");
            python_to_sqf("-1.5", "-1.5");
            python_to_sqf("1234.0", "1234");
            python_to_sqf("12345.6", "12345.6");
            python_to_sqf("22937.2", "22937.2");
            python_to_sqf("229371.268934", "229371.268934");
            python_to_sqf("695619.606753", "695619.606753");
            python_to_sqf("-1.23456789012e-008", "-1.23456789012e-8");

            // "double" overflow
            python_to_sqf("1.7976931348623157E+308", "1.7976931348623157e+308");
            python_to_sqf("1.7976931348623157E+309", "Infinity");
            python_to_sqf("-1.7976931348623157E+308", "-1.7976931348623157e+308");
            python_to_sqf("-1.7976931348623157E+309", "-Infinity");
        }

        TEST_METHOD(PythonListParsing)
        {
            python_to_sqf("[]", "[]");
            python_to_sqf("[True]", "[True]");
            python_to_sqf("[True, False]", "[True,False]");
            python_to_sqf("[[[]]]", "[[[]]]");
            python_to_sqf("[[0, 0], [0, 1], [0, 2]]", "[[0,0],[0,1],[0,2]]");

            // Slices
            python_to_sqf("[0, 1, 2, 3, 4, 5, 6][1:5]", "[1,2,3,4]");

            // Generated list
            python_to_sqf("[i for i in [True, True, False]]", "[True,True,False]");
            python_to_sqf("[i for i in range(5)]", "[0,1,2,3,4]");
        }

        TEST_METHOD(PythonTupleParsing)
        {
            python_to_sqf("()", "[]");
            python_to_sqf("(True,)", "[True]");
            python_to_sqf("(True, False)", "[True,False]");
            python_to_sqf("(((),),)", "[[[]]]");
            python_to_sqf("((0, 0), (0, 1), (0, 2))", "[[0,0],[0,1],[0,2]]");

            // Slices
            python_to_sqf("(0, 1, 2, 3, 4, 5, 6)[1:5]", "[1,2,3,4]");

            // Other
            python_to_sqf("1, 2, 3", "[1,2,3]");
        }

        TEST_METHOD(PythonGeneratorParsing)
        {
            python_to_sqf("(i for i in [True, True, False])", "[True,True,False]");
            python_to_sqf("(i for i in range(5))", "[0,1,2,3,4]");
            python_to_sqf("(i for i in [])", "[]"); // Empty generator
        }

        TEST_METHOD(PythonRangeParsing)
        {
            python_to_sqf("range(5)", "[0,1,2,3,4]");
        }

        TEST_METHOD(PythonSetParsing)
        {
            python_to_sqf("set([5])", "[5]");
            python_to_sqf("set([])", "[]");
        }

        TEST_METHOD(PythonStringParsing)
        {
            python_to_sqf("''", "\"\"");                                  // '' => ""
            python_to_sqf("\"\"", "\"\"");                                // "" => ""
            python_to_sqf("'test'", "\"test\"");
            python_to_sqf("'test\\\\test'", "\"test\\test\"");            // Ignore \ escaping
            python_to_sqf("'test\\ntest'", "\"test\ntest\"");             // Newline in string
            python_to_sqf("'test\"test'", "\"test\"\"test\"");            // test"test => test""test
            python_to_sqf("'test\"\"test'", "\"test\"\"\"\"test\"");      // test""test => test""""test
            python_to_sqf("\"test'test\"", "\"test'test\"");              // test'test => test'test
        }

        TEST_METHOD(PythonStringUTFParsing)
        {
            // Testing with '���'
            python_to_sqf("u'\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87'", "\"\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87\"");
            python_to_sqf("b'\\xc5\\xbc\\xc3\\xb3\\xc5\\x82\\xc4\\x87'.decode('utf-8')", "\"\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87\"");
        }
    };
}
