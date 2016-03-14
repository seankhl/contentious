
template <typename T>
ps_vector<T>::ps_vector(const tr_vector<T> &other)
  : bp_vector_base<T, ps_vector>(
          static_cast<const bp_vector_base<T, tr_vector> &>(other))
{ 
    /* I admit, this code is truly disturbing */
    this->id = 0;
}

template <typename T>
tr_vector<T> ps_vector<T>::make_transient() const
{
    tr_vector<T> ret(*this);
    return ret;
}

template <typename T>
tr_vector<T>::tr_vector(const bp_vector<T> &other)
  : bp_vector_base<T, tr_vector>(
          static_cast<const bp_vector_base<T, bp_vector> &>(other))
{
    /* I admit, this code is truly disturbing */
    this->id = this->get_unique_id();
}

template <typename T>
tr_vector<T>::tr_vector(const ps_vector<T> &other)
  : bp_vector_base<T, tr_vector>(
          static_cast<const bp_vector_base<T, ps_vector> &>(other))
{
    /* I admit, this code is truly disturbing */
    this->id = this->get_unique_id();
}

template <typename T>
ps_vector<T> tr_vector<T>::make_persistent() const
{
    return *this;
}

template <typename T>
tr_vector<T> tr_vector<T>::new_id() const
{
    tr_vector<T> ret(*this);
    ret.id = this->get_unique_id();
    return ret;
}
    

