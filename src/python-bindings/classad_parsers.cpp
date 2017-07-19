#include "python_bindings_common.h"
#include "old_boost.h"

#include <iostream>

#include <classad/source.h>
#include <classad/sink.h>

#include "classad_parsers.h"

#include "stdio.h"

// http://docs.python.org/3/c-api/apiabiversion.html#apiabiversion
#if PY_MAJOR_VERSION >= 3
   #define PyInt_Check(op)  PyNumber_Check(op)
   #define PyString_Check(op)  PyBytes_Check(op)
#endif

static OldClassAdIterator parseOldAds_impl(boost::python::object input);

ClassAdWrapper *parseString(const std::string &str)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parse(string) is deprecated; use parseOne, parseNext, or parseAds instead.");
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(str);
    if (!result)
    {
        std::cerr << "!result (1)\n";
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse string into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    std::cerr << "HERE (1)\n";
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    std::cerr << "HERE (2)\n";
    wrapper_result->CopyFrom(*result);
    std::cerr << "HERE (3)\n";
    delete result;
    std::cerr << "HERE (4)\n";
    return wrapper_result;
}


ClassAdWrapper *parseFile(FILE *stream)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parse is deprecated; use parseOne, parseNext, or parseAds instead.");
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(stream);
    if (!result)
    {
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse input stream into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    wrapper_result->CopyFrom(*result);
    delete result;
    return wrapper_result;
}

ClassAdWrapper *parseOld(boost::python::object input)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parseOld is deprecated; use parseOne, parseNext, or parseAds instead.");

    ClassAdWrapper * wrapper = new ClassAdWrapper();
    boost::python::object input_list;
    boost::python::extract<std::string> input_extract(input);
    if (input_extract.check())
    {
        input_list = input.attr("splitlines")();
    }
    else
    {
        input_list = input.attr("readlines")();
    }
    unsigned input_len = py_len(input_list);
    for (unsigned idx=0; idx<input_len; idx++)
    {
        boost::python::object line = input_list[idx].attr("strip")();
        if (line.attr("startswith")("#"))
        {
            continue;
        }
        std::string line_str = boost::python::extract<std::string>(line);
        size_t pos = line_str.find('=');

        // strip whitespace before the attribute and and around the =
        size_t npos = pos;
        while (npos > 0 && line_str[npos-1] == ' ') { npos--; }
        size_t bpos = 0;
        while (bpos < npos && line_str[bpos] == ' ') { bpos++; }
        std::string name = line_str.substr(bpos, npos - bpos);

        size_t vpos = pos+1;
        while (line_str[vpos] == ' ') { vpos++; }
        std::string szValue = line_str.substr(vpos);

        if (!wrapper->InsertViaCache(name, szValue))
        {
            THROW_EX(ValueError, line_str.c_str());
        }
    }
    return wrapper;
}


bool isOldAd(boost::python::object source)
{
    boost::python::extract<std::string> input_extract(source);
    if (input_extract.check())
    {
        std::cerr << "input_extract.check() OK\n";
        std::string input_str = input_extract();
        std::cerr << "input_extract() == [" << input_str << "]\n";
        const char * adchar = input_str.c_str();
        while (*adchar)
        {
            if ((*adchar == '/') || (*adchar == '[')) {return false;}
            if (!isspace(*adchar)) {return true;}
            adchar++;
        }
        return false;
    }
    if (!py_hasattr(source, "tell") || !py_hasattr(source, "read") || !py_hasattr(source, "seek")) {THROW_EX(ValueError, "Unable to determine if input is old or new classad");}
    size_t end_ptr;
    try
    {
        end_ptr = boost::python::extract<size_t>(source.attr("tell")());
    }
    catch (const boost::python::error_already_set&)
    {
        if (PyErr_ExceptionMatches(PyExc_IOError))
        {
            std::cerr << "HERE (isOldAd) (1) : PyErr_Clear()\n";
            PyErr_Clear();
            THROW_EX(ValueError, "Stream cannot rewind; must explicitly chose either old or new ClassAd parser.  Auto-detection not available.");
        }
        throw;
    }
    bool isold = false;
    while (true)
    {
          // Note: in case of IOError, this leaves the source at a different position than we started.
        std::string character = boost::python::extract<std::string>(source.attr("read")(1));
        if (!character.size()) {break;}
        if ((character == "/") || (character == "[")) {isold = false; break;}
        if (!isspace(character.c_str()[0])) {isold = true; break;}
    }
    source.attr("seek")(end_ptr);
    return isold;
}


