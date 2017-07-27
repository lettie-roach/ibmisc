#ifndef IBMISC_BUNDLE_HPP
#define IBMISC_BUNDLE_HPP

#include <ibmisc/blitz.hpp>
#include <ibmisc/netcdf.hpp>
#include <ibmisc/IndexSet.hpp>

namespace ibmisc {



/** Area of memory where a TOPO-generating procedure can place its outputs.
Should be pre-allocated before the generator is called. */
template<class TypeT, int RANK>
class ArrayBundle {
    // Stores memory for arrays allocated as a multi-array
    TmpAlloc tmp;
public:

    // -------------------------------------------------------------------
    // http://cpptruths.blogspot.com/2012/03/rvalue-references-in-constructor-when.html
    struct Meta {
        friend class ArrayBundle;
    protected:
        std::string name;
        blitz::Array<TypeT, RANK> arr;
        blitz::TinyVector<int, RANK> shape;
        std::array<std::string,RANK> sdims;
        std::vector<std::tuple<std::string, std::string>> attr;    // (attr-name, value)

        /** Users do not use directly; see def() */
        Meta(
            std::string const &_name,
            blitz::Array<TypeT, RANK> const &_arr,
            blitz::TinyVector<int, RANK> const &_shape,
            std::array<std::string,RANK> _sdims,
            std::vector<std::tuple<std::string, std::string>> _attr)
        : name(_name), arr(_arr), shape(_shape), sdims(std::move(_sdims)), attr(std::move(_attr)) {}

    public:
        void set_shape(
            blitz::TinyVector<int, RANK> const &_shape,
            std::array<std::string,RANK> _sdims,
            bool check = true)
        {
            if (check && shape[0] >= 0) (*ibmisc_error)(-1,
                "ArrayBundle variable %s shape already set", name.c_str());
            shape = _shape;
            sdims = std::move(_sdims);
        }

        void allocate(bool check = true,
            blitz::GeneralArrayStorage<RANK> const &storage = blitz::GeneralArrayStorage<RANK>())
        {
            if (check && arr.data()) (*ibmisc_error)(-1,
                "ArrayBundle variable %s already allocated", name.c_str());
            arr.reference(blitz::Array<TypeT,RANK>(shape, storage));
        }

        void allocate(
            blitz::TinyVector<int, RANK> const &_shape,
            std::array<std::string,RANK> _sdims,
            bool check = true,
            blitz::GeneralArrayStorage<RANK> const &storage = blitz::GeneralArrayStorage<RANK>())
        {
            set_shape(_shape, std::move(_sdims), check);
            allocate(check, storage);
        }
    };    // struct Meta
    // -------------------------------------------------------------------

    IndexSet<std::string> index;
    std::vector<Meta> data;

    blitz::Array<TypeT, RANK> const &array(std::string const &name) const
        { return data[index.at(name)].arr; }

    blitz::Array<TypeT, RANK> &array(std::string const &name)
        { return data[index.at(name)].arr; }


    Meta const &at(std::string const &name) const
        { return data[index.at(name)]; }

    Meta &at(std::string const &name)
        { return data[index.at(name)]; }

private:

    static std::vector<std::tuple<std::string, std::string>> make_attrs(
        std::initializer_list<std::string> const &vattr)
    {
        std::vector<std::tuple<std::string, std::string>> ret;
        for (auto ii = vattr.begin(); ii != vattr.end(); ) {
            std::string const &key(*ii++);
            if (ii == vattr.end()) (*ibmisc_error)(-1,
                "Odd number of strings in (key,value) attr list");
            std::string const &value(*ii++);

            ret.push_back(std::make_tuple(key, value));
        }
        return ret;
    }
public:
    static Meta def(
        std::string const &name,
        std::initializer_list<std::string> const &vattr)
    {
        blitz::TinyVector<int,RANK> shape;
        std::array<std::string,RANK> sdims;
        for (int i=0; i<RANK; ++i) shape[i] = -1;
        return Meta(name, blitz::Array<TypeT,RANK>(), shape,
            std::move(sdims), make_attrs(vattr));
    }

    static Meta def(
        std::string const &name,
        blitz::TinyVector<int, RANK> const &shape,
        std::array<std::string,RANK> sdims,
        std::initializer_list<std::string> const &vattr)
    {
        return Meta(name, blitz::Array<TypeT,RANK>(), shape,
            std::move(sdims), make_attrs(vattr));
    }

    ArrayBundle(std::vector<Meta> _data) : data(std::move(_data))
    {
        for (Meta &meta : data) {
            index.insert(meta.name);
        }
    }




