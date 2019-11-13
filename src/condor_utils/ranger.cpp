#include "condor_common.h"

#include <algorithm>
#include "ranger.h"


///// ranger insert/erase/find implementation /////


template <class T>
typename ranger<T>::iterator ranger<T>::insert(range r)
{
    // lower_bound here will coalesce an adjacent disjoint range;
    // can use upper_bound instead to avoid this and leave them fractured
    iterator it_start = lower_bound(r._start);
    iterator it = it_start;
    while (it != end() && it->_start <= r._end)  // '<' for no coalesce
        ++it;

    iterator it_end = it;
    if (it_start == it_end)
        return forest.insert(it_end, r);

    iterator it_back = --it;
    element_type min_start = std::min(it_start->_start, r._start);

    // update back of affected range in-place
    if (min_start < it_back->_start)
        it_back->_start = min_start;

    if (it_back->_end < r._end)
        it_back->_end = r._end;

    return it_start == it_back ? it_back : forest.erase(it_start, it_back);
}

template <class T>
typename ranger<T>::iterator ranger<T>::erase(range r)
{
    iterator it_start = upper_bound(r._start);
    iterator it = it_start;
    while (it != end() && it->_start < r._end)
        ++it;

    iterator it_end = it;
    if (it_start == it_end)
        return it_start;

    iterator it_back = --it;
    range orig_back = *it_back;

    if (it_start->_start < r._start && r._end < it_start->_end) {
        // split a single range
        it_start->_end = r._start;
        return forest.insert(it_end, range(r._end, orig_back._end));
    }

    if (it_start->_start < r._start) {
        it_start->_end = r._start;
        ++it_start;
    }

    if (r._end < orig_back._end) {
        it_back->_start = r._end;
        --it_end;
    }

    return it_start == it_end ? it_end : forest.erase(it_start, it_end);
}

template <class T>
std::pair<typename ranger<T>::iterator, bool>
ranger<T>::find(element_type x) const
{
    iterator it = upper_bound(x);
    return {it, it != end() && it->_start <= x};
}

template <class T>
bool ranger<T>::contains(element_type x) const
{
    return find(x).second;
}

template <class T>
ranger<T>::ranger(const std::initializer_list<ranger<T>::range> &il)
{
    for (const range &rr : il)
        insert(rr);
}

template <class T>
ranger<T>::ranger(const std::initializer_list<ranger<T>::element_type> &il)
{
    for (const element_type &e : il)
        insert(e);
}


// specialize for std::set containers to use std::set::lower_bound
template <class T>
static inline typename std::set<typename ranger<T>::range>::const_iterator
lower_bounder(const typename std::set<typename ranger<T>::range> &f,
                                      typename ranger<T>::range rr)
{
    return f.lower_bound(rr);
}

template <class T>
static inline typename std::set<typename ranger<T>::range>::const_iterator
upper_bounder(const typename std::set<typename ranger<T>::range> &f,
                                      typename ranger<T>::range rr)
{
    return f.upper_bound(rr);
}

// generic containers (other than std::set) use std::lower_bound
template <class C, class T> static inline typename C::const_iterator
lower_bounder(const C &f, typename ranger<T>::range rr)
{
    return std::lower_bound(f.begin(), f.end(), rr);
}

template <class C, class T> static inline typename C::const_iterator
upper_bounder(const C &f, typename ranger<T>::range rr)
{
    return std::upper_bound(f.begin(), f.end(), rr);
}


template <class T>
typename ranger<T>::iterator ranger<T>::lower_bound(element_type x) const
{
    return lower_bounder<forest_type,T>(forest, x);
}

template <class T>
typename ranger<T>::iterator ranger<T>::upper_bound(element_type x) const
{
    return upper_bounder<forest_type,T>(forest, x);
}



///// ranger::elements::iterator implementation /////


/* This allows:
 *
 *      for (int i : myranger.get_elements())
 *          process_int(i);
 *
 *
 * Instead of the still-straightforward:
 *
 *      for (auto &rr : myranger)
 *          for (int i : rr)
 *              process_int(i);
 */