boost::shared_ptr<ClassAdWrapper> parseOne(boost::python::object input, ParserType type)
{
    if (type == CLASSAD_AUTO)
    {
        type = isOldAd(input) ? CLASSAD_OLD : CLASSAD_NEW;
    }
    if (type == CLASSAD_OLD) {
        std::cerr << "[CLASSAD_OLD]\n";
    } else if (type == CLASSAD_NEW) {
        std::cerr << "[CLASSAD_NEW]\n";
    }
    boost::shared_ptr<ClassAdWrapper> result_ad(new ClassAdWrapper());
    input = parseAds(input, type);
    std::cerr << "HERE (parseOne) (0)\n";
    boost::python::eval("print")(input);
    bool input_has_next = py_hasattr(input, NEXT_FN);
    std::cerr << "HERE (parseOne) (1)\n";
    std::cerr << "input_has_next = " << input_has_next << "\n";

    while (true)
    {
        boost::python::object next_obj;
        try
        {
            if (input_has_next)
            {
                std::cerr << "HERE (parseOne) (2)\n";
                next_obj = input.attr(NEXT_FN)();
                std::cerr << "HERE (parseOne) (3)\n";
            }
            else if (input.ptr() && input.ptr()->ob_type && input.ptr()->ob_type->tp_iternext)
            {
                std::cerr << "HERE (parseOne) (4)\n";
                PyObject *next_obj_ptr = input.ptr()->ob_type->tp_iternext(input.ptr());
                if (next_obj_ptr == NULL) {THROW_EX(StopIteration, "All input ads processed (1)");}
                next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
                if (PyErr_Occurred()) throw boost::python::error_already_set();
            }
            else {
                std::cerr << "HERE (parseOne) (5)\n";
                THROW_EX(ValueError, "Unable to iterate through ads.");
            }
        }
        catch (const boost::python::error_already_set&)
        {
            std::cerr << "HERE (parseOne) (6)\n";
            std::cerr << "PyErr_Occurred = " << PyErr_Occurred() << "\n";
            PyObject_Print(PyErr_Occurred(), stderr, Py_PRINT_RAW);
            std::cerr << "\n";
            //std::cerr << "           ... = " << PyObject_Str(PyErr_Occurred()) << "\n";
            if (PyErr_ExceptionMatches(PyExc_StopIteration))
            {
                std::cerr << "HERE (parseOne) (7) : PyErr_Clear()\n";
                PyErr_Clear();
                break;
            }
            else {
                std::cerr << "HERE (parseOne) (8)\n";
                boost::python::throw_error_already_set();
            }
            std::cerr << "HERE (parseOne) (9)\n";
        }
        std::cerr << "HERE (parseOne) (10)\n";
        const ClassAdWrapper &ad = boost::python::extract<ClassAdWrapper>(next_obj);
        std::cerr << "HERE (parseOne) (11)\n";
        result_ad->Update(ad);
        std::cerr << "HERE (parseOne) (12)\n";
    }
    std::cerr << "HERE (parseOne) (13)\n";
    return result_ad;
}


boost::python::object parseNext(boost::python::object source, ParserType type)
{
    boost::python::object ad_iter = parseAds(source, type);
    if (py_hasattr(ad_iter, NEXT_FN))
    {
        return ad_iter.attr(NEXT_FN)();
    }
    if (source.ptr() && source.ptr()->ob_type && source.ptr()->ob_type->tp_iternext)
    {
        PyObject *next_obj_ptr = source.ptr()->ob_type->tp_iternext(source.ptr());
        if (next_obj_ptr == NULL) {THROW_EX(StopIteration, "All input ads processed (2)");}
        boost::python::object next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
        if (PyErr_Occurred()) throw boost::python::error_already_set();
        return next_obj;
    }
    THROW_EX(ValueError, "Unable to iterate through ads.");
    return boost::python::object();
}

