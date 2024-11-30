#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <utility>
#include <type_traits>
#include <cstdint>

namespace soh::save {

static constexpr int VERSION = 0;
static bool littleEndian() {
    const int n = 1;
    return reinterpret_cast<const char*>(&n)[0] == 1;
}

class writer {
  public:
    writer() {
        //
    }
    ~writer() {
        if (buf_) {
            free(static_cast<void*>(buf_));
        }
    }
    writer(const writer& b) : capacity_(b.capacity_), size_(b.size_), cur_(b.cur_) {
        if (b.buf_) {
            buf_ = static_cast<char*>(malloc(b.capacity_));
            memcpy(static_cast<void*>(buf_), static_cast<void*>(b.buf_), b.capacity_);
        }
    }
    writer(writer&& b) : capacity_(b.capacity_), size_(b.size_), cur_(b.cur_), buf_(b.buf_) {
        b.buf_ = nullptr;
        b.size_ = 0;
        b.capacity_ = 0;
    }
    writer& operator=(const writer& b) {
        writer::~writer();
        new (static_cast<void*>(this)) writer(b);
    }
    writer& operator=(writer&& b) {
        writer::~writer();
        new (static_cast<void*>(this)) writer(b);
    }

    writer& operator<<(int n) {
        grow_size(4);
        char* c = reinterpret_cast<char*>(&n);
        buf_[cur_++] = c[0];
        buf_[cur_++] = c[1];
        buf_[cur_++] = c[2];
        buf_[cur_++] = c[3];
        return *this;
    }
    writer& operator<<(int64_t n) {
        grow_size(sizeof(void*));
        char* c = reinterpret_cast<char*>(&n);
        for (int i = 0; i < sizeof(void*); i++) {
            buf_[cur_++] = c[i];
        }
        return *this;
    }
    writer& operator<<(const std::string& s) {
        operator<<(int(s.size()));
        grow_size(s.size());
        for (int i = 0; i < s.size(); i++) {
            buf_[cur_++] = s[i];
        }
        return *this;
    }
    writer& align(int n) {
        int c = n - (cur_ % n);
        grow_size(c);
        return *this;
    }
    writer& dump(const void* ptr, int size) {
        grow_size(size);
        const char* c = static_cast<const char*>(ptr);
        for (int i = 0; i < size; i++) {
            buf_[cur_++] = c[i];
        }
        return *this;
    }

  private:
    void grow_size(int n) {
        ensure_size(size_ + n);
    }
    void ensure_size(int size) {
        if (size < capacity_) {
            size_ = size;
            return;
        }
        capacity_ = ((size + 1) >> 1) + (size + 1);

        char* old = buf_;
        buf_ = static_cast<char*>(malloc(capacity_));
        if (old) {
            memcpy(static_cast<void*>(buf_), static_cast<void*>(old), size_);
            free(static_cast<void*>(old));
        }
        size_ = size;
    }
    char* buf_ = nullptr;
    int size_ = 0;
    int capacity_ = 0;
    int cur_ = 0;
};

static constexpr int UNSIGNED_BIT = 32;
enum class Type {
    INT8 = 0,
    INT16 = 1,
    INT32 = 2,
    INT64 = 3,
    UINT8 = 0 | UNSIGNED_BIT,
    UINT16 = 1 | UNSIGNED_BIT,
    UINT32 = 2 | UNSIGNED_BIT,
    UINT64 = 3 | UNSIGNED_BIT,
    FLOAT32 = 4,
    FLOAT64 = 5,
    STRUCT = 6,
    ARRAY = 7,
    POINTER = 8,
    FUNCPTR = 9
};

struct TypeBase {
    Type type;
    TypeBase(Type type) : type(type) {
        //
    }
    virtual ~TypeBase() {
    }
    virtual int getSize() const {
        return -1;
    }
    virtual const char* getName() const {
        return "error";
    }
    virtual void serialize(writer& w) const {
        w << int(type);
    }
};

struct NumericType : public TypeBase {
    NumericType(Type type) : TypeBase(type) {
        switch (Type(int(type) & ~UNSIGNED_BIT)) {
            case Type::INT8:
                size = 1;
                break;
            case Type::INT16:
                size = 2;
                break;
            case Type::INT32:
            case Type::FLOAT32:
                size = 4;
                break;
            case Type::INT64:
            case Type::FLOAT64:
                size = 8;
                break;
        }
        updateName();
    }
    void updateName() {
        switch (TypeBase::type) {
            case Type::INT8:
                name = "int8";
                break;
            case Type::INT16:
                name = "int16";
                break;
            case Type::INT32:
                name = "int32";
                break;
            case Type::INT64:
                name = "int64";
                break;
            case Type::UINT8:
                name = "int8";
                break;
            case Type::UINT16:
                name = "int16";
                break;
            case Type::UINT32:
                name = "int32";
                break;
            case Type::UINT64:
                name = "int64";
                break;
            case Type::FLOAT32:
                name = "float32";
                break;
            case Type::FLOAT64:
                name = "float64";
                break;
        }
    }
    virtual int getSize() const {
        return size;
    }
    virtual const char* getName() const {
        return name.c_str();
    }

