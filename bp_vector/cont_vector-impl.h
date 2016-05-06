/*
#include "cont_vector.h"

template <typename T>
void cont_vector<T>::get(const size_t i, const uint16_t ts, const const T &out) {
    out = PS_Trie<T>::get(i);
    if (ts > ts_w) {
        dependencies[i].push_back(&T);
    }
}

template <typename T>
void cont_vector<T>::set(const size_t i, const T &val) {
    PS_Trie<T>::set(i, val);
    for (dep : dependencies[i]) {
        resolve(dep, val);
    }
}
*/   