OldClassAdIterator::OldClassAdIterator(boost::python::object source)
    : m_done(false), m_source_has_next(py_hasattr(source, NEXT_FN)),
      m_ad(new ClassAdWrapper()), m_source(source)
{
    std::cerr << "HERE (OldClassAdIterator::OldClassAdIterator) (1)\n";
    std::cerr << "m_source_has_next = " << m_source_has_next << "\n";
    if (!m_source_has_next && !PyIter_Check(m_source.ptr()))
    {
        std::cerr << "HERE (OldClassAdIterator::OldClassAdIterator) (2)\n";
        THROW_EX(TypeError, "Source object is not iterable")
    }
}

boost::shared_ptr<ClassAdWrapper>
OldClassAdIterator::next()
{
    std::cerr << "HERE (OldClassAdIterator::next()) (0)\n";

    if (m_done) {
        std::cerr << "HERE (OldClassAdIterator::next()) (1)\n";
        THROW_EX(StopIteration, "All ads processed (1)");
    }

    bool reset_ptr = py_hasattr(m_source, "tell");
    size_t end_ptr = 0;
    try
    {
        if (reset_ptr) {
            std::cerr << "HERE (OldClassAdIterator::next()) (2)\n";
            end_ptr = boost::python::extract<size_t>(m_source.attr("tell")());
        } else {
            std::cerr << "HERE (OldClassAdIterator::next()) (3)\n";
        }
    }
    catch (const boost::python::error_already_set&)
    {
        std::cerr << "HERE (OldClassAdIterator::next()) (4)\n";
        if (PyErr_ExceptionMatches(PyExc_IOError))
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (5) : PyErr_Clear()\n";
            PyErr_Clear();
            std::cerr << "HERE (OldClassAdIterator::next()) (6)\n";
            reset_ptr = false;
        }
        else
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (7)\n";
            throw;
        }
    }

    while (true)
    {
        std::cerr << "HERE (OldClassAdIterator::next()) (8)\n";
        boost::python::object next_obj;
        try
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (9)\n";
            if (m_source_has_next)
            {
                std::cerr << "HERE (OldClassAdIterator::next()) (10)\n";
                next_obj = m_source.attr(NEXT_FN)();
                std::cerr << "HERE (OldClassAdIterator::next()) (11)\n";
            }
            else
            {
                std::cerr << "HERE (OldClassAdIterator::next()) (12)\n";
                PyObject *next_obj_ptr = m_source.ptr()->ob_type->tp_iternext(m_source.ptr());
                std::cerr << "HERE (OldClassAdIterator::next()) (13)\n";
                if (next_obj_ptr == NULL) {
                    std::cerr << "HERE (OldClassAdIterator::next()) (14)\n";
                    THROW_EX(StopIteration, "All input ads processed (3)");
                }
                std::cerr << "HERE (OldClassAdIterator::next()) (15)\n";
                next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
                std::cerr << "HERE (OldClassAdIterator::next()) (16)\n";
                if (PyErr_Occurred()) {
                    std::cerr << "HERE (OldClassAdIterator::next()) (17)\n";
                    throw boost::python::error_already_set();
                }
                std::cerr << "HERE (OldClassAdIterator::next()) (18)\n";
            }
        }
        catch (const boost::python::error_already_set&)
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (19)\n";
            m_done = true;
            if (PyErr_ExceptionMatches(PyExc_StopIteration))
            {
                std::cerr << "HERE (OldClassAdIterator::next()) (20)\n";
                if (m_ad->size() == 0)
                {
                    std::cerr << "HERE (OldClassAdIterator::next()) (21) : PyErr_Clear()\n";
                    PyErr_Clear();
                    THROW_EX(StopIteration, "All ads processed (2)");
                }
                boost::shared_ptr<ClassAdWrapper> result = m_ad;
                m_ad.reset();
                std::cerr << "HERE (OldClassAdIterator::next()) (22)\n";
                if (reset_ptr && py_hasattr(m_source, "seek"))
                {
                    std::cerr << "HERE (OldClassAdIterator::next()) (23)\n";
                    PyObject *ptype, *pvalue, *ptraceback;
                    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
                    std::cerr << "HERE (OldClassAdIterator::next()) (24)\n";
                    m_source.attr("seek")(end_ptr);
                    std::cerr << "HERE (OldClassAdIterator::next()) (25)\n";
                    PyErr_Restore(ptype, pvalue, ptraceback);
                    std::cerr << "HERE (OldClassAdIterator::next()) (26)\n";
                }
                std::cerr << "HERE (OldClassAdIterator::next()) (27)\n";
                return result;
            }
            std::cerr << "HERE (OldClassAdIterator::next()) (28)\n";
            boost::python::throw_error_already_set();
            std::cerr << "HERE (OldClassAdIterator::next()) (29)\n";
        }
        if (reset_ptr)
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (30)\n";
            end_ptr += py_len(next_obj);
        }
        boost::python::object line = next_obj.attr("strip")();
        std::cerr << "HERE (OldClassAdIterator::next()) (31)\n";
        if (line.attr("startswith")("#"))
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (32)\n";
            continue;
        }
        std::string line_str = boost::python::extract<std::string>(line);
        std::cerr << "HERE (OldClassAdIterator::next()) (33)\n";
        if (line_str.size() == 0)
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (34)\n";
            if (m_ad->size() == 0) {
                std::cerr << "HERE (OldClassAdIterator::next()) (35)\n";
                continue;
            }
            boost::shared_ptr<ClassAdWrapper> result = m_ad;
            m_ad.reset(new ClassAdWrapper());
            std::cerr << "HERE (OldClassAdIterator::next()) (36)\n";
            if (reset_ptr && py_hasattr(m_source, "seek"))
            {
                std::cerr << "HERE (OldClassAdIterator::next()) (37)\n";
                m_source.attr("seek")(end_ptr);
            }
            std::cerr << "HERE (OldClassAdIterator::next()) (38)\n";
            return result;
        }
        const char * adchar = line_str.c_str();
        bool invalid = false;
        while (*adchar)
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (39)\n";
            if (!isspace(*adchar))
            {
                std::cerr << "HERE (OldClassAdIterator::next()) (40)\n";
                if (!isalpha(*adchar) && (*adchar != '_') && (*adchar != '\''))
                {
                    std::cerr << "HERE (OldClassAdIterator::next()) (41)\n";
                    invalid = true;
                    break;
                }
                break;
            }
            adchar++;
        }
        if (invalid) {
            std::cerr << "HERE (OldClassAdIterator::next()) (42)\n";
            continue;
        }

        size_t pos = line_str.find('=');
        std::cerr << "HERE (OldClassAdIterator::next()) (43)\n";

        // strip whitespace before the attribute and and around the =
        size_t npos = pos;
        while (npos > 0 && line_str[npos-1] == ' ') { npos--; }
        size_t bpos = 0;
        while (bpos < npos && line_str[bpos] == ' ') { bpos++; }
        std::string name = line_str.substr(bpos, npos - bpos);

        size_t vpos = pos+1;
        while (line_str[vpos] == ' ') { vpos++; }
        std::string szValue = line_str.substr(vpos);
        std::cerr << "HERE (OldClassAdIterator::next()) (44)\n";
        if (!m_ad->InsertViaCache(name, szValue))
        {
            std::cerr << "HERE (OldClassAdIterator::next()) (45)\n";
            THROW_EX(ValueError, line_str.c_str());
        }
    }
    std::cerr << "HERE (OldClassAdIterator::next()) (46)\n";
}