    Meta &add(
        std::string const &name,
        std::initializer_list<std::string> const &vattr)
    {
        data.push_back(def(name, vattr));
        index.insert(data.back().name);
        return data.back();
    }

    Meta &add(
        std::string const &name,
        blitz::TinyVector<int, RANK> const &shape,
        std::array<std::string,RANK> sdims,
        std::initializer_list<std::string> const &vattr)
    {
        data.push_back(def(name, shape, std::move(sdims), vattr));
        index.insert(data.back().name);
        return data.back();
    }


    // ------------------------------------------------------------------
    // Allocate All Variables in a Bundle

    void set_shape(
        blitz::TinyVector<int, RANK> const &shape,
        std::array<std::string,RANK> sdims,
        bool check = true)
    {
        for (auto &meta : data) {
            if (meta.shape[0] < 0)
                meta.set_shape(shape, std::move(sdims), check);
        }
    }

    void allocate(bool check = true,
        blitz::GeneralArrayStorage<RANK> const &storage = blitz::GeneralArrayStorage<RANK>())
    {
        for (auto &meta : data) {
            if (!meta.arr.data())
                meta.allocate(check, storage);
        }
    }

    void allocate(
        blitz::TinyVector<int, RANK> const &_shape,
        std::array<std::string,RANK> sdims,
        bool check = true,
        blitz::GeneralArrayStorage<RANK> const &storage = blitz::GeneralArrayStorage<RANK>())
    {
        for (auto &meta : data) {
            if (meta.shape[0] < 0 || !meta.data())
                meta.allocate(_shape, std::move(sdims), check, storage);
        }
    }

    // ------------------------------------------------------------------
    // Allocate Some Variables in a Bundle

    void set_shape(
        std::vector<std::string> const &vnames,
        blitz::TinyVector<int, RANK> const &shape,
        std::array<std::string,RANK> sdims,
        bool check = true)
    {
        for (auto &vname : vnames) {
            auto &meta(data[index.at(vname)]);
            meta.set_shape(shape, std::move(sdims), check);
        }
    }

    void allocate(
        std::vector<std::string> const &vnames,
        bool check = true,
        blitz::GeneralArrayStorage<RANK> const &storage = blitz::GeneralArrayStorage<RANK>())
    {
        for (auto &vname : vnames) {
            auto &meta(data[index.at(vname)]);
            meta.allocate(check, storage);
        }
    }

    void allocate(
        std::vector<std::string> const &vnames,
        blitz::TinyVector<int, RANK> const &_shape,
        std::array<std::string,RANK> sdims,
        bool check = true,
        blitz::GeneralArrayStorage<RANK> const &storage = blitz::GeneralArrayStorage<RANK>())
    {
        for (auto &vname : vnames) {
            auto &meta(data[index.at(vname)]);
            meta.allocate(_shape, std::move(sdims), check, storage);
        }
    }


    void ncio(
        NcIO &ncio,
        std::vector<std::string> const &vars,
        bool alloc,
        std::string const &prefix,
        std::string const &snc_type,
        blitz::GeneralArrayStorage<RANK> const &storage = blitz::GeneralArrayStorage<RANK>());    // In case we need to allocate

};
// -----------------------------------------------------------------


// --------------------------------------------------------------------

template<class TypeT, int RANK>
void ArrayBundle<TypeT,RANK>::ncio(
    NcIO &ncio,
    std::vector<std::string> const &vars,
    bool alloc,
    std::string const &prefix,
    std::string const &snc_type,
    blitz::GeneralArrayStorage<RANK> const &storage)    // In case we need to allocate

{
    std::vector<std::string> all_vars;
    std::vector<std::string> const *myvars;
    if (vars.size() == 0) {
        for (size_t i=0; i<index.size(); ++i) {
            all_vars.push_back(index[i]);
        }
        myvars = &all_vars;
    } else {
        myvars = &vars;
    }

    for (auto &var : *myvars) {
        int i=index.at(var);

        auto &meta(data[i]);

        // Set up the dimensions
        auto dims_f(get_or_add_dims(ncio,
            meta.arr,
            to_vector(meta.sdims)));

        // Read/Write the NetCDF variable
        // (will auto-reverse dims if it detects column major)
        auto ncvar(ncio_blitz(ncio, meta.arr, alloc, prefix + meta.name, snc_type, dims_f, storage));

        // Read/write attributes
        for (auto &kv : meta.attr) {
            std::string const &name(std::get<0>(kv));
            std::string &value(std::get<1>(kv));    // Read back into ArrayMeta

            get_or_put_att(ncvar, ncio.rw, name, value);
        }
    }
}



}

#endif // IBMISC_BUNDLE_HPP