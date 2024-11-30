#include "save.h"
#include <span>
#include <map>

namespace soh::save {
template <> const TypeBase* bindType<char>() {
    return new NumericType(Type::INT8);
}
template <> const TypeBase* bindType<signed char>() {
    return new NumericType(Type::INT8);
}
template <> const TypeBase* bindType<signed short>() {
    return new NumericType(Type::INT16);
}
template <> const TypeBase* bindType<signed int>() {
    return new NumericType(Type::INT32);
}
template <> const TypeBase* bindType<unsigned char>() {
    return new NumericType(Type::UINT8);
}
template <> const TypeBase* bindType<unsigned short>() {
    return new NumericType(Type::UINT16);
}
template <> const TypeBase* bindType<unsigned int>() {
    return new NumericType(Type::UINT32);
}
template <> const TypeBase* bindType<float>() {
    return new NumericType(Type::FLOAT32);
}
template <> const TypeBase* bindType<double>() {
    return new NumericType(Type::FLOAT64);
}

static void recurseType(const TypeBase* type, std::map<std::string, const TypeBase*>& visit) {
    if (visit.contains(type->getName())) {
        return;
    }

    visit[type->getName()] = type;
    if (type->type == Type::ARRAY) {
        const ArrayType* array = static_cast<const ArrayType*>(type);
        recurseType(array->subType, visit);
    } else if (type->type == Type::POINTER) {
        const PointerType* array = static_cast<const PointerType*>(type);
        recurseType(array->subType, visit);
    } else if (type->type == Type::STRUCT) {
        const StructType* type = static_cast<const StructType*>(type);
        for (auto& member : type->members) {
            recurseType(member.type, visit);
        }
    }
}
void writeSchema(writer& w, std::span<const TypeBase*> types, std::map<std::string, const TypeBase*>& visit) {
    for (auto* type : types) {
        recurseType(type, visit);
    }

    w << int(visit.size());
    for (auto pair : visit) {
        pair.second->serialize(w);
    }
}

void writeFile(writer& w, std::span<std::pair<const TypeBase*, void*>> objects) {
    w << "SAVE";
    w << VERSION;
    w << int(littleEndian());

    std::map<std::string, const TypeBase*> typeMap;

    // write schema and generate typeMap
    {
        std::vector<const TypeBase*> types;
        for (auto& item : objects) {
            types.push_back(item.first);
        }
        writeSchema(w, types, typeMap);
    }

    std::map<std::string, int> typeIdMap;
    {
        int i = 0;
        for (auto pair : typeMap) {
            typeIdMap[pair.first] = i++;
        }
    }

    // write objects
    for (auto& item : objects) {
        w << typeIdMap[item.first->getName()];
        w << int(item.first->getSize()); // write size
        w << intptr_t(item.second);      // write ptr
        w.dump(item.second, item.first->getSize());
    }
}

} // namespace soh::save
extern "C" void testSaver() {
    using namespace soh::save;

    struct Test {
        int a;
        float b;
        char c;
    };

    StructTypeDef<Test>* def = new StructTypeDef<Test>("Test", sizeof(Test));
    structAdd(def, a);
    structAdd(def, b);
    structAdd(def, c);
}