OldClassAdIterator parseOldAds(boost::python::object input)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parseOldAds is deprecated; use parseAds instead.");
    return parseOldAds_impl(input);
}

static
OldClassAdIterator
parseOldAds_impl(boost::python::object input)
{
    std::cerr << "HERE (parseOldAds_impl : 1)\n";
    bool test1 = PyString_Check(input.ptr());
    bool test2 = PyUnicode_Check(input.ptr());
    bool test = test1 || test2;
    std::cerr << "HERE (parseOldAds_impl) test1 = " << test1 << "\n";
    std::cerr << "HERE (parseOldAds_impl) test2 = " << test2 << "\n";
    std::cerr << "HERE (parseOldAds_impl) test = " << test << "\n";

    boost::python::object input_iter = test ?
          input.attr("splitlines")().attr("__iter__")()
        : input.attr("__iter__")();

    std::cerr << "HERE (parseOldAds_impl : 2)\n";
    return OldClassAdIterator(input_iter);
};

ClassAdFileIterator::ClassAdFileIterator(FILE *source)
  : m_done(false), m_source(source), m_parser(new classad::ClassAdParser())
{}

boost::shared_ptr<ClassAdWrapper>
ClassAdFileIterator::next()
{
    if (m_done) THROW_EX(StopIteration, "All ads processed (3)");

    boost::shared_ptr<ClassAdWrapper> result(new ClassAdWrapper());
    if (!m_parser->ParseClassAd(m_source, *result))
    {
        if (feof(m_source))
        {
            m_done = true;
            THROW_EX(StopIteration, "All ads processed (4)");
        }
        else
        {
            THROW_EX(ValueError, "Unable to parse input stream into a ClassAd.");
        }
    }
    return result;
}