template <class T>
void ranger<T>::elements::iterator::mk_valid()
{
    if (!rit_valid) {
        rit = sit->begin();
        rit_valid = true;
    }
}

template <class T>
typename ranger<T>::element_type
ranger<T>::elements::iterator::operator*()
{
    mk_valid();
    return *rit;
}

template <class T>
typename ranger<T>::elements::iterator &
ranger<T>::elements::iterator::operator++()
{
    mk_valid();
    if (++rit == sit->end()) {
        ++sit;
        rit_valid = false;
    }
    return *this;
}

template <class T>
typename ranger<T>::elements::iterator &
ranger<T>::elements::iterator::operator--()
{
    mk_valid();
    if (rit == sit->begin()) {
        --sit;
        rit = sit->end();
        --rit;
    }
    return *this;
}

template <class T>
bool ranger<T>::elements::iterator::operator==(iterator &it)
{
    if (sit != it.sit)
        return false;
    if (!rit_valid && !it.rit_valid)
        return true;
    // at this point neither sit nor it.sit points to the end() of its set,
    // thus it's OK to call mk_valid() on each, which may dereference *sit
    mk_valid();
    it.mk_valid();
    return rit == it.rit;
}

template <class T>
bool ranger<T>::elements::iterator::operator!=(iterator &it)
{
    return !(*this == it);
}



///// ranger persist/load implementation /////


#include <stdlib.h> // strtol
#include <stdio.h>  // sprintf
#include <string>

/*  persist / load ranger objects
 *
 *  The serialized format is one or more sub-ranges, separated by semicolons,
 *  where each sub-range is either N-M (for inclusive N..M) or N for a single
 *  integer.  Eg, "2", "5-10", "4;7;10-20;44;50-60"
 */

template <class T>
static
void persist_range_single(std::string &s, const typename ranger<T>::range &rr)
{
    char buf[64];
    int n;
    if (rr._start == rr._end - 1)
        n = sprintf(buf, "%d;", rr._start);
    else
        n = sprintf(buf, "%d-%d;", rr._start, rr._end - 1);
    s.append(buf, n);
}

template <class T>
void ranger<T>::persist(std::string &s) const
{
    s.clear();
    if (empty())
        return;

    for (auto &rr : *this)
        persist_range_single<T>(s, rr);

    s.erase(s.size() - 1);
}

template <class T>
void ranger<T>::persist_range(std::string &s, const range &rr) const
{
    s.clear();
    if (empty())
        return;

    auto rit = find(rr._start).first;
    for (; rit != end() && rit->_start < rr._end; ++rit) {
        typename ranger<T>::range rr_new = { std::max(rit->_start, rr._start),
                                             std::min(rit->_end,   rr._end) };
        persist_range_single<T>(s, rr_new);
    }

    s.erase(s.size() - 1);
}

template <class T>
void ranger<T>::persist_slice(std::string &s, element_type start,
                                              element_type back) const
{
    persist_range(s, {start, back+1});
}

// return 0 on success, (-1 - (position in string)) on parse failure
template <class T>
int ranger<T>::load(const char *s)
{
    const char *sstart = s;
    while (*s) {
        char *sp;
        int start = strtol(s, &sp, 10);
        int back;
        if (s == sp)
            // no int parsed is ok as long as we're at the end
            return *s ? -1 - int(s - sstart) : 0;
        s = sp;
        if (*sp == '-') {
            s++;
            back = strtol(s, &sp, 10);
            if (s == sp)
                // a number should have followed '-'
                return -1 - int(s - sstart);
            s = sp;
        } else {
            back = start;
        }
        if (*s == ';')
            s++;
        else if (*s)
            // expected either ';' or end of string
            return -1 - int(s - sstart);
        insert({start, back + 1});
    }
    return 0;
}



// put all explicit template instantiations here:

template struct ranger<int>;