  private:
    int size = -1;
    std::string name;
};

struct ArrayType : public TypeBase {
    ArrayType(int dimen, TypeBase* subtype) : TypeBase(Type::ARRAY), dimen(dimen), subType(subtype) {
        char buf[64];
        sprintf(buf, "[%d]", dimen);
        name = std::string(subtype->getName()) + buf;
    }

    virtual int getSize() const {
        return subType->getSize() * dimen;
    }
    virtual const char* getName() const {
        return name.c_str();
    }
    virtual void serialize(writer &w) const {
        TypeBase::serialize(w);
        w << dimen;
        w << subType->getName();
    }
    int dimen;
    TypeBase* subType;
    std::string name;
};

struct PointerType : public TypeBase {
    PointerType(TypeBase* subtype) : TypeBase(Type::POINTER), subType(subtype) {
        name = std::string("*") + subtype->getName();
    }
    virtual int getSize() const {
        return sizeof(void*);
    }
    virtual const char* getName() const {
        return name.c_str();
    }
    virtual void serialize(writer& w) const {
        TypeBase::serialize(w);
        w << subType->getName();
    }

    TypeBase* subType;
    std::string name;
};

template <typename T> static const TypeBase* bindType();

template <typename T> struct StructTypeDef : public TypeBase {
    struct Member {
        std::string name = "";
        int offset = 0;
        const TypeBase* type = nullptr;
    };

    std::vector<Member> members;

    StructTypeDef(const char* name, int size) : TypeBase(Type::STRUCT), name(name), size(size) {
    }
    StructTypeDef(const StructTypeDef& b) = default;
    StructTypeDef<void>* toStructType() {
        StructTypeDef<void>* b = new StructTypeDef<void>(name.c_str(), size);
        b->name = name;
        b->size = size;
        b->members = members;
        return b;
    }

    virtual int getSize() const {
        return size;
    }
    virtual const char* getName() const {
        return name.c_str();
    }
    virtual void serialize(writer& w) const {
        TypeBase::serialize(w);
        w << size;
        w << name;
        w << int(members.size());
        for (auto& member : members) {
            w << member.name;
            w << member.type->getName();
            w << member.offset;
        }
    }

    template <typename T> StructTypeDef& add(const char* name, T* ptr) {
        Member member;

        member.name = name;
        member.type = bindType<T>();
        member.offset = reinterpret_cast<int64_t>(ptr);

        members.push_back(member);
        return *this;
    }

    std::string name;
    int size;
    T* structBase = nullptr;
};

using StructType = StructTypeDef<void>;

struct Struct {
    std::string name;
};

#define structAdd(s, p) s->add(#p, &s->structBase->p)

} // namespace soh::save