ClassAdStringIterator::ClassAdStringIterator(const std::string & source)
  : m_off(0), m_source(source), m_parser(new classad::ClassAdParser())
{}

boost::shared_ptr<ClassAdWrapper>
ClassAdStringIterator::next()
{
    if (m_off < 0) THROW_EX(StopIteration, "All ads processed (5)");

    boost::shared_ptr<ClassAdWrapper> result(new ClassAdWrapper());
    if (!m_parser->ParseClassAd(m_source, *result, m_off))
    {
        if (m_off != static_cast<int>(m_source.size())-1)
        {
            m_off = -1;
            THROW_EX(StopIteration, "All ads processed (6)");
        }
        else
        {
            THROW_EX(ValueError, "Unable to parse input stream into a ClassAd.");
        }
    }
    return result;
}


boost::python::object
parseAds(boost::python::object input, ParserType type)
{
    if (type == CLASSAD_AUTO)
    {
        type = isOldAd(input) ? CLASSAD_OLD : CLASSAD_NEW;
    }
    if (type == CLASSAD_OLD) {return boost::python::object(parseOldAds_impl(input));}

    boost::python::extract<std::string> input_extract(input);
    if (input_extract.check())
    {
        return boost::python::object(parseAdsString(input_extract()));
    }
    return boost::python::object(parseAdsFile(boost::python::extract<FILE*>(input)));
}


ClassAdStringIterator
parseAdsString(const std::string & input)
{
    return ClassAdStringIterator(input);
}

ClassAdFileIterator
parseAdsFile(FILE * file)
{
    return ClassAdFileIterator(file);
}

// Implement the iterator protocol.
static PyObject *
obj_iternext(PyObject *self)
{
        try
        {
            boost::python::object obj(boost::python::borrowed(self));
            if (!py_hasattr(obj, NEXT_FN)) THROW_EX(TypeError, "instance has no " NEXT_FN "() method");
            boost::python::object result = obj.attr(NEXT_FN)();
            return boost::python::incref(result.ptr());
        }
        catch (...)
        {
            if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
                std::cerr << "HERE (obj_iternext) (1) : PyErr_Clear()\n";
                PyErr_Clear();
                return NULL;
            }
            boost::python::handle_exception();
            return NULL;
        }
}


static PyObject *
obj_getiter(PyObject* self)
{
    boost::python::object obj(boost::python::borrowed(self));
    if (py_hasattr(obj, "__iter__"))
    {
        try
        {
            boost::python::object my_iter = obj.attr("__iter__")();
            if (!PyIter_Check(my_iter.ptr())) {
                PyErr_Format(PyExc_TypeError,
                             "__iter__ returned non-iterator "
                             "of type '%.100s'",
                             my_iter.ptr()->ob_type->tp_name);
                return NULL;
            }
            return boost::python::incref(my_iter.ptr());
        }
        catch (...)
        {
            boost::python::handle_exception();
            return NULL;
        }
    }
    else if (py_hasattr(obj, "__getitem__"))
    {
        return PySeqIter_New(self);
    }
    PyErr_SetString(PyExc_TypeError, "iteration over non-sequence");
    return NULL;
}

boost::python::object
OldClassAdIterator::pass_through(boost::python::object const& o)
{
    PyTypeObject * boost_class = o.ptr()->ob_type;
    if (!boost_class->tp_iter) { boost_class->tp_iter = obj_getiter; }
    boost_class->tp_iternext = obj_iternext;
    return o;
};

